#!/usr/bin/env python3
"""Validate and query projection Layout schemas without evaluating offset expressions."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


VERSION_RE = re.compile(r"(?:10\.0\.)?([0-9]+)\.([0-9]+)\Z")

TOOL_ROOT = Path(__file__).parents[4]
sys.path.insert(0, str(TOOL_ROOT / "Scripts"))
try:
	import projection_schema
	import projection_source_check
finally:
	sys.path.pop(0)

SCHEMA_VERSION = projection_schema.SCHEMA_VERSION


class InputError(Exception):
	pass


@dataclass(frozen=True, order=True)
class Version:
	build: int
	revision: int

	def json(self) -> dict[str, int]:
		return {"build": self.build, "revision": self.revision}


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("repo", nargs="?", default=".")
	parser.add_argument("--architecture", choices=("legacy", "milcomp"), required=True)
	parser.add_argument("--format", choices=("text", "json"), default="text")
	parser.add_argument("--version")
	parser.add_argument("--module", choices=("all", "udwm", "dwmcore"), default="all")
	parser.add_argument("--id", dest="stable_id", help="show one exact Layout stable ID while still validating the selected schema")
	return parser.parse_args(argv)


def parse_requested(raw: str | None) -> Version | None:
	if raw is None:
		return None
	match = VERSION_RE.fullmatch(raw)
	if not match:
		raise InputError("--version must use BUILD.REVISION or 10.0.BUILD.REVISION with unsigned decimal integers")
	values = tuple(int(item) for item in match.groups())
	if not values[0] or any(item > 0xFFFFFFFF for item in values):
		raise InputError("--version is outside the supported unsigned range")
	return Version(*values)


def location(text: str, stable_id: str) -> tuple[int, int]:
	needle = json.dumps(stable_id)
	position = text.find(needle)
	if position < 0:
		return (1, 1)
	line = text.count("\n", 0, position) + 1
	column = position - text.rfind("\n", 0, position)
	return (line, column)


def inspect(repo: Path, architecture: str, requested: Version | None, module_filter: str, stable_id_filter: str | None = None) -> dict[str, Any]:
	if not repo.is_dir():
		raise InputError(f"repository path is not a directory: {repo}")
	constants = projection_source_check.load_os_constants(repo)
	modules: list[dict[str, Any]] = []
	findings: list[dict[str, Any]] = []
	module_names = ("udwm", "dwmcore") if module_filter == "all" else (module_filter,)
	seen_ids: dict[str, str] = {}
	for module in module_names:
		relative = f"OpenGlass/ProjectionSchemas/{architecture}/{module}.json"
		path = repo / relative
		try:
			text = path.read_text(encoding="utf-8")
			schema = projection_schema.validate_schema(path, module, constants)
		except projection_schema.SchemaError as error:
			findings.append({"severity": "error", "file": relative, "line": 1, "column": 1, "table": None, "entry": None, "message": str(error)})
			modules.append({"module": module, "schema": relative, "tables": []})
			continue
		except (OSError, UnicodeError) as error:
			raise InputError(f"cannot read {relative}: {error}") from error
		tables: list[dict[str, Any]] = []
		module_minimum_raw = projection_schema.parse_version(schema.get("min_inclusive"), f"{relative}: min_inclusive")
		module_maximum_raw = projection_schema.parse_version(schema.get("max_exclusive"), f"{relative}: max_exclusive")
		module_minimum = Version(module_minimum_raw.build, module_minimum_raw.revision) if module_minimum_raw.build else None
		module_maximum = Version(module_maximum_raw.build, module_maximum_raw.revision) if module_maximum_raw.build else None
		module_supported = (
			requested is None or
			(module_minimum is None or requested >= module_minimum) and
			(module_maximum is None or requested < module_maximum)
		)
		for table in schema.get("layouts", []):
			stable_id = table.get("id")
			line, column = location(text, str(stable_id))
			valid = True
			notes = table.get("notes")
			if stable_id in seen_ids:
				findings.append({"severity": "error", "file": relative, "line": line, "column": column, "table": stable_id, "entry": None, "message": f"duplicate Layout stable ID; first declared in {seen_ids[stable_id]}"})
				valid = False
			elif isinstance(stable_id, str):
				seen_ids[stable_id] = relative
			entries: list[dict[str, Any]] = []
			left: Version | None = None
			terminal = False
			for index, case in enumerate(table.get("cases", [])):
				boundary: Version | None = None
				if case.get("otherwise") is True:
					terminal = True
				else:
					until = case["until"]
					boundary = Version(
						projection_schema.resolve_boundary_expression(until.get("build"), constants, f"{stable_id}[{index}].build", build=True),
						projection_schema.resolve_boundary_expression(until.get("revision", "0"), constants, f"{stable_id}[{index}].revision", build=False),
					)
				entries.append({"entry": index, "offset_expression": case.get("offset"), "left_inclusive": left.json() if left else None, "right_exclusive": boundary.json() if boundary else None})
				left = boundary
			selection: dict[str, Any] | None = None
			if requested is not None:
				if not module_supported:
					selection = {"status": "module_unsupported"}
				elif not valid:
					selection = {"status": "invalid_table"}
				else:
					matched = next((entry for entry in entries if entry["right_exclusive"] is None or requested < Version(**entry["right_exclusive"])), None)
					if matched:
						selection = {"status": "matched", **matched}
						left_boundary = Version(**matched["left_inclusive"]) if matched["left_inclusive"] else None
						right_boundary = Version(**matched["right_exclusive"]) if matched["right_exclusive"] else None
						if module_minimum is not None and (left_boundary is None or left_boundary < module_minimum):
							selection["left_inclusive"] = module_minimum.json()
						if module_maximum is not None and (right_boundary is None or module_maximum < right_boundary):
							selection["right_exclusive"] = module_maximum.json()
					else:
						selection = {
							"status": "unsupported",
							"left_inclusive": entries[-1]["right_exclusive"] if entries else None,
							"right_exclusive": module_maximum.json() if module_maximum else None,
						}
			if stable_id_filter is None or stable_id == stable_id_filter:
				tables.append({"id": stable_id, "name": table.get("name"), "kind": table.get("kind"), "type": table.get("type"), "notes": notes if isinstance(notes, str) else None, "file": relative, "line": line, "column": column, "valid": valid, "terminal": terminal, "entries": entries, "selection": selection})
		modules.append({
			"module": module,
			"schema": relative,
			"min_inclusive": module_minimum.json() if module_minimum else None,
			"max_exclusive": module_maximum.json() if module_maximum else None,
			"version_supported": module_supported,
			"tables": tables,
		})
	if stable_id_filter is not None and not any(module["tables"] for module in modules):
		raise InputError(f"Layout stable ID not found in selected module schema: {stable_id_filter}")
	return {"schema_version": SCHEMA_VERSION, "architecture": architecture, "source_shape": "generated-projection-schema", "requested_version": requested.json() if requested else None, "requested_id": stable_id_filter, "modules": modules, "findings": findings, "summary": {"tables": sum(len(item["tables"]) for item in modules), "errors": sum(item["severity"] == "error" for item in findings)}}


def print_text(result: dict[str, Any]) -> None:
	def endpoint(value: dict[str, int] | None, *, left: bool) -> str:
		if value is None:
			return "-infinity" if left else "+infinity"
		return f"{value['build']}.{value['revision']}"

	print(f"Layout schemas ({result['architecture']}): {result['summary']['tables']} tables, {result['summary']['errors']} errors")
	for module in result["modules"]:
		print(f"{module['module']}: {len(module['tables'])} tables")
		if result.get("requested_id"):
			for table in module["tables"]:
				if table.get("notes"):
					print("  Reverse-engineering notes:")
					for line in table["notes"].splitlines():
						print(f"    {line}")
		if result["requested_version"]:
			for table in module["tables"]:
				selection = table["selection"]
				if selection["status"] == "matched":
					left = endpoint(selection["left_inclusive"], left=True)
					right = endpoint(selection["right_exclusive"], left=False)
					print(f"  {table['id']}: entry {selection['entry']} = {selection['offset_expression']} [{left}, {right})")
				elif selection["status"] != "matched":
					print(f"  {table['id']}: {selection['status']}")
	for finding in result["findings"]:
		print(f"{finding['severity'].upper()}: {finding['file']}:{finding['line']}:{finding['column']}: {finding['message']}")


def main(argv: list[str] | None = None) -> int:
	args = parse_args(argv)
	try:
		result = inspect(Path(args.repo).resolve(), args.architecture, parse_requested(args.version), args.module, args.stable_id)
	except InputError as error:
		if args.format == "json":
			print(json.dumps({"schema_version": SCHEMA_VERSION, "error": str(error)}, indent=2))
		else:
			print(f"ERROR: {error}", file=sys.stderr)
		return 2
	except Exception as error:
		if args.format == "json":
			print(json.dumps({"schema_version": SCHEMA_VERSION, "internal_error": str(error)}, indent=2))
		else:
			print(f"INTERNAL ERROR: {error}", file=sys.stderr)
		return 3
	if args.format == "json":
		print(json.dumps(result, indent=2, ensure_ascii=False))
	else:
		print_text(result)
	return 1 if result["summary"]["errors"] else 0


if __name__ == "__main__":
	raise SystemExit(main())
