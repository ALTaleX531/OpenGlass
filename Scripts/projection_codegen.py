#!/usr/bin/env python3
"""Validate OpenGlass projection metadata and publish generated C++ files."""

from __future__ import annotations

import argparse
import os
import sys
import tempfile
from pathlib import Path

import projection_emit
import projection_schema
import projection_source_check


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--architecture", choices=projection_schema.ARCHITECTURES, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true", help="validate without writing generated files")
    return parser.parse_args(argv)


def generate(repo: Path, architecture: str) -> dict[str, str]:
    """Validate one architecture and return its complete generated output set."""
    repo = repo.resolve()
    if not repo.is_dir():
        raise projection_schema.SchemaError(f"repository does not exist: {repo}")
    if architecture not in projection_schema.ARCHITECTURES:
        raise projection_schema.SchemaError(f"unknown DWM architecture: {architecture}")

    projection_root = (
        repo
        / "OpenGlass"
        / "Architecture"
        / projection_schema.ARCHITECTURES[architecture]
    )
    if not projection_root.is_dir():
        raise projection_schema.SchemaError(
            f"DWM architecture source does not exist: {projection_root}"
        )

    constants = projection_source_check.load_os_constants(repo)
    schema_root = repo / "OpenGlass" / "ProjectionSchemas" / architecture
    schemas = [
        projection_schema.validate_schema(
            schema_root / f"{module}.json",
            module,
            constants,
        )
        for module in projection_schema.MODULES
    ]
    projection_source_check.validate_source_completeness(projection_root, schemas)
    return projection_emit.generate_files(schemas)


def atomic_write(path: Path, content: str) -> None:
    """Replace one generated file atomically, preserving unchanged timestamps."""
    path.parent.mkdir(parents=True, exist_ok=True)
    encoded = content.encode("utf-8")
    try:
        if path.is_file() and path.read_bytes() == encoded:
            return
        descriptor, temporary_name = tempfile.mkstemp(
            prefix=path.name + ".",
            suffix=".tmp",
            dir=path.parent,
        )
        try:
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(encoded)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary_name, path)
        except Exception:
            try:
                os.unlink(temporary_name)
            except OSError:
                pass
            raise
    except OSError as error:
        raise projection_schema.SchemaError(
            f"cannot atomically write {path}: {error}"
        ) from error


def publish(output: Path, generated: dict[str, str]) -> None:
    """Publish a complete generated mapping to the selected intermediate path."""
    for name, content in generated.items():
        atomic_write(output / name, content)


def run(repo: Path, architecture: str, output: Path, check: bool) -> None:
    generated = generate(repo, architecture)
    if not check:
        publish(output.resolve(), generated)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        run(args.repo, args.architecture, args.output, args.check)
    except projection_schema.SchemaError as error:
        print(f"projection_codegen: error: {error}", file=sys.stderr)
        return 1
    except Exception as error:
        print(f"projection_codegen: internal error: {error}", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
