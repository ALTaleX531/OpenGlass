from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


SCRIPT = Path(__file__).parents[1] / "scripts" / "audit_winbindex_revisions.py"
SPEC = importlib.util.spec_from_file_location("audit_winbindex_revisions", SCRIPT)
assert SPEC and SPEC.loader
AUDIT = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = AUDIT
SPEC.loader.exec_module(AUDIT)


class WinbindexRevisionAuditTests(unittest.TestCase):
	def test_discovers_sorted_amd64_build_samples(self) -> None:
		index = {
			"b" * 64: {"fileInfo": {
				"sha256": "b" * 64, "machineType": 0x8664, "timestamp": 2, "virtualSize": 0x1234,
				"size": 200, "version": "10.0.26100.4202 (WinBuild.160101.0800)",
			}},
			"a" * 64: {"fileInfo": {
				"sha256": "a" * 64, "machineType": 0x8664, "timestamp": 1, "virtualSize": 0x1200,
				"size": 100, "version": "10.0.26100.1 (WinBuild.160101.0800)",
			}},
			"c" * 64: {"fileInfo": {
				"sha256": "c" * 64, "machineType": 0x14C, "timestamp": 3, "virtualSize": 0x1300,
				"size": 300, "version": "10.0.26100.5000",
			}},
		}
		samples = AUDIT.discover_samples(index, 26100)
		self.assertEqual([sample.version.revision for sample in samples], [1, 4202])
		self.assertEqual(samples[1].sha256, "b" * 64)

	def test_revision_filter_is_inclusive(self) -> None:
		index = {}
		for revision in (1, 100, 200):
			digest = f"{revision:064x}"
			index[digest] = {"fileInfo": {
				"sha256": digest, "machineType": 0x8664, "timestamp": revision,
				"virtualSize": 0x1000, "size": 100, "version": f"10.0.26100.{revision}",
			}}
		samples = AUDIT.discover_samples(index, 26100, 100, 200)
		self.assertEqual([sample.version.revision for sample in samples], [100, 200])

	def test_rejects_inconsistent_index_hash(self) -> None:
		index = {"a" * 64: {"fileInfo": {
			"sha256": "b" * 64, "machineType": 0x8664, "timestamp": 1,
			"virtualSize": 0x1000, "size": 100, "version": "10.0.26100.1",
		}}}
		self.assertEqual(AUDIT.discover_samples(index, 26100), [])

	def test_symbol_server_urls_follow_index_and_codeview_keys(self) -> None:
		sample = AUDIT.WinbindexSample(AUDIT.SYMBOL_AUDIT.Version(26100, 1), "a" * 64, 0x1234ABCD, 0x9876, 1, {})
		self.assertEqual(
			AUDIT.image_url("https://symbols", "dwmcore.dll", sample),
			"https://symbols/dwmcore.dll/1234ABCD9876/dwmcore.dll",
		)
		identity = AUDIT.SYMBOL_AUDIT.PdbIdentity(
			"dwmcore.pdb", __import__("uuid").UUID("12345678-1234-5678-90ab-cdef12345678"), 2
		)
		self.assertEqual(
			AUDIT.pdb_url("https://symbols", identity),
			"https://symbols/dwmcore.pdb/123456781234567890ABCDEF123456782/dwmcore.pdb",
		)

	def test_optional_failures_are_strict_by_default(self) -> None:
		descriptors = [
			{"id": "required", "requirement": "required", "status": "missing"},
			{"id": "optional", "requirement": "optional", "status": "ambiguous"},
			{"id": "inactive", "requirement": "required", "status": "inactive"},
		]
		self.assertEqual(len(AUDIT.descriptor_failures(descriptors, False)), 2)
		self.assertEqual(
			[item["id"] for item in AUDIT.descriptor_failures(descriptors, True)], ["required"]
		)


if __name__ == "__main__":
	unittest.main()
