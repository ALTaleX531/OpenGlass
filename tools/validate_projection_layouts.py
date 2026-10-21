#!/usr/bin/env python3
"""Validate and query projection Layout schemas with integer byte offsets."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from tools import projection_schema, projection_target
from tools.projection_schema import Version


SCHEMA_VERSION = projection_schema.SCHEMA_VERSION


class InputError(Exception):
	pass


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("repo", nargs="?", default=".")
	parser.add_argument("--architecture", choices=("legacy", "milcomp"), required=True)
	projection_target.add_platform_argument(parser)
	parser.add_argument("--format", choices=("text", "json"), default="text")
	parser.add_argument("--version")
	parser.add_argument("--module", choices=("all", "udwm", "dwmcore"), default="all")
	parser.add_argument("--id", dest="stable_id", help="show one exact Layout stable ID while still validating the selected schema")
	return parser.parse_args(argv)


def inspect(repo: Path, architecture: str, requested: Version | None, module_filter: str, stable_id_filter: str | None = None, platform: str = "x64") -> dict[str, Any]:
	modules: list[dict[str, Any]] = []
	findings: list[dict[str, Any]] = []
	module_names = ("udwm", "dwmcore") if module_filter == "all" else (module_filter,)
	for module in module_names:
		relative = projection_target.schema_path(repo, architecture, module).relative_to(repo).as_posix()
		path = repo / relative
		try:
			schema = projection_schema.validate_schema(path, module, platform)
			locations = projection_schema.section_locations(path, "layouts", platform)
		except projection_schema.SchemaError as error:
			findings.append({"severity": "error", "file": relative, "line": 1, "column": 1, "table": None, "entry": None, "message": str(error)})
			modules.append({"module": module, "schema": relative, "tables": []})
			continue
		except (OSError, UnicodeError) as error:
			raise InputError(f"cannot read {relative}: {error}") from error
		tables: list[dict[str, Any]] = []
		module_minimum = Version(**schema["min_inclusive"]) if schema.get("min_inclusive") else None
		module_maximum = Version(**schema["max_exclusive"]) if schema.get("max_exclusive") else None
		module_supported = requested is None or projection_schema.in_range(requested, schema)
		for table in schema["layouts"]:
			stable_id = table["id"]
			source, line, column = locations[table["name"]]
			source_relative = source.relative_to(repo).as_posix()
			notes = table.get("notes")
			entries: list[dict[str, Any]] = []
			left: Version | None = None
			terminal = False
			for index, case in enumerate(table["cases"]):
				boundary: Version | None = None
				if case.get("otherwise") is True:
					terminal = True
				else:
					until = case["until"]
					boundary = Version(until["build"], until.get("revision", 0))
				entries.append({"entry": index, "offset": case["offset"], "left_inclusive": left.json() if left else None, "right_exclusive": boundary.json() if boundary else None})
				left = boundary
			selection: dict[str, Any] | None = None
			if requested is not None:
				if not module_supported:
					selection = {"status": "module_unsupported"}
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
				tables.append({"id": stable_id, "name": table.get("name"), "kind": table.get("kind"), "type": table.get("type"), "notes": notes if isinstance(notes, str) else None, "file": source_relative, "line": line, "column": column, "valid": True, "terminal": terminal, "entries": entries, "selection": selection})
		modules.append({
			"module": module,
			"schema": relative,
			"min_inclusive": module_minimum.json() if module_minimum else None,
			"max_exclusive": module_maximum.json() if module_maximum else None,
			"version_supported": module_supported,
			"tables": tables,
		})
	if stable_id_filter is not None and not findings and not any(module["tables"] for module in modules):
		raise InputError(f"Layout stable ID not found in selected module schema: {stable_id_filter}")
	return {"schema_version": SCHEMA_VERSION, "architecture": architecture, "platform": platform, "source_shape": "generated-projection-schema", "requested_version": requested.json() if requested else None, "requested_id": stable_id_filter, "modules": modules, "findings": findings, "summary": {"tables": sum(len(item["tables"]) for item in modules), "errors": sum(item["severity"] == "error" for item in findings)}}


def print_text(result: dict[str, Any]) -> None:
	def endpoint(value: dict[str, int] | None, *, left: bool) -> str:
		if value is None:
			return "-infinity" if left else "+infinity"
		return f"{value['build']}.{value['revision']}"

	print(f"Layout schemas ({result['platform']}/{result['architecture']}): {result['summary']['tables']} tables, {result['summary']['errors']} errors")
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
					print(f"  {table['id']}: entry {selection['entry']} = {selection['offset']} bytes [{left}, {right})")
				elif selection["status"] != "matched":
					print(f"  {table['id']}: {selection['status']}")
	for finding in result["findings"]:
		print(f"{finding['severity'].upper()}: {finding['file']}:{finding['line']}:{finding['column']}: {finding['message']}")


def main(argv: list[str] | None = None) -> int:
	args = parse_args(argv)
	try:
		result = inspect(Path(args.repo).resolve(), args.architecture, Version.parse(args.version) if args.version else None, args.module, args.stable_id, args.platform)
	except (InputError, projection_schema.SchemaError) as error:
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
