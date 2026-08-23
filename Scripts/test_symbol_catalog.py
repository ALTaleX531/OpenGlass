from __future__ import annotations

import copy
import json
import tempfile
import unittest
from pathlib import Path

import projection_schema
import symbol_catalog


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
		self.hashes = {"udwm": ["1" * 64], "dwmcore": ["2" * 64]}
		self.write_fixture()

	def tearDown(self) -> None:
		self.temporary.cleanup()

	def write_json(self, path: Path, value: dict) -> None:
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_text(json.dumps(value), encoding="utf-8")

	def evidence_record(self, module: str, digest: str, revision: int = 7) -> dict:
		schema = next(schema for schema in self.schemas if schema["module"] == module)
		return {
			"revision": revision,
			"image": {
				"time_date_stamp": 123 + revision,
				"size_of_image": 0x10000,
				"sha256": digest,
			},
			"pdb": {
				"name": f"{module}.pdb",
				"guid": "12345678-1234-5678-90ab-cdef01234567",
				"age": revision,
				"sha256": "b" * 64,
			},
			"resolution_contract": symbol_catalog.resolution_contract(
				schema, projection_schema.Version(100, revision), "release"
			),
			"symbols": {
				"Required.Id": {
					"name": "public: void __cdecl Required(void)",
					"rva": "0x1000",
				},
			},
		}

	def write_fixture(self) -> None:
		self.write_json(self.root / "legacy" / "index.json", {
			"schema_version": 1,
			"architecture": "legacy",
			"resolver": {"dbghelp": {"version": "10.0.1.2", "sha256": "c" * 64}},
			"sources": ["udwm/100.json", "dwmcore/100.json"],
		})
		for module in ("udwm", "dwmcore"):
			self.write_json(self.root / "legacy" / module / "100.json", {
				"schema_version": 1,
				"records": [
					self.evidence_record(module, digest, 7 + index)
					for index, digest in enumerate(self.hashes[module])
				],
			})

	def load(self) -> dict:
		return symbol_catalog.load_catalog_sources(self.root / "legacy" / "index.json", "legacy", self.schemas)

	def source(self, module: str = "udwm") -> tuple[Path, dict]:
		path = self.root / "legacy" / module / "100.json"
		return path, json.loads(path.read_text(encoding="utf-8"))

	def test_validates_and_emits_deterministically(self) -> None:
		catalog = self.load()
		first = symbol_catalog.generate_files(catalog)
		second = symbol_catalog.generate_files(catalog)
		self.assertEqual(first, second)
		self.assertNotIn("CatalogAddressClass", first["SymbolCatalog.generated.inc"])
		self.assertIn("\t\t0x1000u,", first["SymbolCatalog.generated.inc"])

	def test_omits_schema_derived_address_class(self) -> None:
		path, source = self.source()
		source["records"][0]["symbols"]["Optional.Id"] = {"name": "int Optional", "rva": "0x2000"}
		self.write_json(path, source)
		generated = symbol_catalog.generate_files(self.load())
		self.assertNotIn("CatalogAddressClass", generated["SymbolCatalog.generated.inc"])

	def test_rejects_stale_resolution_contract(self) -> None:
		path, source = self.source()
		source["records"][0]["resolution_contract"] = "0" * 64
		self.write_json(path, source)
		with self.assertRaisesRegex(projection_schema.SchemaError, "resolution_contract is stale"):
			self.load()

	def test_rejects_invalid_resolver_identity(self) -> None:
		path = self.root / "legacy" / "index.json"
		index = json.loads(path.read_text(encoding="utf-8"))
		index["resolver"]["dbghelp"]["sha256"] = "invalid"
		self.write_json(path, index)
		with self.assertRaisesRegex(projection_schema.SchemaError, "64 hexadecimal"):
			self.load()

	def test_rejects_missing_required_symbol(self) -> None:
		path, source = self.source()
		del source["records"][0]["symbols"]["Required.Id"]
		self.write_json(path, source)
		with self.assertRaisesRegex(projection_schema.SchemaError, "omits Required IDs"):
			self.load()

	def test_allows_missing_optional_symbol(self) -> None:
		self.assertEqual(len(self.load()["records"]), 2)

	def test_rejects_unknown_symbol(self) -> None:
		path, source = self.source()
		source["records"][0]["symbols"]["Unknown.Id"] = {"name": "Unknown", "rva": "0x2000"}
		self.write_json(path, source)
		with self.assertRaisesRegex(projection_schema.SchemaError, "not active"):
			self.load()

	def test_rejects_redundant_record_context(self) -> None:
		path, source = self.source()
		source["records"][0]["module"] = "udwm"
		self.write_json(path, source)
		with self.assertRaisesRegex(projection_schema.SchemaError, "unknown properties: module"):
			self.load()

	def test_rejects_missing_index_source(self) -> None:
		path = self.root / "legacy" / "index.json"
		index = json.loads(path.read_text(encoding="utf-8"))
		index["sources"].pop()
		self.write_json(path, index)
		with self.assertRaisesRegex(projection_schema.SchemaError, "missing explicitly indexed sources"):
			self.load()

	def test_rejects_invalid_source_context(self) -> None:
		path = self.root / "legacy" / "index.json"
		index = json.loads(path.read_text(encoding="utf-8"))
		index["sources"][0] = "../udwm/100.json"
		self.write_json(path, index)
		with self.assertRaisesRegex(projection_schema.SchemaError, "must be module/BUILD.json"):
			self.load()

	def test_same_version_distinct_hashes_emit_independent_records(self) -> None:
		self.hashes["udwm"] = ["1" * 64, "4" * 64]
		self.write_fixture()
		source_path, source = self.source()
		source["records"][1]["revision"] = source["records"][0]["revision"]
		source["records"][1]["pdb"]["age"] += 1
		self.write_json(source_path, source)
		generated = symbol_catalog.generate_files(self.load())["SymbolCatalog.generated.inc"]
		self.assertEqual(generated.count("Projection::ModuleId::uDWM"), 2)

	def test_rejects_duplicate_exact_identity(self) -> None:
		self.hashes["udwm"] = ["1" * 64, "4" * 64]
		self.write_fixture()
		path, source = self.source()
		source["records"][1]["image"]["time_date_stamp"] = source["records"][0]["image"]["time_date_stamp"]
		source["records"][1]["pdb"] = copy.deepcopy(source["records"][0]["pdb"])
		self.write_json(path, source)
		with self.assertRaisesRegex(projection_schema.SchemaError, "duplicate exact module identity"):
			self.load()


if __name__ == "__main__":
	unittest.main()
