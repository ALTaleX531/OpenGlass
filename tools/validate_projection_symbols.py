#!/usr/bin/env python3
"""Validate projection Symbol schemas and source-consumer completeness."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from tools import projection_schema, projection_source_check, projection_target


SCHEMA_VERSION = 3


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("repo", nargs="?", default=".")
	parser.add_argument("--architecture", choices=("legacy", "milcomp"), required=True)
	projection_target.add_platform_argument(parser)
	parser.add_argument("--format", choices=("text", "json"), default="text")
	parser.add_argument("--module", choices=("all", "udwm", "dwmcore"), default="all")
	parser.add_argument("--id", dest="stable_id", help="show one exact Symbol stable ID while still validating both module schemas")
	return parser.parse_args(argv)


def _inspect_modules(repo: Path, architecture: str, module_filter: str, stable_id_filter: str | None, platform: str) -> list[dict[str, Any]]:
	projection_root = repo / "OpenGlass" / "Architecture" / projection_schema.ARCHITECTURES[architecture]
	all_schemas = [
		projection_schema.validate_schema(
			projection_target.schema_path(repo, architecture, module),
			module, platform,
		)
		for module in projection_schema.MODULES
	]
	source = projection_source_check.validate_source_completeness(projection_root, all_schemas, platform)
	modules: list[dict[str, Any]] = []
	selected = {"udwm", "dwmcore"} if module_filter == "all" else {module_filter}
	for schema in all_schemas:
		if schema["module"] not in selected:
			continue
		manifest = projection_target.schema_path(repo, architecture, schema["module"])
		locations = projection_schema.section_locations(manifest, "symbols", platform)
		descriptors: list[dict[str, Any]] = []
		for index, symbol in enumerate(schema["symbols"]):
			consumer_count = source.count(symbol["name"])
			callsite_count = None
			if symbol["kind"] == "projected_function":
				callsite_count, consumer_count = projection_source_check.projected_function_consumer_counts(source, symbol)
			bindings = symbol["bindings"]
			symbol_names = [
				candidate
				for binding in bindings
				for candidate in binding["symbol_names"]
			]
			descriptor = {
				"file": locations[symbol["name"]][0].relative_to(repo).as_posix(),
				"line": locations[symbol["name"]][1],
				"column": locations[symbol["name"]][2],
				"index": index,
				"name": symbol["name"],
				"id": symbol["id"],
				"bindings": bindings,
				"binding_count": len(bindings),
				"symbol_names": symbol_names,
				"candidate_count": len(symbol_names),
				"multiple_complete_names": len(symbol_names) > 1,
				"kind": symbol["kind"],
				"type": symbol.get("type"),
				"abi_compatibility": symbol.get("abi_compatibility"),
				"usage": symbol.get("usage"),
				"requirement": symbol["requirement"],
				"consumer_count": consumer_count,
				"callsite_count": callsite_count,
			}
			if stable_id_filter is None or symbol["id"] == stable_id_filter:
				descriptors.append(descriptor)
		modules.append({
			"module": schema["module"],
			"schema": manifest.relative_to(repo).as_posix(),
			"min_inclusive": schema.get("min_inclusive"),
			"max_exclusive": schema.get("max_exclusive"),
			"descriptors": descriptors,
		})
	if stable_id_filter is not None and not any(module["descriptors"] for module in modules):
		raise ValueError(f"Symbol stable ID not found in selected module schema: {stable_id_filter}")
	return modules


def inspect(repo: Path, architecture: str, module_filter: str, stable_id_filter: str | None = None, platform: str = "x64") -> dict[str, Any]:
	findings: list[dict[str, Any]] = []
	try:
		modules = _inspect_modules(repo, architecture, module_filter, stable_id_filter, platform)
	except projection_schema.SchemaError as error:
		modules = []
		findings.append({"severity": "error", "message": str(error)})
	return {"schema_version": SCHEMA_VERSION, "architecture": architecture, "platform": platform, "requested_id": stable_id_filter, "modules": modules, "findings": findings, "summary": {"symbols": sum(len(item["descriptors"]) for item in modules), "errors": sum(item["severity"] == "error" for item in findings)}}


def main(argv: list[str] | None = None) -> int:
	args = parse_args(argv)
	try:
		result = inspect(Path(args.repo).resolve(), args.architecture, args.module, args.stable_id, args.platform)
	except (ValueError, OSError, UnicodeError, json.JSONDecodeError) as error:
		print(json.dumps({"schema_version": SCHEMA_VERSION, "error": str(error)}, indent=2) if args.format == "json" else f"ERROR: {error}", file=sys.stderr)
		return 2
	except Exception as error:
		print(json.dumps({"schema_version": SCHEMA_VERSION, "error": str(error)}, indent=2) if args.format == "json" else f"ERROR: {error}", file=sys.stderr)
		return 1
	if args.format == "json":
		print(json.dumps(result, indent=2, ensure_ascii=False))
	else:
		print(f"Symbol schemas ({result['platform']}/{result['architecture']}): {result['summary']['symbols']} descriptors, {result['summary']['errors']} errors")
		for module in result["modules"]:
			print(f"{module['module']}: {len(module['descriptors'])} descriptors")
			for descriptor in module["descriptors"]:
				if result.get("requested_id"):
					print(f"  {descriptor['id']} [{descriptor['requirement']}, {descriptor['kind']}]")
					for binding_index, binding in enumerate(descriptor["bindings"]):
						minimum = binding["min_inclusive"]
						maximum = binding["max_exclusive"]
						minimum_text = "*" if minimum is None else f"{minimum['build']}.{minimum.get('revision', 0)}"
						maximum_text = "*" if maximum is None else f"{maximum['build']}.{maximum.get('revision', 0)}"
						print(f"    binding {binding_index}: [{minimum_text}, {maximum_text})")
						for name in binding["symbol_names"]:
							print(f"      {name}")
		for finding in result["findings"]:
			print(f"{finding['severity'].upper()}: {finding['message']}")
	return 1 if result["summary"]["errors"] else 0


if __name__ == "__main__":
	raise SystemExit(main())
