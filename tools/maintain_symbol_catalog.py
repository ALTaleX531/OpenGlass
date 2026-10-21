#!/usr/bin/env python3
"""Collect CodeView/PDB symbol records and validate the built-in catalog."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
from typing import Any
import uuid

from tools.file_io import write_json
from tools import audit_symbol_resolution
from tools import audit_winbindex_revisions
from tools import projection_schema
from tools import symbol_catalog, projection_target
from tools.projection_schema import SchemaError


MODULES = projection_schema.MODULES
FILENAMES = {"udwm": "uDWM.dll", "dwmcore": "dwmcore.dll"}


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    collect = subparsers.add_parser("collect", help="collect new or stale records from recognized Winbindex builds")
    collect.add_argument("repo", type=Path)
    collect.add_argument("--cache", type=Path, required=True)
    collect.add_argument("--dbghelp", type=Path, required=True)
    collect.add_argument("--missing-output", type=Path, help="failure report (default: CACHE/collection-failures.json)")
    collect.add_argument(
        "--replace-resolver",
        action="store_true",
        help="clear and recollect every record when changing the architecture DbgHelp identity",
    )


    verify = subparsers.add_parser("verify", help="validate the committed source corpus")
    verify.add_argument("repo", type=Path)
    verify.add_argument("--cache", type=Path)
    verify.add_argument("--dbghelp", type=Path)
    projection_target.add_platform_argument(collect)
    projection_target.add_platform_argument(verify)
    return parser.parse_args(argv)


def load_schemas(repo: Path, architecture: str, platform: str = "x64") -> dict[str, projection_schema.ProjectionSchema]:
    return {
        module: projection_schema.validate_schema(
            projection_target.schema_path(repo, architecture, module),
            module, platform,
        )
        for module in MODULES
    }


def record_from_pdb(pdb: Path, identity: audit_symbol_resolution.PdbIdentity,
    version: projection_schema.Version, schema: projection_schema.ProjectionSchema, dbghelp: Path) -> dict[str, Any]:
    report = audit_symbol_resolution.inspect_pdb(pdb, schema, version, dbghelp)
    if not report["module_supported"] or audit_symbol_resolution.required_failures(report["descriptors"]):
        raise SchemaError("Required symbols do not resolve uniquely for a supported module version")
    symbols = {}
    for descriptor in report["descriptors"]:
        if descriptor["status"] != "unique":
            continue
        rva = projection_schema.parse_uint(int(descriptor["rvas"][0], 0), f"{descriptor['id']}.rva", nonzero=True)
        symbols[descriptor["id"]] = {"name": min(descriptor["matched_names"]), "rva": f"0x{rva:X}"}
    return {
        "revision": version.revision,
        "pdb": {**identity.json(), "sha256": audit_symbol_resolution.sha256_file(pdb)},
        "symbols": symbols,
    }


def source_path(root: Path, architecture: str, module: str, build: int) -> Path:
    return root / architecture / module / f"{build}.json"


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
    index = projection_schema.read_json(index_path)
    expected = symbol_catalog.resolver_identity(index.get("resolver"), f"{index_path}.resolver")
    if expected != identity:
        raise SchemaError(
            f"{architecture} catalog uses DbgHelp {expected['version']} {expected['sha256']}; "
            "recollect the complete architecture catalog before changing resolver"
        )


def prepare_indexes(
    root: Path,
    resolver: dict[str, str],
    schemas: dict[str, dict[str, projection_schema.ProjectionSchema]],
    *,
    replace_resolver: bool = False,
) -> dict[Path, dict[str, Any]]:
    pending: dict[Path, dict[str, Any]] = {}
    for architecture, modules in schemas.items():
        sources = [f"{module}/{build}.json" for module in MODULES for build in modules[module]["known_builds"]]
        records = {
            relative: symbol_catalog.read_catalog_shard(root / architecture / relative)
            if (root / architecture / relative).is_file() else []
            for relative in sources
        }
        index_path = root / architecture / "index.json"
        reset_records = False
        if index_path.is_file():
            try:
                require_catalog_resolver(root, architecture, resolver)
            except SchemaError:
                if not replace_resolver:
                    raise
                reset_records = True
        elif any(records.values()):
            if not replace_resolver:
                raise SchemaError(
                    f"{architecture} catalog index is missing while shards contain records; "
                    "restore the index or use --replace-resolver to recollect the complete architecture catalog"
                )
            reset_records = True
        pending[index_path] = {
            "schema_version": symbol_catalog.SCHEMA_VERSION,
            "architecture": architecture,
            "resolver": {"dbghelp": resolver},
            "sources": sources,
        }
        for relative, existing in records.items():
            pending[root / architecture / relative] = {
                "schema_version": symbol_catalog.SCHEMA_VERSION,
                "records": [] if reset_records else existing,
            }
    return pending


def discover_candidates(
    schemas: dict[str, dict[str, projection_schema.ProjectionSchema]],
    cache: Path,
    platform: str = "x64",
) -> dict[tuple[str, int], list[audit_winbindex_revisions.Sample]]:
    candidates: dict[tuple[str, int], list[audit_winbindex_revisions.Sample]] = {}
    for module in MODULES:
        loaded = audit_winbindex_revisions.load_index(FILENAMES[module], cache, platform)
        builds = sorted({build for modules in schemas.values() for build in modules[module]["known_builds"]})
        for build in builds:
            candidates[(module, build)] = audit_winbindex_revisions.discover_samples(
                loaded, argparse.Namespace(build=build, min_revision=None, max_revision=None), platform
            )
    return candidates


def collect(
    repo: Path,
    cache: Path,
    dbghelp: Path,
    missing_output: Path | None = None,
    *,
    replace_resolver: bool = False,
    platform: str = "x64",
) -> int:
    repo = repo.resolve()
    dbghelp = dbghelp.resolve()
    resolver = dbghelp_identity(dbghelp)
    schemas = {architecture: load_schemas(repo, architecture, platform) for architecture in projection_schema.ARCHITECTURES}
    sources_by_architecture: dict[str, dict[Path, dict[str, Any]]] = {}
    keys_by_architecture: dict[str, set[tuple[str, str, int]]] = {}
    for architecture, modules in schemas.items():
        root = projection_target.catalog_root(repo, architecture, platform)
        pending_sources = prepare_indexes(root, resolver, {architecture: modules}, replace_resolver=replace_resolver)
        retained_records = []
        # Validate all retained records before acquiring candidates or publishing files.
        for module, schema in modules.items():
            for build in schema["known_builds"]:
                path = source_path(root, architecture, module, build)
                source = pending_sources[path]
                retained_records.extend(symbol_catalog.normalize_records(source["records"], schema, path, module, build))
        sources_by_architecture[architecture] = pending_sources
        keys_by_architecture[architecture] = symbol_catalog.validate_record_keys(retained_records)

    candidates = discover_candidates(schemas, cache, platform)
    missing: list[dict[str, Any]] = []
    for architecture, modules in schemas.items():
        root = projection_target.catalog_root(repo, architecture, platform)
        pending_sources = sources_by_architecture[architecture]
        existing_keys = keys_by_architecture[architecture]
        for module, schema in modules.items():
            for build in schema["known_builds"]:
                path = source_path(root, architecture, module, build)
                source = pending_sources[path]
                samples = candidates[(module, build)]
                for position, sample in enumerate(samples, 1):
                    digest = sample.sha256
                    label = f"{sample.version.build}.{sample.version.revision}"
                    print(f"{architecture}/{module}/{label} [{position}/{len(samples)}]", file=sys.stderr)
                    try:
                        identity = audit_winbindex_revisions.sample_codeview(cache, FILENAMES[module], sample)
                        key = (module, str(identity.guid), identity.age)
                        if key in existing_keys:
                            continue
                        pdb = audit_winbindex_revisions.prime_symbol_cache(identity, cache)
                        normalized = record_from_pdb(pdb, identity, sample.version, schema, dbghelp)
                        source["records"].append(normalized)
                        existing_keys.add(key)
                    except (SchemaError, audit_winbindex_revisions.AuditError, audit_symbol_resolution.AuditError, OSError) as error:
                        missing.append({
                            "architecture": architecture, "platform": platform,
                            "module": module,
                            "version": {"build": sample.version.build, "revision": sample.version.revision},
                            "sha256": digest,
                            "error": str(error),
                        })
                source["records"].sort(key=lambda item: (
                    item["revision"], item["pdb"]["guid"], item["pdb"]["age"],
                ))
    for architecture, pending_sources in sources_by_architecture.items():
        for path, source in pending_sources.items():
            write_json(path, source)
        print(f"{architecture}: records={len(keys_by_architecture[architecture])}")
    write_json(missing_output or projection_target.platform_root(cache, platform) / "collection-failures.json", {"schema_version": 1, "records": missing})
    if missing:
        print(f"catalog collection skipped {len(missing)} unavailable or unresolved candidates", file=sys.stderr)
    return 0


def verify_sources(repo: Path, schemas_by_architecture: dict[str, dict[str, projection_schema.ProjectionSchema]] | None = None, platform: str = "x64") -> dict[str, int]:
    counts: dict[str, int] = {}
    for architecture in projection_schema.ARCHITECTURES:
        root = projection_target.catalog_root(repo, architecture, platform)
        schemas = schemas_by_architecture[architecture] if schemas_by_architecture is not None else load_schemas(repo, architecture, platform)
        catalog = symbol_catalog.load_catalog_sources(
            root / architecture / "index.json", architecture, list(schemas.values())
        )
        counts[architecture] = len(catalog["records"])
        print(f"{architecture}: records={counts[architecture]}")
    return counts


def replay(repo: Path, cache: Path, dbghelp: Path, schemas_by_architecture: dict[str, dict[str, projection_schema.ProjectionSchema]], platform: str = "x64") -> None:
    dbghelp = dbghelp.resolve()
    resolver = dbghelp_identity(dbghelp)
    for architecture in projection_schema.ARCHITECTURES:
        root = projection_target.catalog_root(repo, architecture, platform)
        require_catalog_resolver(root, architecture, resolver)
        schemas = schemas_by_architecture[architecture]
        for module, schema in schemas.items():
            for build in schema["known_builds"]:
                source = projection_schema.read_json(source_path(root, architecture, module, build))
                for committed in source["records"]:
                    revision = committed["revision"]
                    identity = committed["pdb"]
                    expected = audit_symbol_resolution.PdbIdentity(identity["name"], uuid.UUID(identity["guid"]), identity["age"])
                    pdb_key = expected.key()
                    pdb = cache / "symbols" / identity["name"] / pdb_key / identity["name"]
                    audit_winbindex_revisions.verify_pdb_identity(pdb, expected)
                    actual = record_from_pdb(pdb, expected, projection_schema.Version(build, revision), schema, dbghelp)
                    if actual != committed:
                        raise SchemaError(f"evidence replay differs for {module} {build}.{revision} {pdb_key}")
    print("evidence replay: all records match")


def run(args: argparse.Namespace) -> int:
    repo = args.repo.resolve()
    platform = args.platform
    if args.command == "collect":
        return collect(
            repo,
            args.cache.resolve(),
            args.dbghelp.resolve(),
            args.missing_output.resolve() if args.missing_output is not None else None,
            replace_resolver=args.replace_resolver, platform=args.platform,
        )
    schemas = {architecture: load_schemas(repo, architecture, platform) for architecture in projection_schema.ARCHITECTURES}
    verify_sources(repo, schemas, platform)
    if (args.cache is None) != (args.dbghelp is None):
        raise SchemaError("--cache and --dbghelp must be supplied together")
    if args.cache is not None:
        replay(repo, args.cache.resolve(), args.dbghelp.resolve(), schemas, platform)
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
