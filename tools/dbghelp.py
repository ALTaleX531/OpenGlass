"""Shared DbgHelp binding for exact UNDNAME_COMPLETE public-symbol enumeration."""

from __future__ import annotations

import ctypes
from ctypes import wintypes
from dataclasses import dataclass
import os
from pathlib import Path


COMPLETE_NAME_CAPACITY = 64 * 1024
SYMOPT_DEFERRED_LOADS = 0x00000004
SYMOPT_FAIL_CRITICAL_ERRORS = 0x00000200
SYMOPT_EXACT_SYMBOLS = 0x00000400
SYMOPT_IGNORE_NT_SYMPATH = 0x00001000
SYMOPT_PUBLICS_ONLY = 0x00004000
SYMOPT_SECURE = 0x00040000


class DbgHelpError(Exception):
	pass


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


@dataclass(frozen=True, order=True)
class Symbol:
	rva: int
	name: str
	decorated_name: str


def enumerate_symbols(
	image: Path,
	search_path: str,
	dbghelp_path: Path | None,
	options: int,
	base: int = 0,
	module_name: str | None = None,
) -> tuple[list[Symbol], int]:
	dll_directory = os.add_dll_directory(str(dbghelp_path.parent)) if dbghelp_path else None
	try:
		dbghelp = ctypes.WinDLL(str(dbghelp_path) if dbghelp_path else "dbghelp.dll", use_last_error=True)
		kernel32 = ctypes.WinDLL("kernel32.dll", use_last_error=True)
		kernel32.GetCurrentProcess.argtypes = []
		kernel32.GetCurrentProcess.restype = wintypes.HANDLE
		process = kernel32.GetCurrentProcess()
		dbghelp.SymInitializeW.argtypes = [wintypes.HANDLE, wintypes.LPCWSTR, wintypes.BOOL]
		dbghelp.SymInitializeW.restype = wintypes.BOOL
		dbghelp.SymCleanup.argtypes = [wintypes.HANDLE]
		dbghelp.SymCleanup.restype = wintypes.BOOL
		dbghelp.SymSetOptions.argtypes = [wintypes.DWORD]
		dbghelp.SymSetOptions.restype = wintypes.DWORD
		dbghelp.SymLoadModuleExW.argtypes = [
			wintypes.HANDLE, wintypes.HANDLE, wintypes.LPCWSTR, wintypes.LPCWSTR,
			ctypes.c_ulonglong, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
		]
		dbghelp.SymLoadModuleExW.restype = ctypes.c_ulonglong
		dbghelp.SymUnloadModule64.argtypes = [wintypes.HANDLE, ctypes.c_ulonglong]
		dbghelp.SymUnloadModule64.restype = wintypes.BOOL
		dbghelp.UnDecorateSymbolName.argtypes = [ctypes.c_char_p, ctypes.c_char_p, wintypes.DWORD, wintypes.DWORD]
		dbghelp.UnDecorateSymbolName.restype = wintypes.DWORD
		callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, ctypes.POINTER(SYMBOL_INFO), wintypes.ULONG, ctypes.c_void_p)
		dbghelp.SymEnumSymbols.argtypes = [wintypes.HANDLE, ctypes.c_ulonglong, ctypes.c_char_p, callback_type, ctypes.c_void_p]
		dbghelp.SymEnumSymbols.restype = wintypes.BOOL
		dbghelp.SymSetOptions(options)
		if not dbghelp.SymInitializeW(process, search_path, False):
			raise DbgHelpError(f"SymInitializeW failed: {ctypes.get_last_error()}")
		module = 0
		try:
			module = dbghelp.SymLoadModuleExW(process, None, str(image), module_name, base, 0, None, 0)
			if not module:
				raise DbgHelpError(f"SymLoadModuleExW failed for {image}: {ctypes.get_last_error()}")
			results: list[Symbol] = []
			failures = 0
			scratch = ctypes.create_string_buffer(COMPLETE_NAME_CAPACITY)
			@callback_type
			def callback(info: ctypes.POINTER(SYMBOL_INFO), _size: int, _context: int) -> bool:
				nonlocal failures
				decorated = ctypes.string_at(ctypes.addressof(info.contents) + SYMBOL_INFO.Name.offset, info.contents.NameLen)
				length = dbghelp.UnDecorateSymbolName(decorated, scratch, len(scratch), 0)  # UNDNAME_COMPLETE
				if not length or length + 1 >= len(scratch):
					failures += 1
					return True
				try:
					results.append(Symbol(info.contents.Address - module, scratch.raw[:length].decode("ascii"), decorated.decode("ascii")))
				except UnicodeDecodeError:
					failures += 1
				return True
			if not dbghelp.SymEnumSymbols(process, module, None, callback, None):
				raise DbgHelpError(f"SymEnumSymbols failed for {image}: {ctypes.get_last_error()}")
			return results, failures
		finally:
			if module:
				dbghelp.SymUnloadModule64(process, module)
			dbghelp.SymCleanup(process)
	except OSError as error:
		raise DbgHelpError(f"cannot load DbgHelp: {error}") from error
	finally:
		if dll_directory:
			dll_directory.close()
