#!/usr/bin/env python3
"""Audit projection Symbol resolution against an exact PE/PDB pair."""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import struct
import sys
import uuid
from typing import Any


TOOL_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOL_ROOT / "Scripts"))
try:
	import projection_schema
	import projection_source_check
finally:
	sys.path.pop(0)


SCHEMA_VERSION = 1
SYMOPT_EXACT_SYMBOLS = 0x00000400
SYMOPT_IGNORE_NT_SYMPATH = 0x00001000
SYMOPT_PUBLICS_ONLY = 0x00004000
UNDNAME_COMPLETE = 0
COMPLETE_NAME_CAPACITY = 64 * 1024


class AuditError(Exception):
	pass


@dataclass(frozen=True, order=True)
class Version:
	build: int
	revision: int

	@classmethod
	def parse(cls, value: str) -> "Version":
		parts = value.removeprefix("10.0.").split(".")
		if len(parts) != 2 or not all(part.isdecimal() for part in parts):
			raise AuditError("--version must use BUILD.REVISION or 10.0.BUILD.REVISION")
		build, revision = map(int, parts)
		if not build or build > 0xFFFFFFFF or revision > 0xFFFFFFFF:
			raise AuditError("--version is outside the unsigned 32-bit range")
		return cls(build, revision)

	def json(self) -> dict[str, int]:
		return {"build": self.build, "revision": self.revision}


@dataclass(frozen=True)
class PdbIdentity:
	name: str
	guid: uuid.UUID
	age: int

	def key(self) -> str:
		return self.guid.hex.upper() + str(self.age)

	def json(self) -> dict[str, Any]:
		return {"name": self.name, "guid": str(self.guid), "age": self.age, "symbol_server_key": self.key()}


class SYMBOL_INFO(ctypes.Structure):
	_fields_ = [
		("SizeOfStruct", wintypes.ULONG), ("TypeIndex", wintypes.ULONG),
		("Reserved", ctypes.c_ulonglong * 2), ("Index", wintypes.ULONG),
		("Size", wintypes.ULONG), ("ModBase", ctypes.c_ulonglong),
		("Flags", wintypes.ULONG), ("Value", ctypes.c_ulonglong),
		("Address", ctypes.c_ulonglong), ("Register", wintypes.ULONG),
		("Scope", wintypes.ULONG), ("Tag", wintypes.ULONG),
		("NameLen", wintypes.ULONG), ("MaxNameLen", wintypes.ULONG),
		("Name", ctypes.c_char * 1),
	]


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("repo", type=Path)
	parser.add_argument("--architecture", choices=("legacy", "milcomp"), required=True)
	parser.add_argument("--module", choices=("udwm", "dwmcore"), required=True)
	parser.add_argument("--version", required=True)
	parser.add_argument("--image", type=Path, required=True)
	parser.add_argument("--symbol-path", type=Path, required=True)
	parser.add_argument("--dbghelp", type=Path, help="use this DbgHelp build instead of the host system copy")
	parser.add_argument("--configuration", choices=("debug", "release"), default="release")
	parser.add_argument("--id", dest="stable_id")
	parser.add_argument("--format", choices=("text", "json"), default="text")
	return parser.parse_args(argv)


def read_pe_codeview(path: Path) -> PdbIdentity:
	try:
		data = path.read_bytes()
	except OSError as error:
		raise AuditError(f"cannot read image {path}: {error}") from error
	try:
		pe = struct.unpack_from("<I", data, 0x3C)[0]
		if data[pe:pe + 4] != b"PE\0\0":
			raise ValueError("missing PE signature")
		section_count = struct.unpack_from("<H", data, pe + 6)[0]
		optional_size = struct.unpack_from("<H", data, pe + 20)[0]
		optional = pe + 24
		magic = struct.unpack_from("<H", data, optional)[0]
		data_directories = optional + (112 if magic == 0x20B else 96 if magic == 0x10B else 0)
		if not data_directories:
			raise ValueError("unknown optional-header magic")
		debug_rva, debug_size = struct.unpack_from("<II", data, data_directories + 6 * 8)
		sections = []
		section_table = optional + optional_size
		for index in range(section_count):
			virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from(
				"<IIII", data, section_table + index * 40 + 8
			)
			sections.append((virtual_address, max(virtual_size, raw_size), raw_pointer))
		def rva_to_offset(rva: int) -> int:
			for virtual_address, size, raw_pointer in sections:
				if virtual_address <= rva < virtual_address + size:
					return raw_pointer + rva - virtual_address
			raise ValueError(f"RVA 0x{rva:X} is outside image sections")
		debug_offset = rva_to_offset(debug_rva)
		for index in range(debug_size // 28):
			entry = debug_offset + index * 28
			type_value, size, _address, raw_pointer = struct.unpack_from("<IIII", data, entry + 12)
			if type_value != 2:
				continue
			codeview = data[raw_pointer:raw_pointer + size]
			if codeview[:4] != b"RSDS" or len(codeview) < 25:
				continue
			guid = uuid.UUID(bytes_le=codeview[4:20])
			age = struct.unpack_from("<I", codeview, 20)[0]
			name = codeview[24:].split(b"\0", 1)[0].decode("utf-8", errors="strict")
			return PdbIdentity(Path(name).name, guid, age)
	except (IndexError, struct.error, UnicodeError, ValueError) as error:
		raise AuditError(f"cannot parse PE CodeView identity from {path}: {error}") from error
	raise AuditError(f"image has no RSDS CodeView identity: {path}")


def read_pdb_identity(path: Path) -> PdbIdentity:
	try:
		with path.open("rb") as stream:
			header = stream.read(56)
			if not header.startswith(b"Microsoft C/C++ MSF 7.00"):
				raise AuditError(f"unsupported PDB container: {path}")
			block_size, _free_map, _block_count, directory_size, _reserved, block_map = struct.unpack_from("<IIIIII", header, 32)
			if not block_size or block_size > 1024 * 1024:
				raise AuditError(f"invalid PDB block size: {path}")
			directory_block_count = (directory_size + block_size - 1) // block_size
			stream.seek(block_map * block_size)
			directory_blocks = struct.unpack(f"<{directory_block_count}I", stream.read(directory_block_count * 4))
			directory = bytearray()
			for block in directory_blocks:
				stream.seek(block * block_size)
				directory.extend(stream.read(block_size))
			directory = directory[:directory_size]
			stream_count = struct.unpack_from("<I", directory, 0)[0]
			stream_sizes = struct.unpack_from(f"<{stream_count}I", directory, 4)
			cursor = 4 + stream_count * 4
			stream_blocks: list[list[int]] = []
			for size in stream_sizes:
				count = 0 if size == 0xFFFFFFFF else (size + block_size - 1) // block_size
				blocks = list(struct.unpack_from(f"<{count}I", directory, cursor)) if count else []
				cursor += count * 4
				stream_blocks.append(blocks)
			if stream_count <= 1 or stream_sizes[1] < 28:
				raise AuditError(f"PDB has no identity stream: {path}")
			info = bytearray()
			for block in stream_blocks[1]:
				stream.seek(block * block_size)
				info.extend(stream.read(block_size))
			info = info[:stream_sizes[1]]
			guid = uuid.UUID(bytes_le=bytes(info[12:28]))
			if stream_count <= 3 or stream_sizes[3] < 12:
				raise AuditError(f"PDB has no DBI age: {path}")
			dbi = bytearray()
			for block in stream_blocks[3]:
				stream.seek(block * block_size)
				dbi.extend(stream.read(block_size))
			age = struct.unpack_from("<I", dbi, 8)[0]
			return PdbIdentity(path.name, guid, age)
	except AuditError:
		raise
	except (OSError, struct.error, ValueError) as error:
		raise AuditError(f"cannot parse PDB identity from {path}: {error}") from error


def image_version(path: Path) -> Version | None:
	version = ctypes.WinDLL("version.dll", use_last_error=True)
	version.GetFileVersionInfoSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
	version.GetFileVersionInfoSizeW.restype = wintypes.DWORD
	version.GetFileVersionInfoW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p]
	version.GetFileVersionInfoW.restype = wintypes.BOOL
	version.VerQueryValueW.argtypes = [ctypes.c_void_p, wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(wintypes.UINT)]
	version.VerQueryValueW.restype = wintypes.BOOL
	handle = wintypes.DWORD()
	size = version.GetFileVersionInfoSizeW(str(path), ctypes.byref(handle))
	if not size:
		return None
	buffer = ctypes.create_string_buffer(size)
	if not version.GetFileVersionInfoW(str(path), 0, size, buffer):
		return None
	pointer = ctypes.c_void_p()
	length = wintypes.UINT()
	if not version.VerQueryValueW(buffer, "\\", ctypes.byref(pointer), ctypes.byref(length)) or length.value < 16:
		return None
	values = ctypes.cast(pointer, ctypes.POINTER(wintypes.DWORD * 4)).contents
	return Version(values[3] >> 16, values[3] & 0xFFFF)


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
	if dbghelp_path is not None and not dbghelp_path.is_file():
		raise AuditError(f"DbgHelp path is not a file: {dbghelp_path}")
	dbghelp = ctypes.WinDLL(str(dbghelp_path.resolve()) if dbghelp_path else "dbghelp.dll", use_last_error=True)
	kernel32 = ctypes.WinDLL("kernel32.dll", use_last_error=True)
	kernel32.GetCurrentProcess.argtypes = []
	kernel32.GetCurrentProcess.restype = wintypes.HANDLE
	process = kernel32.GetCurrentProcess()
	dbghelp.SymInitializeW.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR, wintypes.BOOL]
	dbghelp.SymInitializeW.restype = wintypes.BOOL
	dbghelp.SymCleanup.argtypes = [wintypes.HANDLE]
	dbghelp.SymCleanup.restype = wintypes.BOOL
	dbghelp.SymSetOptions.argtypes = [wintypes.DWORD]
	dbghelp.SymLoadModuleExW.argtypes = [wintypes.HANDLE, wintypes.HANDLE, wintypes.LPCWSTR, wintypes.LPCWSTR, ctypes.c_ulonglong, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
	dbghelp.SymLoadModuleExW.restype = ctypes.c_ulonglong
	dbghelp.SymUnloadModule64.argtypes = [wintypes.HANDLE, ctypes.c_ulonglong]
	dbghelp.UnDecorateSymbolName.argtypes = [ctypes.c_char_p, ctypes.c_char_p, wintypes.DWORD, wintypes.DWORD]
	dbghelp.UnDecorateSymbolName.restype = wintypes.DWORD
	callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, ctypes.POINTER(SYMBOL_INFO), wintypes.ULONG, ctypes.c_void_p)
	dbghelp.SymEnumSymbols.argtypes = [wintypes.HANDLE, ctypes.c_ulonglong, ctypes.c_char_p, callback_type, ctypes.c_void_p]
	dbghelp.SymEnumSymbols.restype = wintypes.BOOL
	if not dbghelp.SymInitializeW(process, str(pdb.parent), False):
		raise AuditError(f"SymInitializeW failed: {ctypes.get_last_error()}")
	base = 0x180000000
	module = 0
	try:
		dbghelp.SymSetOptions(SYMOPT_PUBLICS_ONLY | SYMOPT_EXACT_SYMBOLS | SYMOPT_IGNORE_NT_SYMPATH)
		module = dbghelp.SymLoadModuleExW(process, None, str(pdb), "OpenGlassSymbolAudit", base, 0, None, 0)
		if not module:
			raise AuditError(f"SymLoadModuleExW failed for {pdb}: {ctypes.get_last_error()}")
		results: dict[str, set[int]] = {}
		failures = 0
		scratch = ctypes.create_string_buffer(COMPLETE_NAME_CAPACITY)
		@callback_type
		def callback(info: ctypes.POINTER(SYMBOL_INFO), _size: int, _context: int) -> bool:
			nonlocal failures
			name_address = ctypes.addressof(info.contents) + SYMBOL_INFO.Name.offset
			decorated = ctypes.string_at(name_address, info.contents.NameLen)
			length = dbghelp.UnDecorateSymbolName(decorated, scratch, len(scratch), UNDNAME_COMPLETE)
			if not length or length + 1 >= len(scratch):
				failures += 1
				return True
			try:
				name = scratch.raw[:length].decode("ascii", errors="strict")
			except UnicodeDecodeError:
				failures += 1
				return True
			results.setdefault(name, set()).add(info.contents.Address - module)
			return True
		if not dbghelp.SymEnumSymbols(process, module, None, callback, None):
			raise AuditError(f"SymEnumSymbols failed for {pdb}: {ctypes.get_last_error()}")
		return results, failures
	finally:
		if module:
			dbghelp.SymUnloadModule64(process, module)
		dbghelp.SymCleanup(process)


def sha256_file(path: Path) -> str:
	digest = hashlib.sha256()
	with path.open("rb") as stream:
		for chunk in iter(lambda: stream.read(1024 * 1024), b""):
			digest.update(chunk)
	return digest.hexdigest()


def resolve_dbghelp_path(requested: Path | None) -> Path:
	path = requested.resolve() if requested else (
		Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32" / "dbghelp.dll"
	).resolve()
	if not path.is_file():
		raise AuditError(f"DbgHelp path is not a file: {path}")
	return path


def in_range(version: Version, symbol: dict[str, Any]) -> bool:
	current = projection_schema.Version(version.build, version.revision)
	minimum = projection_schema.parse_version(symbol.get("min_inclusive"), "symbol.min_inclusive")
	maximum = projection_schema.parse_version(symbol.get("max_exclusive"), "symbol.max_exclusive")
	return current >= minimum and projection_schema.before(current, maximum)


def select_binding(
	version: Version,
	symbol: dict[str, Any],
	configuration: str,
) -> tuple[int, dict[str, Any]] | None:
	if symbol.get("condition") == "debug" and configuration != "debug":
		return None
	for index, binding in enumerate(symbol.get("bindings", [])):
		if in_range(version, binding):
			return index, binding
	return None


def inspect(args: argparse.Namespace) -> dict[str, Any]:
	repo = args.repo.resolve()
	image = args.image.resolve()
	requested = Version.parse(args.version)
	if image.name.lower() != ("udwm.dll" if args.module == "udwm" else "dwmcore.dll"):
		raise AuditError(f"image name does not match --module {args.module}: {image.name}")
	image_identity = read_pe_codeview(image)
	pdb, pdb_identity, paired = find_pdb(args.symbol_path.resolve(), image_identity)
	actual_version = image_version(image)
	dbghelp_path = resolve_dbghelp_path(args.dbghelp)
	names, undecoration_failures = enumerate_complete_names(pdb, dbghelp_path)
	dbghelp_version = image_version(dbghelp_path)
	schema_path = repo / "OpenGlass" / "ProjectionSchemas" / args.architecture / f"{args.module}.json"
	try:
		constants = projection_source_check.load_os_constants(repo)
		schema = projection_schema.validate_schema(schema_path, args.module, constants)
	except projection_schema.SchemaError as error:
		raise AuditError(str(error)) from error
	module_supported = in_range(requested, schema)
	descriptors = []
	for symbol in schema.get("symbols", []):
		if args.stable_id and symbol.get("id") != args.stable_id:
			continue
		selected = select_binding(requested, symbol, args.configuration)
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
	if args.stable_id and not descriptors:
		raise AuditError(f"Symbol stable ID not found: {args.stable_id}")
	version_matches = actual_version == requested if actual_version else False
	evidence = "production_candidate" if paired and version_matches else "discovery"
	evidence_issues = []
	if not paired:
		evidence_issues.append("PDB GUID/age does not match the image CodeView record")
	if actual_version is None:
		evidence_issues.append("image file version is unavailable")
	elif not version_matches:
		evidence_issues.append(
			f"requested version {requested.build}.{requested.revision} does not match image version "
			f"{actual_version.build}.{actual_version.revision}"
		)
	return {
		"schema_version": SCHEMA_VERSION, "architecture": args.architecture, "module": args.module,
		"configuration": args.configuration,
		"requested_version": requested.json(), "image_version": actual_version.json() if actual_version else None,
		"module_range": {
			"min_inclusive": schema.get("min_inclusive"),
			"max_exclusive": schema.get("max_exclusive"),
		},
		"module_supported": module_supported,
		"image": {"path": str(image), "sha256": sha256_file(image), "codeview": image_identity.json()},
		"pdb": {"path": str(pdb), "sha256": sha256_file(pdb), "identity": pdb_identity.json(), "paired": paired},
		"dbghelp": {
			"path": str(dbghelp_path),
			"version": dbghelp_version.json() if dbghelp_version else None,
			"sha256": sha256_file(dbghelp_path),
		},
		"evidence": evidence, "evidence_issues": evidence_issues,
		"undecoration_failures": undecoration_failures, "descriptors": descriptors,
	}


def print_text(result: dict[str, Any]) -> None:
	print(
		f"{result['architecture']} {result['module']} "
		f"{result['requested_version']['build']}.{result['requested_version']['revision']} "
		f"({result['configuration']})"
	)
	print(f"Module support range contains version: {result['module_supported']}")
	print(f"Evidence: {result['evidence']} (PDB paired: {result['pdb']['paired']})")
	for issue in result["evidence_issues"]:
		print(f"  Discovery limitation: {issue}")
	print(f"Image: {result['image']['path']}")
	print(f"SHA-256: {result['image']['sha256']}")
	print(f"PDB: {result['pdb']['path']} [{result['pdb']['identity']['symbol_server_key']}]")
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
	except AuditError as error:
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
	active_bad = any(item["status"] in {"missing", "ambiguous"} and item["requirement"] == "required" for item in result["descriptors"])
	return 1 if active_bad else 0


if __name__ == "__main__":
	raise SystemExit(main())
