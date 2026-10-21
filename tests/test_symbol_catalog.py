from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from tools import projection_schema
from tools import symbol_catalog


def make_schema(module: str) -> dict:
	return {
		"schema_version": projection_schema.SCHEMA_VERSION,
		"module": module,
		"namespace": f"OpenGlass::{module}",
		"tag": "ModuleTag",
		"known_builds": [100],
		"symbols": [
			{
				"name": "Symbol_Required",
				"id": "Required.Id",
				"bindings": [{
					"symbol_names": ["public: void __cdecl Required(void)"],
					"min_inclusive": None,
					"max_exclusive": None,
				}],
				"kind": "projected_function",
				"target": "&Required",
				"requirement": "required",
			},
			{
				"name": "Symbol_Optional",
				"id": "Optional.Id",
				"bindings": [{
					"symbol_names": ["int Optional"],
					"min_inclusive": None,
					"max_exclusive": None,
				}],
				"kind": "projected_variable",
				"target": "&Optional",
				"requirement": "optional",
			},
		],
		"layouts": [],
	}


class SymbolCatalogTests(unittest.TestCase):
	def setUp(self) -> None:
		self.temporary = tempfile.TemporaryDirectory()
		self.root = Path(self.temporary.name)
		self.schemas = [make_schema("udwm"), make_schema("dwmcore")]
		self.ages = {"udwm": [7], "dwmcore": [7]}
		self.write_fixture()

	def tearDown(self) -> None:
		self.temporary.cleanup()

	def write_json(self, path: Path, value: dict) -> None:
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_text(json.dumps(value), encoding="utf-8")

	def evidence_record(self, module: str, age: int, revision: int = 7) -> dict:
		return {
			"revision": revision,
			"pdb": {
				"name": f"{module}.pdb",
				"guid": "12345678-1234-5678-90ab-cdef01234567",
				"age": age,
				"sha256": "b" * 64,
			},
			"symbols": {
				"Required.Id": {
					"name": "public: void __cdecl Required(void)",
					"rva": "0x1000",
				},
			},
		}

	def write_fixture(self) -> None:
		self.write_json(self.root / "legacy" / "index.json", {
			"schema_version": symbol_catalog.SCHEMA_VERSION,
			"architecture": "legacy",
			"resolver": {"dbghelp": {"version": "10.0.1.2", "sha256": "c" * 64}},
			"sources": ["udwm/100.json", "dwmcore/100.json"],
		})
		for module in ("udwm", "dwmcore"):
			self.write_json(self.root / "legacy" / module / "100.json", {
				"schema_version": symbol_catalog.SCHEMA_VERSION,
				"records": [
					self.evidence_record(module, age, 7 + index)
					for index, age in enumerate(self.ages[module])
				],
			})

	def load(self) -> dict:
		return symbol_catalog.load_catalog_sources(self.root / "legacy" / "index.json", "legacy", self.schemas)

	def source(self, module: str = "udwm") -> tuple[Path, dict]:
		path = self.root / "legacy" / module / "100.json"
		return path, json.loads(path.read_text(encoding="utf-8"))

	def test_validates_and_emits_deterministically(self) -> None:
		catalog = self.load()
		self.assertEqual(len(catalog["records"]), 2)  # Missing Optional symbols are valid.
		first = symbol_catalog.generate_files(catalog)
		second = symbol_catalog.generate_files(catalog)
		self.assertEqual(first, second)
		self.assertIn("\t\t0x1000u,", first["SymbolCatalog.generated.inc"])

	def test_rejects_changed_complete_name_without_fingerprint(self) -> None:
		self.schemas[0]["symbols"][0]["bindings"][0]["symbol_names"] = ["New ABI"]
		with self.assertRaisesRegex(projection_schema.SchemaError, "not in the selected binding"):
			self.load()

	def test_rejects_invalid_resolver_identity(self) -> None:
		path = self.root / "legacy/index.json"
		original = json.loads(path.read_text(encoding="utf-8"))
		for resolver in ({}, {"dbghelp": {"version": "1.2", "sha256": "bad"}}, {"dbghelp": {"version": [], "sha256": "a" * 64}}):
			with self.subTest(resolver=resolver):
				self.write_json(path, {**original, "resolver": resolver})
				with self.assertRaises(projection_schema.SchemaError):
					self.load()

	def test_added_alternative_name_keeps_verified_records_valid(self) -> None:
		before = symbol_catalog.generate_files(self.load())
		self.schemas[0]["symbols"][0]["bindings"][0]["symbol_names"].append("Alternate complete name")
		self.assertEqual(before, symbol_catalog.generate_files(self.load()))

	def test_rejects_missing_required_symbol(self) -> None:
		path, source = self.source()
		del source["records"][0]["symbols"]["Required.Id"]
		self.write_json(path, source)
		with self.assertRaisesRegex(projection_schema.SchemaError, "omits Required IDs"):
			self.load()

	def test_rejects_unknown_symbol(self) -> None:
		path, source = self.source()
		source["records"][0]["symbols"]["Unknown.Id"] = {"name": "Unknown", "rva": "0x2000"}
		self.write_json(path, source)
		with self.assertRaisesRegex(projection_schema.SchemaError, "not active"):
			self.load()

	def test_rejects_missing_index_source(self) -> None:
		path = self.root / "legacy" / "index.json"
		index = json.loads(path.read_text(encoding="utf-8"))
		index["sources"].pop()
		self.write_json(path, index)
		with self.assertRaisesRegex(projection_schema.SchemaError, "missing explicitly indexed sources"):
			self.load()

	def test_same_version_distinct_pdb_keys_emit_independent_records(self) -> None:
		self.ages["udwm"] = [7, 8]
		self.write_fixture()
		source_path, source = self.source()
		source["records"][1]["revision"] = source["records"][0]["revision"]
		self.write_json(source_path, source)
		generated = symbol_catalog.generate_files(self.load())["SymbolCatalog.generated.inc"]
		self.assertEqual(generated.count("Projection::ModuleId::uDWM"), 2)

	def test_rejects_duplicate_key_across_build_shards(self) -> None:
		self.schemas[0]["known_builds"].append(101)
		path, source = self.source()
		source["records"][0]["pdb"].update({"name": "other.pdb", "sha256": "e" * 64})
		self.write_json(path.parent / "101.json", source)
		index_path = self.root / "legacy" / "index.json"
		index = json.loads(index_path.read_text(encoding="utf-8"))
		index["sources"].append("udwm/101.json")
		self.write_json(index_path, index)
		with self.assertRaisesRegex(projection_schema.SchemaError, "duplicate module/PDB GUID/age across catalog sources"):
			self.load()


if __name__ == "__main__":
	unittest.main()
