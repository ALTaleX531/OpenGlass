#!/usr/bin/env python3
"""Validate OpenGlass projection metadata and publish generated C++ files."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from tools import file_io
from tools import projection_emit
from tools import projection_schema
from tools import projection_source_check
from tools import symbol_catalog
from tools import projection_target


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--architecture", choices=projection_schema.ARCHITECTURES, required=True)
    projection_target.add_platform_argument(parser)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true", help="validate without writing generated files")
    return parser.parse_args(argv)


def generate(repo: Path, architecture: str, platform: str = "x64") -> dict[str, str]:
    """Validate one CPU/backend inventory and return its complete generated output set."""
    repo = repo.resolve()
    projection_root = (
        repo
        / "OpenGlass"
        / "Architecture"
        / projection_schema.ARCHITECTURES[architecture]
    )
    schemas = [
        projection_schema.validate_schema(
            projection_target.schema_path(repo, architecture, module),
            module, platform,
        )
        for module in projection_schema.MODULES
    ]
    projection_source_check.validate_source_completeness(projection_root, schemas, platform)
    generated = projection_emit.generate_files(schemas)
    catalog = symbol_catalog.load_catalog_sources(
        projection_target.catalog_root(repo, architecture, platform) / architecture / "index.json",
        architecture,
        schemas,
    )
    generated.update(symbol_catalog.generate_files(catalog))
    return generated


def publish(output: Path, generated: dict[str, str]) -> None:
    """Publish a complete generated mapping to the selected intermediate path."""
    for name, content in generated.items():
        file_io.write_bytes(output / name, content.encode("utf-8"))


def run(repo: Path, architecture: str, output: Path, check: bool, platform: str = "x64") -> None:
    generated = generate(repo, architecture, platform)
    if not check:
        publish(output.resolve(), generated)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        run(args.repo, args.architecture, args.output, args.check, args.platform)
    except (projection_schema.SchemaError, OSError) as error:
        print(f"projection_codegen: error: {error}", file=sys.stderr)
        return 1
    except Exception as error:
        print(f"projection_codegen: internal error: {error}", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
