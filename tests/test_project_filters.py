"""Keep imported project files out of loose Solution Explorer nodes."""

from collections import Counter
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
FILE_ITEMS = {"ClCompile", "ClInclude", "None", "Image", "ResourceCompile", "FxCompile", "MASM", "Manifest", "CustomBuild", "Natvis", "Content", "Midl", "Xsd"}


def local_path(owner: Path, value: str) -> Path:
	return (owner.parent / value.replace("$(MSBuildThisFileDirectory)", "").replace("\\", "/")).resolve()


def imported_files(project: Path, visited: set[Path]):
	if project in visited:
		return
	visited.add(project)
	document = ET.parse(project)
	for item in document.findall("m:ItemGroup/*", NS):
		kind = item.tag.rsplit("}", 1)[-1]
		if kind in FILE_ITEMS:
			yield kind, local_path(project, item.attrib["Include"])
	for item in document.findall(".//m:Import", NS):
		value = item.attrib["Project"].replace("$(MSBuildThisFileDirectory)", "")
		if "$" not in value:
			path = local_path(project, value)
			if path.is_relative_to(ROOT) and path.is_file():
				yield from imported_files(path, visited)


class ProjectFilterTests(unittest.TestCase):
	def test_visible_files_are_owned_and_filtered_once_after_imports(self) -> None:
		for node in ET.parse(ROOT / "OpenGlass.slnx").getroot().findall("Project"):
			project = ROOT / node.attrib["Path"]
			with self.subTest(project=project.name):
				expected = list(imported_files(project, set()))
				self.assertTrue(all(path.is_relative_to(project.parent) for _, path in expected))
				self.assertTrue(all(count == 1 for count in Counter(path for _, path in expected).values()))
				if not expected:
					continue
				filters = ET.parse(str(project) + ".filters")
				declared = [item.attrib["Include"] for item in filters.findall("m:ItemGroup/m:Filter", NS)]
				self.assertEqual(len(declared), len(set(declared)))
				actual = []
				for item in filters.findall("m:ItemGroup/*", NS):
					kind = item.tag.rsplit("}", 1)[-1]
					if kind == "Filter":
						continue
					actual.append((kind, local_path(project, item.attrib["Include"])))
					category = item.findtext("m:Filter", namespaces=NS)
					self.assertIn(category, declared)
					if kind in {"ClCompile", "MASM", "FxCompile"}:
						self.assertEqual(category, "Source Files")
					if kind == "ClInclude":
						self.assertEqual(category, "Header Files")
				self.assertCountEqual(actual, expected)
