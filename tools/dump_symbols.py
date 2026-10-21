#!/usr/bin/env python3
"""Download matching public symbols for a PE image and print complete C++ names."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path
from typing import Iterable

from tools import dbghelp
from tools.dbghelp import Symbol


MICROSOFT_SYMBOL_SERVER = "https://msdl.microsoft.com/download/symbols"
DEFAULT_SYMBOL_CACHE = r"%TEMP%\symbols"


class DumpError(Exception):
	pass


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--input", type=str, required=True, help="PE image whose exact symbols should be loaded")
	parser.add_argument(
		"--output",
		type=str,
		default=DEFAULT_SYMBOL_CACHE,
		help="local symbol-server cache directory (default: %(default)s)",
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
	try:
		results, failures = dbghelp.enumerate_symbols(
			image, f"srv*{cache}*{symbol_server}", dbghelp_path or discover_dbghelp(),
			dbghelp.SYMOPT_DEFERRED_LOADS | dbghelp.SYMOPT_FAIL_CRITICAL_ERRORS | dbghelp.SYMOPT_EXACT_SYMBOLS |
			dbghelp.SYMOPT_PUBLICS_ONLY | dbghelp.SYMOPT_SECURE,
		)
	except dbghelp.DbgHelpError as error:
		raise DumpError(str(error)) from error
	if not results:
		raise DumpError("DbgHelp loaded no public symbols; use --dbghelp with a matching symsrv.dll")
	return results, failures


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
