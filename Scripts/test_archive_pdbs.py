from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
import zipfile

import archive_pdbs


class ArchivePdbsTests(unittest.TestCase):
	def setUp(self) -> None:
		self.temporary = tempfile.TemporaryDirectory()
		self.repo = Path(self.temporary.name)
		self.build_root = self.repo / "Build" / "x64" / "Release"
		self.contents = {
			"legacy/OpenGlass.pdb": b"legacy-pdb",
			"milcomp/OpenGlass.pdb": b"milcomp-pdb",
			"OpenGlassHost.pdb": b"host-pdb",
			"OpenGlassGUI.pdb": b"gui-pdb",
		}
		for archive_name, content in self.contents.items():
			if "/" in archive_name:
				architecture, name = archive_name.split("/", 1)
				path = self.build_root / architecture / name
			else:
				path = self.build_root / "common" / archive_name
			path.parent.mkdir(parents=True, exist_ok=True)
			path.write_bytes(content)

	def tearDown(self) -> None:
		self.temporary.cleanup()

	def test_writes_all_pdbs_deterministically(self) -> None:
		output = archive_pdbs.default_output(self.repo, "Release")
		inputs = archive_pdbs.collect_inputs(self.repo, "Release")
		archive_pdbs.write_archive(inputs, output)
		first = output.read_bytes()

		with zipfile.ZipFile(output) as archive:
			self.assertEqual(archive.namelist(), list(self.contents))
			for name, content in self.contents.items():
				self.assertEqual(archive.read(name), content)
				self.assertEqual(archive.getinfo(name).date_time, archive_pdbs.ZIP_TIMESTAMP)

		archive_pdbs.write_archive(inputs, output)
		self.assertEqual(output.read_bytes(), first)

	def test_rejects_missing_or_empty_pdb(self) -> None:
		legacy_pdb = self.build_root / "legacy" / "OpenGlass.pdb"
		legacy_pdb.unlink()
		with self.assertRaisesRegex(archive_pdbs.InputError, "does not exist"):
			archive_pdbs.collect_inputs(self.repo, "Release")

		legacy_pdb.write_bytes(b"")
		with self.assertRaisesRegex(archive_pdbs.InputError, "is empty"):
			archive_pdbs.collect_inputs(self.repo, "Release")


if __name__ == "__main__":
	unittest.main()
