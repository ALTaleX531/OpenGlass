from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from tests.test_projection_codegen import source_descriptor
from tools import projection_schema


ROOT = Path(__file__).resolve().parents[1]


class ProjectionSymbolValidationTests(unittest.TestCase):
	def setUp(self) -> None:
		self.temporary = tempfile.TemporaryDirectory()
		self.addCleanup(self.temporary.cleanup)
		self.repo = Path(self.temporary.name)
		self.schema_root = self.repo / "OpenGlass" / "ProjectionSchemas" / "legacy"
		self.projection_root = self.repo / "OpenGlass" / "Architecture" / "Legacy"
		self.projection_root.mkdir(parents=True)
		(self.projection_root / "uDwmProjection.hpp").write_text(
			"inline void Test::Run() { OPENGLASS_MUSTTAIL return Projection::Invoke<&Test::Run>(); }\n"
			"inline void Test::RunExtra() { OPENGLASS_MUSTTAIL return Projection::Invoke<&Test::RunExtra>(); }\n",
			encoding="utf-8",
		)
		self.consumer = self.repo / "OpenGlass" / "ProjectionConsumer.cpp"
		self.consumer.write_text(
			"void Use(Test* test) { test->Run(); }\n"
			"auto directConsumer = Symbol_Test_RunExtra;\n",
			encoding="utf-8",
		)
		self.write("udwm", [
			{
				"name": "Symbol_Test_RunBefore", "id": "Test::Run",
				"kind": "projected_function", "target": "&Test::Run", "requirement": "required",
				"abi_compatibility": "discard_return", "type": "long (WINAPI*)(Test*)",
				"bindings": [{
					"symbol_names": ["public: long __cdecl Test::Run(void)"],
					"min_inclusive": None, "max_exclusive": {"build": 100},
				}],
			},
			{
				"name": "Symbol_Test_Run", "id": "Test::Run",
				"kind": "projected_function", "target": "&Test::Run", "requirement": "required",
				"bindings": [{
					"symbol_names": ["public: void __cdecl Test::Run(void)"],
					"min_inclusive": {"build": 100}, "max_exclusive": None,
				}],
			},
			{
				"name": "Symbol_Test_RunExtra", "id": "Test::RunExtra",
				"kind": "projected_function", "target": "&Test::RunExtra", "requirement": "required",
				"bindings": [{
					"symbol_names": ["public: void __cdecl Test::RunExtra(void)"],
					"min_inclusive": None, "max_exclusive": None,
				}],
			},
		])
		self.write("dwmcore", [])

	def write(self, module: str, symbols: list[dict]) -> None:
		relative = f"{module}/symbols/test.json"
		section = self.schema_root / relative
		section.parent.mkdir(parents=True, exist_ok=True)
		section.write_text(json.dumps({
			"symbols": [source_descriptor(symbol, "symbols") for symbol in symbols],
		}), encoding="utf-8")
		(self.schema_root / f"{module}.json").write_text(json.dumps({
			"schema_version": projection_schema.SCHEMA_VERSION,
			"module": module, "namespace": module, "tag": "ModuleTag",
			"known_builds": [100], "symbols": [relative], "layouts": [],
		}), encoding="utf-8")

	def run_validator(self, *arguments: str) -> subprocess.CompletedProcess[str]:
		return subprocess.run([
			sys.executable, "-m", "tools.validate_projection_symbols", str(self.repo),
			"--architecture", "legacy", *arguments,
		], cwd=ROOT, capture_output=True, text=True, check=False)

	def test_json_report_contract(self) -> None:
		completed = self.run_validator("--format", "json")
		self.assertEqual(completed.returncode, 0, completed.stderr)
		self.assertEqual(completed.stderr, "")
		result = json.loads(completed.stdout)
		self.assertEqual(result["schema_version"], 3)
		self.assertEqual(result["architecture"], "legacy")
		self.assertEqual(result["platform"], "x64")
		self.assertIsNone(result["requested_id"])
		self.assertEqual(result["summary"], {"symbols": 3, "errors": 0})
		self.assertEqual(result["findings"], [])
		self.assertEqual([module["module"] for module in result["modules"]], ["udwm", "dwmcore"])
		for descriptor in result["modules"][0]["descriptors"]:
			self.assertEqual(descriptor["candidate_count"], 1)
			self.assertEqual(descriptor["binding_count"], 1)
			self.assertFalse(descriptor["multiple_complete_names"])
			self.assertEqual(descriptor["symbol_names"], descriptor["bindings"][0]["symbol_names"])
			self.assertEqual(descriptor["file"], "OpenGlass/ProjectionSchemas/legacy/udwm/symbols/test.json")
			self.assertGreater(descriptor["line"], 0)
			self.assertGreater(descriptor["column"], 0)

	def test_exact_id_reports_all_projected_abi_variants(self) -> None:
		completed = self.run_validator("--format", "json", "--module", "udwm", "--id", "Test::Run")
		self.assertEqual(completed.returncode, 0, completed.stderr)
		result = json.loads(completed.stdout)
		self.assertEqual(result["requested_id"], "Test::Run")
		self.assertEqual(result["summary"]["symbols"], 2)
		self.assertEqual(len(result["modules"]), 1)
		descriptors = result["modules"][0]["descriptors"]
		self.assertEqual([item["id"] for item in descriptors], ["Test::Run", "Test::Run"])
		by_name = {item["name"]: item for item in descriptors}
		self.assertEqual(by_name["Symbol_Test_RunBefore"]["abi_compatibility"], "discard_return")
		self.assertEqual(by_name["Symbol_Test_RunBefore"]["type"], "long (WINAPI*)(Test*)")
		self.assertIsNone(by_name["Symbol_Test_Run"]["abi_compatibility"])
		self.assertIsNone(by_name["Symbol_Test_Run"]["type"])

	def test_missing_id_retains_input_error_contract(self) -> None:
		completed = self.run_validator("--format", "json", "--id", "Test::Ru")
		self.assertEqual(completed.returncode, 2)
		self.assertEqual(completed.stdout, "")
		result = json.loads(completed.stderr)
		self.assertEqual(result["schema_version"], 3)
		self.assertIn("Symbol stable ID not found", result["error"])

	def test_wrapper_and_direct_symbol_consumers_are_distinct(self) -> None:
		completed = self.run_validator("--format", "json")
		self.assertEqual(completed.returncode, 0, completed.stderr)
		by_name = {item["name"]: item for item in json.loads(completed.stdout)["modules"][0]["descriptors"]}
		self.assertEqual(by_name["Symbol_Test_Run"]["callsite_count"], 1)
		self.assertEqual(by_name["Symbol_Test_Run"]["consumer_count"], 0)
		self.assertEqual(by_name["Symbol_Test_RunExtra"]["callsite_count"], 0)
		self.assertEqual(by_name["Symbol_Test_RunExtra"]["consumer_count"], 1)

	def test_direct_consumer_on_later_abi_variant_satisfies_target(self) -> None:
		(self.projection_root / "uDwmProjection.hpp").write_text(
			"struct Test { void Run() {} void RunExtra() {} };\n", encoding="utf-8",
		)
		self.consumer.write_text(
			"auto runConsumer = Symbol_Test_Run;\n"
			"auto extraConsumer = Symbol_Test_RunExtra;\n", encoding="utf-8",
		)
		completed = self.run_validator("--format", "json")
		self.assertEqual(completed.returncode, 0, completed.stderr)
		by_name = {item["name"]: item for item in json.loads(completed.stdout)["modules"][0]["descriptors"]}
		self.assertEqual(by_name["Symbol_Test_RunBefore"]["callsite_count"], 0)
		self.assertEqual(by_name["Symbol_Test_RunBefore"]["consumer_count"], 0)
		self.assertEqual(by_name["Symbol_Test_Run"]["callsite_count"], 0)
		self.assertEqual(by_name["Symbol_Test_Run"]["consumer_count"], 1)

	def test_schema_error_is_reported_even_outside_module_and_id_filters(self) -> None:
		manifest = self.schema_root / "dwmcore.json"
		value = json.loads(manifest.read_text(encoding="utf-8"))
		value["known_builds"] = []
		manifest.write_text(json.dumps(value), encoding="utf-8")
		for stable_id in ("Test::Run", "Missing"):
			with self.subTest(stable_id=stable_id):
				completed = self.run_validator("--format", "json", "--module", "udwm", "--id", stable_id)
				self.assertEqual(completed.returncode, 1, completed.stderr)
				self.assertEqual(completed.stderr, "")
				result = json.loads(completed.stdout)
				self.assertEqual(result["schema_version"], 3)
				self.assertEqual(result["requested_id"], stable_id)
				self.assertEqual(result["modules"], [])
				self.assertEqual(result["summary"], {"symbols": 0, "errors": 1})
				self.assertEqual(result["findings"][0]["severity"], "error")
				self.assertIn("dwmcore.json", result["findings"][0]["message"])
				self.assertIn("known_builds must not be empty", result["findings"][0]["message"])

	def test_missing_runtime_consumers_are_validation_findings(self) -> None:
		self.consumer.write_text("", encoding="utf-8")
		for output_format in ("json", "text"):
			with self.subTest(output_format=output_format):
				completed = self.run_validator("--format", output_format)
				self.assertEqual(completed.returncode, 1, completed.stderr)
				self.assertEqual(completed.stderr, "")
				if output_format == "json":
					result = json.loads(completed.stdout)
					self.assertEqual(result["summary"]["errors"], 1)
					self.assertIn("no runtime call site or direct Symbol consumer", result["findings"][0]["message"])
				else:
					self.assertIn("0 descriptors, 1 errors", completed.stdout)
					self.assertIn("ERROR:", completed.stdout)
					self.assertIn("no runtime call site or direct Symbol consumer", completed.stdout)


if __name__ == "__main__":
	unittest.main()
