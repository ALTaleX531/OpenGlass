#!/usr/bin/env python3
"""Freeze candidates, collect resolvable symbols, and verify the built-in catalog."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
from typing import Any

import audit_symbol_resolution
import audit_winbindex_revisions
import projection_schema
import projection_source_check
import symbol_catalog
from projection_schema import SchemaError


class UnresolvableCandidate(SchemaError):
    """A catalog candidate whose required symbols cannot be resolved."""


ARCHITECTURE_BUILDS = {
    "legacy": (17763, 18362, 19041, 20348, 22000, 22621, 26100),
    "milcomp": (28000,),
}
MODULES = ("udwm", "dwmcore")
FILENAMES = {"udwm": "uDWM.dll", "dwmcore": "dwmcore.dll"}


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    inventory = subparsers.add_parser("inventory", help="refresh frozen Winbindex inventories")
    inventory.add_argument("repo", type=Path)
    inventory.add_argument("--output-root", type=Path)
    inventory.add_argument("--cache", type=Path)

    collect = subparsers.add_parser("collect", help="collect every missing frozen inventory record")
    collect.add_argument("repo", type=Path)
    collect.add_argument("--cache", type=Path, required=True)
    collect.add_argument("--dbghelp", type=Path, required=True)
    collect.add_argument("--missing-output", type=Path, required=True)
    collect.add_argument(
        "--replace-resolver",
        action="store_true",
        help="clear and recollect every record when changing the architecture DbgHelp identity",
    )


    verify = subparsers.add_parser("verify", help="validate the committed source corpus")
    verify.add_argument("repo", type=Path)
    verify.add_argument("--cache", type=Path)
    verify.add_argument("--dbghelp", type=Path)
    return parser.parse_args(argv)


def atomic_write(path: Path, contents: bytes) -> None:
    if path.is_file() and path.read_bytes() == contents:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    try:
        with os.fdopen(descriptor, "wb") as stream:
            stream.write(contents)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary_name, path)
    except Exception:
        try:
            os.unlink(temporary_name)
        except OSError:
            pass
        raise


def write_json(path: Path, value: Any) -> None:
    atomic_write(path, (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise SchemaError(f"cannot read {path}: {error}") from error
    if not isinstance(value, dict):
        raise SchemaError(f"{path} must contain an object")
    return value


def load_schemas(repo: Path, architecture: str) -> dict[str, projection_schema.ProjectionSchema]:
    constants = projection_source_check.load_os_constants(repo)
    return {
        module: projection_schema.validate_schema(
            repo / "OpenGlass" / "ProjectionSchemas" / architecture / f"{module}.json",
            module,
            constants,
        )
        for module in MODULES
    }


def inventory_snapshot(filename: str, loaded: audit_winbindex_revisions.LoadedIndex) -> dict[str, Any]:
    records: list[dict[str, Any]] = []
    builds = sorted({build for values in ARCHITECTURE_BUILDS.values() for build in values})
    for build in builds:
        arguments = argparse.Namespace(build=build, min_revision=None, max_revision=None)
        samples = audit_winbindex_revisions.discover_samples(loaded, arguments)
        records.extend(sample.inventory_record(filename, loaded.compressed_sha256) for sample in samples)
    return {
        "schema_version": symbol_catalog.SCHEMA_VERSION,
        "module": filename,
        "source_index_sha256": loaded.compressed_sha256,
        "recognized_builds": builds,
        "records": records,
    }


def refresh_inventory(repo: Path, output_root: Path | None, cache: Path | None) -> None:
    root = (output_root or repo / "OpenGlass" / "SymbolCatalogs" / "inventory").resolve()
    cache_root = (cache or Path(tempfile.gettempdir()) / "openglass-winbindex").resolve()
    for module in MODULES:
        filename = FILENAMES[module]
        loaded = audit_winbindex_revisions.load_index(filename, cache_root)
        snapshot = inventory_snapshot(filename, loaded)
        write_json(root / f"{module}.json", snapshot)
        counts = {
            build: sum(record["version"]["build"] == build for record in snapshot["records"])
            for build in snapshot["recognized_builds"]
        }
        print(f"{module}: index={loaded.compressed_sha256} records={len(snapshot['records'])} counts={counts}")


def read_pe_headers(path: Path) -> tuple[int, int, int, list[tuple[int, int, int]]]:
    try:
        data = path.read_bytes()
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe:pe + 4] != b"PE\0\0":
            raise ValueError("missing PE signature")
        machine, section_count, timestamp, _symbols, _symbol_count, optional_size = struct.unpack_from("<HHIIIH", data, pe + 4)
        optional = pe + 24
        magic = struct.unpack_from("<H", data, optional)[0]
        if magic not in {0x10B, 0x20B} or optional_size < 60:
            raise ValueError("invalid optional header")
        size_of_image = struct.unpack_from("<I", data, optional + 56)[0]
        if not machine or not size_of_image or not section_count:
            raise ValueError("missing machine, SizeOfImage, or sections")
        sections: list[tuple[int, int, int]] = []
        section_table = optional + optional_size
        for index in range(section_count):
            section = section_table + index * 40
            virtual_size, virtual_address, raw_size = struct.unpack_from("<III", data, section + 8)
            characteristics = struct.unpack_from("<I", data, section + 36)[0]
            section_size = max(virtual_size, raw_size)
            if not section_size or virtual_address >= size_of_image:
                raise ValueError("invalid PE section")
            sections.append((virtual_address, section_size, characteristics))
        return machine, timestamp, size_of_image, sections
    except (OSError, IndexError, struct.error, ValueError) as error:
        raise SchemaError(f"cannot read audited PE identity from {path}: {error}") from error


def address_class(rva: int, sections: list[tuple[int, int, int]], context: str) -> str:
    for virtual_address, size, characteristics in sections:
        if virtual_address <= rva < virtual_address + size:
            return "Code" if characteristics & 0x20000000 else "Data"
    raise SchemaError(f"{context}: RVA is outside the audited PE sections")




def normalize_audit(
    report: dict[str, Any],
    architecture: str,
    schemas: dict[str, projection_schema.ProjectionSchema],
) -> dict[str, Any]:
    context = "symbol audit"
    if report.get("schema_version") != 1 or report.get("architecture") != architecture:
        raise SchemaError(f"{context}: schema version or architecture mismatch")
    module = report.get("module")
    if module not in schemas:
        raise SchemaError(f"{context}: invalid module")
    if report.get("evidence") != "production_candidate":
        raise SchemaError(f"{context}: evidence must be production_candidate")
    if report.get("module_supported") is not True:
        raise UnresolvableCandidate(f"{context}: Required symbols do not resolve uniquely")
    if report.get("configuration") != "release":
        raise SchemaError(f"{context}: configuration must be release")
    requested = report.get("requested_version")
    if requested != report.get("image_version") or not isinstance(requested, dict):
        raise SchemaError(f"{context}: requested and PE versions must agree")
    version = projection_schema.parse_version(requested, f"{context}.requested_version", allow_open=False)

    image = report.get("image")
    pdb = report.get("pdb")
    dbghelp = report.get("dbghelp")
    if not isinstance(image, dict) or not isinstance(pdb, dict) or not isinstance(dbghelp, dict):
        raise SchemaError(f"{context}: image, PDB, and DbgHelp identities are required")
    if pdb.get("paired") is not True:
        raise SchemaError(f"{context}: PDB must be paired")
    image_path = Path(str(image.get("path", "")))
    pdb_path = Path(str(pdb.get("path", "")))
    dbghelp_path = Path(str(dbghelp.get("path", "")))
    for label, path, expected in (
        ("image", image_path, image.get("sha256")),
        ("PDB", pdb_path, pdb.get("sha256")),
        ("DbgHelp", dbghelp_path, dbghelp.get("sha256")),
    ):
        if not path.is_file() or audit_symbol_resolution.sha256_file(path).lower() != str(expected).lower():
            raise SchemaError(f"{context}: {label} file is unavailable or changed")
    machine, timestamp, size_of_image, sections = read_pe_headers(image_path)
    if machine != audit_winbindex_revisions.AMD64:
        raise SchemaError(f"{context}: image machine must be x64")

    codeview = image.get("codeview")
    pdb_identity = pdb.get("identity")
    if not isinstance(codeview, dict) or not isinstance(pdb_identity, dict):
        raise SchemaError(f"{context}: CodeView and PDB identities are required")
    if any(codeview.get(key) != pdb_identity.get(key) for key in ("name", "guid", "age")):
        raise SchemaError(f"{context}: CodeView and PDB identity disagree")

    descriptors = report.get("descriptors")
    if not isinstance(descriptors, list):
        raise SchemaError(f"{context}: descriptors must be an array")
    schema = schemas[module]
    order = {symbol["id"]: index for index, symbol in enumerate(schema["symbols"])}
    symbols: dict[str, dict[str, str]] = {}
    for index, descriptor in enumerate(descriptors):
        descriptor_context = f"{context}.descriptors[{index}]"
        symbol_id = descriptor.get("id") if isinstance(descriptor, dict) else None
        if not isinstance(symbol_id, str) or symbol_id not in order:
            raise SchemaError(f"{descriptor_context}: invalid descriptor")
        status = descriptor.get("status")
        if status == "inactive":
            continue
        if status not in symbol_catalog.STATUSES:
            raise SchemaError(f"{descriptor_context}: invalid status")
        if status != "unique":
            if descriptor.get("requirement") == "required":
                raise UnresolvableCandidate(f"{descriptor_context}: Required symbol must resolve uniquely")
            continue
        rvas = descriptor.get("rvas")
        names = sorted(name for name in descriptor.get("matched_names", []) if isinstance(name, str) and name)
        if not isinstance(rvas, list) or len(rvas) != 1 or not names:
            raise SchemaError(f"{descriptor_context}: unique result needs one RVA and matched name")
        try:
            rva = int(rvas[0], 0)
        except (TypeError, ValueError) as error:
            raise SchemaError(f"{descriptor_context}: invalid RVA") from error
        actual_class = address_class(rva, sections, descriptor_context)
        symbol = schema["symbols"][order[symbol_id]]
        expected_class = "Data" if symbol["kind"] == "projected_variable" else "Code"
        if actual_class != expected_class:
            raise UnresolvableCandidate(f"{descriptor_context}: RVA address class must be {expected_class}")
        symbols[symbol_id] = {"name": names[0], "rva": f"0x{rva:X}"}

    return {
        "revision": version.revision,
        "image": {
            "time_date_stamp": timestamp,
            "size_of_image": size_of_image,
            "sha256": str(image["sha256"]).lower(),
        },
        "pdb": {
            "name": pdb_identity["name"],
            "guid": pdb_identity["guid"],
            "age": pdb_identity["age"],
            "sha256": str(pdb["sha256"]).lower(),
        },
        "resolution_contract": symbol_catalog.resolution_contract(schema, version, "release"),
        "symbols": symbols,
    }


def source_path(root: Path, architecture: str, module: str, build: int) -> Path:
    return root / architecture / module / f"{build}.json"


def read_frozen_inventory(root: Path) -> dict[tuple[str, int], list[dict[str, Any]]]:
    result: dict[tuple[str, int], list[dict[str, Any]]] = {}
    for module in MODULES:
        raw = read_json(root / "inventory" / f"{module}.json")
        for record in raw.get("records", []):
            version = record.get("version", {})
            normalized = dict(record)
            normalized["module"] = module
            result.setdefault((module, int(version["build"])), []).append(normalized)
    for records in result.values():
        records.sort(key=lambda item: (item["version"]["revision"], item["sha256"]))
    return result


def dbghelp_identity(path: Path) -> dict[str, str]:
    version = audit_symbol_resolution.image_version(path)
    if version is None:
        raise SchemaError("production DbgHelp must expose a file version")
    return {
        "version": f"{version.build}.{version.revision}",
        "sha256": audit_symbol_resolution.sha256_file(path).lower(),
    }


def require_catalog_resolver(root: Path, architecture: str, identity: dict[str, str]) -> None:
    index_path = root / architecture / "index.json"
    if not index_path.is_file():
        return
    index = read_json(index_path)
    if "resolver" not in index:
        return
    expected = symbol_catalog.resolver_identity(index["resolver"], f"{index_path}.resolver")
    if expected != identity:
        raise SchemaError(
            f"{architecture} catalog uses DbgHelp {expected['version']} {expected['sha256']}; "
            "recollect the complete architecture catalog before changing resolver"
        )


def write_indexes(root: Path, resolver: dict[str, str], *, replace_resolver: bool = False) -> None:
    for architecture, builds in ARCHITECTURE_BUILDS.items():
        index_path = root / architecture / "index.json"
        reset_records = False
        if index_path.is_file():
            try:
                require_catalog_resolver(root, architecture, resolver)
            except SchemaError:
                if not replace_resolver:
                    raise
                reset_records = True
        else:
            has_records = any(
                source_path(root, architecture, module, build).is_file() and
                bool(read_json(source_path(root, architecture, module, build)).get("records"))
                for module in MODULES
                for build in builds
            )
            if has_records and not replace_resolver:
                raise SchemaError(
                    f"{architecture} catalog index is missing while shards contain records; "
                    "restore the index or use --replace-resolver to recollect the complete architecture catalog"
                )
            reset_records = has_records

        sources = [f"{module}/{build}.json" for module in MODULES for build in builds]
        index = {
            "schema_version": symbol_catalog.SCHEMA_VERSION,
            "architecture": architecture,
            "resolver": {"dbghelp": resolver},
            "sources": sources,
        }
        write_json(index_path, index)
        for module in MODULES:
            for build in builds:
                path = source_path(root, architecture, module, build)
                existing = read_json(path) if path.is_file() else {}
                records = [] if reset_records else (
                    existing.get("records", []) if isinstance(existing.get("records", []), list) else []
                )
                write_json(path, {
                    "schema_version": symbol_catalog.SCHEMA_VERSION,
                    "records": records,
                })




def write_collision_report(root: Path, records: list[dict[str, Any]]) -> None:
    records.sort(key=lambda item: (
        item["module"], item["version"]["build"], item["version"]["revision"], item["expected_sha256"]
    ))
    write_json(root / "inventory" / "symbol-server-collisions.json", {
        "schema_version": 1,
        "records": records,
    })


def load_collision_report(
    root: Path,
    inventory: dict[tuple[str, int], list[dict[str, Any]]],
    *,
    drop_stale: bool = False,
) -> list[dict[str, Any]]:
    path = root / "inventory" / "symbol-server-collisions.json"
    if drop_stale and not path.is_file():
        return []
    report = read_json(path)
    if report.get("schema_version") != 1 or not isinstance(report.get("records"), list):
        raise SchemaError(f"{path} must contain schema_version 1 and a records array")
    result: list[dict[str, Any]] = []
    seen: set[tuple[str, int, int, str]] = set()
    for index, value in enumerate(report["records"]):
        context = f"{path}.records[{index}]"
        if not isinstance(value, dict) or set(value) != {
            "module", "version", "expected_sha256", "returned_sha256", "symbol_server_key"
        }:
            raise SchemaError(f"{context} has invalid properties")
        module = value["module"]
        version = value["version"]
        expected = str(value["expected_sha256"]).lower()
        returned = str(value["returned_sha256"]).lower()
        if module not in MODULES or not isinstance(version, dict):
            raise SchemaError(f"{context} has an invalid module or version")
        try:
            build = int(version["build"])
            revision = int(version["revision"])
        except (KeyError, TypeError, ValueError) as error:
            raise SchemaError(f"{context}.version is invalid") from error
        if not symbol_catalog.SHA256_RE.fullmatch(expected) or not symbol_catalog.SHA256_RE.fullmatch(returned) or expected == returned:
            raise SchemaError(f"{context} has invalid collision hashes")
        candidates = [
            record for record in inventory.get((module, build), [])
            if record["version"]["revision"] == revision and record["sha256"] == expected
        ]
        if not candidates and drop_stale:
            continue
        if len(candidates) != 1:
            raise SchemaError(f"{context} does not identify one frozen inventory candidate")
        candidate = candidates[0]
        expected_key = f"{int(candidate['time_date_stamp']):08X}{int(candidate['size_of_image']):x}"
        if value["symbol_server_key"] != expected_key:
            raise SchemaError(f"{context}.symbol_server_key disagrees with the frozen inventory")
        identity = (module, build, revision, expected)
        if identity in seen:
            raise SchemaError(f"{context} duplicates a collision identity")
        seen.add(identity)
        result.append(value)
    return result


def validate_collision_report(root: Path, inventory: dict[tuple[str, int], list[dict[str, Any]]]) -> int:
    return len(load_collision_report(root, inventory))
def classify_exclusion(error: Exception) -> str | None:
    if isinstance(error, UnresolvableCandidate):
        return "symbol_resolution_rejected"
    detail = str(error)
    if "cannot acquire paired PDB" in detail and detail.count("HTTP Error 404") >= 2:
        return "pdb_unavailable"
    return None


def write_exclusion_report(root: Path, records: list[dict[str, Any]]) -> None:
    records.sort(key=lambda item: (
        item["module"], item["version"]["build"], item["version"]["revision"], item["sha256"]
    ))
    write_json(root / "inventory" / "catalog-exclusions.json", {
        "schema_version": 1,
        "records": records,
    })


def load_exclusion_report(
    root: Path,
    inventory: dict[tuple[str, int], list[dict[str, Any]]],
    *,
    drop_stale: bool = False,
) -> list[dict[str, Any]]:
    path = root / "inventory" / "catalog-exclusions.json"
    if drop_stale and not path.is_file():
        return []
    report = read_json(path)
    if report.get("schema_version") != 1 or not isinstance(report.get("records"), list):
        raise SchemaError(f"{path} must contain schema_version 1 and a records array")
    result: list[dict[str, Any]] = []
    seen: set[tuple[str, int, int, str]] = set()
    for index, value in enumerate(report["records"]):
        context = f"{path}.records[{index}]"
        if not isinstance(value, dict) or set(value) != {"module", "version", "sha256", "kind", "detail"}:
            raise SchemaError(f"{context} has invalid properties")
        module = value["module"]
        version = value["version"]
        digest = str(value["sha256"]).lower()
        if module not in MODULES or not isinstance(version, dict):
            raise SchemaError(f"{context} has an invalid module or version")
        try:
            build = int(version["build"])
            revision = int(version["revision"])
        except (KeyError, TypeError, ValueError) as error:
            raise SchemaError(f"{context}.version is invalid") from error
        if (
            not symbol_catalog.SHA256_RE.fullmatch(digest) or
            value["kind"] not in {"pdb_unavailable", "symbol_resolution_rejected"} or
            not isinstance(value["detail"], str) or not value["detail"]
        ):
            raise SchemaError(f"{context} has invalid exclusion evidence")
        candidates = [
            record for record in inventory.get((module, build), [])
            if record["version"]["revision"] == revision and record["sha256"] == digest
        ]
        if not candidates and drop_stale:
            continue
        if len(candidates) != 1:
            raise SchemaError(f"{context} does not identify one frozen inventory candidate")
        identity = (module, build, revision, digest)
        if identity in seen:
            raise SchemaError(f"{context} duplicates an exclusion identity")
        seen.add(identity)
        result.append(value)
    return result




def collect(
    repo: Path,
    cache: Path,
    dbghelp: Path,
    missing_output: Path,
    *,
    replace_resolver: bool = False,
) -> int:
    repo = repo.resolve()
    root = repo / "OpenGlass" / "SymbolCatalogs"
    if not dbghelp.is_file():
        raise SchemaError(f"explicit DbgHelp is unavailable: {dbghelp}")
    dbghelp = dbghelp.resolve()
    resolver = dbghelp_identity(dbghelp)
    inventory = read_frozen_inventory(root)
    write_indexes(root, resolver, replace_resolver=replace_resolver)
    schemas = {architecture: load_schemas(repo, architecture) for architecture in ARCHITECTURE_BUILDS}
    missing: list[dict[str, Any]] = []
    collisions = {
        (
            item["module"],
            item["version"]["build"],
            item["version"]["revision"],
            item["expected_sha256"],
        ): item
        for item in load_collision_report(root, inventory, drop_stale=True)
    }
    exclusions = {
        (
            item["module"],
            item["version"]["build"],
            item["version"]["revision"],
            item["sha256"],
        ): item
        for item in load_exclusion_report(root, inventory, drop_stale=True)
    }

    for architecture, builds in ARCHITECTURE_BUILDS.items():
        for build in builds:
            for module in MODULES:
                path = source_path(root, architecture, module, build)
                source = read_json(path)
                records = inventory.get((module, build), [])
                inventory_hashes = {record["sha256"] for record in records}
                existing = {
                    record["image"]["sha256"]: record
                    for record in source["records"]
                    if record["image"]["sha256"] in inventory_hashes
                }
                for position, inventory_record in enumerate(records, 1):
                    digest = inventory_record["sha256"]
                    version = inventory_record["version"]
                    collision_identity = (module, version["build"], version["revision"], digest)
                    selected_version = projection_schema.Version(version["build"], version["revision"])
                    expected_contract = symbol_catalog.resolution_contract(
                        schemas[architecture][module], selected_version, "release"
                    )
                    if digest in existing and existing[digest].get("resolution_contract") == expected_contract:
                        collisions.pop(collision_identity, None)
                        exclusions.pop(collision_identity, None)
                        continue
                    existing.pop(digest, None)
                    label = f"{version['build']}.{version['revision']}"
                    print(f"{architecture}/{module}/{label} [{position}/{len(records)}]", file=sys.stderr)
                    try:
                        sample = audit_winbindex_revisions.Sample(
                            audit_winbindex_revisions.AUDIT.Version(version["build"], version["revision"]), digest,
                            int(inventory_record["time_date_stamp"]), int(inventory_record["size_of_image"]),
                            int(inventory_record["file_size"]), tuple(inventory_record.get("provenance", [])),
                        )
                        image = audit_winbindex_revisions.ensure_image(cache, FILENAMES[module], sample)
                        pdb = audit_winbindex_revisions.prime_symbol_cache(image, cache)
                        audit_args = argparse.Namespace(
                            repo=repo, architecture=architecture, module=module, version=label,
                            image=image, symbol_path=pdb, dbghelp=dbghelp,
                            configuration="release", stable_id=None,
                        )
                        report = audit_symbol_resolution.inspect(audit_args)
                        normalized = normalize_audit(report, architecture, schemas[architecture])
                        if normalized["image"]["sha256"] != digest:
                            raise SchemaError("audited PE SHA-256 disagrees with frozen inventory")
                        existing[digest] = normalized
                        collisions.pop(collision_identity, None)
                        exclusions.pop(collision_identity, None)
                    except (
                        UnresolvableCandidate,
                        audit_winbindex_revisions.AuditError,
                        audit_symbol_resolution.AuditError,
                        OSError,
                    ) as error:
                        if isinstance(error, audit_winbindex_revisions.SymbolServerIdentityCollision):
                            collisions[collision_identity] = {
                                "module": module,
                                "version": version,
                                "expected_sha256": error.sample.sha256,
                                "returned_sha256": error.received_sha256,
                                "symbol_server_key": error.sample.image_key(),
                            }
                            exclusions.pop(collision_identity, None)
                        else:
                            exclusion_kind = classify_exclusion(error)
                            if exclusion_kind is not None:
                                collisions.pop(collision_identity, None)
                                exclusions[collision_identity] = {
                                    "module": module,
                                    "version": version,
                                    "sha256": digest,
                                    "kind": exclusion_kind,
                                    "detail": str(error),
                                }
                        missing.append({
                            "module": module,
                            "version": version,
                            "sha256": digest,
                            "error": str(error),
                            "provenance": inventory_record.get("provenance", [inventory_record.get("source_label", "")]),
                        })
                source["records"] = sorted(existing.values(), key=lambda item: (
                    item["revision"], item["image"]["sha256"], item["pdb"]["guid"], item["pdb"]["age"],
                ))
                write_json(path, source)
    write_json(missing_output, {"schema_version": 1, "records": missing})
    write_collision_report(root, list(collisions.values()))
    write_exclusion_report(root, list(exclusions.values()))
    verify_sources(repo)
    if missing:
        print(f"catalog collection skipped {len(missing)} unavailable or unresolved candidates", file=sys.stderr)
    return 0


def verify_sources(repo: Path) -> dict[str, int]:
    counts: dict[str, int] = {}
    root = repo / "OpenGlass" / "SymbolCatalogs"
    catalog_identities: set[tuple[str, int, int, str]] = set()
    for architecture in ARCHITECTURE_BUILDS:
        schemas = load_schemas(repo, architecture)
        catalog = symbol_catalog.load_catalog_sources(
            root / architecture / "index.json",
            architecture,
            list(schemas.values()),
        )
        counts[architecture] = len(catalog["records"])
        print(f"{architecture}: records={counts[architecture]}")
        catalog_identities.update(
            (record["module"], record["version"].build, record["version"].revision, record["image_sha256"])
            for record in catalog["records"]
        )

    inventory = read_frozen_inventory(root)
    inventory_identities = {
        (module, build, record["version"]["revision"], record["sha256"])
        for (module, build), records in inventory.items()
        for record in records
    }
    collision_records = load_collision_report(root, inventory)
    collision_identities = {
        (record["module"], record["version"]["build"], record["version"]["revision"], record["expected_sha256"])
        for record in collision_records
    }
    exclusion_records = load_exclusion_report(root, inventory)
    exclusion_identities = {
        (record["module"], record["version"]["build"], record["version"]["revision"], record["sha256"])
        for record in exclusion_records
    }
    if (
        catalog_identities & collision_identities or
        catalog_identities & exclusion_identities or
        collision_identities & exclusion_identities
    ):
        raise SchemaError("catalog, collision, and exclusion identities must be disjoint")
    classified = catalog_identities | collision_identities | exclusion_identities
    if classified != inventory_identities:
        missing = sorted(inventory_identities - classified)
        extra = sorted(classified - inventory_identities)
        raise SchemaError(f"frozen inventory classification mismatch; missing={missing}, extra={extra}")
    print(f"symbol-server collisions={len(collision_records)}")
    print(f"catalog exclusions={len(exclusion_records)}")
    return counts




def replay(repo: Path, cache: Path, dbghelp: Path) -> None:
    if not dbghelp.is_file():
        raise SchemaError(f"explicit DbgHelp is unavailable: {dbghelp}")
    dbghelp = dbghelp.resolve()
    resolver = dbghelp_identity(dbghelp)
    root = repo / "OpenGlass" / "SymbolCatalogs"
    for architecture, builds in ARCHITECTURE_BUILDS.items():
        require_catalog_resolver(root, architecture, resolver)
        schemas = load_schemas(repo, architecture)
        for build in builds:
            for module in MODULES:
                source = read_json(source_path(root, architecture, module, build))
                for committed in source["records"]:
                    revision = committed["revision"]
                    image_hash = committed["image"]["sha256"]
                    image = cache / "images" / FILENAMES[module].lower() / f"{build}.{revision}" / image_hash / FILENAMES[module]
                    identity = committed["pdb"]
                    pdb_key = str(identity["guid"]).replace("-", "").upper() + str(identity["age"])
                    pdb = cache / "symbols" / identity["name"] / pdb_key / identity["name"]
                    report = audit_symbol_resolution.inspect(argparse.Namespace(
                        repo=repo, architecture=architecture, module=module,
                        version=f"{build}.{revision}", image=image,
                        symbol_path=pdb, dbghelp=dbghelp, configuration="release", stable_id=None,
                    ))
                    actual = normalize_audit(report, architecture, schemas)
                    if actual != committed:
                        raise SchemaError(f"evidence replay differs for {module} {build}.{revision} {image_hash}")
    print("evidence replay: all records match")


def run(args: argparse.Namespace) -> int:
    repo = args.repo.resolve()
    if args.command == "inventory":
        refresh_inventory(repo, args.output_root, args.cache)
        return 0
    if args.command == "collect":
        return collect(
            repo,
            args.cache.resolve(),
            args.dbghelp.resolve(),
            args.missing_output.resolve(),
            replace_resolver=args.replace_resolver,
        )
    verify_sources(repo)
    if (args.cache is None) != (args.dbghelp is None):
        raise SchemaError("--cache and --dbghelp must be supplied together")
    if args.cache is not None:
        replay(repo, args.cache.resolve(), args.dbghelp.resolve())
    return 0


def main(argv: list[str] | None = None) -> int:
    try:
        return run(parse_args(argv))
    except (SchemaError, audit_winbindex_revisions.AuditError, audit_symbol_resolution.AuditError, OSError) as error:
        print(f"maintain_symbol_catalog: error: {error}", file=sys.stderr)
        return 1
    except Exception as error:
        print(f"maintain_symbol_catalog: internal error: {error}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    raise SystemExit(main())
