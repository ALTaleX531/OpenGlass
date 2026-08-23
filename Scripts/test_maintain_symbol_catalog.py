from __future__ import annotations

import argparse
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import audit_symbol_resolution
import audit_winbindex_revisions as revisions
import maintain_symbol_catalog as maintain
import projection_schema


class MaintenanceTests(unittest.TestCase):
	def setUp(self) -> None:
		self.temporary = tempfile.TemporaryDirectory()
		self.root = Path(self.temporary.name)

	def tearDown(self) -> None:
		self.temporary.cleanup()

	def sample_entry(self, digest: str, revision: int = 7) -> dict:
		return {
			"fileInfo": {
				"version": f"10.0.100.{revision}", "machineType": revisions.AMD64,
				"sha256": digest, "timestamp": 123, "virtualSize": 0x1000, "size": 4,
			},
			"windowsVersions": {"Windows": {"assembly": ["KB1"]}},
		}

	def arguments(self) -> argparse.Namespace:
		return argparse.Namespace(build=100, min_revision=None, max_revision=None)

	def test_duplicate_hash_deduplicates_and_output_is_deterministic(self) -> None:
		digest = "1" * 64
		index = {digest: self.sample_entry(digest), "alias": self.sample_entry(digest)}
		first = revisions.discover_samples(index, self.arguments())
		second = revisions.discover_samples(index, self.arguments())
		self.assertEqual(first, second)
		self.assertEqual(len(first), 1)
		self.assertEqual(first[0].provenance, ("Windows | KB1",))

	def test_recovers_missing_fileinfo_version_and_virtual_size(self) -> None:
		digest = "1" * 64
		entry = self.sample_entry(digest)
		info = entry["fileInfo"]
		del info["version"]
		del info["virtualSize"]
		info["size"] = 0x8000
		info["lastSectionVirtualAddress"] = 0xF000
		info["lastSectionPointerToRawData"] = 0x7000
		entry["windowsVersions"] = {
			"Windows": {
				"KB1": {
					"assemblies": {
						"component": {
							"assemblyIdentity": {"version": "10.0.100.7"}
						}
					}
				}
			}
		}
		samples = revisions.discover_samples({digest: entry}, self.arguments())
		self.assertEqual(len(samples), 1)
		self.assertEqual(samples[0].version, audit_symbol_resolution.Version(100, 7))
		self.assertEqual(samples[0].virtual_size, 0x10000)

	def test_same_version_distinct_hashes_are_retained(self) -> None:
		index = {"1" * 64: self.sample_entry("1" * 64), "2" * 64: self.sample_entry("2" * 64)}
		self.assertEqual(len(revisions.discover_samples(index, self.arguments())), 2)

	def test_symbol_server_collision_uses_exact_image_root(self) -> None:
		candidate = self.root / "images" / "uDWM.dll"
		candidate.parent.mkdir()
		candidate.write_bytes(b"good")
		digest = hashlib.sha256(b"good").hexdigest()
		sample = revisions.Sample(audit_symbol_resolution.Version(100, 7), digest, 123, 0x1000, 4)
		with (
			mock.patch.object(revisions, "fetch", return_value=b"evil"),
			mock.patch.object(revisions, "_pe_machine", return_value=revisions.AMD64),
			mock.patch.object(revisions.AUDIT, "image_version", return_value=sample.version),
		):
			result = revisions.ensure_image(self.root / "cache", "uDWM.dll", sample, {digest: [candidate]})
		self.assertEqual(result.read_bytes(), b"good")

	def test_symbol_server_collision_is_never_cached(self) -> None:
		sample = revisions.Sample(audit_symbol_resolution.Version(100, 7), "1" * 64, 123, 0x1000, 4)
		with mock.patch.object(revisions, "fetch", return_value=b"evil"):
			with self.assertRaises(revisions.SymbolServerIdentityCollision) as raised:
				revisions.ensure_image(self.root / "cache", "uDWM.dll", sample, {})
		self.assertEqual(raised.exception.sample, sample)
		self.assertEqual(raised.exception.received_sha256, hashlib.sha256(b"evil").hexdigest())
		self.assertFalse(any((self.root / "cache").rglob("uDWM.dll")))

	def test_image_root_rejects_wrong_module_machine_and_version(self) -> None:
		candidate = self.root / "dwmcore.dll"
		candidate.write_bytes(b"good")
		digest = hashlib.sha256(b"good").hexdigest()
		sample = revisions.Sample(audit_symbol_resolution.Version(100, 7), digest, 123, 0x1000, 4)
		with mock.patch.object(revisions, "fetch", side_effect=revisions.AuditError("offline")):
			with self.assertRaisesRegex(revisions.AuditError, "module name"):
				revisions.ensure_image(self.root / "cache1", "uDWM.dll", sample, {digest: [candidate]})
		candidate = self.root / "uDWM.dll"
		candidate.write_bytes(b"good")
		with (
			mock.patch.object(revisions, "fetch", side_effect=revisions.AuditError("offline")),
			mock.patch.object(revisions, "_pe_machine", return_value=0x14C),
		):
			with self.assertRaisesRegex(revisions.AuditError, "machine"):
				revisions.ensure_image(self.root / "cache2", "uDWM.dll", sample, {digest: [candidate]})
		with (
			mock.patch.object(revisions, "fetch", side_effect=revisions.AuditError("offline")),
			mock.patch.object(revisions, "_pe_machine", return_value=revisions.AMD64),
			mock.patch.object(revisions.AUDIT, "image_version", return_value=audit_symbol_resolution.Version(100, 8)),
		):
			with self.assertRaisesRegex(revisions.AuditError, "version"):
				revisions.ensure_image(self.root / "cache3", "uDWM.dll", sample, {digest: [candidate]})

	def test_image_root_rejects_wrong_hash_before_metadata(self) -> None:
		candidate = self.root / "uDWM.dll"
		candidate.write_bytes(b"evil")
		sample = revisions.Sample(audit_symbol_resolution.Version(100, 7), "1" * 64, 123, 0x1000, 4)
		with mock.patch.object(revisions, "fetch", side_effect=revisions.AuditError("offline")):
			with self.assertRaisesRegex(revisions.AuditError, "offline"):
				revisions.ensure_image(self.root / "cache", "uDWM.dll", sample, {hashlib.sha256(b"evil").hexdigest(): [candidate]})

	def test_prime_symbol_cache_rejects_wrong_pdb_guid_age(self) -> None:
		image = self.root / "uDWM.dll"
		image.write_bytes(b"image")
		identity = mock.Mock()
		identity.name = "x.pdb"
		identity.key.return_value = "KEY"
		identity.guid = "expected"
		identity.age = 1
		other = mock.Mock()
		other.guid = "other"
		other.age = 1
		with (
			mock.patch.object(revisions.AUDIT, "read_pe_codeview", return_value=identity),
			mock.patch.object(revisions.AUDIT, "read_pdb_identity", return_value=other),
			mock.patch.object(revisions, "fetch", return_value=b"pdb"),
			mock.patch.object(revisions.subprocess, "run", return_value=mock.Mock(returncode=1, stderr="invalid cabinet", stdout="")),
		):
			with self.assertRaisesRegex(revisions.AuditError, "wrong GUID/age"):
				revisions.prime_symbol_cache(image, self.root / "cache")

	def test_compressed_pdb_fallback_expands_and_validates_identity(self) -> None:
		identity = mock.Mock()
		identity.name = "uDWM.pdb"
		identity.key.return_value = "GUID1"
		identity.guid = "expected"
		identity.age = 1
		pdb = self.root / "symbols" / "uDWM.pdb" / "GUID1" / "uDWM.pdb"

		def expand(arguments: list[str], **_kwargs: object) -> mock.Mock:
			Path(arguments[2]).write_bytes(b"expanded-pdb")
			return mock.Mock(returncode=0, stderr="", stdout="")

		with (
			mock.patch.object(revisions, "fetch", side_effect=[revisions.AuditError("HTTP 404"), b"compressed-pdb"]) as fetch,
			mock.patch.object(revisions.subprocess, "run", side_effect=expand) as run,
			mock.patch.object(revisions.AUDIT, "read_pdb_identity", return_value=identity),
		):
			revisions._download_pdb(pdb, identity)

		self.assertEqual(pdb.read_bytes(), b"expanded-pdb")
		self.assertTrue(fetch.call_args_list[1].args[0].endswith("/uDWM.pd_"))
		self.assertEqual(run.call_count, 1)


	def test_atomic_failure_preserves_previous_file(self) -> None:
		path = self.root / "catalog.json"
		path.write_bytes(b"old")
		with mock.patch.object(maintain.os, "replace", side_effect=OSError("failure")):
			with self.assertRaises(OSError):
				maintain.atomic_write(path, b"new")
		self.assertEqual(path.read_bytes(), b"old")

	def test_missing_report_is_deterministic(self) -> None:
		value = {"schema_version": 1, "records": [{"module": "udwm", "sha256": "1" * 64}]}
		path = self.root / "missing.json"
		maintain.write_json(path, value)
		first = path.read_bytes()
		maintain.write_json(path, value)
		self.assertEqual(first, path.read_bytes())

	def test_collision_report_is_deterministic_and_inventory_bound(self) -> None:
		inventory_root = self.root / "inventory"
		record = {
			"version": {"build": 100, "revision": 7},
			"sha256": "1" * 64,
			"time_date_stamp": 0x123,
			"size_of_image": 0x1000,
		}
		for module in ("udwm", "dwmcore"):
			maintain.write_json(inventory_root / f"{module}.json", {
				"schema_version": 1,
				"records": [record] if module == "udwm" else [],
			})
		collisions = [{
			"module": "udwm",
			"version": {"build": 100, "revision": 7},
			"expected_sha256": "1" * 64,
			"returned_sha256": "2" * 64,
			"symbol_server_key": "000001231000",
		}]
		maintain.write_collision_report(self.root, collisions)
		inventory = maintain.read_frozen_inventory(self.root)
		self.assertEqual(maintain.validate_collision_report(self.root, inventory), 1)
		self.assertEqual(len(maintain.load_collision_report(self.root, inventory, drop_stale=True)), 1)
		self.assertEqual(len(maintain.load_collision_report(self.root, {}, drop_stale=True)), 0)

	def test_exclusion_report_is_inventory_bound(self) -> None:
		record = {
			"version": {"build": 100, "revision": 7},
			"sha256": "1" * 64,
			"time_date_stamp": 0x123,
			"size_of_image": 0x1000,
		}
		for module in ("udwm", "dwmcore"):
			maintain.write_json(self.root / "inventory" / f"{module}.json", {
				"schema_version": 1,
				"records": [record] if module == "udwm" else [],
			})
		exclusions = [{
			"module": "udwm",
			"version": {"build": 100, "revision": 7},
			"sha256": "1" * 64,
			"kind": "symbol_resolution_rejected",
			"detail": "Required symbol did not resolve",
		}]
		maintain.write_exclusion_report(self.root, exclusions)
		inventory = maintain.read_frozen_inventory(self.root)
		self.assertEqual(len(maintain.load_exclusion_report(self.root, inventory)), 1)
		self.assertEqual(len(maintain.load_exclusion_report(self.root, {}, drop_stale=True)), 0)
		self.assertEqual(
			maintain.classify_exclusion(maintain.UnresolvableCandidate("failure")),
			"symbol_resolution_rejected",
		)

	def test_write_indexes_keeps_inventory_out_of_build_sources(self) -> None:
		resolver = {"version": "10.0.1.2", "sha256": "a" * 64}
		maintain.write_indexes(self.root, resolver)
		index = json.loads((self.root / "legacy" / "index.json").read_text(encoding="utf-8"))
		self.assertEqual(set(index), {"schema_version", "architecture", "resolver", "sources"})
		self.assertEqual(index["resolver"], {"dbghelp": resolver})
		shard = json.loads((self.root / "legacy" / "udwm" / "17763.json").read_text(encoding="utf-8"))
		self.assertEqual(shard, {"schema_version": 1, "records": []})

	def test_write_indexes_requires_explicit_full_resolver_replacement(self) -> None:
		maintain.write_indexes(self.root, {"version": "10.0.1.2", "sha256": "a" * 64})
		shard_path = self.root / "legacy" / "udwm" / "17763.json"
		maintain.write_json(shard_path, {"schema_version": 1, "records": [{"old": "record"}]})
		with self.assertRaisesRegex(projection_schema.SchemaError, "recollect the complete architecture"):
			maintain.write_indexes(self.root, {"version": "10.0.1.3", "sha256": "b" * 64})
		maintain.write_indexes(
			self.root,
			{"version": "10.0.1.3", "sha256": "b" * 64},
			replace_resolver=True,
		)
		self.assertEqual(
			json.loads(shard_path.read_text(encoding="utf-8")),
			{"schema_version": 1, "records": []},
		)

	def test_normalizer_rejects_nonproduction_and_unpaired(self) -> None:
		with self.assertRaisesRegex(projection_schema.SchemaError, "production_candidate"):
			maintain.normalize_audit({"schema_version": 1, "architecture": "legacy", "module": "udwm"}, "legacy", {"udwm": {}})
		report = {"schema_version": 1, "architecture": "legacy", "module": "udwm", "evidence": "production_candidate", "module_supported": True, "configuration": "release", "requested_version": {"build": 100, "revision": 7}, "image_version": {"build": 100, "revision": 7}, "image": {}, "pdb": {"paired": False}, "dbghelp": {}}
		with self.assertRaisesRegex(projection_schema.SchemaError, "PDB must be paired"):
			maintain.normalize_audit(report, "legacy", {"udwm": {}})

	def test_normalizer_classifies_unresolved_required_symbols(self) -> None:
		report = {
			"schema_version": 1,
			"architecture": "legacy",
			"module": "udwm",
			"evidence": "production_candidate",
			"module_supported": False,
		}
		with self.assertRaisesRegex(maintain.UnresolvableCandidate, "Required symbols"):
			maintain.normalize_audit(report, "legacy", {"udwm": {}})



if __name__ == "__main__":
	unittest.main()
