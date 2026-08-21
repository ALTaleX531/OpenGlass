from __future__ import annotations

import copy
import json
import tempfile
import unittest
from unittest import mock
from pathlib import Path

import projection_codegen as codegen
import projection_emit
import projection_schema
import projection_source_check


def schema(module: str, *, projected: bool) -> dict:
	symbols = []
	invoke = ""
	if projected:
		symbols.append({
			"name": "Symbol_Test_Run", "id": "Test::Run",
			"bindings": [{
				"symbol_names": ["public: void __cdecl Test::Run(void)"],
				"min_inclusive": None, "max_exclusive": None,
			}],
			"kind": "projected_function", "target": "&Test::Run", "requirement": "required",
		})
		invoke = "Projection::Invoke<&Test::Run>();"
	return {
		"schema_version": projection_schema.SCHEMA_VERSION, "module": module, "namespace": f"OpenGlass::{module}", "tag": "ModuleTag",
		"known_builds": [100],
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

	def test_generate_is_pure_and_owns_the_fixed_output_contract(self) -> None:
		before = {
			module: json.loads(
				(self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture / f"{module}.json").read_text(encoding="utf-8")
			)
			for module in projection_schema.MODULES
		}
		generated = codegen.generate(self.repo, self.architecture)
		after = {
			module: json.loads(
				(self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture / f"{module}.json").read_text(encoding="utf-8")
			)
			for module in projection_schema.MODULES
		}
		self.assertEqual(before, after)
		self.assertEqual(
			{
				"udwm.Layouts.generated.hpp",
				"udwm.Symbols.generated.hpp",
				"dwmcore.Layouts.generated.hpp",
				"dwmcore.Symbols.generated.hpp",
				"ProjectionRegistry.generated.inc",
			},
			set(generated),
		)
		self.assertFalse(self.output.exists())

	def test_check_mode_validates_without_publishing(self) -> None:
		codegen.run(self.repo, self.architecture, self.output, True)
		self.assertFalse(self.output.exists())

	def test_source_read_failure_is_an_expected_schema_error(self) -> None:
		(self.repo / "OpenGlass" / "OSHelper.hpp").unlink()
		with self.assertRaisesRegex(projection_schema.SchemaError, r"cannot read projection source"):
			codegen.generate(self.repo, self.architecture)

	def test_atomic_write_removes_temporary_file_after_replace_failure(self) -> None:
		target = self.output / "ProjectionRegistry.generated.inc"
		with mock.patch.object(codegen.os, "replace", side_effect=OSError("replace failed")):
			with self.assertRaisesRegex(projection_schema.SchemaError, r"cannot atomically write"):
				codegen.atomic_write(target, "generated")
		self.assertEqual([], list(self.output.glob("*.tmp")))

	def test_rejects_duplicate_id_and_orphan_invoke(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"].append(copy.deepcopy(bad["symbols"][0]))
		bad["symbols"][1]["name"] = "Symbol_Test_Run2"
		self.write("udwm", bad)
		with self.assertRaises(projection_schema.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_old_matcher_fields(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"][0]["pdb_name"] = "Test::Run"
		bad["symbols"][0]["matcher"] = "undecorated"
		self.write("udwm", bad)
		with self.assertRaises(projection_schema.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_invalid_and_duplicate_complete_names(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"][0]["bindings"][0]["symbol_names"] = ["public: void Test::Run(void)", "public: void Test::Run(void)"]
		self.write("udwm", bad)
		with self.assertRaises(projection_schema.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

		bad = schema("udwm", projected=True)
		bad["symbols"][0]["bindings"][0]["symbol_names"] = ["public:\0 void Test::Run(void)"]
		self.write("udwm", bad)
		with self.assertRaises(projection_schema.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_overlapping_complete_name_candidates(self) -> None:
		bad = schema("udwm", projected=True)
		second = copy.deepcopy(bad["symbols"][0])
		second["name"] = "Symbol_Test_Run_Alias"
		second["id"] = "Test::RunAlias"
		bad["symbols"].append(second)
		self.write("udwm", bad)
		with self.assertRaises(projection_schema.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_logical_symbol_accepts_disjoint_bindings_and_rejects_overlap(self) -> None:
		value = schema("udwm", projected=True)
		first = value["symbols"][0]["bindings"][0]
		first["max_exclusive"] = {"build": 100, "revision": 0}
		value["symbols"][0]["bindings"].append({
			"symbol_names": ["public: void __cdecl Test::Run(void)"],
			"min_inclusive": {"build": 100, "revision": 1},
			"max_exclusive": None,
		})
		self.write("udwm", value)
		codegen.run(self.repo, self.architecture, self.output, False)
		generated = (self.output / "ProjectionRegistry.generated.inc").read_text(encoding="utf-8")
		self.assertIn("g_udwmCandidates[1]", generated)
		self.assertEqual(2, generated.count("Projection::Requirement::Required"))

		value["symbols"][0]["bindings"][1]["min_inclusive"] = {"build": 99, "revision": 0}
		self.write("udwm", value)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"binding range overlaps"):
			codegen.run(self.repo, self.architecture, self.output, False)

		value["symbols"][0]["bindings"][1]["min_inclusive"] = {"build": 100, "revision": 1}
		value["symbols"][0]["bindings"].reverse()
		self.write("udwm", value)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"bindings must be ordered"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_optional_projected_function_without_fallback(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"][0]["requirement"] = "optional"
		self.write("udwm", bad)
		with self.assertRaises(projection_schema.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_migration_provenance_as_notes(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"][0]["notes"] = "Migrated " + "mechanically from old.hpp:42."
		self.write("udwm", bad)
		with self.assertRaises(projection_schema.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_projected_wrapper_without_musttail_macro(self) -> None:
		(self.projection_root / "uDwmProjection.hpp").write_text(
			"__forceinline void Test::Run() { return Projection::Invoke<&Test::Run>(); }",
			encoding="utf-8",
		)
		with self.assertRaises(projection_schema.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_projected_wrapper_without_inline(self) -> None:
		(self.projection_root / "uDwmProjection.hpp").write_text(
			"void Test::Run() { OPENGLASS_MUSTTAIL return Projection::Invoke<&Test::Run>(); }",
			encoding="utf-8",
		)
		with self.assertRaises(projection_schema.SchemaError):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_projected_wrapper_with_extra_statement(self) -> None:
		(self.projection_root / "uDwmProjection.hpp").write_text(
			"inline void Test::Run() { TouchState(); OPENGLASS_MUSTTAIL return Projection::Invoke<&Test::Run>(); }",
			encoding="utf-8",
		)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"body must contain only"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_layout_without_typed_source_consumer(self) -> None:
		(self.repo / "OpenGlass" / "ProjectionLayoutConsumer.cpp").unlink()
		with self.assertRaisesRegex(projection_schema.SchemaError, r"Layout schema has no typed source consumer"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_projected_wrapper_without_runtime_consumer(self) -> None:
		(self.repo / "OpenGlass" / "ProjectionConsumer.cpp").unlink()
		with self.assertRaisesRegex(projection_schema.SchemaError, r"no runtime call site or direct Symbol consumer"):
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
			"bindings": [{
				"symbol_names": ["public: void __cdecl Test::Raw(void)"],
				"min_inclusive": None, "max_exclusive": None,
			}],
			"kind": "raw", "type": "PVOID", "requirement": "required",
		}]
		self.write("dwmcore", bad)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"typed ABI"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_code_address_requires_explicit_usage(self) -> None:
		bad = schema("dwmcore", projected=False)
		bad["symbols"] = [{
			"name": "Symbol_Test_Anchor", "id": "Test::Anchor",
			"bindings": [{
				"symbol_names": ["public: void __cdecl Test::Anchor(void)"],
				"min_inclusive": None, "max_exclusive": None,
			}],
			"kind": "raw", "type": "BYTE*", "requirement": "required",
		}]
		self.write("dwmcore", bad)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"code_address"):
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
		old["bindings"][0]["symbol_names"] = ["public: long __cdecl Test::Run(void)"]
		old["bindings"][0]["max_exclusive"] = {"build": 100, "revision": 0}
		old["type"] = "HRESULT (*)(Test*)"
		old["abi_compatibility"] = "discard_return"
		current = copy.deepcopy(value["symbols"][0])
		current["name"] = "Symbol_Test_Run"
		current["bindings"][0]["symbol_names"] = ["public: void __cdecl Test::Run(void)"]
		current["bindings"][0]["min_inclusive"] = {"build": 100, "revision": 0}
		current["bindings"][0]["max_exclusive"] = None
		current.pop("type")
		current.pop("abi_compatibility")
		value["symbols"].append(current)
		self.write("udwm", value)
		codegen.run(self.repo, self.architecture, self.output, False)
		generated = (self.output / "ProjectionRegistry.generated.inc").read_text(encoding="utf-8")
		self.assertIn("is_discard_return_compatible_v<HRESULT (*)(Test*)", generated)

		value["symbols"][1]["bindings"][0]["min_inclusive"] = None
		self.write("udwm", value)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"range overlaps"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_projected_abi_compatibility_requires_explicit_type(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"][0]["abi_compatibility"] = "extra_trailing_argument"
		self.write("udwm", bad)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"\.type must be a non-empty string"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_overlapping_projected_variable_targets(self) -> None:
		bad = schema("dwmcore", projected=False)
		bad["symbols"] = [
			{
				"name": "Symbol_Test_ValueA", "id": "Test::ValueA",
				"bindings": [{
					"symbol_names": ["public: static void * Test::s_value"],
					"min_inclusive": None, "max_exclusive": None,
				}],
				"kind": "projected_variable", "target": "&Test::s_value", "requirement": "required",
			},
			{
				"name": "Symbol_Test_ValueB", "id": "Test::ValueB",
				"bindings": [{
					"symbol_names": ["private: static void * Test::s_value"],
					"min_inclusive": None, "max_exclusive": None,
				}],
				"kind": "projected_variable", "target": "&Test::s_value", "requirement": "required",
			},
		]
		self.write("dwmcore", bad)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"projected variable target range overlaps"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_rejects_projected_variable_without_runtime_consumer(self) -> None:
		bad = schema("dwmcore", projected=False)
		bad["symbols"] = [{
			"name": "Symbol_Test_Value", "id": "Test::Value",
			"bindings": [{
				"symbol_names": ["public: static void * Test::s_value"],
				"min_inclusive": None, "max_exclusive": None,
			}],
			"kind": "projected_variable", "target": "&Test::s_value", "requirement": "required",
		}]
		self.write("dwmcore", bad)
		(self.repo / "OpenGlass" / "ProjectionVariable.cpp").write_text(
			"struct Test { inline static void* s_value; }; struct Other { inline static void* s_value; }; "
			"void UseOther() { (void)Other::s_value; }",
			encoding="utf-8",
		)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"projected_variable schema has no runtime consumer"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_accepts_projected_variable_with_qualified_runtime_consumer(self) -> None:
		value = schema("dwmcore", projected=False)
		value["symbols"] = [{
			"name": "Symbol_Test_Value", "id": "Test::Value",
			"bindings": [{
				"symbol_names": ["public: static void * Test::s_value"],
				"min_inclusive": None, "max_exclusive": None,
			}],
			"kind": "projected_variable", "target": "&Test::s_value", "requirement": "required",
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
			"bindings": [{
				"symbol_names": ["public: void __cdecl Test::Raw(void)"],
				"min_inclusive": None, "max_exclusive": None,
			}],
			"kind": "raw", "type": "void (*)()", "requirement": "required",
		}]
		self.write("dwmcore", bad)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"raw symbol schema has no direct consumer"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_emits_module_supported_version_range(self) -> None:
		value = schema("dwmcore", projected=False)
		value["min_inclusive"] = {"build": 100, "revision": 1}
		value["max_exclusive"] = {"build": 100, "revision": 3}
		self.write("dwmcore", value)
		validated = projection_schema.validate_schema(
			self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture / "dwmcore.json",
			"dwmcore",
			projection_source_check.load_os_constants(self.repo),
		)
		generated = projection_emit.generate_registry_inc([validated])
		self.assertIn("Projection::VersionRange{Generated::g_dwmcoreVersions[1], Generated::g_dwmcoreVersions[2]}", generated)
		self.assertIn("constexpr ULONG g_dwmcoreKnownBuilds[]", generated)
		self.assertIn("100u,", generated)
		self.assertIn("std::span{Generated::g_dwmcoreKnownBuilds}", generated)

	def test_rejects_inverted_module_supported_version_range(self) -> None:
		value = schema("dwmcore", projected=False)
		value["min_inclusive"] = {"build": 100, "revision": 3}
		value["max_exclusive"] = {"build": 100, "revision": 3}
		self.write("dwmcore", value)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"max_exclusive must follow min_inclusive"):
			projection_schema.validate_schema(
				self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture / "dwmcore.json",
				"dwmcore",
				projection_source_check.load_os_constants(self.repo),
			)

	def test_rejects_invalid_known_builds(self) -> None:
		for known_builds, message in (
			([], "must not be empty"),
			([0], "must contain nonzero builds"),
			([100, 100], "must be strictly increasing"),
			([101, 100], "must be strictly increasing"),
			([99], "outside the module version range"),
			([101], "outside the module version range"),
		):
			with self.subTest(known_builds=known_builds):
				value = schema("dwmcore", projected=False)
				value["min_inclusive"] = {"build": 100, "revision": 1}
				value["max_exclusive"] = {"build": 101, "revision": 0}
				value["known_builds"] = known_builds
				self.write("dwmcore", value)
				with self.assertRaisesRegex(projection_schema.SchemaError, message):
					projection_schema.validate_schema(
						self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture / "dwmcore.json",
						"dwmcore",
						projection_source_check.load_os_constants(self.repo),
					)

	def test_metadata_counts_are_not_byte_sized(self) -> None:
		large = schema("udwm", projected=False)
		large["symbols"] = [
			{
				"name": f"Symbol_Test_{index}", "id": f"Test::{index}",
				"bindings": [{
					"symbol_names": [f"public: void __cdecl Test::Run{index}(void)"],
					"min_inclusive": None, "max_exclusive": None,
				}],
				"kind": "raw", "type": "void (*)()", "requirement": "required",
				"diagnostic_only": True,
			}
			for index in range(300)
		]
		large["layouts"][0]["cases"] = [
			{"offset": str(index * 4), "until": {"build": str(index + 1), "revision": "0"}}
			for index in range(300)
		] + [{"offset": "1200", "otherwise": True}]
		self.write("udwm", large)
		validated = projection_schema.validate_schema(
			self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture / "udwm.json",
			"udwm",
			projection_source_check.load_os_constants(self.repo),
		)
		generated = projection_emit.generate_registry_inc([validated])
		self.assertIn("Symbol_Test_299", projection_emit.generate_symbol_header(validated))
		self.assertIn("static_cast<LONG>(1200)", generated)
		self.assertNotIn("static_cast<SHORT>", generated)

	def test_comments_and_literals_are_not_projected_consumers(self) -> None:
		(self.repo / "OpenGlass" / "ProjectionConsumer.cpp").write_text(
			'// test->Run();\nconst char* text = "Symbol_Test_Run Test::Run()";',
			encoding="utf-8",
		)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"no runtime call site or direct Symbol consumer"):
			codegen.run(self.repo, self.architecture, self.output, False)


class ProductionProjectionContractTests(unittest.TestCase):
	def test_server_2022_collect_rectangle_uses_updated_abi(self) -> None:
		repo = Path(__file__).resolve().parent.parent
		schema_path = repo / "OpenGlass" / "ProjectionSchemas" / "legacy" / "dwmcore.json"
		validated = projection_schema.validate_schema(
			schema_path,
			"dwmcore",
			projection_source_check.load_os_constants(repo),
		)
		symbols = {symbol["id"]: symbol for symbol in validated["symbols"]}
		pre_server = symbols["COcclusionContext::CollectRectangleForOcclusion.pre20348"]
		server = symbols["COcclusionContext::CollectRectangleForOcclusion.20348"]

		self.assertEqual((20348, 0), projection_schema.version_key(pre_server["bindings"][0]["max_exclusive"]))
		self.assertEqual((20348, 0), projection_schema.version_key(server["bindings"][0]["min_inclusive"]))
		self.assertTrue(any(name.startswith("private: long ") for name in pre_server["bindings"][0]["symbol_names"]))
		self.assertTrue(
			any(name.startswith("private: void ") and ",bool,bool," in name for name in server["bindings"][0]["symbol_names"])
		)

		def active(symbol: projection_schema.Symbol, version: tuple[int, int]) -> bool:
			return any(
				projection_schema.version_key(binding["min_inclusive"]) <= version and
				(not projection_schema.version_key(binding["max_exclusive"])[0] or
				 version < projection_schema.version_key(binding["max_exclusive"]))
				for binding in symbol["bindings"]
			)

		collect_rectangle = [pre_server, server]
		self.assertEqual(
			["COcclusionContext::CollectRectangleForOcclusion.pre20348"],
			[symbol["id"] for symbol in collect_rectangle if active(symbol, (19041, 1))],
		)
		self.assertEqual(
			["COcclusionContext::CollectRectangleForOcclusion.20348"],
			[symbol["id"] for symbol in collect_rectangle if active(symbol, (20348, 2582))],
		)


if __name__ == "__main__":
	unittest.main()
