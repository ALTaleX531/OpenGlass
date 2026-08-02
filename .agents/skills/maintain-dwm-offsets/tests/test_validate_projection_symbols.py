from __future__ import annotations

import json
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[4]
SCRIPT = ROOT / ".agents" / "skills" / "maintain-dwm-offsets" / "scripts" / "validate_projection_symbols.py"


class ProjectionSymbolValidationTests(unittest.TestCase):
	def test_current_schema_and_json_contract(self) -> None:
		for architecture in ("legacy", "milcomp"):
			completed = subprocess.run([sys.executable, str(SCRIPT), str(ROOT), "--architecture", architecture, "--format", "json"], capture_output=True, text=True, check=False)
			self.assertEqual(completed.returncode, 0, completed.stderr)
			result = json.loads(completed.stdout)
			self.assertEqual(result["schema_version"], 3)
			self.assertEqual(result["architecture"], architecture)
			self.assertGreater(result["summary"]["symbols"], 0)
			self.assertEqual(result["summary"]["errors"], 0)
			self.assertIn("symbol_names", result["modules"][0]["descriptors"][0])
			self.assertIn("candidate_count", result["modules"][0]["descriptors"][0])
			self.assertIn("multiple_complete_names", result["modules"][0]["descriptors"][0])
			self.assertIn("callsite_count", result["modules"][0]["descriptors"][0])
			self.assertIn("abi_compatibility", result["modules"][0]["descriptors"][0])
			self.assertIn("type", result["modules"][0]["descriptors"][0])

	def test_exact_id_filter(self) -> None:
		completed = subprocess.run([
			sys.executable, str(SCRIPT), str(ROOT), "--architecture", "milcomp", "--module", "dwmcore",
			"--id", "CVisual::SetClip", "--format", "json",
		], capture_output=True, text=True, check=False)
		self.assertEqual(completed.returncode, 0, completed.stderr)
		result = json.loads(completed.stdout)
		self.assertEqual(result["summary"]["symbols"], 1)
		self.assertEqual(result["modules"][0]["descriptors"][0]["id"], "CVisual::SetClip")

	def test_reports_explicit_projected_abi_compatibility(self) -> None:
		completed = subprocess.run([
			sys.executable, str(SCRIPT), str(ROOT), "--architecture", "legacy", "--module", "udwm",
			"--id", "CVisual::SetSize", "--format", "json",
		], capture_output=True, text=True, check=False)
		self.assertEqual(completed.returncode, 0, completed.stderr)
		descriptors = json.loads(completed.stdout)["modules"][0]["descriptors"]
		self.assertEqual(len(descriptors), 2)
		self.assertEqual(descriptors[0]["abi_compatibility"], "discard_return")
		self.assertIsNone(descriptors[1]["abi_compatibility"])

	def test_wrapper_callsite_and_direct_symbol_consumers_are_distinct(self) -> None:
		wrapper = subprocess.run([
			sys.executable, str(SCRIPT), str(ROOT), "--architecture", "milcomp", "--module", "udwm",
			"--id", "CTopLevelWindow::OnSystemBackdropUpdated", "--format", "json",
		], capture_output=True, text=True, check=False)
		self.assertEqual(wrapper.returncode, 0, wrapper.stderr)
		wrapper_descriptor = json.loads(wrapper.stdout)["modules"][0]["descriptors"][0]
		self.assertGreater(wrapper_descriptor["callsite_count"], 0)
		self.assertEqual(wrapper_descriptor["consumer_count"], 0)

		direct = subprocess.run([
			sys.executable, str(SCRIPT), str(ROOT), "--architecture", "milcomp", "--module", "dwmcore",
			"--id", "CChannel::CombinedGeometryUpdate", "--format", "json",
		], capture_output=True, text=True, check=False)
		self.assertEqual(direct.returncode, 0, direct.stderr)
		direct_descriptor = json.loads(direct.stdout)["modules"][0]["descriptors"][0]
		self.assertEqual(direct_descriptor["callsite_count"], 0)
		self.assertGreater(direct_descriptor["consumer_count"], 0)


if __name__ == "__main__":
	unittest.main()
