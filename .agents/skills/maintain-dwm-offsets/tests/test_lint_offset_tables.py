from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "scripts" / "lint_offset_tables.py"
SPEC = importlib.util.spec_from_file_location("lint_offset_tables", SCRIPT)
assert SPEC and SPEC.loader
LINTER = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = LINTER
SPEC.loader.exec_module(LINTER)


OS_HELPER = """\
namespace os {
enum os_build : ULONG {
    build_first = 100,
    build_alias = build_first,
    build_second = 200,
};
enum os_revision : ULONG {
    revision_first = 10,
    revision_second = 20,
};
}
"""


def table_source(name: str, entries: str, marker: str = "") -> str:
    return f"""\
#pragma once
// Comment fake: struct Fake_Offsets {{ Util::OffsetInfo{{ .offset=1, .build=0, .revision=0 }}; }}
constexpr auto text = "{{ Util::OffsetInfo fake }}";
{marker}
struct {name}
{{
    consteval static auto operator()()
    {{
        return std::array{{
{entries}
        }};
    }}
}};
"""


class RepoFixture:
    def __init__(self, test: unittest.TestCase, first_entries: str, *, marker: str = "legacy") -> None:
        self._temp = tempfile.TemporaryDirectory()
        test.addCleanup(self._temp.cleanup)
        self.root = Path(self._temp.name)
        source = self.root / "OpenGlass"
        source.mkdir()
        (source / "OSHelper.hpp").write_text(OS_HELPER, encoding="utf-8")
        first_name = "CVisual_GetScale_MilSizeD_Offsets" if marker == "legacy" else "CVisual_GetScale_Offsets"
        (source / "dwmcoreProjection.Offsets.hpp").write_text(
            table_source(first_name, first_entries), encoding="utf-8"
        )
        (source / "uDwmProjection.Offsets.hpp").write_text(
            table_source(
                "Other_Offsets",
                "                Util::OffsetInfo{ .offset = 8, .build = 0, .revision = 0 }\n",
            ),
            encoding="utf-8",
        )
        (source / "dwmcoreProjection.hpp").write_text(
            f"PointerExecuteUnsafe<{first_name}, Method>();\n", encoding="utf-8"
        )
        (source / "uDwmProjection.hpp").write_text(
            "PointerExecuteUnsafe<Other_Offsets, Method>();\n", encoding="utf-8"
        )

    def inspect(self, version: LINTER.Version | None = None) -> dict:
        return LINTER.inspect_repo(self.root, version)


class VersionSemanticsTests(unittest.TestCase):
    def test_version_before_matches_cpp_rules(self) -> None:
        self.assertTrue(LINTER.version_before(LINTER.Version(199, 999), LINTER.Version(200, 0)))
        self.assertFalse(LINTER.version_before(LINTER.Version(200, 0), LINTER.Version(200, 0)))
        self.assertTrue(LINTER.version_before(LINTER.Version(200, 9), LINTER.Version(200, 10)))
        self.assertFalse(LINTER.version_before(LINTER.Version(200, 10), LINTER.Version(200, 10)))
        self.assertTrue(LINTER.version_before(LINTER.Version(999, 999), LINTER.Version(0, 0)))

    def test_requested_version_is_strict(self) -> None:
        self.assertEqual(LINTER.parse_requested_version("26100.4202"), LINTER.Version(26100, 4202))
        for invalid in ("26100", "26100.", " 26100.4202", "26100.4202.1", "-1.0"):
            with self.subTest(invalid=invalid), self.assertRaises(LINTER.InputError):
                LINTER.parse_requested_version(invalid)

    def test_unsigned_literals(self) -> None:
        self.assertEqual(LINTER.parse_uint_literal("00010u"), 10)
        self.assertEqual(LINTER.parse_uint_literal("0x20UL"), 32)
        self.assertIsNone(LINTER.parse_uint_literal("0x100000000"))


class RepositoryLintTests(unittest.TestCase):
    def test_legacy_inventory_and_exact_boundary_selection(self) -> None:
        fixture = RepoFixture(
            self,
            """\
                Util::OffsetInfo{ .offset = 17 * sizeof(ULONG_PTR), .build = os::build_second, .revision = 0 },
                Util::OffsetInfo{ .offset = 24, .build = 0, .revision = 0 }
""",
        )
        before = fixture.inspect(LINTER.Version(199, 999))
        at_boundary = fixture.inspect(LINTER.Version(200, 0))
        self.assertEqual(before["status"], "ok")
        self.assertEqual(before["branch_shape"], "legacy")
        self.assertEqual(before["tables"][0]["selection"]["offset_expression"], "17 * sizeof(ULONG_PTR)")
        self.assertEqual(before["tables"][0]["selection"]["interval"]["right"], {"build": 200, "revision": 0})
        self.assertEqual(at_boundary["tables"][0]["selection"]["entry"], 2)
        self.assertIsNone(at_boundary["tables"][0]["selection"]["interval"]["right"])

    def test_same_build_revision_boundaries(self) -> None:
        fixture = RepoFixture(
            self,
            """\
                Util::OffsetInfo{ .offset = 1, .build = os::build_second, .revision = 0 },
                Util::OffsetInfo{ .offset = 2, .build = os::build_second, .revision = os::revision_second },
                Util::OffsetInfo{ .offset = 3, .build = 0, .revision = 0 }
""",
        )
        self.assertEqual(fixture.inspect(LINTER.Version(199, 99))["tables"][0]["selection"]["entry"], 1)
        self.assertEqual(fixture.inspect(LINTER.Version(200, 19))["tables"][0]["selection"]["entry"], 2)
        self.assertEqual(fixture.inspect(LINTER.Version(200, 20))["tables"][0]["selection"]["entry"], 3)

    def test_missing_terminal_is_supported_structure(self) -> None:
        fixture = RepoFixture(
            self,
            "                Util::OffsetInfo{ .offset = 1, .build = os::build_second, .revision = 0 }\n",
        )
        result = fixture.inspect(LINTER.Version(300, 0))
        self.assertEqual(result["status"], "ok")
        self.assertEqual(result["tables"][0]["selection"]["status"], "unsupported")

    def test_milcomp_shape(self) -> None:
        fixture = RepoFixture(
            self,
            "                Util::OffsetInfo{ .offset = 1, .build = 0, .revision = 0 }\n",
            marker="milcomp",
        )
        self.assertEqual(fixture.inspect()["branch_shape"], "milcomp")

    def assert_lint_error(self, entries: str, message: str) -> None:
        result = RepoFixture(self, entries).inspect()
        self.assertEqual(result["status"], "error")
        self.assertTrue(any(message in item["message"] for item in result["findings"]), result["findings"])

    def test_boundary_failures(self) -> None:
        cases = (
            (
                """\
                Util::OffsetInfo{ .offset = 1, .build = os::build_second, .revision = 0 },
                Util::OffsetInfo{ .offset = 2, .build = os::build_second, .revision = 0 }
""",
                "duplicate boundary",
            ),
            (
                """\
                Util::OffsetInfo{ .offset = 1, .build = os::build_second, .revision = 0 },
                Util::OffsetInfo{ .offset = 2, .build = os::build_first, .revision = 0 }
""",
                "does not follow",
            ),
            (
                """\
                Util::OffsetInfo{ .offset = 1, .build = 0, .revision = 0 },
                Util::OffsetInfo{ .offset = 2, .build = os::build_second, .revision = 0 }
""",
                "terminal entry is not final",
            ),
            (
                "                Util::OffsetInfo{ .offset = 1, .build = 0, .revision = os::revision_first }\n",
                "terminal build 0 must use revision 0",
            ),
        )
        for entries, message in cases:
            with self.subTest(message=message):
                self.assert_lint_error(entries, message)

    def test_field_and_expression_failures(self) -> None:
        cases = (
            ("                Util::OffsetInfo{ .offset = 1, .build = 0 }\n", "missing field"),
            (
                "                Util::OffsetInfo{ .offset = 1, .offset = 2, .build = 0, .revision = 0 }\n",
                "duplicate field",
            ),
            (
                "                Util::OffsetInfo{ .offset = 1, .build = 0, .revision = 0, .extra = 3 }\n",
                "unexpected field",
            ),
            (
                "                Util::OffsetInfo{ .offset = 1, .build = os::missing, .revision = 0 }\n",
                "cannot be resolved safely",
            ),
            (
                "                Util::OffsetInfo{ .offset = 1, .build = 100 + 1, .revision = 0 }\n",
                "cannot be resolved safely",
            ),
        )
        for entries, message in cases:
            with self.subTest(message=message):
                self.assert_lint_error(entries, message)

    def test_comments_literals_and_bom_do_not_create_tables(self) -> None:
        fixture = RepoFixture(
            self,
            "                Util::OffsetInfo{ .offset = 1, .build = 0, .revision = 0 }\n",
        )
        path = fixture.root / "OpenGlass" / "dwmcoreProjection.Offsets.hpp"
        path.write_text("\ufeff" + path.read_text(encoding="utf-8").replace("\n", "\r\n"), encoding="utf-8")
        result = fixture.inspect()
        self.assertEqual(result["summary"]["tables"], 2)
        self.assertEqual(result["status"], "ok")

    def test_offset_expression_is_never_evaluated(self) -> None:
        expression = "dangerous_call() + 17 * sizeof(ULONG_PTR)"
        fixture = RepoFixture(
            self,
            f"                Util::OffsetInfo{{ .offset = {expression}, .build = 0, .revision = 0 }}\n",
        )
        result = fixture.inspect(LINTER.Version(500, 0))
        self.assertEqual(result["status"], "ok")
        self.assertEqual(result["tables"][0]["selection"]["offset_expression"], expression)

    def test_enum_alias_is_resolved(self) -> None:
        fixture = RepoFixture(
            self,
            """\
                Util::OffsetInfo{ .offset = 1, .build = os::build_alias, .revision = 0 },
                Util::OffsetInfo{ .offset = 2, .build = 0, .revision = 0 }
""",
        )
        result = fixture.inspect(LINTER.Version(100, 0))
        self.assertEqual(result["tables"][0]["selection"]["entry"], 2)

    def test_parser_completeness_failures(self) -> None:
        fixture = RepoFixture(
            self,
            "                Util::OffsetInfo{ .offset = 1, .build = 0, .revision = 0 }\n",
        )
        projection = fixture.root / "OpenGlass" / "dwmcoreProjection.Offsets.hpp"
        projection.write_text(
            projection.read_text(encoding="utf-8")
            + "\nUtil::OffsetInfo{ .offset = 2, .build = 0, .revision = 0 };\n",
            encoding="utf-8",
        )
        result = fixture.inspect()
        self.assertTrue(any("outside a parsed offset table" in item["message"] for item in result["findings"]))

        duplicate = RepoFixture(
            self,
            "                Util::OffsetInfo{ .offset = 1, .build = 0, .revision = 0 }\n",
        )
        first = duplicate.root / "OpenGlass" / "dwmcoreProjection.Offsets.hpp"
        second = duplicate.root / "OpenGlass" / "uDwmProjection.Offsets.hpp"
        second.write_text(first.read_text(encoding="utf-8"), encoding="utf-8")
        result = duplicate.inspect()
        self.assertTrue(any("duplicate table definition" in item["message"] for item in result["findings"]))

        multiple_arrays = RepoFixture(
            self,
            "                Util::OffsetInfo{ .offset = 1, .build = 0, .revision = 0 }\n",
        )
        projection = multiple_arrays.root / "OpenGlass" / "dwmcoreProjection.Offsets.hpp"
        projection.write_text(
            projection.read_text(encoding="utf-8").replace(
                "return std::array{",
                "auto ignored = std::array{ Util::OffsetInfo{ .offset = 9, .build = 0, .revision = 0 } };\n        return std::array{",
            ),
            encoding="utf-8",
        )
        result = multiple_arrays.inspect()
        self.assertTrue(any("exactly one std::array" in item["message"] for item in result["findings"]))

    def test_consumer_without_definition(self) -> None:
        fixture = RepoFixture(
            self,
            "                Util::OffsetInfo{ .offset = 1, .build = 0, .revision = 0 }\n",
        )
        consumer = fixture.root / "OpenGlass" / "dwmcoreProjection.hpp"
        consumer.write_text("PointerExecuteUnsafe<Missing_Offsets, Method>();\n", encoding="utf-8")
        result = fixture.inspect()
        self.assertTrue(any("consumer has no offset table" in item["message"] for item in result["findings"]))


class CliTests(unittest.TestCase):
    def test_json_contract_and_exit_codes(self) -> None:
        fixture = RepoFixture(
            self,
            "                Util::OffsetInfo{ .offset = 1, .build = 0, .revision = 0 }\n",
        )
        completed = subprocess.run(
            [sys.executable, str(SCRIPT), str(fixture.root), "--version", "26100.4202", "--format", "json"],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        payload = json.loads(completed.stdout)
        self.assertEqual(payload["schema_version"], 1)
        self.assertEqual(payload["requested_version"], {"build": 26100, "revision": 4202})
        self.assertEqual(payload["tables"][0]["selection"]["entry"], 1)

        invalid = subprocess.run(
            [sys.executable, str(SCRIPT), str(fixture.root), "--version", "26100", "--format", "json"],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(invalid.returncode, 2)
        self.assertEqual(json.loads(invalid.stdout)["status"], "input_error")

    def test_current_checkout_smoke(self) -> None:
        repo = Path(__file__).parents[4]
        completed = subprocess.run(
            [sys.executable, str(SCRIPT), str(repo)], check=False, capture_output=True, text=True
        )
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)


if __name__ == "__main__":
    unittest.main()
