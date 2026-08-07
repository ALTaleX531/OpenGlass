#!/usr/bin/env python3
"""Download matching public symbols for a PE image and print complete C++ names."""

from __future__ import annotations

import argparse
import ctypes
import os
import sys
from ctypes import wintypes
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


COMPLETE_NAME_CAPACITY = 64 * 1024
MICROSOFT_SYMBOL_SERVER = "https://msdl.microsoft.com/download/symbols"
DEFAULT_SYMBOL_CACHE = r"%TEMP%\symbols"

SYMOPT_DEFERRED_LOADS = 0x00000004
SYMOPT_FAIL_CRITICAL_ERRORS = 0x00000200
SYMOPT_EXACT_SYMBOLS = 0x00000400
SYMOPT_PUBLICS_ONLY = 0x00004000
SYMOPT_SECURE = 0x00040000


class DumpError(Exception):
	pass


class SYMBOL_INFO(ctypes.Structure):
	_fields_ = [
		("SizeOfStruct", wintypes.ULONG),
		("TypeIndex", wintypes.ULONG),
		("Reserved", ctypes.c_ulonglong * 2),
		("Index", wintypes.ULONG),
		("Size", wintypes.ULONG),
		("ModBase", ctypes.c_ulonglong),
		("Flags", wintypes.ULONG),
		("Value", ctypes.c_ulonglong),
		("Address", ctypes.c_ulonglong),
		("Register", wintypes.ULONG),
		("Scope", wintypes.ULONG),
		("Tag", wintypes.ULONG),
		("NameLen", wintypes.ULONG),
		("MaxNameLen", wintypes.ULONG),
		("Name", ctypes.c_char * 1),
	]


@dataclass(frozen=True, order=True)
class Symbol:
	rva: int
	name: str
	decorated_name: str


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--input", type=str, required=True, help="PE image whose exact symbols should be loaded")
	parser.add_argument(
		"--output",
		type=str,
		default=DEFAULT_SYMBOL_CACHE,
		help=f"local symbol-server cache directory (default: {DEFAULT_SYMBOL_CACHE})",
	)
	parser.add_argument("--grep", dest="pattern", help="substring filter applied to complete undecorated names")
	parser.add_argument("--ignore-case", action="store_true", help="make --grep case-insensitive")
	parser.add_argument("--rva", action="store_true", help="prefix each result with its image-relative address")
	parser.add_argument(
		"--dbghelp",
		type=str,
		help="alternate dbghelp.dll; by default prefer the Windows SDK copy paired with symsrv.dll",
	)
	parser.add_argument("--symbol-server", default=MICROSOFT_SYMBOL_SERVER)
	return parser.parse_args(argv)


def expanded_path(value: str) -> Path:
	return Path(os.path.expandvars(os.path.expanduser(value))).resolve()


def discover_dbghelp() -> Path | None:
	for environment_name in ("ProgramFiles(x86)", "ProgramFiles"):
		root = os.environ.get(environment_name)
		if not root:
			continue
		candidate = Path(root) / "Windows Kits" / "10" / "Debuggers" / "x64" / "dbghelp.dll"
		if candidate.is_file() and candidate.with_name("symsrv.dll").is_file():
			return candidate.resolve()
	return None


def select_symbols(symbols: Iterable[Symbol], pattern: str | None, ignore_case: bool) -> list[Symbol]:
	needle = pattern.casefold() if pattern is not None and ignore_case else pattern
	selected: dict[tuple[int, str], Symbol] = {}
	for symbol in symbols:
		haystack = symbol.name.casefold() if ignore_case else symbol.name
		if needle is None or needle in haystack:
			selected.setdefault((symbol.rva, symbol.name), symbol)
	return sorted(selected.values(), key=lambda symbol: (symbol.rva, symbol.name))


def enumerate_symbols(
	image: Path,
	cache: Path,
	dbghelp_path: Path | None,
	symbol_server: str,
) -> tuple[list[Symbol], int]:
	if sys.platform != "win32":
		raise DumpError("DbgHelp symbol enumeration is only available on Windows")

	selected_dbghelp = dbghelp_path or discover_dbghelp()
	dll_directory = os.add_dll_directory(str(selected_dbghelp.parent)) if selected_dbghelp else None
	try:
		dbghelp = ctypes.WinDLL(str(selected_dbghelp) if selected_dbghelp else "dbghelp.dll", use_last_error=True)
		kernel32 = ctypes.WinDLL("kernel32.dll", use_last_error=True)
	except OSError as error:
		if dll_directory:
			dll_directory.close()
		raise DumpError(f"cannot load DbgHelp: {error}") from error

	kernel32.GetCurrentProcess.argtypes = []
	kernel32.GetCurrentProcess.restype = wintypes.HANDLE
	dbghelp.SymInitializeW.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR, wintypes.BOOL]
	dbghelp.SymInitializeW.restype = wintypes.BOOL
	dbghelp.SymCleanup.argtypes = [wintypes.HANDLE]
	dbghelp.SymCleanup.restype = wintypes.BOOL
	dbghelp.SymSetOptions.argtypes = [wintypes.DWORD]
	dbghelp.SymSetOptions.restype = wintypes.DWORD
	dbghelp.SymLoadModuleExW.argtypes = [
		wintypes.HANDLE,
		wintypes.HANDLE,
		wintypes.LPCWSTR,
		wintypes.LPCWSTR,
		ctypes.c_ulonglong,
		wintypes.DWORD,
		ctypes.c_void_p,
		wintypes.DWORD,
	]
	dbghelp.SymLoadModuleExW.restype = ctypes.c_ulonglong
	dbghelp.SymUnloadModule64.argtypes = [wintypes.HANDLE, ctypes.c_ulonglong]
	dbghelp.SymUnloadModule64.restype = wintypes.BOOL
	dbghelp.UnDecorateSymbolName.argtypes = [ctypes.c_char_p, ctypes.c_char_p, wintypes.DWORD, wintypes.DWORD]
	dbghelp.UnDecorateSymbolName.restype = wintypes.DWORD
	callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, ctypes.POINTER(SYMBOL_INFO), wintypes.ULONG, ctypes.c_void_p)
	dbghelp.SymEnumSymbols.argtypes = [wintypes.HANDLE, ctypes.c_ulonglong, ctypes.c_char_p, callback_type, ctypes.c_void_p]
	dbghelp.SymEnumSymbols.restype = wintypes.BOOL

	process = kernel32.GetCurrentProcess()
	search_path = f"srv*{cache}*{symbol_server}"
	dbghelp.SymSetOptions(
		SYMOPT_DEFERRED_LOADS |
		SYMOPT_FAIL_CRITICAL_ERRORS |
		SYMOPT_EXACT_SYMBOLS |
		SYMOPT_PUBLICS_ONLY |
		SYMOPT_SECURE
	)
	if not dbghelp.SymInitializeW(process, search_path, False):
		if dll_directory:
			dll_directory.close()
		raise DumpError(f"SymInitializeW failed with Win32 error {ctypes.get_last_error()}")

	module_base = 0
	try:
		module_base = dbghelp.SymLoadModuleExW(process, None, str(image), None, 0, 0, None, 0)
		if not module_base:
			raise DumpError(
				f"SymLoadModuleExW failed with Win32 error {ctypes.get_last_error()}; "
				"check network access and that dbghelp.dll has a matching symsrv.dll"
			)

		results: list[Symbol] = []
		undecoration_failures = 0
		scratch = ctypes.create_string_buffer(COMPLETE_NAME_CAPACITY)

		@callback_type
		def callback(info: ctypes.POINTER(SYMBOL_INFO), _size: int, _context: int) -> bool:
			nonlocal undecoration_failures
			name_address = ctypes.addressof(info.contents) + SYMBOL_INFO.Name.offset
			decorated_bytes = ctypes.string_at(name_address, info.contents.NameLen)
			length = dbghelp.UnDecorateSymbolName(
				decorated_bytes,
				scratch,
				COMPLETE_NAME_CAPACITY,
				0,
			)
			if not length or length >= COMPLETE_NAME_CAPACITY - 1:
				undecoration_failures += 1
				return True
			try:
				decorated = decorated_bytes.decode("ascii")
				complete_name = scratch.raw[:length].decode("ascii")
			except UnicodeDecodeError:
				undecoration_failures += 1
				return True
			results.append(Symbol(info.contents.Address - module_base, complete_name, decorated))
			return True

		if not dbghelp.SymEnumSymbols(process, module_base, None, callback, None):
			raise DumpError(f"SymEnumSymbols failed with Win32 error {ctypes.get_last_error()}")
		if not results:
			raise DumpError(
				"DbgHelp loaded the image but no public symbols; install Windows Debugging Tools or pass "
				"--dbghelp pointing to a dbghelp.dll beside its matching symsrv.dll"
			)
		return results, undecoration_failures
	finally:
		if module_base:
			dbghelp.SymUnloadModule64(process, module_base)
		dbghelp.SymCleanup(process)
		if dll_directory:
			dll_directory.close()


def main(argv: list[str] | None = None) -> int:
	args = parse_args(argv)
	try:
		image = expanded_path(args.input)
		cache = expanded_path(args.output)
		dbghelp = expanded_path(args.dbghelp) if args.dbghelp else None
		if not image.is_file():
			raise DumpError(f"input image does not exist: {image}")
		if dbghelp is not None and not dbghelp.is_file():
			raise DumpError(f"DbgHelp does not exist: {dbghelp}")
		if cache.exists() and not cache.is_dir():
			raise DumpError(f"symbol cache is not a directory: {cache}")
		cache.mkdir(parents=True, exist_ok=True)
		symbols, undecoration_failures = enumerate_symbols(image, cache, dbghelp, args.symbol_server)
		selected = select_symbols(symbols, args.pattern, args.ignore_case)
		for symbol in selected:
			print(f"0x{symbol.rva:08X} {symbol.name}" if args.rva else symbol.name)
		if undecoration_failures:
			print(f"warning: skipped {undecoration_failures} symbols that could not be completely undecorated", file=sys.stderr)
		if not selected:
			print("no matching complete symbol names", file=sys.stderr)
			return 1
		return 0
	except (DumpError, OSError) as error:
		print(f"error: {error}", file=sys.stderr)
		return 2
	except Exception as error:
		print(f"internal error: {error}", file=sys.stderr)
		return 3


if __name__ == "__main__":
	raise SystemExit(main())
