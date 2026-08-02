#!/usr/bin/env python3
"""Validate projection Symbol schemas and source-consumer completeness."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 3


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("repo", nargs="?", default=".")
	parser.add_argument("--architecture", choices=("legacy", "milcomp"), required=True)
	parser.add_argument("--format", choices=("text", "json"), default="text")
	parser.add_argument("--module", choices=("all", "udwm", "dwmcore"), default="all")
	parser.add_argument("--id", dest="stable_id", help="show one exact Symbol stable ID while still validating both module schemas")
	return parser.parse_args(argv)


def inspect(repo: Path, architecture: str, module_filter: str, stable_id_filter: str | None = None) -> dict[str, Any]:
	if not repo.is_dir():
		raise ValueError(f"repository path is not a directory: {repo}")
	sys.path.insert(0, str(repo / "Scripts"))
	try:
		import projection_codegen as codegen
	finally:
		sys.path.pop(0)
	constants = codegen.load_os_constants(repo)
	projection_root = repo / "OpenGlass" / "Architecture" / codegen.ARCHITECTURES[architecture]
	all_schemas = [codegen.validate_schema(repo, repo / "OpenGlass" / "ProjectionSchemas" / architecture / f"{module}.json", module, constants) for module in ("udwm", "dwmcore")]
	codegen.validate_source_completeness(projection_root, all_schemas)
	texts: list[str] = []
	paths = list((repo / "OpenGlass").glob("*")) + list(projection_root.rglob("*"))
	for path in paths:
		if path.is_file() and path.suffix.lower() in {".cpp", ".hpp", ".h"}:
			texts.append(codegen.strip_non_code(path.read_text(encoding="utf-8-sig")))
	source = "\n".join(texts)
	findings: list[dict[str, Any]] = []
	modules: list[dict[str, Any]] = []
	selected = {"udwm", "dwmcore"} if module_filter == "all" else {module_filter}
	for schema in all_schemas:
		if schema["module"] not in selected:
			continue
		descriptors: list[dict[str, Any]] = []
		for index, symbol in enumerate(schema["symbols"]):
			consumer_count = source.count(symbol["name"])
			callsite_count = None
			if symbol["kind"] == "projected_function":
				callsite_count, consumer_count = codegen.projected_function_consumer_counts(source, symbol)
			if symbol["kind"] == "raw" and not symbol.get("diagnostic_only") and not consumer_count:
				findings.append({"severity": "error", "module": schema["module"], "id": symbol["id"], "message": "raw Symbol has no source consumer"})
			descriptor = {
				"index": index,
				"name": symbol["name"],
				"id": symbol["id"],
				"symbol_names": symbol["symbol_names"],
				"candidate_count": len(symbol["symbol_names"]),
				"multiple_complete_names": len(symbol["symbol_names"]) > 1,
				"kind": symbol["kind"],
				"type": symbol.get("type"),
				"abi_compatibility": symbol.get("abi_compatibility"),
				"usage": symbol.get("usage"),
				"requirement": symbol["requirement"],
				"min_inclusive": symbol.get("min_inclusive"),
				"max_exclusive": symbol.get("max_exclusive"),
				"consumer_count": consumer_count,
				"callsite_count": callsite_count,
			}
			if stable_id_filter is None or symbol["id"] == stable_id_filter:
				descriptors.append(descriptor)
		modules.append({"module": schema["module"], "schema": f"OpenGlass/ProjectionSchemas/{architecture}/{schema['module']}.json", "descriptors": descriptors})
	if stable_id_filter is not None and not any(module["descriptors"] for module in modules):
		raise ValueError(f"Symbol stable ID not found in selected module schema: {stable_id_filter}")
	return {"schema_version": SCHEMA_VERSION, "architecture": architecture, "requested_id": stable_id_filter, "modules": modules, "findings": findings, "summary": {"symbols": sum(len(item["descriptors"]) for item in modules), "errors": sum(item["severity"] == "error" for item in findings)}}


def main(argv: list[str] | None = None) -> int:
	args = parse_args(argv)
	try:
		result = inspect(Path(args.repo).resolve(), args.architecture, args.module, args.stable_id)
	except (ValueError, OSError, UnicodeError, json.JSONDecodeError) as error:
		print(json.dumps({"schema_version": SCHEMA_VERSION, "error": str(error)}, indent=2) if args.format == "json" else f"ERROR: {error}", file=sys.stderr)
		return 2
	except Exception as error:
		print(json.dumps({"schema_version": SCHEMA_VERSION, "error": str(error)}, indent=2) if args.format == "json" else f"ERROR: {error}", file=sys.stderr)
		return 1
	if args.format == "json":
		print(json.dumps(result, indent=2, ensure_ascii=False))
	else:
		print(f"Symbol schemas ({result['architecture']}): {result['summary']['symbols']} descriptors, {result['summary']['errors']} errors")
		for module in result["modules"]:
			print(f"{module['module']}: {len(module['descriptors'])} descriptors")
			for descriptor in module["descriptors"]:
				if result.get("requested_id"):
					print(f"  {descriptor['id']} [{descriptor['requirement']}, {descriptor['kind']}]")
					for name in descriptor["symbol_names"]:
						print(f"    {name}")
		for finding in result["findings"]:
			print(f"{finding['severity'].upper()}: {finding['module']}:{finding['id']}: {finding['message']}")
	return 1 if result["summary"]["errors"] else 0


if __name__ == "__main__":
	raise SystemExit(main())
