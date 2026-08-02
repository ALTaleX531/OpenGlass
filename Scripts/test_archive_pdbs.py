from __future__ import annotations

import hashlib
import io
from pathlib import Path
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
import zipfile

import archive_pdbs


class ArchivePdbsTests(unittest.TestCase):
	def setUp(self) -> None:
		self.temporary = tempfile.TemporaryDirectory()
		self.repo = Path(self.temporary.name)
		self.build = self.repo / "Build" / "x64" / "Release"
		(self.build / "legacy").mkdir(parents=True)
		(self.build / "common").mkdir()
		(self.build / "legacy" / "OpenGlass.pdb").write_bytes(b"core-pdb" * 128)
		(self.build / "common" / "OpenGlassHost.pdb").write_bytes(b"host-pdb" * 128)
		(self.build / "common" / "OpenGlassGUI.pdb").write_bytes(b"gui-pdb" * 128)

	def tearDown(self) -> None:
		self.temporary.cleanup()

	def run_tool(self, *arguments: str) -> tuple[int, str, str]:
		stdout = io.StringIO()
		stderr = io.StringIO()
		with redirect_stdout(stdout), redirect_stderr(stderr):
			result = archive_pdbs.main([str(self.repo), "--architecture", "legacy", *arguments])
		return result, stdout.getvalue(), stderr.getvalue()

	def test_creates_expected_flat_archive(self) -> None:
		result, stdout, stderr = self.run_tool()
		self.assertEqual(result, 0, stderr)
		archive_path = self.build / "OpenGlassSymbols.Legacy.zip"
		self.assertEqual(Path(stdout.strip()), archive_path)
		with zipfile.ZipFile(archive_path) as archive:
			self.assertEqual(archive.namelist(), ["OpenGlass.pdb", "OpenGlassHost.pdb", "OpenGlassGUI.pdb"])
			self.assertEqual(archive.read("OpenGlass.pdb"), b"core-pdb" * 128)
			self.assertTrue(all(item.date_time == archive_pdbs.ZIP_TIMESTAMP for item in archive.infolist()))

	def test_archive_is_deterministic(self) -> None:
		self.assertEqual(self.run_tool()[0], 0)
		archive_path = self.build / "OpenGlassSymbols.Legacy.zip"
		first = hashlib.sha256(archive_path.read_bytes()).digest()
		self.assertEqual(self.run_tool()[0], 0)
		second = hashlib.sha256(archive_path.read_bytes()).digest()
		self.assertEqual(first, second)

	def test_missing_input_preserves_existing_output(self) -> None:
		output = self.build / "symbols.zip"
		output.write_bytes(b"existing")
		(self.build / "common" / "OpenGlassGUI.pdb").unlink()
		result, _, stderr = self.run_tool("--output", str(output))
		self.assertEqual(result, 2)
		self.assertIn("OpenGlassGUI.pdb", stderr)
		self.assertEqual(output.read_bytes(), b"existing")


if __name__ == "__main__":
	unittest.main()
