from __future__ import annotations

import ast
import copy
import json
import shutil
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from tools import projection_codegen as codegen
from tools import projection_emit
from tools import projection_schema
from tools import projection_source_check
from tools import projection_target
from tools import symbol_catalog


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
			{"offset": 16, "until": {"build": 100, "revision": 0}},
			{"offset": -8, "otherwise": True},
		], "notes": "Accessor::GetField\nWARNING: validate the adjusted this pointer."}],
		"notes": [invoke] if invoke else [],
	}


def source_descriptor(value: dict, section: str) -> dict:
	"""Encode a normalized test descriptor as one common platform group."""
	item = copy.deepcopy(value)
	key = "bindings" if section == "symbols" else "cases"
	group = {"platform": None, "ranges": item[key]}
	if section == "symbols":
		for field in projection_schema.SYMBOL_GROUP_FIELDS:
			if field in item:
				group[field] = item.pop(field)
	item[key] = [group]
	return item


class ProjectionCodegenTests(unittest.TestCase):
	def setUp(self) -> None:
		self.temporary = tempfile.TemporaryDirectory()
		self.repo = Path(self.temporary.name)
		self.architecture = "legacy"
		self.projection_root = self.repo / "OpenGlass" / "Architecture" / "Legacy"
		(self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture).mkdir(parents=True)
		self.projection_root.mkdir(parents=True)
		(self.repo / "OpenGlass" / "SymbolCatalogs").mkdir(parents=True)
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
		catalog_root = self.repo / "OpenGlass" / "SymbolCatalogs"
		for index, module in enumerate(("udwm", "dwmcore"), 1):
			symbols = {} if module == "dwmcore" else {
				"Test::Run": {
					"name": "public: void __cdecl Test::Run(void)",
					"rva": "0x1000",
				},
			}
			source = catalog_root / self.architecture / module / "100.json"
			source.parent.mkdir(parents=True, exist_ok=True)
			source.write_text(json.dumps({
				"schema_version": symbol_catalog.SCHEMA_VERSION,
				"records": [{
					"revision": 7,
					"pdb": {
						"name": f"{module}.pdb",
						"guid": "12345678-1234-5678-90ab-cdef01234567",
						"age": index,
						"sha256": "b" * 64,
					},
					"symbols": symbols,
				}],
			}), encoding="utf-8")
		index_root = catalog_root / self.architecture
		(index_root / "index.json").write_text(json.dumps({
			"schema_version": symbol_catalog.SCHEMA_VERSION,
			"architecture": self.architecture,
			"resolver": {"dbghelp": {"version": "10.0.1.2", "sha256": "c" * 64}},
			"sources": ["udwm/100.json", "dwmcore/100.json"],
		}), encoding="utf-8")
		self.output = self.repo / "out"

	def tearDown(self) -> None:
		self.temporary.cleanup()

	def write(self, module: str, value: dict, *, grouped: bool = False) -> None:
		root = self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture
		manifest = copy.deepcopy(value)
		for section in ("symbols", "layouts"):
			if not grouped:
				manifest[section] = [source_descriptor(item, section) for item in manifest[section]]
			relative = f"{module}/{section}/test.json"
			path = root / relative
			path.parent.mkdir(parents=True, exist_ok=True)
			path.write_text(json.dumps({section: manifest[section]}), encoding="utf-8")
			manifest[section] = [relative]
		(root / f"{module}.json").write_text(json.dumps(manifest), encoding="utf-8")

	def refresh_catalog(self, module: str, value: dict) -> None:
		version = projection_schema.Version(100, 7)
		path = self.repo / "OpenGlass" / "SymbolCatalogs" / self.architecture / module / "100.json"
		source = json.loads(path.read_text(encoding="utf-8"))
		symbols = {}
		for symbol in value["symbols"]:
			selected = projection_schema.select_binding(version, symbol, "release")
			if selected is None:
				continue
			symbols[symbol["id"]] = {
				"name": selected[1]["symbol_names"][0],
				"rva": "0x1000",
			}
		source["records"][0]["symbols"] = symbols
		path.write_text(json.dumps(source), encoding="utf-8")

	def test_shared_version_and_binding_selection(self) -> None:
		self.assertEqual(projection_schema.Version.parse("10.0.26100.8972"), projection_schema.Version(26100, 8972))
		descriptor = {"min_inclusive": {"build": 22000, "revision": 10}, "max_exclusive": {"build": 26100, "revision": 5}}
		self.assertFalse(projection_schema.in_range(projection_schema.Version(22000, 9), descriptor))
		self.assertTrue(projection_schema.in_range(projection_schema.Version(22000, 10), descriptor))
		self.assertFalse(projection_schema.in_range(projection_schema.Version(26100, 5), descriptor))
		debug_descriptor = {
			"bindings": [
				{"symbol_names": ["Debug"], "min_inclusive": None, "max_exclusive": {"build": 26100, "revision": 5}},
				{"symbol_names": ["Debug"], "min_inclusive": {"build": 26100, "revision": 10}, "max_exclusive": None},
			],
			"condition": "debug",
		}
		self.assertIsNone(projection_schema.select_binding(projection_schema.Version(26100, 1), debug_descriptor, "release"))
		self.assertEqual(0, projection_schema.select_binding(projection_schema.Version(26100, 1), debug_descriptor, "debug")[0])
		self.assertIsNone(projection_schema.select_binding(projection_schema.Version(26100, 7), debug_descriptor, "debug"))
		self.assertEqual(1, projection_schema.select_binding(projection_schema.Version(26100, 10), debug_descriptor, "debug")[0])

	def test_deterministic_incremental_atomic_output(self) -> None:
		codegen.run(self.repo, self.architecture, self.output, False)
		before = {path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in self.output.iterdir()}
		codegen.run(self.repo, self.architecture, self.output, False)
		after = {path.name: (path.read_bytes(), path.stat().st_mtime_ns) for path in self.output.iterdir()}
		self.assertEqual(before, after)

	def test_cpp_strings_preserve_utf8_bytes_and_pool_offsets(self) -> None:
		pool = projection_emit.StringPool()
		values = ['0x', 'A', 'é0f', 'quote"\\\n']
		offsets = [pool.add(value) for value in values]
		emitted = ast.literal_eval(pool.cpp_literal()).encode("latin1")
		self.assertEqual(emitted, bytes(pool.data))
		for value, offset in zip(values, offsets):
			self.assertEqual(emitted[offset:].split(b"\0", 1)[0], value.encode("utf-8"))
		message = 'ABI mismatch: Test."Run\\é0'
		self.assertEqual(ast.literal_eval(projection_emit.cpp_string_literal(message)).encode("latin1"), message.encode("utf-8"))

	def test_metadata_type_errors_and_migration_notes_are_schema_errors(self) -> None:
		for field in ("requirement", "abi_compatibility"):
			with self.subTest(field=field):
				value = schema("udwm", projected=True)
				value["symbols"][0][field] = []
				self.write("udwm", value)
				with self.assertRaisesRegex(projection_schema.SchemaError, "non-empty string"):
					codegen.run(self.repo, self.architecture, self.output, True)
		value = schema("udwm", projected=True)
		value["layouts"][0]["kind"] = []
		self.write("udwm", value)
		with self.assertRaisesRegex(projection_schema.SchemaError, "non-empty string"):
			codegen.run(self.repo, self.architecture, self.output, True)
		for note in ("Migrated mechanically from old tables", "Guidance migrated verbatim from old tables", "MIGRATION MECHANICALLY copied"):
			with self.subTest(note=note):
				with self.assertRaisesRegex(projection_schema.SchemaError, "migration provenance"):
					projection_schema.require_notes([note], "manifest.notes")
				with self.assertRaisesRegex(projection_schema.SchemaError, "migration provenance"):
					projection_schema.require_optional_note(note, "descriptor.notes")

	def test_platform_groups_and_catalog_selection(self) -> None:
		value = schema("dwmcore", projected=False)
		value["symbols"] = [{
			"name": "Symbol_Test_Raw", "id": "Test::Raw", "kind": "raw",
			"type": "float (*)(float)", "requirement": "optional", "diagnostic_only": True,
			"bindings": [{"symbol_names": ["float __cdecl Test::Raw(float)"], "min_inclusive": None, "max_exclusive": None}],
		}]
		value = {**value, **{section: [source_descriptor(item, section) for item in value[section]] for section in ("symbols", "layouts")}}
		# Omitted platform and explicit null both denote the common group.
		value["symbols"][0]["bindings"][0].pop("platform")
		self.write("dwmcore", value, grouped=True)
		manifest = projection_target.schema_path(self.repo, self.architecture, "dwmcore")
		common = projection_schema.validate_schema(manifest, "dwmcore")
		for platform in projection_target.MACHINES:
			self.assertEqual(common, projection_schema.validate_schema(manifest, "dwmcore", platform))
		baseline = codegen.generate(self.repo, self.architecture)
		value["symbols"][0]["bindings"].append({
			"platform": "x86", "type": "double (*)(double)",
			"ranges": [{"symbol_names": ["double __cdecl Test::Raw(double)"], "min_inclusive": None, "max_exclusive": {"build": 100}}],
		})
		value["layouts"][0]["cases"].append({
			"platform": "x86", "type": "short", "ranges": [{"offset": 32, "until": {"build": 100}}],
		})
		self.write("dwmcore", value, grouped=True)
		selected = projection_schema.validate_schema(manifest, "dwmcore", "x86")
		self.assertEqual(selected["layouts"][0]["cases"], [{"offset": 32, "until": {"build": 100}}])
		self.assertEqual(selected["layouts"][0]["type"], "short")
		self.assertIsNone(projection_schema.select_binding(projection_schema.Version(100, 7), selected["symbols"][0], "release"))
		generated = codegen.generate(self.repo, self.architecture, "Win32")
		self.assertIn("double (*)(double)", generated["dwmcore.Symbols.generated.hpp"])
		self.assertNotIn("float __cdecl Test::Raw(float)", generated["ProjectionRegistry.generated.inc"])
		self.assertEqual(baseline["SymbolCatalog.generated.inc"], generated["SymbolCatalog.generated.inc"])
		# An existing specialization directory replaces the whole backend catalog.
		base = self.repo / "OpenGlass/SymbolCatalogs"
		for platform, rva in (("x86", "0x2000"), ("x64", "0x3000")):
			with self.subTest(platform=platform):
				specialized = base / platform / self.architecture
				specialized.mkdir(parents=True)
				with self.assertRaisesRegex(projection_schema.SchemaError, "index.json"):
					codegen.run(self.repo, self.architecture, self.output, False, platform)
				self.assertFalse(self.output.exists())
				shutil.copytree(base / self.architecture, specialized, dirs_exist_ok=True)
				catalog = specialized / "udwm/100.json"
				original = catalog.read_text(encoding="utf-8")
				catalog.unlink()
				with self.assertRaisesRegex(projection_schema.SchemaError, "100.json"):
					codegen.run(self.repo, self.architecture, self.output, False, platform)
				self.assertFalse(self.output.exists())
				data = json.loads(original)
				data["records"][0]["symbols"]["Test::Run"]["rva"] = rva
				catalog.write_text(json.dumps(data), encoding="utf-8")
				generated = codegen.generate(self.repo, self.architecture, platform)
				self.assertIn(f"{rva}u", generated["SymbolCatalog.generated.inc"])
		self.assertEqual(common, projection_schema.validate_schema(manifest, "dwmcore", "arm64ec"))

	def test_platform_group_filtering_and_diagnostic_locations(self) -> None:
		value = schema("dwmcore", projected=False)
		common = source_descriptor(value["layouts"][0], "layouts")
		common = {"notes": common["name"], **{key: item for key, item in common.items() if key != "notes"}}
		special = {"platform": "x86", "ranges": [{"offset": 64, "otherwise": True}]}
		common["cases"].append(special)
		only = {**common, "name": "CPU_Field", "id": "CPU.Field", "cases": [special]}
		value["layouts"] = [common, only]
		self.write("dwmcore", value, grouped=True)
		manifest = projection_target.schema_path(self.repo, self.architecture, "dwmcore")
		section = manifest.parent / "dwmcore/layouts/test.json"
		section.write_text(json.dumps({"layouts": value["layouts"]}, indent=2).replace(
			'"name": "Test_Field"', '"na\\u006de": "\\u0054est_Field"', 1,
		), encoding="utf-8")
		x64 = projection_schema.validate_schema(manifest, "dwmcore")["layouts"]
		x86 = projection_schema.validate_schema(manifest, "dwmcore", "x86")["layouts"]
		self.assertEqual([item["name"] for item in x64], ["Test_Field"])
		self.assertEqual(len(x86), 2)
		self.assertEqual(next(item for item in x86 if item["name"] == common["name"])["cases"], special["ranges"])
		x86_locations = projection_schema.section_locations(manifest, "layouts", "x86")
		x64_locations = projection_schema.section_locations(manifest, "layouts", "x64")
		self.assertNotIn("CPU_Field", x64_locations)
		self.assertIn("CPU_Field", x86_locations)
		self.assertEqual(x86_locations[common["name"]], x64_locations[common["name"]])
		text = section.read_text(encoding="utf-8")
		position = text.index('"\\u0054est_Field"')
		self.assertEqual(x64_locations[common["name"]], (section, text.count("\n", 0, position) + 1, position - text.rfind("\n", 0, position)))
		common["cases"].reverse()
		value["layouts"].reverse()
		self.write("dwmcore", value, grouped=True)
		self.assertEqual(x86, projection_schema.validate_schema(manifest, "dwmcore", "x86")["layouts"])

	def test_selected_platform_group_integrity(self) -> None:
		special = {"platform": "x86", "ranges": [{"offset": 64, "otherwise": True}]}
		for additions, error in (
			([special, special], "duplicate x86 platform group"),
			([{**special, "platform": None}], "duplicate common platform group"),
			([{**special, "platform": "unknown"}], "unknown CPU platform"),
			([{**special, "ranges": [{"offset": "64", "otherwise": True}]}], "signed 32-bit integer"),
			([{**special, "extra": True}], "unknown group fields"),
		):
			with self.subTest(error=error):
				value = schema("dwmcore", projected=False)
				value["layouts"] = [source_descriptor(value["layouts"][0], "layouts")]
				value["layouts"][0]["cases"].extend(additions)
				self.write("dwmcore", value, grouped=True)
				manifest = projection_target.schema_path(self.repo, self.architecture, "dwmcore")
				with self.assertRaisesRegex(projection_schema.SchemaError, error):
					projection_schema.validate_schema(manifest, "dwmcore", "x86")
				if all(item["platform"] == "x86" for item in additions):
					projection_schema.validate_schema(manifest, "dwmcore", "x64")
		# The common range body is not validated when a specialization replaces it.
		value = schema("dwmcore", projected=False)
		value["layouts"] = [source_descriptor(value["layouts"][0], "layouts")]
		value["layouts"][0]["cases"] = [{"ranges": [{"offset": "invalid", "otherwise": True}]}, special]
		self.write("dwmcore", value, grouped=True)
		manifest = projection_target.schema_path(self.repo, self.architecture, "dwmcore")
		self.assertEqual(projection_schema.validate_schema(manifest, "dwmcore", "x86")["layouts"][0]["cases"], special["ranges"])
		with self.assertRaisesRegex(projection_schema.SchemaError, "signed 32-bit integer"):
			projection_schema.validate_schema(manifest, "dwmcore", "x64")

	def test_platform_only_descriptors_follow_source_guards_and_required_activation(self) -> None:
		for module in projection_schema.MODULES:
			value = schema(module, projected=module == "udwm")
			if module == "dwmcore":
				value["symbols"] = [{
					"name": "Symbol_Test_Raw", "id": "Test::Raw", "kind": "raw",
					"type": "void (*)()", "requirement": "required",
					"bindings": [{"symbol_names": ["void __cdecl Raw(void)"],
						"min_inclusive": None, "max_exclusive": {"build": 100}}],
				}]
			for section in ("symbols", "layouts"):
				value[section] = [source_descriptor(item, section) for item in value[section]]
				for item in value[section]:
					item["bindings" if section == "symbols" else "cases"][0]["platform"] = "x64"
			self.write(module, value, grouped=True)
		(self.repo / "OpenGlass/ProjectionConsumer.cpp").write_text(
			"void Use(Test* test) { test->Run(); auto raw = Symbol_Test_Raw; }", encoding="utf-8",
		)
		paths = [self.projection_root / "uDwmProjection.hpp",
			self.repo / "OpenGlass/ProjectionConsumer.cpp", self.repo / "OpenGlass/ProjectionLayoutConsumer.cpp"]
		for path in paths:
			path.write_text("#if defined(_M_X64) && !defined(_M_ARM64EC)\n" + path.read_text(encoding="utf-8") + "\n#endif\n", encoding="utf-8")
		# The bounded Required raw Symbol is inactive at the catalog version.
		codegen.generate(self.repo, self.architecture)
		base = self.repo / "OpenGlass/SymbolCatalogs"
		for platform in sorted(projection_target.MACHINES.keys() - {"x64"}):
			with self.subTest(platform=platform):
				specialized = base / platform / self.architecture
				shutil.copytree(base / self.architecture, specialized)
				catalog = specialized / "udwm/100.json"
				data = json.loads(catalog.read_text(encoding="utf-8"))
				# Omitted Required descriptors have no catalog obligation on this CPU.
				with self.assertRaisesRegex(projection_schema.SchemaError, "not active"):
					codegen.generate(self.repo, self.architecture, platform)
				data["records"][0]["symbols"] = {}
				catalog.write_text(json.dumps(data), encoding="utf-8")
				generated = codegen.generate(self.repo, self.architecture, platform)
				self.assertNotIn("Projection::Requirement::Required", generated["ProjectionRegistry.generated.inc"])
				for module in projection_schema.MODULES:
					self.assertNotIn("Symbol_Test", generated[f"{module}.Symbols.generated.hpp"])
					self.assertNotIn("Test_Field", generated[f"{module}.Layouts.generated.hpp"])
					self.assertIn(f"extern std::array<LONG, 0> g_{module}SelectedOffsets;", generated[f"{module}.Layouts.generated.hpp"])
		# An unguarded wrapper for an omitted Symbol must fail source validation.
		paths[0].write_text("inline void Test::Run() { OPENGLASS_MUSTTAIL return Projection::Invoke<&Test::Run>(); }", encoding="utf-8")
		with self.assertRaisesRegex(projection_schema.SchemaError, "Invoke target has no projected_function schema"):
			codegen.generate(self.repo, self.architecture, "x86")

	def test_source_checks_select_cpu_branches_and_retain_unknown_conditions(self) -> None:
		wrapper = "inline void Test::Run() { OPENGLASS_MUSTTAIL return Projection::Invoke<&Test::Run>(); }\n"
		header = self.projection_root / "uDwmProjection.hpp"
		header.write_text(
			'#include "not-opened.hpp"\n'
			'#if defined(_M_ARM64EC)\nEC\n' + wrapper +
			'#elif defined(_M_ARM64)\nNative\n' + wrapper +
			"#elif defined(_M_X64) && _M_X64 == 100 && 'A' == 65\nAMD\n" + wrapper +
			'#elif _M_IX86 == 600\nX86\n' + wrapper +
			'#else\n#error unknown CPU\n#endif\n'
			'#if defined(_ARM64EC_)\nSDK_EC\n#elif defined(_ARM64_)\nSDK_Native\n'
			'#elif defined(_AMD64_)\nSDK_AMD\n#elif defined(_X86_)\nSDK_X86\n#endif\n'
			'#ifdef _DEBUG\nDebugBranch\n#else\nReleaseBranch\n#endif\n'
			'#if defined(_M_ARM64) && defined(FEATURE)\nNativeFeature\n'
			'#elif defined(_M_X64) || defined(FEATURE)\nAMDOrFeature\n'
			'#else\nOtherFeature\n#endif\n'
			'#define CONSUMER Test_Field\n', encoding="utf-8",
		)
		schemas = [projection_schema.validate_schema(projection_target.schema_path(self.repo, self.architecture, module), module)
			for module in projection_schema.MODULES]
		for platform, marker in (("x64", "AMD"), ("arm64", "Native"), ("arm64ec", "EC"), ("x86", "X86")):
			with self.subTest(platform=platform):
				source = projection_source_check.validate_source_completeness(self.projection_root, schemas, platform)
				self.assertIn(marker, source)
				self.assertIn(f"SDK_{marker}", source)
				self.assertEqual(1, source.count("SDK_"))
				self.assertEqual(1, source.count("Projection::Invoke"))
				self.assertIn("DebugBranch", source)
				self.assertIn("ReleaseBranch", source)
				self.assertIn("#define CONSUMER Test_Field", source)
				if platform in ("x64", "arm64ec"):
					self.assertNotIn("NativeFeature", source)
					self.assertIn("AMDOrFeature", source)
					self.assertNotIn("OtherFeature", source)
		# A consumer in an excluded branch cannot satisfy the selected CPU's contract.
		(self.repo / "OpenGlass/ProjectionLayoutConsumer.cpp").write_text(
			"#ifdef _M_ARM64\nauto field = Test_Field;\n#endif\n", encoding="utf-8",
		)
		header.write_text(wrapper, encoding="utf-8")
		projection_source_check.validate_source_completeness(self.projection_root, schemas, "arm64")
		with self.assertRaisesRegex(projection_schema.SchemaError, "Layout schema has no typed source consumer"):
			projection_source_check.validate_source_completeness(self.projection_root, schemas, "x64")

	def test_partition_order_does_not_change_generated_output(self) -> None:
		before = codegen.generate(self.repo, self.architecture)
		root = self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture
		manifest_path = root / "udwm.json"
		manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
		original = schema("udwm", projected=True)["layouts"][0]
		second = {**original, "id": "Test.AnotherField", "name": "AnotherField"}
		(root / "udwm/layouts/another.json").write_text(json.dumps({"layouts": [source_descriptor(second, "layouts")]}), encoding="utf-8")
		(self.repo / "OpenGlass" / "ProjectionLayoutConsumer.cpp").write_text(
			"auto first = Test_Field; auto second = AnotherField;", encoding="utf-8",
		)
		manifest["layouts"].append("udwm/layouts/another.json")
		manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
		partitioned = codegen.generate(self.repo, self.architecture)
		self.assertNotEqual(before, partitioned)
		manifest["layouts"].reverse()
		manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
		self.assertEqual(partitioned, codegen.generate(self.repo, self.architecture))
		self.write("udwm", {**schema("udwm", projected=True), "layouts": [second, original]})
		self.assertEqual(partitioned, codegen.generate(self.repo, self.architecture))

	def test_duplicate_ids_across_partitions_are_rejected(self) -> None:
		root = self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture
		manifest_path = root / "udwm.json"
		for section, message in (("symbols", r"duplicate C\+\+ handle name"), ("layouts", "duplicate layout name or stable ID")):
			with self.subTest(section=section):
				self.write("udwm", schema("udwm", projected=True))
				manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
				relative = f"udwm/{section}/duplicate.json"
				(root / relative).write_text(json.dumps({section: [source_descriptor(item, section) for item in schema("udwm", projected=True)[section]]}), encoding="utf-8")
				manifest[section].append(relative)
				manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
				with self.assertRaisesRegex(projection_schema.SchemaError, message):
					codegen.generate(self.repo, self.architecture)

	def test_missing_or_wrong_section_is_rejected_before_publication(self) -> None:
		root = self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture
		path = root / "udwm/symbols/test.json"
		path.unlink()
		with self.assertRaisesRegex(projection_schema.SchemaError, "cannot read.*test.json"):
			codegen.run(self.repo, self.architecture, self.output, False)
		path.write_text(json.dumps({"layouts": []}), encoding="utf-8")
		with self.assertRaisesRegex(projection_schema.SchemaError, "section file must contain only symbols"):
			codegen.run(self.repo, self.architecture, self.output, False)
		self.assertFalse(self.output.exists())

	def test_offsets_are_signed_integer_bytes(self) -> None:
		for offset in ("16", "2 * sizeof(void*)", True, 1.5, None, -0x80000001, 0x80000000):
			with self.subTest(offset=offset):
				value = schema("udwm", projected=True)
				value["layouts"][0]["cases"][0]["offset"] = offset
				self.write("udwm", value)
				with self.assertRaisesRegex(projection_schema.SchemaError, "offset must be a signed 32-bit integer byte offset"):
					codegen.generate(self.repo, self.architecture)
		value = schema("udwm", projected=True)
		value["layouts"][0]["cases"][0]["offset"] = -0x80000000
		value["layouts"][0]["cases"][1]["offset"] = 0x7FFFFFFF
		self.write("udwm", value)
		generated = codegen.generate(self.repo, self.architecture)["ProjectionRegistry.generated.inc"]
		self.assertIn("{-2147483648,", generated)
		self.assertIn("{2147483647,", generated)

	def test_check_mode_validates_without_publishing(self) -> None:
		codegen.run(self.repo, self.architecture, self.output, True)
		self.assertFalse(self.output.exists())

	def test_layout_versions_use_symbol_integer_rules(self) -> None:
		for boundary in (
			{"build": "100"}, {"build": "os::build_test"}, {"build": 0},
			{"build": -1}, {"build": True}, {"build": 0x100000000},
			{"build": 100, "revision": "0"}, {"build": 100, "revision": -1},
			{"build": 100, "revision": True}, {"build": 100, "revision": 0x100000000},
		):
			with self.subTest(boundary=boundary):
				value = schema("udwm", projected=True)
				value["layouts"][0]["cases"][0]["until"] = boundary
				self.write("udwm", value)
				with self.assertRaises(projection_schema.SchemaError):
					codegen.generate(self.repo, self.architecture)


	def test_rejects_duplicate_id_and_orphan_invoke(self) -> None:
		bad = schema("udwm", projected=True)
		bad["symbols"].append(copy.deepcopy(bad["symbols"][0]))
		bad["symbols"][1]["name"] = "Symbol_Test_Run2"
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
		self.refresh_catalog("udwm", value)
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
		(self.projection_root / "uDwmProjection.hpp").write_text("", encoding="utf-8")
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
		self.refresh_catalog("dwmcore", bad)
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
		self.refresh_catalog("udwm", value)
		codegen.run(self.repo, self.architecture, self.output, False)
		generated = (self.output / "ProjectionRegistry.generated.inc").read_text(encoding="utf-8")
		self.assertIn("is_discard_return_compatible_v<HRESULT (*)(Test*)", generated)
		self.assertIn("g_udwmBindingSymbols{0, 1}", generated)
		self.assertIn("std::span{g_udwmBindingSymbols}.subspan<0, 2>()", generated)
		self.assertEqual(generated.count("Projection::ProjectedSlotStorage<&Test::Run>()"), 1)

		value["symbols"][1]["fallback"] = "&OtherFallback"
		self.write("udwm", value)
		with self.assertRaisesRegex(projection_schema.SchemaError, "variants must use the same fallback"):
			codegen.run(self.repo, self.architecture, self.output, False)
		value["symbols"][1].pop("fallback")

		value["symbols"][1]["bindings"][0]["min_inclusive"] = None
		self.write("udwm", value)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"range overlaps"):
			codegen.run(self.repo, self.architecture, self.output, False)

	def test_grouped_bindings_keep_interleaved_candidates_and_distinct_slots(self) -> None:
		value = schema("udwm", projected=True)
		old = value["symbols"][0]
		old.update(id="A", name="RunOld", type="HRESULT (*)(Test*)", abi_compatibility="discard_return")
		old["bindings"][0]["max_exclusive"] = {"build": 100}
		other = copy.deepcopy(schema("udwm", projected=True)["symbols"][0])
		other.update(id="B", name="Other", target="&Test::Other")
		other["bindings"][0]["symbol_names"] = ["public: void __cdecl Test::Other(void)"]
		current = copy.deepcopy(schema("udwm", projected=True)["symbols"][0])
		current.update(id="C", name="RunNew")
		current["bindings"][0]["min_inclusive"] = {"build": 200}
		variable = {
			"id": "D", "name": "Value", "kind": "projected_variable", "target": "&Test::s_value", "requirement": "required",
			"bindings": [{"symbol_names": ["public: static void * Test::s_value"], "min_inclusive": None, "max_exclusive": None}],
		}
		value["symbols"] = [old, other, current, variable]
		self.write("udwm", value)
		root = self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture
		validated = projection_schema.validate_schema(root / "udwm.json", "udwm")
		generated = projection_emit.generate_registry_inc([validated])
		self.assertIn("g_udwmBindingSymbols{0, 2, 1, 3}", generated)
		self.assertIn("std::span{g_udwmBindingSymbols}.subspan<0, 2>(), Projection::ProjectedSlotStorage<&Test::Run>()", generated)
		self.assertIn("std::span{g_udwmBindingSymbols}.subspan<2, 1>(), Projection::ProjectedSlotStorage<&Test::Other>()", generated)
		self.assertIn("std::span{g_udwmBindingSymbols}.subspan<3, 1>(), Projection::ProjectedVariableStorage<&Test::s_value>(), nullptr", generated)

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
		self.refresh_catalog("dwmcore", value)
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
					)

	def test_comments_and_literals_are_not_projected_consumers(self) -> None:
		(self.repo / "OpenGlass" / "ProjectionConsumer.cpp").write_text(
			'// test->Run();\nconst char* text = "Symbol_Test_Run Test::Run()";\n'
			'const char* raw = R"tag(inner "quotes"; test->Run(); Symbol_Test_Run)tag";\n'
			'#define TEXT "Symbol_Test_Run Test::Run()"\n'
			'// Symbol_Test_Run test->Run();',
			encoding="utf-8",
		)
		with self.assertRaisesRegex(projection_schema.SchemaError, r"no runtime call site or direct Symbol consumer"):
			codegen.run(self.repo, self.architecture, self.output, False)


class ProductionProjectionContractTests(unittest.TestCase):
	def test_build_inputs_cover_every_projection_and_catalog_file(self) -> None:
		repo = Path(__file__).resolve().parent.parent
		for architecture, project_name in projection_schema.ARCHITECTURES.items():
			with self.subTest(architecture=architecture):
				root = repo / "OpenGlass" / "ProjectionSchemas" / architecture
				expected = set()
				for module in projection_schema.MODULES:
					manifest = root / f"{module}.json"
					expected.add(manifest.resolve())
					data = projection_schema.read_json(manifest)
					for section in ("symbols", "layouts"):
						expected.update(path.resolve() for path in projection_schema.section_paths(manifest, section, data[section]))
				xml = ET.parse(repo / "OpenGlass" / f"OpenGlass.{project_name}.vcxproj")
				ns = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
				items = xml.findall("m:ItemGroup/m:None", ns)
				for item in items:
					self.assertNotIn("@(", item.attrib["Include"]) # Visual Studio requires individual project items.
				actual = [(repo / "OpenGlass" / item.attrib["Include"].replace("\\", "/")).resolve()
					for item in items if item.attrib["Include"].endswith(".json")]
				self.assertEqual(len(actual), len(set(actual)))
				self.assertEqual(expected, {path.resolve() for path in root.rglob("*.json")})
				expected.update(path.resolve() for path in (repo / "OpenGlass" / "SymbolCatalogs" / architecture).rglob("*.json"))
				self.assertEqual(expected, set(actual))
				target = xml.find("m:Target[@Name='GenerateProjectionMetadata']", ns)
				self.assertIn("@(None->WithMetadataValue('Extension', '.json'))", target.attrib["Inputs"])
				self.assertIn("$(ProjectDir)..\\requirements.txt", target.attrib["Inputs"])

if __name__ == "__main__":
	unittest.main()
