from __future__ import annotations

import copy
import json
import tempfile
import unittest
from pathlib import Path

import projection_codegen as codegen


def schema(module: str, *, projected: bool) -> dict:
	symbols = []
	invoke = ""
	if projected:
		symbols.append({
			"name": "Symbol_Test_Run", "id": "Test::Run",
			"symbol_names": ["public: void __cdecl Test::Run(void)"],
			"kind": "projected_function", "target": "&Test::Run", "requirement": "required",
			"min_inclusive": None, "max_exclusive": None,
		})
		invoke = "Projection::Invoke<&Test::Run>();"
	return {
		"schema_version": 2, "module": module, "namespace": f"OpenGlass::{module}", "tag": "ModuleTag",
		"symbols": symbols,
		"layouts": [{"name": "Test_Field", "id": "Test.Field", "kind": "field", "type": "int", "cases": [
			{"offset": "2 * sizeof(void*)", "until": {"build": "os::build_test", "revision": "0"}},
			{"offset": "-8", "otherwise": True},
		], "notes": "Accessor::GetField\nWARNING: validate the adjusted this pointer."}],
		"notes": [invoke] if invoke else [],
	}


class ProjectionCodegenTests(unittest.TestCase):
	def setUp(self) -> None:
		self.temporary = tempfile.TemporaryDirectory()
		self.repo = Path(self.temporary.name)
		self.architecture = "legacy"
		self.projection_root = self.repo / "OpenGlass" / "Architecture" / "Legacy"
		(self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture).mkdir(parents=True)
		self.projection_root.mkdir(parents=True)
		(self.repo / "OpenGlass" / "OSHelper.hpp").write_text("enum os_build : ULONG { build_test = 100 }; enum os_revision : ULONG { revision_test = 1 };", encoding="utf-8")
		(self.projection_root / "uDwmProjection.hpp").write_text(
			"inline void Test::Run() { OPENGLASS_MUSTTAIL return Projection::Invoke<&Test::Run>(); }",
			encoding="utf-8",
		)
		(self.projection_root / "dwmcoreProjection.hpp").write_text("", encoding="utf-8")
		(self.repo / "OpenGlass" / "ProjectionConsumer.cpp").write_text(
			"void Use(Test* test) { test->Run(); }",
			encoding="utf-8",
		)
		(self.repo / "OpenGlass" / "ProjectionLayoutConsumer.cpp").write_text(
			"auto layoutConsumer = Test_Field;",
			encoding="utf-8",
		)
		self.write("udwm", schema("udwm", projected=True))
		self.write("dwmcore", schema("dwmcore", projected=False))
		self.output = self.repo / "out"

	def tearDown(self) -> None:
		self.temporary.cleanup()

	def write(self, module: str, value: dict) -> None:
		(self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture / f"{module}.json").write_text(json.dumps(value), encoding="utf-8")

	def test_deterministic_incremental_atomic_output(self) -> None:
		codegen.run(self.repo, self.architecture, self.output, False)
		before = {path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in self.output.iterdir()}
		codegen.run(self.repo, self.architecture, self.output, False)
		after = {path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in self.output.iterdir()}
		self.assertEqual(before, after)
		self.assertIn(b"2 * sizeof(void*)", before["ProjectionRegistry.generated.inc"][0])
		layout_header = before["udwm.Layouts.generated.hpp"][0]
		self.assertIn(b"// Accessor::GetField", layout_header)
		self.assertIn(b"// WARNING: validate the adjusted this pointer.", layout_header)
	def test_rejects_duplicate_id_and_orphan_invoke(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"].append(copy.deepcopy(bad["symbols"][0]))
		bad["symbols"][1]["name"] = "Symbol_Test_Run2"
		self.write("udwm", bad)
		with self.assertRaises(codegen.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_old_matcher_fields(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"][0]["pdb_name"] = "Test::Run"
		bad["symbols"][0]["matcher"] = "undecorated"
		self.write("udwm", bad)
		with self.assertRaises(codegen.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_invalid_and_duplicate_complete_names(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"][0]["symbol_names"] = ["public: void Test::Run(void)", "public: void Test::Run(void)"]
		self.write("udwm", bad)
		with self.assertRaises(codegen.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

		bad = schema("udwm", projected=True)
		bad["symbols"][0]["symbol_names"] = ["public:\0 void Test::Run(void)"]
		self.write("udwm", bad)
		with self.assertRaises(codegen.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_overlapping_complete_name_candidates(self) -> None:
		bad = schema("udwm", projected=True)
		second = copy.deepcopy(bad["symbols"][0])
		second["name"] = "Symbol_Test_Run_Alias"
		second["id"] = "Test::RunAlias"
		bad["symbols"].append(second)
		self.write("udwm", bad)
		with self.assertRaises(codegen.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_optional_projected_function_without_fallback(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"][0]["requirement"] = "optional"
		self.write("udwm", bad)
		with self.assertRaises(codegen.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_migration_provenance_as_notes(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"][0]["notes"] = "Migrated " + "mechanically from old.hpp:42."
		self.write("udwm", bad)
		with self.assertRaises(codegen.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_projected_wrapper_without_musttail_macro(self) -> None:
		(self.projection_root / "uDwmProjection.hpp").write_text(
			"__forceinline void Test::Run() { return Projection::Invoke<&Test::Run>(); }",
			encoding="utf-8",
		)
		with self.assertRaises(codegen.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_projected_wrapper_without_inline(self) -> None:
		(self.projection_root / "uDwmProjection.hpp").write_text(
			"void Test::Run() { OPENGLASS_MUSTTAIL return Projection::Invoke<&Test::Run>(); }",
			encoding="utf-8",
		)
		with self.assertRaises(codegen.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_projected_wrapper_with_extra_statement(self) -> None:
		(self.projection_root / "uDwmProjection.hpp").write_text(
			"inline void Test::Run() { TouchState(); OPENGLASS_MUSTTAIL return Projection::Invoke<&Test::Run>(); }",
			encoding="utf-8",
		)
		with self.assertRaisesRegex(codegen.SchemaError, r"body must contain only"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_layout_without_typed_source_consumer(self) -> None:
		(self.repo / "OpenGlass" / "ProjectionLayoutConsumer.cpp").unlink()
		with self.assertRaisesRegex(codegen.SchemaError, r"Layout schema has no typed source consumer"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_projected_wrapper_without_runtime_consumer(self) -> None:
		(self.repo / "OpenGlass" / "ProjectionConsumer.cpp").unlink()
		with self.assertRaisesRegex(codegen.SchemaError, r"no runtime call site or direct Symbol consumer"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_accepts_direct_projected_symbol_consumer(self) -> None:
		(self.repo / "OpenGlass" / "ProjectionConsumer.cpp").write_text(
			"auto directConsumer = Symbol_Test_Run;",
			encoding="utf-8",
		)
		codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_untyped_raw_symbol(self) -> None:
		bad = schema("dwmcore", projected=False)
		bad["symbols"] = [{
			"name": "Symbol_Test_Raw", "id": "Test::Raw",
			"symbol_names": ["public: void __cdecl Test::Raw(void)"],
			"kind": "raw", "type": "PVOID", "requirement": "required",
			"min_inclusive": None, "max_exclusive": None,
		}]
		self.write("dwmcore", bad)
		with self.assertRaisesRegex(codegen.SchemaError, r"typed ABI"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_code_address_requires_explicit_usage(self) -> None:
		bad = schema("dwmcore", projected=False)
		bad["symbols"] = [{
			"name": "Symbol_Test_Anchor", "id": "Test::Anchor",
			"symbol_names": ["public: void __cdecl Test::Anchor(void)"],
			"kind": "raw", "type": "BYTE*", "requirement": "required",
			"min_inclusive": None, "max_exclusive": None,
		}]
		self.write("dwmcore", bad)
		with self.assertRaisesRegex(codegen.SchemaError, r"code_address"):
			codegen.run(self.repo, self.architecture, self.output, False)

		bad["symbols"][0]["usage"] = "code_address"
		self.write("dwmcore", bad)
		(self.repo / "OpenGlass" / "ProjectionConsumer.cpp").write_text(
			"auto anchor = Symbol_Test_Anchor.get(); void Use(Test* test) { test->Run(); }",
			encoding="utf-8",
		)
		codegen.run(self.repo, self.architecture, self.output, False)

	def test_projected_abi_compatibility_and_disjoint_target_variants(self) -> None:
		value = schema("udwm", projected=True)
		old = value["symbols"][0]
		old["name"] = "Symbol_Test_Run_Pre100"
		old["symbol_names"] = ["public: long __cdecl Test::Run(void)"]
		old["max_exclusive"] = {"build": 100, "revision": 0}
		old["type"] = "HRESULT (*)(Test*)"
		old["abi_compatibility"] = "discard_return"
		current = copy.deepcopy(value["symbols"][0])
		current["name"] = "Symbol_Test_Run"
		current["symbol_names"] = ["public: void __cdecl Test::Run(void)"]
		current["min_inclusive"] = {"build": 100, "revision": 0}
		current["max_exclusive"] = None
		current.pop("type")
		current.pop("abi_compatibility")
		value["symbols"].append(current)
		self.write("udwm", value)
		codegen.run(self.repo, self.architecture, self.output, False)
		generated = (self.output / "ProjectionRegistry.generated.inc").read_text(encoding="utf-8")
		self.assertIn("is_discard_return_compatible_v<HRESULT (*)(Test*)", generated)

		value["symbols"][1]["min_inclusive"] = None
		self.write("udwm", value)
		with self.assertRaisesRegex(codegen.SchemaError, r"range overlaps"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_projected_abi_compatibility_requires_explicit_type(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"][0]["abi_compatibility"] = "extra_trailing_argument"
		self.write("udwm", bad)
		with self.assertRaisesRegex(codegen.SchemaError, r"\.type must be a non-empty string"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_overlapping_projected_variable_targets(self) -> None:
		bad = schema("dwmcore", projected=False)
		bad["symbols"] = [
			{
				"name": "Symbol_Test_ValueA", "id": "Test::ValueA",
				"symbol_names": ["public: static void * Test::s_value"],
				"kind": "projected_variable", "target": "&Test::s_value", "requirement": "required",
				"min_inclusive": None, "max_exclusive": None,
			},
			{
				"name": "Symbol_Test_ValueB", "id": "Test::ValueB",
				"symbol_names": ["private: static void * Test::s_value"],
				"kind": "projected_variable", "target": "&Test::s_value", "requirement": "required",
				"min_inclusive": None, "max_exclusive": None,
			},
		]
		self.write("dwmcore", bad)
		with self.assertRaisesRegex(codegen.SchemaError, r"projected variable target range overlaps"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_projected_variable_without_runtime_consumer(self) -> None:
		bad = schema("dwmcore", projected=False)
		bad["symbols"] = [{
			"name": "Symbol_Test_Value", "id": "Test::Value",
			"symbol_names": ["public: static void * Test::s_value"],
			"kind": "projected_variable", "target": "&Test::s_value", "requirement": "required",
			"min_inclusive": None, "max_exclusive": None,
		}]
		self.write("dwmcore", bad)
		(self.repo / "OpenGlass" / "ProjectionVariable.cpp").write_text(
			"struct Test { inline static void* s_value; }; struct Other { inline static void* s_value; }; "
			"void UseOther() { (void)Other::s_value; }",
			encoding="utf-8",
		)
		with self.assertRaisesRegex(codegen.SchemaError, r"projected_variable schema has no runtime consumer"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_accepts_projected_variable_with_qualified_runtime_consumer(self) -> None:
		value = schema("dwmcore", projected=False)
		value["symbols"] = [{
			"name": "Symbol_Test_Value", "id": "Test::Value",
			"symbol_names": ["public: static void * Test::s_value"],
			"kind": "projected_variable", "target": "&Test::s_value", "requirement": "required",
			"min_inclusive": None, "max_exclusive": None,
		}]
		self.write("dwmcore", value)
		(self.repo / "OpenGlass" / "ProjectionVariable.cpp").write_text(
			"struct Test { inline static void* s_value; }; void UseValue() { (void)Test::s_value; }",
			encoding="utf-8",
		)
		codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_raw_symbol_without_consumer(self) -> None:
		bad = schema("dwmcore", projected=False)
		bad["symbols"] = [{
			"name": "Symbol_Test_Raw", "id": "Test::Raw",
			"symbol_names": ["public: void __cdecl Test::Raw(void)"],
			"kind": "raw", "type": "void (*)()", "requirement": "required",
			"min_inclusive": None, "max_exclusive": None,
		}]
		self.write("dwmcore", bad)
		with self.assertRaisesRegex(codegen.SchemaError, r"raw symbol schema has no direct consumer"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_metadata_counts_are_not_byte_sized(self) -> None:
		large = schema("udwm", projected=False)
		large["symbols"] = [
			{
				"name": f"Symbol_Test_{index}", "id": f"Test::{index}",
				"symbol_names": [f"public: void __cdecl Test::Run{index}(void)"],
				"kind": "raw", "type": "void (*)()", "requirement": "required",
				"min_inclusive": None, "max_exclusive": None, "diagnostic_only": True,
			}
			for index in range(300)
		]
		large["layouts"][0]["cases"] = [
			{"offset": str(index * 4), "until": {"build": str(index + 1), "revision": "0"}}
			for index in range(300)
		] + [{"offset": "1200", "otherwise": True}]
		self.write("udwm", large)
		validated = codegen.validate_schema(
			self.repo,
			self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture / "udwm.json",
			"udwm",
			codegen.load_os_constants(self.repo),
		)
		generated = codegen.generate_registry_inc([validated])
		self.assertIn("Symbol_Test_299", codegen.generate_symbol_header(validated))
		self.assertIn("static_cast<LONG>(1200)", generated)
		self.assertNotIn("static_cast<SHORT>", generated)

	def test_comments_and_literals_are_not_projected_consumers(self) -> None:
		(self.repo / "OpenGlass" / "ProjectionConsumer.cpp").write_text(
			'// test->Run();\nconst char* text = "Symbol_Test_Run Test::Run()";',
			encoding="utf-8",
		)
		with self.assertRaisesRegex(codegen.SchemaError, r"no runtime call site or direct Symbol consumer"):
			codegen.run(self.repo, self.architecture, self.output, False)

if __name__ == "__main__":
	unittest.main()
