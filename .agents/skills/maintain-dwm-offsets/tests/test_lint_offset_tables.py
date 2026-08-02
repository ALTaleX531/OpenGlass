from __future__ import annotations

import importlib.util
import io
import json
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "scripts" / "lint_offset_tables.py"
SPEC = importlib.util.spec_from_file_location("lint_offset_tables", SCRIPT)
assert SPEC and SPEC.loader
LINTER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = LINTER
SPEC.loader.exec_module(LINTER)


class LayoutSchemaLinterTests(unittest.TestCase):
	def setUp(self) -> None:
		self.temporary = tempfile.TemporaryDirectory()
		self.repo = Path(self.temporary.name)
		self.architecture = "legacy"
		(self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture).mkdir(parents=True)
		(self.repo / "OpenGlass" / "OSHelper.hpp").write_text(
			"enum os_build : ULONG { build_a = 100, build_b = 200 };\n"
			"enum os_revision : ULONG { revision_a = 10, revision_b = 20 };\n",
			encoding="utf-8",
		)

	def tearDown(self) -> None:
		self.temporary.cleanup()

	def write(self, module: str, layouts: list[dict]) -> None:
		payload = {"schema_version": 2, "module": module, "namespace": module, "tag": "Tag", "symbols": [], "layouts": layouts}
		(self.repo / "OpenGlass" / "ProjectionSchemas" / self.architecture / f"{module}.json").write_text(json.dumps(payload), encoding="utf-8")

	def test_precise_boundaries_and_unsupported(self) -> None:
		layout = {"name": "Field", "id": "Class.Field", "kind": "field", "type": "int", "cases": [
			{"offset": "dangerous()", "until": {"build": "os::build_a", "revision": "os::revision_a"}},
			{"offset": "-8", "until": {"build": "os::build_b", "revision": "0"}},
		]}
		self.write("udwm", [layout]); self.write("dwmcore", [])
		before = LINTER.inspect(self.repo, self.architecture, LINTER.Version(100, 9), "all")["modules"][0]["tables"][0]["selection"]
		equal = LINTER.inspect(self.repo, self.architecture, LINTER.Version(100, 10), "all")["modules"][0]["tables"][0]["selection"]
		after = LINTER.inspect(self.repo, self.architecture, LINTER.Version(200, 0), "all")["modules"][0]["tables"][0]["selection"]
		self.assertEqual(before["entry"], 0)
		self.assertEqual(before["offset_expression"], "dangerous()")
		self.assertEqual(equal["entry"], 1)
		self.assertEqual(after["status"], "unsupported")

	def test_otherwise_and_duplicate_boundary_error(self) -> None:
		layout = {"name": "Field", "id": "Field", "kind": "field", "type": "int", "cases": [
			{"offset": "1", "until": {"build": "100", "revision": "0"}},
			{"offset": "2", "until": {"build": "100", "revision": "0"}},
			{"offset": "3", "otherwise": True},
		]}
		self.write("udwm", [layout]); self.write("dwmcore", [])
		result = LINTER.inspect(self.repo, self.architecture, None, "all")
		self.assertEqual(result["summary"]["errors"], 1)

	def test_rejects_invalid_notes(self) -> None:
		layout = {"name": "Field", "id": "Field", "kind": "field", "type": "int", "notes": [], "cases": [
			{"offset": "1", "otherwise": True},
		]}
		self.write("udwm", [layout]); self.write("dwmcore", [])
		result = LINTER.inspect(self.repo, self.architecture, None, "all")
		self.assertEqual(result["summary"]["errors"], 1)
		self.assertEqual(result["modules"][0]["tables"], [])
		self.assertIn("notes must be a non-empty string", result["findings"][0]["message"])

	def test_full_windows_version_id_filter_and_open_endpoint(self) -> None:
		layouts = [
			{"name": "First", "id": "Class.First", "kind": "field", "type": "int", "cases": [{"offset": "4", "otherwise": True}]},
			{"name": "Second", "id": "Class.Second", "kind": "field", "type": "int", "notes": "Accessor::GetSecond\nCross-check the constructor.", "cases": [{"offset": "8", "otherwise": True}]},
		]
		self.write("udwm", layouts); self.write("dwmcore", [])
		version = LINTER.parse_requested("10.0.26100.8972")
		self.assertEqual(version, LINTER.Version(26100, 8972))
		result = LINTER.inspect(self.repo, self.architecture, version, "udwm", "Class.Second")
		self.assertEqual(result["summary"]["tables"], 1)
		self.assertEqual(result["modules"][0]["tables"][0]["id"], "Class.Second")
		output = io.StringIO()
		with redirect_stdout(output):
			LINTER.print_text(result)
		self.assertIn("[-infinity, +infinity)", output.getvalue())
		self.assertIn("Accessor::GetSecond", output.getvalue())
		self.assertEqual(result["modules"][0]["tables"][0]["notes"], "Accessor::GetSecond\nCross-check the constructor.")
		with self.assertRaises(LINTER.InputError):
			LINTER.inspect(self.repo, self.architecture, version, "udwm", "Missing")


if __name__ == "__main__":
	unittest.main()
