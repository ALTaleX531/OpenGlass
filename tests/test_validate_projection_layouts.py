from __future__ import annotations

import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

from tools import validate_projection_layouts as VALIDATOR
from tests.test_projection_codegen import source_descriptor


class ProjectionLayoutValidationTests(unittest.TestCase):
	def setUp(self) -> None:
		self.temporary = tempfile.TemporaryDirectory()
		self.repo = Path(self.temporary.name)
		self.architecture = "legacy"
		(self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture).mkdir(parents=True)


	def tearDown(self) -> None:
		self.temporary.cleanup()

	def write(
		self,
		module: str,
		layouts: list[dict],
		min_inclusive: dict[str, int] | None = None,
		max_exclusive: dict[str, int] | None = None,
	) -> None:
		payload = {
			"schema_version": VALIDATOR.SCHEMA_VERSION,
			"module": module,
			"namespace": module,
			"tag": "Tag",
			"symbols": [],
			"known_builds": [100],
			"layouts": [f"{module}/layouts/test.json"],
		}
		if min_inclusive is not None:
			payload["min_inclusive"] = min_inclusive
		if max_exclusive is not None:
			payload["max_exclusive"] = max_exclusive
		root = self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture
		section = root / module / "layouts" / "test.json"
		section.parent.mkdir(parents=True, exist_ok=True)
		section.write_text(json.dumps({"layouts": [source_descriptor(item, "layouts") for item in layouts]}), encoding="utf-8")
		(root / f"{module}.json").write_text(json.dumps(payload), encoding="utf-8")

	def test_precise_boundaries_and_unsupported(self) -> None:
		layout = {"name": "Field", "id": "Class.Field", "kind": "field", "type": "int", "cases": [
			{"offset": 24, "until": {"build": 100, "revision": 10}},
			{"offset": -8, "until": {"build": 200, "revision": 0}},
		]}
		self.write("udwm", [layout]); self.write("dwmcore", [])
		before = VALIDATOR.inspect(self.repo, self.architecture, VALIDATOR.Version(100, 9), "all")["modules"][0]["tables"][0]["selection"]
		equal = VALIDATOR.inspect(self.repo, self.architecture, VALIDATOR.Version(100, 10), "all")["modules"][0]["tables"][0]["selection"]
		after = VALIDATOR.inspect(self.repo, self.architecture, VALIDATOR.Version(200, 0), "all")["modules"][0]["tables"][0]["selection"]
		self.assertEqual(before["entry"], 0)
		self.assertEqual(before["offset"], 24)
		self.assertEqual(equal["entry"], 1)
		self.assertEqual(after["status"], "unsupported")

	def test_id_filter_preserves_schema_diagnostics(self) -> None:
		self.write("udwm", [{"name": "Field", "id": "Class.Field", "kind": "field", "type": "int", "cases": [
			{"offset": "invalid", "otherwise": True},
		]}])
		output = io.StringIO()
		with redirect_stdout(output):
			status = VALIDATOR.main([str(self.repo), "--architecture", self.architecture, "--module", "udwm", "--id", "Class.Field", "--format", "json"])
		self.assertEqual(status, 1)
		result = json.loads(output.getvalue())
		self.assertIn("offset must be a signed 32-bit", result["findings"][0]["message"])

	def test_module_range_bounds_otherwise_case(self) -> None:
		layout = {
			"name": "Field",
			"id": "Class.Field",
			"kind": "field",
			"type": "int",
			"cases": [{"offset": 4, "otherwise": True}],
		}
		self.write(
			"udwm",
			[layout],
			{"build": 100, "revision": 10},
			{"build": 200, "revision": 0},
		)
		self.write("dwmcore", [])
		inside = VALIDATOR.inspect(
			self.repo, self.architecture, VALIDATOR.Version(100, 10), "udwm"
		)["modules"][0]
		outside = VALIDATOR.inspect(
			self.repo, self.architecture, VALIDATOR.Version(200, 0), "udwm"
		)["modules"][0]
		self.assertEqual(inside["tables"][0]["selection"]["left_inclusive"], {"build": 100, "revision": 10})
		self.assertEqual(inside["tables"][0]["selection"]["right_exclusive"], {"build": 200, "revision": 0})
		self.assertTrue(inside["version_supported"])
		self.assertEqual(inside["tables"][0]["selection"]["status"], "matched")
		self.assertFalse(outside["version_supported"])
		self.assertEqual(outside["tables"][0]["selection"]["status"], "module_unsupported")

	def test_full_windows_version_id_filter_and_open_endpoint(self) -> None:
		layouts = [
			{"name": "First", "id": "Class.First", "kind": "field", "type": "int", "cases": [{"offset": 4, "otherwise": True}]},
			{"name": "Second", "id": "Class.Second", "kind": "field", "type": "int", "notes": "Accessor::GetSecond\nCross-check the constructor.", "cases": [{"offset": 8, "otherwise": True}]},
		]
		self.write("udwm", layouts); self.write("dwmcore", [])
		version = VALIDATOR.Version.parse("10.0.26100.8972")
		self.assertEqual(version, VALIDATOR.Version(26100, 8972))
		result = VALIDATOR.inspect(self.repo, self.architecture, version, "udwm", "Class.Second")
		self.assertEqual(result["summary"]["tables"], 1)
		self.assertEqual(result["modules"][0]["tables"][0]["id"], "Class.Second")
		self.assertEqual(result["modules"][0]["tables"][0]["file"], "OpenGlass/ProjectionSchemas/legacy/udwm/layouts/test.json")
		output = io.StringIO()
		with redirect_stdout(output):
			VALIDATOR.print_text(result)
		self.assertIn("[-infinity, +infinity)", output.getvalue())
		self.assertIn("Accessor::GetSecond", output.getvalue())
		self.assertEqual(result["modules"][0]["tables"][0]["notes"], "Accessor::GetSecond\nCross-check the constructor.")
		with self.assertRaises(VALIDATOR.InputError):
			VALIDATOR.inspect(self.repo, self.architecture, version, "udwm", "Missing")


if __name__ == "__main__":
	unittest.main()
