from __future__ import annotations

import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import uuid


SCRIPT = Path(__file__).with_name("audit_symbol_resolution.py")
SPEC = importlib.util.spec_from_file_location("audit_symbol_resolution", SCRIPT)
assert SPEC and SPEC.loader
AUDIT = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = AUDIT
SPEC.loader.exec_module(AUDIT)


class SymbolResolutionAuditTests(unittest.TestCase):
	def test_version_and_activity(self) -> None:
		self.assertEqual(AUDIT.Version.parse("10.0.26100.8972"), AUDIT.Version(26100, 8972))
		descriptor = {"min_inclusive": {"build": 22000, "revision": 10}, "max_exclusive": {"build": 26100, "revision": 5}}
		self.assertFalse(AUDIT.in_range(AUDIT.Version(22000, 9), descriptor))
		self.assertTrue(AUDIT.in_range(AUDIT.Version(22000, 10), descriptor))
		self.assertFalse(AUDIT.in_range(AUDIT.Version(26100, 5), descriptor))
		debug_descriptor = {
			"bindings": [
				{"symbol_names": ["Debug"], "min_inclusive": None, "max_exclusive": {"build": 26100, "revision": 5}},
				{"symbol_names": ["Debug"], "min_inclusive": {"build": 26100, "revision": 10}, "max_exclusive": None},
			],
			"condition": "debug",
		}
		self.assertIsNone(AUDIT.select_binding(AUDIT.Version(26100, 1), debug_descriptor, "release"))
		self.assertEqual(0, AUDIT.select_binding(AUDIT.Version(26100, 1), debug_descriptor, "debug")[0])
		self.assertIsNone(AUDIT.select_binding(AUDIT.Version(26100, 7), debug_descriptor, "debug"))
		self.assertEqual(1, AUDIT.select_binding(AUDIT.Version(26100, 10), debug_descriptor, "debug")[0])

	def test_pe_and_pdb_guid_age_pairing(self) -> None:
		guid = uuid.UUID("12345678-1234-5678-90ab-cdef12345678")
		age = 7
		with tempfile.TemporaryDirectory() as directory:
			root = Path(directory)
			image = root / "uDWM.dll"
			pdb = root / "uDWM.pdb"
			self._write_pe(image, guid, age)
			self._write_pdb(pdb, guid, age)
			image_identity = AUDIT.read_pe_codeview(image)
			pdb_identity = AUDIT.read_pdb_identity(pdb)
			self.assertEqual(image_identity.guid, guid)
			self.assertEqual(image_identity.age, age)
			self.assertEqual(pdb_identity.guid, guid)
			self.assertEqual(pdb_identity.age, age)
			selected, _, paired = AUDIT.find_pdb(root, image_identity)
			self.assertEqual(selected, pdb.resolve())
			self.assertTrue(paired)

	@staticmethod
	def _write_pe(path: Path, guid: uuid.UUID, age: int) -> None:
		data = bytearray(0x400)
		struct.pack_into("<I", data, 0x3C, 0x80)
		data[0x80:0x84] = b"PE\0\0"
		struct.pack_into("<HHIIIHH", data, 0x84, 0x8664, 1, 0, 0, 0, 0xF0, 0)
		optional = 0x98
		struct.pack_into("<H", data, optional, 0x20B)
		struct.pack_into("<II", data, optional + 112 + 6 * 8, 0x1000, 28)
		section = optional + 0xF0
		data[section:section + 8] = b".rdata\0\0"
		struct.pack_into("<IIII", data, section + 8, 0x200, 0x1000, 0x200, 0x200)
		codeview = b"RSDS" + guid.bytes_le + struct.pack("<I", age) + b"uDWM.pdb\0"
		struct.pack_into("<IIHHIIII", data, 0x200, 0, 0, 0, 0, 2, len(codeview), 0x1020, 0x220)
		data[0x220:0x220 + len(codeview)] = codeview
		path.write_bytes(data)

	@staticmethod
	def _write_pdb(path: Path, guid: uuid.UUID, age: int) -> None:
		block_size = 512
		data = bytearray(block_size * 5)
		magic = b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\0\0\0"
		data[:len(magic)] = magic
		directory = struct.pack("<I4I2I", 4, 0xFFFFFFFF, 28, 0xFFFFFFFF, 12, 3, 4)
		struct.pack_into("<IIIIII", data, 32, block_size, 0, 5, len(directory), 0, 1)
		struct.pack_into("<I", data, block_size, 2)
		data[block_size * 2:block_size * 2 + len(directory)] = directory
		info = struct.pack("<III", 0, 0, age + 1) + guid.bytes_le
		data[block_size * 3:block_size * 3 + len(info)] = info
		dbi = struct.pack("<iII", -1, 0, age)
		data[block_size * 4:block_size * 4 + len(dbi)] = dbi
		path.write_bytes(data)


if __name__ == "__main__":
	unittest.main()
