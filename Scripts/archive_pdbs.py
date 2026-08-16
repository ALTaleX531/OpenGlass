#!/usr/bin/env python3
"""Create a deterministic ZIP containing all OpenGlass PDB files."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile


CONFIGURATIONS = ("Release", "ReleaseSigned")
PDB_INPUTS = (
	("legacy", "OpenGlass.pdb", "legacy/OpenGlass.pdb"),
	("milcomp", "OpenGlass.pdb", "milcomp/OpenGlass.pdb"),
	("common", "OpenGlassHost.pdb", "OpenGlassHost.pdb"),
	("common", "OpenGlassGUI.pdb", "OpenGlassGUI.pdb"),
)
ZIP_TIMESTAMP = (1980, 1, 1, 0, 0, 0)


class InputError(Exception):
	pass


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("repo", nargs="?", default=".", type=Path)
	parser.add_argument("--configuration", choices=CONFIGURATIONS, default="Release")
	parser.add_argument("--output", type=Path)
	return parser.parse_args(argv)


def collect_inputs(repo: Path, configuration: str) -> list[tuple[Path, str]]:
	build_root = repo / "Build" / "x64" / configuration
	locations = {
		"legacy": build_root / "legacy",
		"milcomp": build_root / "milcomp",
		"common": build_root / "common",
	}
	inputs: list[tuple[Path, str]] = []
	for location, name, archive_name in PDB_INPUTS:
		path = locations[location] / name
		if not path.is_file():
			raise InputError(f"required PDB does not exist: {path}")
		if path.stat().st_size == 0:
			raise InputError(f"required PDB is empty: {path}")
		inputs.append((path, archive_name))
	return inputs


def default_output(repo: Path, configuration: str) -> Path:
	return repo / "Build" / "x64" / configuration / "OpenGlassSymbols.zip"


def write_archive(inputs: list[tuple[Path, str]], output: Path) -> None:
	output.parent.mkdir(parents=True, exist_ok=True)
	descriptor, temporary_name = tempfile.mkstemp(prefix=f"{output.name}.", suffix=".tmp", dir=output.parent)
	os.close(descriptor)
	temporary = Path(temporary_name)
	try:
		with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
			for source, archive_name in inputs:
				info = zipfile.ZipInfo(archive_name, date_time=ZIP_TIMESTAMP)
				info.compress_type = zipfile.ZIP_DEFLATED
				info.create_system = 3
				info.external_attr = 0o100644 << 16
				with source.open("rb") as input_stream, archive.open(info, "w", force_zip64=True) as output_stream:
					shutil.copyfileobj(input_stream, output_stream, length=1024 * 1024)
		os.replace(temporary, output)
	finally:
		if temporary.exists():
			temporary.unlink()


def main(argv: list[str] | None = None) -> int:
	args = parse_args(argv)
	try:
		repo = args.repo.resolve()
		if not repo.is_dir():
			raise InputError(f"repository path is not a directory: {repo}")
		inputs = collect_inputs(repo, args.configuration)
		output = args.output.resolve() if args.output else default_output(repo, args.configuration)
		if any(output == source.resolve() for source, _ in inputs):
			raise InputError("output archive must not overwrite an input PDB")
		write_archive(inputs, output)
	except InputError as error:
		print(f"ERROR: {error}", file=sys.stderr)
		return 2
	except (OSError, zipfile.BadZipFile) as error:
		print(f"ERROR: failed to create PDB archive: {error}", file=sys.stderr)
		return 3
	print(output)
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
