from __future__ import annotations

import argparse
import hashlib
import io
import json
import struct
import tempfile
import unittest
import uuid
from pathlib import Path
from unittest import mock

from tools import audit_symbol_resolution
from tools import audit_winbindex_revisions as revisions
from tools import maintain_symbol_catalog as maintain
from tools import projection_schema, projection_target
from tools import symbol_catalog
from tests.test_symbol_catalog import make_schema
from tests import test_audit_symbol_resolution as fixtures


class MaintenanceTests(unittest.TestCase):
	def setUp(self) -> None:
		self.temporary = tempfile.TemporaryDirectory()
		self.root = Path(self.temporary.name)

	def tearDown(self) -> None:
		self.temporary.cleanup()

	def sample_entry(self, digest: str, revision: int = 7) -> dict:
		return {
			"fileInfo": {
				"version": f"10.0.100.{revision}", "machineType": projection_target.MACHINES["x64"],
				"sha256": digest, "timestamp": 123, "virtualSize": 0x1000, "size": 4,
			},
			"windowsVersions": {"Windows": {"assembly": ["KB1"]}},
		}

	def arguments(self) -> argparse.Namespace:
		return argparse.Namespace(build=100, min_revision=None, max_revision=None)

	def schemas(self) -> dict:
		return {
			architecture: {module: make_schema(module) for module in maintain.MODULES}
			for architecture in projection_schema.ARCHITECTURES
		}

	def write_indexes(self, root: Path, resolver: dict, schemas: dict, *, replace_resolver: bool = False) -> None:
		for path, source in maintain.prepare_indexes(root, resolver, schemas, replace_resolver=replace_resolver).items():
			maintain.write_json(path, source)

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

	def test_candidate_discovery_selects_cpu_independently_of_backend(self) -> None:
		entries = {}
		for index, (platform, machine) in enumerate(projection_target.MACHINES.items(), 1):
			digest = str(index) * 64
			entry = self.sample_entry(digest)
			entry["fileInfo"]["machineType"] = machine
			entries[digest] = entry
		for platform in projection_target.MACHINES:
			with self.subTest(platform=platform), mock.patch.object(revisions, "load_index", return_value=entries) as load:
				candidates = maintain.discover_candidates(self.schemas(), self.root, platform)
				self.assertEqual(len(candidates[("udwm", 100)]), 1)
				self.assertEqual(candidates[("udwm", 100)][0].platform, platform)
				self.assertEqual(load.call_args_list, [mock.call("uDWM.dll", self.root, platform), mock.call("dwmcore.dll", self.root, platform)])

	def test_collection_selects_specialized_or_common_catalog_per_backend(self) -> None:
		schemas, resolver, x64_root, dbghelp = self.prepare_collection()
		before = self.catalog_bytes(x64_root)
		for architecture in schemas:
			self.assertEqual(projection_target.catalog_root(self.root, architecture, "x86"), x64_root)
		(x64_root / "x86/legacy").mkdir(parents=True)
		with (
			mock.patch.object(maintain, "load_schemas", side_effect=lambda _repo, arch, _platform: schemas[arch]) as load,
			mock.patch.object(maintain, "dbghelp_identity", return_value=resolver),
			mock.patch.object(maintain, "discover_candidates", return_value={(module, 100): [] for module in maintain.MODULES}) as discover,
		):
			self.assertEqual(maintain.collect(self.root, self.root / "cache", dbghelp, platform="x86"), 0)
			self.assertEqual(load.call_args_list, [mock.call(self.root.resolve(), arch, "x86") for arch in schemas])
			discover.assert_called_once_with(schemas, self.root / "cache", "x86")
			self.assertEqual(maintain.verify_sources(self.root, schemas, "x86"), {"legacy": 0, "milcomp": 2})
		after = {path: value for path, value in self.catalog_bytes(x64_root).items() if path.parts[0] != "x86"}
		self.assertEqual(before, after)

	def test_symbol_server_collision_is_never_cached(self) -> None:
		sample = revisions.Sample(audit_symbol_resolution.Version(100, 7), "1" * 64, 123, 0x1000, 4)
		with mock.patch.object(revisions, "fetch", return_value=b"evil"):
			with self.assertRaises(revisions.SymbolServerIdentityCollision) as raised:
				revisions.ensure_image(self.root / "cache", "uDWM.dll", sample, {})
		self.assertEqual(raised.exception.sample, sample)
		self.assertEqual(raised.exception.received_sha256, hashlib.sha256(b"evil").hexdigest())
		self.assertFalse(any((self.root / "cache").rglob("uDWM.dll")))

	def test_binary_audit_rejects_invalid_image_candidates_before_publication(self) -> None:
		image = self.root / "uDWM.dll"
		fixtures.SymbolResolutionAuditTests._write_pe(image, uuid.UUID(int=1), 1)
		original = image.read_bytes()
		version = audit_symbol_resolution.Version(100, 7)
		for failure in ("module", "size", "hash", "machine", "version"):
			with self.subTest(failure=failure):
				image.write_bytes(original)
				candidate = image
				contents = bytearray(original)
				if failure == "module":
					candidate = self.root / "dwmcore.dll"
					candidate.write_bytes(original)
				elif failure == "machine":
					# Keep the expected hash paired to these bytes, so machine validation runs.
					struct.pack_into("<H", contents, struct.unpack_from("<I", contents, 0x3C)[0] + 4, projection_target.MACHINES["x86"])
					image.write_bytes(contents)
				sample = revisions.Sample(version, hashlib.sha256(contents).hexdigest(), 123, 0x2000, len(contents))
				if failure == "size":
					image.write_bytes(contents + b"extra")
				elif failure == "hash":
					contents[-1] ^= 1
					image.write_bytes(contents)
				actual_version = audit_symbol_resolution.Version(100, 8) if failure == "version" else version
				with mock.patch.object(audit_symbol_resolution, "image_version", return_value=actual_version):
					with self.assertRaises(revisions.AuditError):
						revisions._validate_image(candidate, "uDWM.dll", sample)
					cache = self.root / "cache" / failure
					with mock.patch.object(revisions, "fetch", side_effect=revisions.AuditError("unavailable")):
						with self.assertRaises(revisions.AuditError):
							revisions.ensure_image(cache, "uDWM.dll", sample, {sample.sha256: [candidate]})
					self.assertFalse(any(cache.rglob("uDWM.dll")))

	def test_collection_rejects_invalid_shard_envelopes_without_writing(self) -> None:
		schemas, resolver, root, dbghelp = self.prepare_collection()
		path = maintain.source_path(root, "legacy", "udwm", 100)
		original = json.loads(path.read_text(encoding="utf-8"))
		for invalid in ({**original, "schema_version": 999}, {**original, "unknown": True}, {**original, "records": {}}):
			with self.subTest(invalid=invalid):
				path.write_text(json.dumps(invalid), encoding="utf-8")
				before = self.catalog_bytes(root)
				with mock.patch.object(maintain, "load_schemas", side_effect=lambda _repo, arch, _platform: schemas[arch]), mock.patch.object(maintain, "dbghelp_identity", return_value=resolver), mock.patch.object(maintain, "discover_candidates") as acquire:
					with self.assertRaises(projection_schema.SchemaError):
						maintain.collect(self.root, self.root / "cache", dbghelp)
				acquire.assert_not_called()
				self.assertEqual(self.catalog_bytes(root), before)

	def test_prime_symbol_cache_rejects_wrong_pdb_guid_age(self) -> None:
		identity = self.identity(0x1A)
		source = self.root / identity.name
		for guid, age in [(uuid.UUID(int=1), identity.age), (identity.guid, identity.age + 1)]:
			with self.subTest(guid=guid, age=age):
				fixtures.SymbolResolutionAuditTests._write_pdb(source, guid, age)
				contents = source.read_bytes()
				def expand(arguments, **_kwargs):
					Path(arguments[2]).write_bytes(contents)
					return mock.Mock(returncode=0)
				with (
					mock.patch.object(revisions, "fetch", return_value=contents),
					mock.patch.object(revisions.subprocess, "run", side_effect=expand),
				):
					with self.assertRaisesRegex(revisions.AuditError, "wrong GUID/DBI age"):
						revisions.prime_symbol_cache(identity, self.root / "cache")
				self.assertFalse(any(path.is_file() for path in (self.root / "cache").rglob(identity.name)))

	def test_malformed_direct_pdb_uses_paired_compressed_pdb_then_reuses_cache(self) -> None:
		identity = self.identity(0x1A)
		source = self.root / identity.name
		fixtures.SymbolResolutionAuditTests._write_pdb(source, identity.guid, identity.age)
		contents = source.read_bytes()
		def expand(arguments, **_kwargs):
			Path(arguments[2]).write_bytes(contents)
			return mock.Mock(returncode=0)
		with (
			mock.patch.object(revisions, "fetch", side_effect=[b"invalid PDB", b"compressed PDB"]) as fetch,
			mock.patch.object(revisions.subprocess, "run", side_effect=expand),
		):
			pdb = revisions.prime_symbol_cache(identity, self.root / "cache")
			self.assertEqual(pdb.read_bytes(), contents)
			self.assertEqual(fetch.call_count, 2)
			self.assertTrue(fetch.call_args_list[1].args[0].endswith("/udwm.pd_"))
			self.assertEqual(revisions.prime_symbol_cache(identity, self.root / "cache"), pdb)
			self.assertEqual(fetch.call_count, 2)

	def record(self, module: str, revision: int = 7) -> dict:
		return {
			"revision": revision,
			"pdb": {
				"name": f"{module}.pdb", "guid": "12345678-1234-5678-90ab-cdef01234567",
				"age": revision, "sha256": "b" * 64,
			},
			"symbols": {"Required.Id": {"name": "public: void __cdecl Required(void)", "rva": "0x1000"}},
		}

	def prepare_collection(self) -> tuple[dict, dict, Path, Path]:
		schemas = self.schemas()
		resolver = {"version": "10.0.1.2", "sha256": "a" * 64}
		root = self.root / "OpenGlass" / "SymbolCatalogs"
		self.write_indexes(root, resolver, schemas)
		for architecture in schemas:
			for index, module in enumerate(maintain.MODULES, 1):
				maintain.write_json(maintain.source_path(root, architecture, module, 100), {
					"schema_version": symbol_catalog.SCHEMA_VERSION,
					"records": [self.record(module)],
				})
		dbghelp = self.root / "dbghelp.dll"
		dbghelp.write_bytes(b"resolver")
		(self.root / "uDWM.pdb").write_bytes(b"pdb")
		return schemas, resolver, root, dbghelp

	def catalog_bytes(self, root: Path) -> dict:
		return {path.relative_to(root): path.read_bytes() for path in root.rglob("*.json")}

	def test_collection_rejects_invalid_existing_records_without_writing_catalogs(self) -> None:
		schemas, resolver, root, dbghelp = self.prepare_collection()
		path = maintain.source_path(root, "milcomp", "dwmcore", 100)
		stale = self.record("dwmcore", 8)
		stale["symbols"]["Required.Id"]["name"] = "old binding name"
		path.write_text(json.dumps({
			"schema_version": symbol_catalog.SCHEMA_VERSION,
			"records": [self.record("dwmcore"), stale],
		}), encoding="utf-8")
		# Preparing an earlier architecture must not publish its new index/shard.
		schemas["legacy"]["udwm"]["known_builds"] = [99, 100]
		before = self.catalog_bytes(root)
		for flags in ([], ["--replace-resolver"]):
			stderr = io.StringIO()
			with (
				self.subTest(flags=flags),
				mock.patch.object(maintain, "load_schemas", side_effect=lambda _repo, arch, _platform: schemas[arch]),
				mock.patch.object(maintain, "dbghelp_identity", return_value=resolver),
				mock.patch.object(maintain, "discover_candidates", return_value={
					("udwm", 99): [], ("udwm", 100): [], ("dwmcore", 100): [],
				}) as discover,
				mock.patch.object(maintain.sys, "stderr", stderr),
			):
				result = maintain.main([
					"collect", str(self.root), "--cache", str(self.root / "cache"), "--dbghelp", str(dbghelp), *flags,
				])
				self.assertEqual(result, 1)
				self.assertIn(f"{path}.records[1].symbols.Required.Id.name is not in the selected binding", stderr.getvalue())
				self.assertEqual(before, self.catalog_bytes(root))
				discover.assert_not_called()

	def test_collection_rejects_duplicate_existing_keys_without_writing_catalogs(self) -> None:
		schemas, resolver, root, dbghelp = self.prepare_collection()
		schemas["milcomp"]["udwm"]["known_builds"] = [99, 100]
		path = maintain.source_path(root, "milcomp", "udwm", 99)
		maintain.write_json(path, {
			"schema_version": symbol_catalog.SCHEMA_VERSION,
			"records": [self.record("udwm")],
		})
		before = self.catalog_bytes(root)
		with (
			mock.patch.object(maintain, "load_schemas", side_effect=lambda _repo, arch, _platform: schemas[arch]),
			mock.patch.object(maintain, "dbghelp_identity", return_value=resolver),
			mock.patch.object(maintain, "discover_candidates", return_value={
				("udwm", 99): [], ("udwm", 100): [], ("dwmcore", 100): [],
			}) as discover,
		):
			with self.assertRaisesRegex(projection_schema.SchemaError, "duplicate module/PDB GUID/age"):
				maintain.collect(self.root, self.root / "cache", dbghelp)
		self.assertEqual(before, self.catalog_bytes(root))
		discover.assert_not_called()

	def test_collection_reports_schema_errors_only_for_new_candidates(self) -> None:
		schemas, resolver, root, dbghelp = self.prepare_collection()
		before = self.catalog_bytes(root)
		cache = self.root / "cache"
		samples = revisions.discover_samples({"3" * 64: self.sample_entry("3" * 64, 8)}, self.arguments())
		with (
			mock.patch.object(maintain, "load_schemas", side_effect=lambda _repo, arch, _platform: schemas[arch]),
			mock.patch.object(maintain, "dbghelp_identity", return_value=resolver),
			mock.patch.object(maintain, "discover_candidates", return_value={
				("udwm", 100): samples, ("dwmcore", 100): [],
			}),
			mock.patch.object(revisions, "sample_codeview", return_value=self.identity(8)),
			mock.patch.object(revisions, "prime_symbol_cache", return_value=self.root / "uDWM.pdb"),
			mock.patch.object(maintain, "record_from_pdb", side_effect=projection_schema.SchemaError("Required symbols unresolved")),
		):
			self.assertEqual(maintain.collect(self.root, cache, dbghelp), 0)
		self.assertEqual(before, self.catalog_bytes(root))
		failures = json.loads((cache / "collection-failures.json").read_text(encoding="utf-8"))["records"]
		self.assertEqual(len(failures), 2)
		self.assertTrue(all(row["version"] == {"build": 100, "revision": 8} for row in failures))
		self.assertTrue(all(row["error"] == "Required symbols unresolved" for row in failures))

	def test_collection_explicitly_replaces_resolver_and_discards_old_records(self) -> None:
		schemas, _resolver, root, dbghelp = self.prepare_collection()
		path = maintain.source_path(root, "milcomp", "dwmcore", 100)
		maintain.write_json(path, {"schema_version": symbol_catalog.SCHEMA_VERSION, "records": [{"old": "record"}]})
		resolver = {"version": "10.0.1.3", "sha256": "c" * 64}
		with (
			mock.patch.object(maintain, "load_schemas", side_effect=lambda _repo, arch, _platform: schemas[arch]),
			mock.patch.object(maintain, "dbghelp_identity", return_value=resolver),
			mock.patch.object(maintain, "discover_candidates", return_value={(module, 100): [] for module in maintain.MODULES}),
		):
			self.assertEqual(maintain.collect(self.root, self.root / "cache", dbghelp, replace_resolver=True), 0)
		self.assertEqual(maintain.verify_sources(self.root, schemas), {"legacy": 0, "milcomp": 0})
		for architecture in schemas:
			maintain.require_catalog_resolver(root, architecture, resolver)

	def test_collection_failures_are_local_and_preserve_existing_records(self) -> None:
		schemas, resolver, root, dbghelp = self.prepare_collection()
		before = self.catalog_bytes(root)
		cache = self.root / "cache"
		samples = revisions.discover_samples({
			"1" * 64: self.sample_entry("1" * 64),
			"3" * 64: self.sample_entry("3" * 64, 8),
		}, self.arguments())
		with (
			mock.patch.object(maintain, "load_schemas", side_effect=lambda _repo, arch, _platform: schemas[arch]),
			mock.patch.object(maintain, "dbghelp_identity", return_value=resolver),
			mock.patch.object(maintain, "discover_candidates", return_value={
				("udwm", 100): samples, ("dwmcore", 100): [],
			}),
			mock.patch.object(revisions, "sample_codeview", side_effect=lambda _cache, _module, sample: self.identity(sample.version.revision) if sample.version.revision == 7 else self.unavailable()) as acquire,
		):
			self.assertEqual(maintain.collect(self.root, cache, dbghelp), 0)
		self.assertEqual(acquire.call_count, 4)
		self.assertEqual(before, self.catalog_bytes(root))
		failures = json.loads((cache / "collection-failures.json").read_text(encoding="utf-8"))
		self.assertEqual(len(failures["records"]), 2)
		self.assertTrue(all(row["sha256"] == "3" * 64 for row in failures["records"]))
		self.assertFalse((root / "inventory").exists())

	def test_collection_adds_verified_records_and_skips_them_on_repeat(self) -> None:
		schemas, resolver, root, dbghelp = self.prepare_collection()
		cache = self.root / "cache"
		samples = revisions.discover_samples({"3" * 64: self.sample_entry("3" * 64, 8)}, self.arguments())
		with (
			mock.patch.object(maintain, "load_schemas", side_effect=lambda _repo, arch, _platform: schemas[arch]),
			mock.patch.object(maintain, "dbghelp_identity", return_value=resolver),
			mock.patch.object(maintain, "discover_candidates", return_value={
				("udwm", 100): samples, ("dwmcore", 100): [],
			}),
			mock.patch.object(revisions, "sample_codeview", return_value=self.identity(8)) as acquire,
			mock.patch.object(revisions, "ensure_image", side_effect=AssertionError("collection must not fetch a PE")),
			mock.patch.object(revisions, "prime_symbol_cache", return_value=self.root / "uDWM.pdb"),
			mock.patch.object(audit_symbol_resolution, "enumerate_complete_names", return_value=({"public: void __cdecl Required(void)": {0x1000}}, 0)) as inspect,
		):
			maintain.collect(self.root, cache, dbghelp)
			first = self.catalog_bytes(root)
			self.assertEqual(maintain.verify_sources(self.root), {"legacy": 3, "milcomp": 3})
			maintain.collect(self.root, cache, dbghelp)
			self.assertEqual(acquire.call_count, 4)
			self.assertEqual(inspect.call_count, 2)
			self.assertEqual(first, self.catalog_bytes(root))


	def test_collection_reuses_identity_from_another_build_shard(self) -> None:
		schemas, resolver, root, dbghelp = self.prepare_collection()
		for modules in schemas.values():
			modules["udwm"]["known_builds"] = [99, 100]
		self.write_indexes(root, resolver, schemas)
		before = self.catalog_bytes(root)
		sample = revisions.Sample(projection_schema.Version(99, 7), "3" * 64, 123, 0x1000, 4)
		with (
			mock.patch.object(maintain, "load_schemas", side_effect=lambda _repo, arch, _platform: schemas[arch]),
			mock.patch.object(maintain, "dbghelp_identity", return_value=resolver),
			mock.patch.object(maintain, "discover_candidates", return_value={
				("udwm", 99): [sample], ("udwm", 100): [], ("dwmcore", 100): [],
			}),
			mock.patch.object(revisions, "sample_codeview", return_value=self.identity(7)),
			mock.patch.object(revisions, "prime_symbol_cache", return_value=self.root / "uDWM.pdb"),
			mock.patch.object(maintain, "record_from_pdb", side_effect=AssertionError("known module/GUID/age must skip resolution")),
		):
			self.assertEqual(maintain.collect(self.root, self.root / "cache", dbghelp), 0)
		self.assertEqual(before, self.catalog_bytes(root))
		self.assertEqual(maintain.verify_sources(self.root, schemas), {"legacy": 2, "milcomp": 2})

	def test_index_preparation_requires_explicit_full_resolver_replacement(self) -> None:
		self.write_indexes(self.root, {"version": "10.0.1.2", "sha256": "a" * 64}, self.schemas())
		shard_path = self.root / "legacy" / "udwm" / "100.json"
		maintain.write_json(shard_path, {"schema_version": symbol_catalog.SCHEMA_VERSION, "records": [{"old": "record"}]})
		with self.assertRaisesRegex(projection_schema.SchemaError, "recollect the complete architecture"):
			self.write_indexes(self.root, {"version": "10.0.1.3", "sha256": "b" * 64}, self.schemas())
		self.write_indexes(
			self.root,
			{"version": "10.0.1.3", "sha256": "b" * 64},
			self.schemas(),
			replace_resolver=True,
		)
		self.assertEqual(
			json.loads(shard_path.read_text(encoding="utf-8")),
			{"schema_version": symbol_catalog.SCHEMA_VERSION, "records": []},
		)

	def identity(self, age: int) -> audit_symbol_resolution.PdbIdentity:
		return audit_symbol_resolution.PdbIdentity("udwm.pdb", uuid.UUID("12345678-1234-5678-90ab-cdef01234567"), age)

	def unavailable(self):
		raise revisions.AuditError("unavailable")

	def test_catalog_admission_requires_unique_required_symbols_without_pe(self) -> None:
		pdb = self.root / "udwm.pdb"
		pdb.write_bytes(b"pdb")
		with mock.patch.object(audit_symbol_resolution, "enumerate_complete_names", return_value=({}, 0)) as resolve:
			with self.assertRaisesRegex(projection_schema.SchemaError, "Required symbols"):
				maintain.record_from_pdb(pdb, self.identity(7), projection_schema.Version(100, 7), make_schema("udwm"), self.root / "dbghelp.dll")
			resolve.return_value = ({"public: void __cdecl Required(void)": {0x1000}}, 0)
			result = maintain.record_from_pdb(pdb, self.identity(7), projection_schema.Version(100, 7), make_schema("udwm"), self.root / "dbghelp.dll")
		self.assertEqual(result["pdb"]["sha256"], hashlib.sha256(b"pdb").hexdigest())
		self.assertNotIn("image", result)
		self.assertEqual(result["symbols"]["Required.Id"]["rva"], "0x1000")

if __name__ == "__main__":
	unittest.main()
