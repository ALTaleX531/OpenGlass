from __future__ import annotations

from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock
import uuid

from tools import audit_symbol_resolution as AUDIT, audit_winbindex_revisions as revisions
from tests.test_symbol_catalog import make_schema


class SymbolResolutionAuditTests(unittest.TestCase):
	def test_shared_audit_failures_and_entrypoint_reports(self) -> None:
		guid = uuid.UUID("12345678-1234-5678-90ab-cdef12345678")
		age = 0x1A
		version = AUDIT.Version(100, 7)
		names = {"public: void __cdecl Required(void)": {0x1000}}
		schema = make_schema("udwm")
		schema["min_inclusive"] = {"build": 100, "revision": 7}
		schema["max_exclusive"] = {"build": 100, "revision": 9}
		schema["symbols"][0]["bindings"][0]["max_exclusive"] = {"build": 100, "revision": 8}
		identity_issue = "PDB GUID/DBI age does not match the image CodeView record"
		cases = [
			("paired", guid, age, version, "100.7", names, []),
			("wrong GUID", uuid.UUID(int=1), age, version, "100.7", names, [identity_issue]),
			("wrong DBI age", guid, age + 1, version, "100.7", names, [identity_issue]),
			("wrong version", guid, age, AUDIT.Version(100, 8), "100.7", names,
				["requested version 100.7 does not match image version 100.8"]),
			("missing version", guid, age, None, "100.7", names, ["image file version is unavailable"]),
			("unsupported version", guid, age, AUDIT.Version(100, 9), "100.9", names,
				["version is outside module support range"]),
			("missing Required", guid, age, version, "100.7", {}, ["Required.Id=missing"]),
			("ambiguous Required", guid, age, version, "100.7", {next(iter(names)): {0x1000, 0x1010}}, ["Required.Id=ambiguous"]),
			("inactive Required", guid, age, AUDIT.Version(100, 8), "100.8", {}, []),
			("combined", uuid.UUID(int=1), age, version, "100.6", {}, ["Required.Id=missing", identity_issue,
				"requested version 100.6 does not match image version 100.7", "version is outside module support range"]),
		]
		with tempfile.TemporaryDirectory() as directory:
			root = Path(directory).resolve()
			image, pdb = root / "uDWM.dll", root / "uDWM.pdb"
			self._write_pe(image, guid, age)
			data = bytearray(image.read_bytes())
			struct.pack_into("<I", data, 0x98 + 56, 0x2000)
			struct.pack_into("<I", data, 0x188 + 36, 0x20000000)
			image.write_bytes(data)
			resolver = {"path": str(root / "dbghelp.dll"), "version": None, "sha256": "a" * 64}
			for label, pdb_guid, pdb_age, actual, requested, resolved, expected in cases:
				with (
					self.subTest(case=label),
					mock.patch.object(AUDIT, "image_version", return_value=actual),
					mock.patch.object(AUDIT, "enumerate_complete_names", return_value=(resolved, 0)),
				):
					self._write_pdb(pdb, pdb_guid, pdb_age)
					arguments = [str(root), "--architecture", "legacy", "--module", "udwm",
						"--version", requested, "--image", str(image), "--symbol-path", str(pdb)]
					report = AUDIT.inspect(AUDIT.parse_args(arguments), schema=schema, resolver=resolver)
					self.assertEqual(AUDIT.audit_failures(report), expected)
					if label not in {"paired", "combined"}:
						continue
					# Thin command adapters need one success and one combined-failure report.
					with mock.patch.object(AUDIT, "inspect", return_value=report):
						output = io.StringIO()
						with redirect_stdout(output):
							self.assertEqual(AUDIT.main(arguments + ["--format", "json"]), int(bool(expected)))
						self.assertEqual(json.loads(output.getvalue()), report)
						output = io.StringIO()
						with redirect_stdout(output):
							self.assertEqual(AUDIT.main(arguments + ["--format", "text"]), int(bool(expected)))
						for detail in [f"Image: {image}", f"PDB: {pdb}", "Image CodeView:", "PDB SHA-256:",
							f"Evidence: {report['evidence']}", *report["evidence_issues"]]:
							self.assertIn(detail, output.getvalue())
						output = io.StringIO()
						with (
							mock.patch.object(revisions, "load_index", return_value={}),
							mock.patch.object(revisions, "discover_samples", return_value=[
								revisions.Sample(AUDIT.Version.parse(requested), "b" * 64, 0, 0x2000, image.stat().st_size),
							]),
							mock.patch.object(revisions, "ensure_image", return_value=image),
							mock.patch.object(revisions, "prime_symbol_cache", return_value=pdb),
							mock.patch.object(AUDIT.projection_schema, "validate_schema", return_value=schema),
							mock.patch.object(AUDIT, "resolver_identity", return_value=resolver),
							redirect_stdout(output), redirect_stderr(io.StringIO()),
						):
							self.assertEqual(revisions.main([str(root), "--architecture", "legacy", "--module", "udwm",
								"--build", "100", "--binary-audit"]), int(bool(expected)))
						detail = f" ({'; '.join(expected)})" if expected else ""
						self.assertEqual(output.getvalue(), f"{requested}: {'failed' if expected else 'passed'}{detail}\n")
					# PDB-only revision audits share Required/support checks without image evidence.
					pdb_report = {"module_supported": report["module_supported"], "descriptors": report["descriptors"]}
					self.assertEqual(AUDIT.audit_failures(pdb_report, binary_audit=False),
						[issue for issue in expected if issue not in report["evidence_issues"]])

	def test_pe_and_pdb_guid_age_pairing(self) -> None:
		guid = uuid.UUID("12345678-1234-5678-90ab-cdef12345678")
		age = 0x1A
		with tempfile.TemporaryDirectory() as directory:
			root = Path(directory)
			image = root / "uDWM.dll"
			pdb = root / "uDWM.pdb"
			self._write_pe(image, guid, age)
			self._write_pdb(pdb, guid, age)
			image_identity = AUDIT.read_pe_codeview(image)
			self.assertIsNone(AUDIT.image_version(image))
			pdb_identity = AUDIT.read_pdb_identity(pdb)
			self.assertEqual(image_identity.guid, guid)
			self.assertEqual(image_identity.age, age)
			self.assertEqual(image_identity.key(), "123456781234567890ABCDEF123456781a")
			self.assertEqual(pdb_identity.guid, guid)
			self.assertEqual(pdb_identity.age, age)
			self.assertEqual(pdb_identity.key(), image_identity.key())
			selected, _, paired = AUDIT.find_pdb(root, image_identity)
			self.assertEqual(selected, pdb.resolve())
			self.assertTrue(paired)
			# Either identity mismatch keeps a readable PDB discovery-only.
			for pdb_guid, pdb_age in [(guid, age + 1), (uuid.UUID(int=0), age)]:
				with self.subTest(guid=pdb_guid, age=pdb_age):
					self._write_pdb(pdb, pdb_guid, pdb_age)
					_, _, paired = AUDIT.find_pdb(root, image_identity)
					self.assertFalse(paired)

	def test_explicit_binary_audit_checks_rvas_and_complete_headers(self) -> None:
		with tempfile.TemporaryDirectory() as directory:
			image = Path(directory) / "udwm.dll"
			self._write_pe(image, uuid.UUID(int=1), 1)
			data = bytearray(image.read_bytes())
			struct.pack_into("<I", data, 0x98 + 56, 0x2000)
			struct.pack_into("<I", data, 0x188 + 36, 0x20000000)
			image.write_bytes(data)
			descriptor = {"id": "Function", "status": "unique", "rvas": ["0x1000"], "kind": "projected_function"}
			AUDIT.validate_image_rvas(image, [descriptor])
			descriptor["rvas"] = ["0x2000"]
			with self.assertRaisesRegex(AUDIT.AuditError, "outside SizeOfImage"):
				AUDIT.validate_image_rvas(image, [descriptor])
			for offset, value, message in [(0x98, 0x101, "optional header"), (0x86, 2, "section table")]:
				with self.subTest(message=message):
					malformed = data.copy()
					struct.pack_into("<H", malformed, offset, value)
					image.write_bytes(malformed)
					with self.assertRaisesRegex(AUDIT.AuditError, message):
						AUDIT.validate_image_rvas(image, [descriptor])

	@staticmethod
	def _write_pe(path: Path, guid: uuid.UUID, age: int) -> None:
		data = bytearray(0x400)
		data[:2] = b"MZ"
		struct.pack_into("<I", data, 0x3C, 0x80)
		data[0x80:0x84] = b"PE\0\0"
		struct.pack_into("<HHIIIHH", data, 0x84, 0x8664, 1, 0, 0, 0, 0xF0, 0)
		optional = 0x98
		struct.pack_into("<H", data, optional, 0x20B)
		struct.pack_into("<I", data, optional + 108, 16)
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
