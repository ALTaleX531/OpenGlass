#!/usr/bin/env python3
"""Audit projection Symbol resolution against an exact PE/PDB pair."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path, PureWindowsPath
import sys
import uuid
from typing import Any

import pefile
from construct import (
	Array, Bytes, Check, Const, ConstructError, CString, Int32ul, Padding, Pointer, PrefixedArray, RestreamData, Struct, this,
)

from tools import dbghelp, projection_schema, projection_target
from tools.pe_image import open_image, require_machine
from tools.projection_schema import Version, in_range, select_binding


SCHEMA_VERSION = 1


class AuditError(Exception):
	pass


@dataclass(frozen=True)
class PdbIdentity:
	name: str
	guid: uuid.UUID
	age: int

	def key(self) -> str:
		return f"{self.guid.hex.upper()}{self.age:x}"

	def json(self) -> dict[str, Any]:
		return {"name": self.name, "guid": str(self.guid), "age": self.age}


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("repo", type=Path)
	parser.add_argument("--architecture", choices=("legacy", "milcomp"), required=True)
	projection_target.add_platform_argument(parser)
	parser.add_argument("--module", choices=("udwm", "dwmcore"), required=True)
	parser.add_argument("--version", required=True)
	parser.add_argument("--image", type=Path, required=True)
	parser.add_argument("--symbol-path", type=Path, required=True)
	parser.add_argument("--dbghelp", type=Path, help="use this DbgHelp build instead of the host system copy")
	parser.add_argument("--configuration", choices=("debug", "release"), default="release")
	parser.add_argument("--id", dest="stable_id")
	parser.add_argument("--format", choices=("text", "json"), default="text")
	return parser.parse_args(argv)


_CODEVIEW = Struct(Const(b"RSDS"), "guid" / Bytes(16), "age" / Int32ul, "name" / CString("utf8"))


def decode_codeview(data: bytes) -> PdbIdentity:
	try:
		value = _CODEVIEW.parse(data)
		return PdbIdentity(PureWindowsPath(value.name).name, uuid.UUID(bytes_le=value.guid), value.age)
	except (ConstructError, UnicodeError, ValueError) as error:
		raise AuditError(f"invalid RSDS CodeView identity: {error}") from error


def read_pe_codeview(path: Path, platform: str | None = None) -> PdbIdentity:
	try:
		with open_image(path) as image, path.open("rb") as stream:
			if platform is not None:
				require_machine(image, platform)
			image.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_DEBUG"]])
			for debug in getattr(image, "DIRECTORY_ENTRY_DEBUG", []):
				if debug.struct.Type != pefile.DEBUG_TYPE["IMAGE_DEBUG_TYPE_CODEVIEW"] or debug.struct.SizeOfData < 25:
					continue
				stream.seek(debug.struct.PointerToRawData)
				data = stream.read(debug.struct.SizeOfData)
				if data.startswith(b"RSDS"):
					return decode_codeview(data)
	except (OSError, pefile.PEFormatError, UnicodeError, ValueError) as error:
		raise AuditError(f"cannot parse PE CodeView identity from {path}: {error}") from error
	raise AuditError(f"image has no RSDS CodeView identity: {path}")


# Only the MSF directory and identity headers are needed; symbol/type streams stay unread.
_PDB_IDENTITY = Struct(
	Const(b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0"),
	"block_size" / Int32ul,
	Check((this.block_size >= 512) & (this.block_size <= 1024 * 1024)),
	Padding(8),
	"directory_size" / Int32ul,
	Padding(4),
	"block_map" / Int32ul,
	"directory_pages" / Pointer(this.block_map * this.block_size, Array(
		lambda c: (c.directory_size + c.block_size - 1) // c.block_size,
		Struct("block" / Int32ul, "data" / Pointer(this.block * this._root.block_size, Bytes(this._root.block_size))),
	)),
	"directory" / RestreamData(lambda c: b"".join(page.data for page in c.directory_pages)[:c.directory_size], Struct(
		"sizes" / PrefixedArray(Int32ul, Int32ul),
		Check(lambda c: len(c.sizes) > 3 and 28 <= c.sizes[1] < 0xFFFFFFFF and 12 <= c.sizes[3] < 0xFFFFFFFF),
		"blocks" / Array(lambda c: len(c.sizes), Array(
			lambda c: 0 if c.sizes[c._index] == 0xFFFFFFFF else (c.sizes[c._index] + c._.block_size - 1) // c._.block_size,
			Int32ul,
		)),
	)),
	"info" / Pointer(this.directory.blocks[1][0] * this.block_size, Struct(Padding(12), "guid" / Bytes(16))),
	"dbi" / Pointer(this.directory.blocks[3][0] * this.block_size, Struct(Padding(8), "age" / Int32ul)),
)


def read_pdb_identity(path: Path) -> PdbIdentity:
	try:
		with path.open("rb") as stream:
			identity = _PDB_IDENTITY.parse_stream(stream)
		return PdbIdentity(path.name, uuid.UUID(bytes_le=identity.info.guid), identity.dbi.age)
	except (OSError, ConstructError, ValueError) as error:
		raise AuditError(f"cannot parse PDB identity from {path}: {error}") from error


def image_version(path: Path) -> Version | None:
	try:
		with open_image(path) as image:
			image.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
			versions = getattr(image, "VS_FIXEDFILEINFO", [])
			if versions:
				value = versions[0].FileVersionLS
				return Version(value >> 16, value & 0xFFFF)
	except (OSError, pefile.PEFormatError):
		pass
	return None


def find_pdb(symbol_path: Path, expected: PdbIdentity) -> tuple[Path, PdbIdentity, bool]:
	if symbol_path.is_file():
		candidates = [symbol_path]
	elif symbol_path.is_dir():
		candidates = list(symbol_path.rglob(expected.name))
	else:
		raise AuditError(f"symbol path does not exist: {symbol_path}")
	parsed: list[tuple[Path, PdbIdentity]] = []
	for candidate in candidates:
		try:
			identity = read_pdb_identity(candidate)
		except AuditError:
			continue
		parsed.append((candidate.resolve(), identity))
		if identity.guid == expected.guid and identity.age == expected.age:
			return candidate.resolve(), identity, True
	if len(parsed) == 1:
		return (*parsed[0], False)
	if not parsed:
		raise AuditError(f"no readable {expected.name} found below {symbol_path}")
	raise AuditError(f"no PDB matches image GUID/age and multiple discovery candidates exist below {symbol_path}")


def enumerate_complete_names(pdb: Path, dbghelp_path: Path | None = None) -> tuple[dict[str, set[int]], int]:
	try:
		symbols, failures = dbghelp.enumerate_symbols(
			pdb, str(pdb.parent), dbghelp_path.resolve() if dbghelp_path else None,
			dbghelp.SYMOPT_PUBLICS_ONLY | dbghelp.SYMOPT_EXACT_SYMBOLS | dbghelp.SYMOPT_IGNORE_NT_SYMPATH,
			base=0x180000000, module_name="OpenGlassSymbolAudit",
		)
	except dbghelp.DbgHelpError as error:
		raise AuditError(str(error)) from error
	results: dict[str, set[int]] = {}
	for symbol in symbols:
		results.setdefault(symbol.name, set()).add(symbol.rva)
	return results, failures


def sha256_file(path: Path) -> str:
	with path.open("rb") as stream:
		return hashlib.file_digest(stream, "sha256").hexdigest()


def resolve_dbghelp_path(requested: Path | None) -> Path:
	path = requested.resolve() if requested else (
		Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32" / "dbghelp.dll"
	).resolve()
	return path


def resolver_identity(requested: Path | None) -> dict[str, Any]:
	path = resolve_dbghelp_path(requested)
	version = image_version(path)
	return {"path": str(path), "version": version.json() if version else None, "sha256": sha256_file(path)}


def required_failures(descriptors: list[dict[str, Any]]) -> list[dict[str, Any]]:
	return [item for item in descriptors if item["requirement"] == "required" and item["status"] not in {"unique", "inactive"}]


def audit_failures(result: dict[str, Any], *, binary_audit: bool = True) -> list[str]:
	"""An empty list means success; inspect owns identity/version evidence reasons."""
	issues = [f"{item['id']}={item['status']}" for item in required_failures(result["descriptors"])]
	if binary_audit:
		issues.extend(result["evidence_issues"])
	if not result["module_supported"]:
		issues.append("version is outside module support range")
	return issues


def resolve_descriptors(schema: projection_schema.ProjectionSchema, requested: Version, configuration: str,
	names: dict[str, set[int]], stable_id: str | None = None) -> list[dict[str, Any]]:
	descriptors = []
	for symbol in schema["symbols"]:
		if stable_id and symbol.get("id") != stable_id:
			continue
		selected = select_binding(requested, symbol, configuration)
		active = selected is not None
		binding_index, binding = selected if selected is not None else (None, None)
		addresses: set[int] = set()
		matched_names = []
		for candidate in binding.get("symbol_names", []) if binding else []:
			candidate_addresses = names.get(candidate, set())
			if candidate_addresses:
				matched_names.append(candidate)
				addresses.update(candidate_addresses)
		status = "inactive" if not active else "missing" if not addresses else "unique" if len(addresses) == 1 else "ambiguous"
		descriptors.append({
			"id": symbol.get("id"), "name": symbol.get("name"), "requirement": symbol.get("requirement"),
			"kind": symbol.get("kind"), "type": symbol.get("type"),
			"abi_compatibility": symbol.get("abi_compatibility"), "usage": symbol.get("usage"),
			"binding_index": binding_index,
			"range": None if binding is None else {
				"min_inclusive": binding.get("min_inclusive"),
				"max_exclusive": binding.get("max_exclusive"),
			},
			"symbol_names": binding.get("symbol_names") if binding else [],
			"matched_names": matched_names,
			"rvas": [f"0x{address:X}" for address in sorted(addresses)], "status": status,
		})
	if stable_id and not descriptors:
		raise AuditError(f"Symbol stable ID not found: {stable_id}")
	return descriptors


def inspect_pdb(pdb: Path, schema: projection_schema.ProjectionSchema, requested: Version,
	dbghelp_path: Path, stable_id: str | None = None) -> dict[str, Any]:
	"""Resolve an acquisition-validated PDB; no PE or second identity validation."""
	names, failures = enumerate_complete_names(pdb, dbghelp_path)
	return {
		"module_supported": in_range(requested, schema),
		"descriptors": resolve_descriptors(schema, requested, "release", names, stable_id),
		"undecoration_failures": failures,
	}


def validate_image_rvas(path: Path, descriptors: list[dict[str, Any]], platform: str = "x64") -> None:
	"""Full-binary audit only; catalog collection needs no image bounds or sections."""
	try:
		with open_image(path) as image:
			require_machine(image, platform)
			for descriptor in descriptors:
				if descriptor["status"] != "unique":
					continue
				rva = int(descriptor["rvas"][0], 0)
				if not 0 < rva < image.OPTIONAL_HEADER.SizeOfImage:
					raise AuditError(f"{descriptor['id']}: RVA is outside SizeOfImage")
				section = image.get_section_by_rva(rva)
				if section is None:
					raise AuditError(f"{descriptor['id']}: RVA is outside the audited PE sections")
				is_code = bool(section.Characteristics & 0x20000000)
				if is_code == (descriptor["kind"] == "projected_variable"):
					raise AuditError(f"{descriptor['id']}: RVA address class does not match Symbol kind")
	except (OSError, pefile.PEFormatError) as error:
		raise AuditError(f"cannot inspect audited PE sections: {error}") from error


def inspect(
	args: argparse.Namespace,
	*,
	schema: projection_schema.ProjectionSchema | None = None,
	resolver: dict[str, Any] | None = None,
) -> dict[str, Any]:
	repo = args.repo.resolve()
	image = args.image.resolve()
	requested = Version.parse(args.version)
	if image.name.lower() != ("udwm.dll" if args.module == "udwm" else "dwmcore.dll"):
		raise AuditError(f"image name does not match --module {args.module}: {image.name}")
	if schema is None:
		schema = projection_schema.validate_schema(projection_target.schema_path(repo, args.architecture, args.module), args.module, args.platform)
	if resolver is None:
		resolver = resolver_identity(args.dbghelp)
	image_identity = read_pe_codeview(image, args.platform)
	pdb, pdb_identity, paired = find_pdb(args.symbol_path.resolve(), image_identity)
	actual_version = image_version(image)
	names, undecoration_failures = enumerate_complete_names(pdb, Path(resolver["path"]))
	module_supported = in_range(requested, schema)
	descriptors = resolve_descriptors(schema, requested, args.configuration, names, args.stable_id)
	version_matches = actual_version == requested if actual_version else False
	if paired and version_matches:
		validate_image_rvas(image, descriptors, args.platform)
	evidence = "production_candidate" if paired and version_matches else "discovery"
	evidence_issues = []
	if not paired:
		evidence_issues.append("PDB GUID/DBI age does not match the image CodeView record")
	if actual_version is None:
		evidence_issues.append("image file version is unavailable")
	elif not version_matches:
		evidence_issues.append(
			f"requested version {requested.build}.{requested.revision} does not match image version "
			f"{actual_version.build}.{actual_version.revision}"
		)
	return {
		"schema_version": SCHEMA_VERSION, "architecture": args.architecture, "platform": args.platform, "module": args.module,
		"configuration": args.configuration,
		"requested_version": requested.json(), "image_version": actual_version.json() if actual_version else None,
		"module_range": {
			"min_inclusive": schema.get("min_inclusive"),
			"max_exclusive": schema.get("max_exclusive"),
		},
		"module_supported": module_supported,
		"image": {"path": str(image), "sha256": sha256_file(image), "codeview": image_identity.json()},
		"pdb": {"path": str(pdb), "sha256": sha256_file(pdb), "identity": pdb_identity.json(), "paired": paired},
		"dbghelp": resolver,
		"evidence": evidence, "evidence_issues": evidence_issues,
		"undecoration_failures": undecoration_failures, "descriptors": descriptors,
	}


def print_text(result: dict[str, Any]) -> None:
	print(
		f"{result['platform']}/{result['architecture']} {result['module']} "
		f"{result['requested_version']['build']}.{result['requested_version']['revision']} "
		f"({result['configuration']})"
	)
	print(f"Module support range contains version: {result['module_supported']}")
	print(f"Evidence: {result['evidence']} (PDB paired: {result['pdb']['paired']})")
	for issue in result["evidence_issues"]:
		print(f"  Discovery limitation: {issue}")
	print(f"Image: {result['image']['path']}")
	print(f"SHA-256: {result['image']['sha256']}")
	codeview = result["image"]["codeview"]
	print(f"Image CodeView: {codeview['name']} [GUID={codeview['guid']}, age={codeview['age']}]")
	identity = result["pdb"]["identity"]
	print(f"PDB: {result['pdb']['path']} [GUID={identity['guid']}, age={identity['age']}]")
	print(f"PDB SHA-256: {result['pdb']['sha256']}")
	dbghelp_version = result["dbghelp"]["version"]
	dbghelp_label = (
		f"{dbghelp_version['build']}.{dbghelp_version['revision']}" if dbghelp_version else "system search path"
	)
	print(f"DbgHelp: {result['dbghelp']['path']} ({dbghelp_label})")
	print(f"Complete-name undecoration failures: {result['undecoration_failures']}")
	for descriptor in result["descriptors"]:
		addresses = ", ".join(descriptor["rvas"]) or "-"
		print(f"{descriptor['id']}: {descriptor['status']} ({addresses})")
		for name in descriptor["matched_names"]:
			print(f"  {name}")


def main(argv: list[str] | None = None) -> int:
	args = parse_args(argv)
	try:
		result = inspect(args)
	except (AuditError, projection_schema.SchemaError, OSError) as error:
		payload = {"schema_version": SCHEMA_VERSION, "error": str(error)}
		print(json.dumps(payload, indent=2) if args.format == "json" else f"ERROR: {error}", file=sys.stderr)
		return 2
	except Exception as error:
		payload = {"schema_version": SCHEMA_VERSION, "internal_error": str(error)}
		print(json.dumps(payload, indent=2) if args.format == "json" else f"INTERNAL ERROR: {error}", file=sys.stderr)
		return 3
	if args.format == "json":
		print(json.dumps(result, indent=2, ensure_ascii=False))
	else:
		print_text(result)
	return int(bool(audit_failures(result)))


if __name__ == "__main__":
	raise SystemExit(main())
