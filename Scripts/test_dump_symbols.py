from __future__ import annotations

import contextlib
import io
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import dump_symbols


class DumpSymbolsTests(unittest.TestCase):
	def test_filters_complete_names_and_deduplicates_records(self) -> None:
		symbols = [
			dump_symbols.Symbol(0x20, "public: void __cdecl COcclusionContext::PreSubgraph(void)", "?PreSubgraph@@"),
			dump_symbols.Symbol(0x20, "public: void __cdecl COcclusionContext::PreSubgraph(void)", "?PreSubgraphAlias@@"),
			dump_symbols.Symbol(0x10, "public: void __cdecl COcclusionContext::Compute(void)", "?Compute@@"),
		]
		selected = dump_symbols.select_symbols(symbols, "COcclusionContext::PreSubgraph", False)
		self.assertEqual(selected, [symbols[0]])
		self.assertEqual(
			dump_symbols.select_symbols(symbols, "cocclusioncontext::presubgraph", True),
			[symbols[0]],
		)

	def test_cli_output_and_exit_codes(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			image = root / "dwmcore.dll"
			image.write_bytes(b"PE test fixture")
			cache = root / "symbols"

			def fake_enumerator(_image: Path, _cache: Path, _dbghelp: Path | None, _server: str):
				return [dump_symbols.Symbol(0x123, "public: void __cdecl Type::Method(void)", "?Method@Type@@")], 0

			output = io.StringIO()
			with contextlib.redirect_stdout(output):
				result = dump_symbols.main([
					"--input", str(image), "--output", str(cache), "--grep", "Type::Method", "--rva",
				], fake_enumerator)
			self.assertEqual(result, 0)
			self.assertEqual(output.getvalue(), "0x00000123 public: void __cdecl Type::Method(void)\n")
			self.assertTrue(cache.is_dir())

			error = io.StringIO()
			with contextlib.redirect_stderr(error):
				result = dump_symbols.main([
					"--input", str(image), "--output", str(cache), "--grep", "Missing",
				], fake_enumerator)
			self.assertEqual(result, 1)
			self.assertIn("no matching", error.getvalue())

	def test_cli_defaults_to_temp_symbol_cache(self) -> None:
		with tempfile.TemporaryDirectory() as temporary:
			root = Path(temporary)
			image = root / "dwmcore.dll"
			image.write_bytes(b"PE test fixture")
			expected_cache = (root / "temp" / "symbols").resolve()
			observed_caches: list[Path] = []

			def fake_enumerator(_image: Path, cache: Path, _dbghelp: Path | None, _server: str):
				observed_caches.append(cache)
				return [dump_symbols.Symbol(0x123, "public: void __cdecl Type::Method(void)", "?Method@Type@@")], 0

			with (
				mock.patch.dict(os.environ, {"TEMP": str(root / "temp")}),
				contextlib.redirect_stdout(io.StringIO()),
			):
				result = dump_symbols.main(["--input", str(image), "--grep", "Type::Method"], fake_enumerator)

			self.assertEqual(result, 0)
			self.assertEqual(observed_caches, [expected_cache])
			self.assertTrue(expected_cache.is_dir())


if __name__ == "__main__":
	unittest.main()
