#!/usr/bin/env python3
"""Check projection symbols against every Winbindex revision of a DWM module."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import gzip
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
from typing import Any
import urllib.error
import urllib.request


SCRIPT_DIR = Path(__file__).resolve().parent


def load_script(name: str) -> Any:
	spec = importlib.util.spec_from_file_location(name, SCRIPT_DIR / f"{name}.py")
	if not spec or not spec.loader:
		raise RuntimeError(f"cannot load {name}.py")
	module = importlib.util.module_from_spec(spec)
	sys.modules[spec.name] = module
	spec.loader.exec_module(module)
	return module


AUDIT = load_script("audit_symbol_resolution")
DUMP = load_script("dump_symbols")

WINBINDEX = "https://winbindex.m417z.com/data/by_filename_compressed"
SYMBOL_SERVER = "https://msdl.microsoft.com/download/symbols"
AMD64 = 0x8664
MAX_INDEX_SIZE = 32 * 1024 * 1024
MAX_UNCOMPRESSED_INDEX_SIZE = 256 * 1024 * 1024
MAX_IMAGE_SIZE = 128 * 1024 * 1024


class AuditError(Exception):
	pass


@dataclass(frozen=True, order=True)
class Sample:
	version: AUDIT.Version
	sha256: str
	timestamp: int
	virtual_size: int
	size: int

	def image_key(self) -> str:
		return f"{self.timestamp:08X}{self.virtual_size:x}"


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("repo", type=Path)
	parser.add_argument("--architecture", choices=("legacy", "milcomp"), required=True)
	parser.add_argument("--module", choices=("udwm", "dwmcore"), default="dwmcore")
	parser.add_argument("--build", type=int, required=True)
	parser.add_argument("--min-revision", type=int)
	parser.add_argument("--max-revision", type=int)
	parser.add_argument("--id", dest="stable_id", help="check only one stable Symbol ID")
	parser.add_argument("--list-only", action="store_true")
	return parser.parse_args(argv)


def fetch(url: str, limit: int) -> bytes:
	request = urllib.request.Request(url, headers={"User-Agent": "OpenGlass-Winbindex-check/1"})
	try:
		with urllib.request.urlopen(request, timeout=90) as response:
			length = response.headers.get("Content-Length")
			if length and int(length) > limit:
				raise AuditError(f"download exceeds {limit} bytes: {url}")
			contents = response.read(limit + 1)
	except (OSError, urllib.error.URLError, urllib.error.HTTPError, ValueError) as error:
		raise AuditError(f"cannot download {url}: {error}") from error
	if len(contents) > limit:
		raise AuditError(f"download exceeds {limit} bytes: {url}")
	return contents


def load_index(filename: str) -> dict[str, Any]:
	url = f"{WINBINDEX}/{filename.lower()}.json.gz"
	try:
		with gzip.GzipFile(fileobj=io.BytesIO(fetch(url, MAX_INDEX_SIZE))) as stream:
			contents = stream.read(MAX_UNCOMPRESSED_INDEX_SIZE + 1)
		if len(contents) > MAX_UNCOMPRESSED_INDEX_SIZE:
			raise AuditError("uncompressed Winbindex index is too large")
		index = json.loads(contents)
	except (gzip.BadGzipFile, UnicodeDecodeError, json.JSONDecodeError) as error:
		raise AuditError(f"invalid Winbindex index: {error}") from error
	if not isinstance(index, dict):
		raise AuditError("Winbindex index root is not an object")
	return index


def parse_version(value: Any) -> AUDIT.Version | None:
	if not isinstance(value, str):
		return None
	parts = value.split(" ", 1)[0].split(".")
	if len(parts) != 4 or not all(part.isdigit() for part in parts):
		return None
	return AUDIT.Version(int(parts[2]), int(parts[3]))


def discover_samples(index: dict[str, Any], args: argparse.Namespace) -> list[Sample]:
	samples = []
	seen = set()
	for digest, entry in index.items():
		if not isinstance(entry, dict) or not isinstance(entry.get("fileInfo"), dict):
			continue
		info = entry["fileInfo"]
		version = parse_version(info.get("version"))
		if info.get("machineType") != AMD64 or not version or version.build != args.build:
			continue
		if args.min_revision is not None and version.revision < args.min_revision:
			continue
		if args.max_revision is not None and version.revision > args.max_revision:
			continue
		sha256 = str(info.get("sha256", digest)).lower()
		if (
			len(sha256) != 64 or any(character not in "0123456789abcdef" for character in sha256) or
			isinstance(digest, str) and len(digest) == 64 and digest.lower() != sha256 or
			sha256 in seen
		):
			continue
		try:
			samples.append(Sample(
				version, sha256, int(info["timestamp"]), int(info["virtualSize"]), int(info["size"])
			))
		except (KeyError, TypeError, ValueError):
			continue
		seen.add(sha256)
	return sorted(samples, key=lambda sample: (sample.version, sample.sha256))


def ensure_image(cache: Path, filename: str, sample: Sample) -> Path:
	path = cache / "images" / f"{sample.version.build}.{sample.version.revision}" / sample.sha256 / filename
	if path.is_file() and AUDIT.sha256_file(path).lower() == sample.sha256:
		return path
	if sample.size <= 0 or sample.size > MAX_IMAGE_SIZE:
		raise AuditError(f"invalid image size: {sample.size}")
	url = f"{SYMBOL_SERVER}/{filename}/{sample.image_key()}/{filename}"
	contents = fetch(url, MAX_IMAGE_SIZE)
	if len(contents) != sample.size or hashlib.sha256(contents).hexdigest() != sample.sha256:
		raise AuditError("downloaded image does not match the Winbindex identity")
	path.parent.mkdir(parents=True, exist_ok=True)
	path.write_bytes(contents)
	actual_version = AUDIT.image_version(path)
	if actual_version != sample.version:
		path.unlink(missing_ok=True)
		raise AuditError("downloaded image version does not match the Winbindex version")
	return path


def prime_symbol_cache(image: Path, cache: Path, dbghelp: Path | None) -> Path:
	identity = AUDIT.read_pe_codeview(image)
	symbol_cache = cache / "symbols"
	symbol_cache.mkdir(parents=True, exist_ok=True)
	DUMP.enumerate_symbols(image, symbol_cache, dbghelp, SYMBOL_SERVER)
	pdb, _, paired = AUDIT.find_pdb(symbol_cache, identity)
	if not paired:
		raise AuditError("symbol server returned a PDB with the wrong GUID/age")
	return pdb


def descriptor_failures(descriptors: list[dict[str, Any]]) -> list[dict[str, Any]]:
	return [item for item in descriptors if item.get("status") in {"missing", "ambiguous"}]


def run(args: argparse.Namespace) -> int:
	filename = "uDWM.dll" if args.module == "udwm" else "dwmcore.dll"
	samples = discover_samples(load_index(filename), args)
	if not samples:
		raise AuditError(f"no x64 {filename} samples found for build {args.build}")
	if args.list_only:
		for sample in samples:
			print(f"{sample.version.build}.{sample.version.revision} {sample.sha256}")
		return 0

	cache = Path(tempfile.gettempdir()) / "openglass-winbindex"
	dbghelp = DUMP.discover_dbghelp()
	failures = 0
	for position, sample in enumerate(samples, 1):
		label = f"{sample.version.build}.{sample.version.revision}"
		print(f"[{position}/{len(samples)}] {label}", file=sys.stderr)
		try:
			image = ensure_image(cache, filename, sample)
			pdb = prime_symbol_cache(image, cache, dbghelp)
			audit_args = argparse.Namespace(
				repo=args.repo, architecture=args.architecture, module=args.module, version=label,
				image=image, symbol_path=pdb, dbghelp=dbghelp,
				configuration="release", stable_id=args.stable_id,
			)
			result = AUDIT.inspect(audit_args)
			mismatches = descriptor_failures(result["descriptors"])
			if result["evidence"] != "production_candidate" or mismatches:
				failures += 1
				detail = ", ".join(f"{item['id']}={item['status']}" for item in mismatches)
				print(f"{label}: failed{f' ({detail})' if detail else ''}")
			else:
				print(f"{label}: passed")
		except (AuditError, AUDIT.AuditError, DUMP.DumpError, OSError) as error:
			failures += 1
			print(f"{label}: error ({error})")
	return 1 if failures else 0


def main(argv: list[str] | None = None) -> int:
	try:
		return run(parse_args(argv))
	except (AuditError, AUDIT.AuditError, DUMP.DumpError, OSError) as error:
		print(f"error: {error}", file=sys.stderr)
		return 2


if __name__ == "__main__":
	raise SystemExit(main())
