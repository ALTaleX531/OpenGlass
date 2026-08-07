#!/usr/bin/env python3
"""Audit every Winbindex revision of one DWM module against a projection schema.

Winbindex is used only as an image inventory. Images and their exact CodeView-
matched PDBs are downloaded from Microsoft's public symbol server, verified,
and passed to audit_symbol_resolution.py for exact UNDNAME_COMPLETE matching.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import gzip
import hashlib
import io
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from typing import Any, Callable, Iterable
import urllib.error
import urllib.request
import uuid


SCRIPT_DIR = Path(__file__).resolve().parent
AUDIT_SPEC = importlib.util.spec_from_file_location(
	"openglass_audit_symbol_resolution", SCRIPT_DIR / "audit_symbol_resolution.py"
)
if not AUDIT_SPEC or not AUDIT_SPEC.loader:
	raise RuntimeError("cannot load audit_symbol_resolution.py")
SYMBOL_AUDIT = importlib.util.module_from_spec(AUDIT_SPEC)
sys.modules[AUDIT_SPEC.name] = SYMBOL_AUDIT
AUDIT_SPEC.loader.exec_module(SYMBOL_AUDIT)


SCHEMA_VERSION = 1
DEFAULT_INDEX_BASE = "https://winbindex.m417z.com/data/by_filename_compressed"
DEFAULT_SYMBOL_SERVER = "https://msdl.microsoft.com/download/symbols"
AMD64_MACHINE = 0x8664
MAX_INDEX_SIZE = 32 * 1024 * 1024
MAX_UNCOMPRESSED_INDEX_SIZE = 256 * 1024 * 1024
MAX_IMAGE_SIZE = 128 * 1024 * 1024
MAX_PDB_SIZE = 512 * 1024 * 1024
UrlOpen = Callable[..., Any]


class BatchAuditError(Exception):
	pass


@dataclass(frozen=True, order=True)
class WinbindexSample:
	version: SYMBOL_AUDIT.Version
	sha256: str
	timestamp: int
	virtual_size: int
	size: int
	windows_versions: dict[str, Any]

	def image_key(self) -> str:
		return f"{self.timestamp:08X}{self.virtual_size:x}"


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("repo", type=Path)
	parser.add_argument("--architecture", choices=("legacy", "milcomp"), required=True)
	parser.add_argument("--module", choices=("udwm", "dwmcore"), default="dwmcore")
	parser.add_argument("--build", type=int, required=True, help="Windows build number, for example 26100")
	parser.add_argument("--min-revision", type=int)
	parser.add_argument("--max-revision", type=int)
	parser.add_argument("--configuration", choices=("debug", "release"), default="release")
	parser.add_argument("--id", dest="stable_ids", action="append", help="audit only this stable Symbol ID; repeatable")
	parser.add_argument("--cache", type=Path, default=Path(tempfile.gettempdir()) / "openglass-winbindex-audit")
	parser.add_argument("--symbol-cache", type=Path, default=Path(tempfile.gettempdir()) / "symbols")
	parser.add_argument("--index-base", default=DEFAULT_INDEX_BASE)
	parser.add_argument("--symbol-server", default=DEFAULT_SYMBOL_SERVER)
	parser.add_argument("--dbghelp", type=Path)
	parser.add_argument("--offline", action="store_true", help="use only already cached index, images, and PDBs")
	parser.add_argument("--list-only", action="store_true", help="list matching Winbindex samples without downloading them")
	parser.add_argument(
		"--allow-optional-missing", action="store_true",
		help="do not fail for active Optional descriptors that are missing or ambiguous"
	)
	parser.add_argument("--format", choices=("text", "json"), default="text")
	parser.add_argument("--output", type=Path, help="write the complete JSON report to this file")
	return parser.parse_args(argv)


def parse_winbindex_version(value: Any) -> SYMBOL_AUDIT.Version | None:
	if not isinstance(value, str):
		return None
	text = value.split(" ", 1)[0]
	parts = text.split(".")
	if len(parts) != 4 or not all(part.isdigit() for part in parts):
		return None
	return SYMBOL_AUDIT.Version(int(parts[2]), int(parts[3]))


def image_url(symbol_server: str, filename: str, sample: WinbindexSample) -> str:
	return f"{symbol_server.rstrip('/')}/{filename}/{sample.image_key()}/{filename}"


def pdb_url(symbol_server: str, identity: Any, compressed: bool = False) -> str:
	name = identity.name
	leaf = name[:-1] + "_" if compressed else name
	return f"{symbol_server.rstrip('/')}/{name}/{identity.key()}/{leaf}"


def discover_samples(
	index: dict[str, Any], build: int, min_revision: int | None = None, max_revision: int | None = None
) -> list[WinbindexSample]:
	result = []
	seen_hashes = set()
	for digest, entry in index.items():
		if not isinstance(entry, dict):
			continue
		info = entry.get("fileInfo")
		if not isinstance(info, dict) or info.get("machineType") != AMD64_MACHINE:
			continue
		version = parse_winbindex_version(info.get("version"))
		if not version or version.build != build:
			continue
		if min_revision is not None and version.revision < min_revision:
			continue
		if max_revision is not None and version.revision > max_revision:
			continue
		sha256 = str(info.get("sha256", digest)).lower()
		if len(sha256) != 64 or any(character not in "0123456789abcdef" for character in sha256):
			continue
		if isinstance(digest, str) and len(digest) == 64 and digest.lower() != sha256:
			continue
		if sha256 in seen_hashes:
			continue
		try:
			sample = WinbindexSample(
				version, sha256, int(info["timestamp"]), int(info["virtualSize"]), int(info["size"]),
				entry.get("windowsVersions", {}) if isinstance(entry.get("windowsVersions", {}), dict) else {}
			)
		except (KeyError, TypeError, ValueError):
			continue
		result.append(sample)
		seen_hashes.add(sha256)
	return sorted(result, key=lambda item: (item.version, item.sha256))


def _read_response(response: Any, limit: int) -> bytes:
	length = response.headers.get("Content-Length")
	if length and int(length) > limit:
		raise BatchAuditError(f"download exceeds {limit} bytes")
	chunks = []
	total = 0
	while True:
		chunk = response.read(1024 * 1024)
		if not chunk:
			break
		total += len(chunk)
		if total > limit:
			raise BatchAuditError(f"download exceeds {limit} bytes")
		chunks.append(chunk)
	return b"".join(chunks)


def fetch_bytes(url: str, limit: int, urlopen: UrlOpen = urllib.request.urlopen) -> bytes:
	request = urllib.request.Request(url, headers={"User-Agent": "OpenGlass-projection-audit/1"})
	try:
		with urlopen(request, timeout=90) as response:
			return _read_response(response, limit)
	except (OSError, urllib.error.URLError, urllib.error.HTTPError, ValueError) as error:
		raise BatchAuditError(f"cannot download {url}: {error}") from error


def _atomic_write(path: Path, contents: bytes) -> None:
	path.parent.mkdir(parents=True, exist_ok=True)
	temporary = path.with_name(f".{path.name}.{os.getpid()}.{uuid.uuid4().hex}.tmp")
	try:
		temporary.write_bytes(contents)
		os.replace(temporary, path)
	finally:
		temporary.unlink(missing_ok=True)


def load_index(args: argparse.Namespace) -> tuple[dict[str, Any], str, str]:
	filename = "uDWM.dll" if args.module == "udwm" else "dwmcore.dll"
	url = f"{args.index_base.rstrip('/')}/{filename.lower()}.json.gz"
	cache_path = args.cache.resolve() / "index" / f"{filename.lower()}.json.gz"
	if args.offline:
		if not cache_path.is_file():
			raise BatchAuditError(f"offline index is not cached: {cache_path}")
		compressed = cache_path.read_bytes()
	else:
		compressed = fetch_bytes(url, MAX_INDEX_SIZE)
		_atomic_write(cache_path, compressed)
	try:
		with gzip.GzipFile(fileobj=io.BytesIO(compressed)) as stream:
			uncompressed = stream.read(MAX_UNCOMPRESSED_INDEX_SIZE + 1)
		if len(uncompressed) > MAX_UNCOMPRESSED_INDEX_SIZE:
			raise BatchAuditError(f"uncompressed Winbindex metadata exceeds {MAX_UNCOMPRESSED_INDEX_SIZE} bytes")
		index = json.loads(uncompressed)
	except (gzip.BadGzipFile, UnicodeDecodeError, json.JSONDecodeError) as error:
		raise BatchAuditError(f"invalid Winbindex metadata {cache_path}: {error}") from error
	if not isinstance(index, dict):
		raise BatchAuditError("Winbindex metadata root is not an object")
	return index, url, hashlib.sha256(compressed).hexdigest()


def ensure_image(args: argparse.Namespace, filename: str, sample: WinbindexSample) -> tuple[Path, str]:
	path = args.cache.resolve() / "images" / f"{sample.version.build}.{sample.version.revision}" / sample.sha256 / filename
	url = image_url(args.symbol_server, filename, sample)
	if path.is_file() and SYMBOL_AUDIT.sha256_file(path).lower() == sample.sha256:
		return path, url
	if args.offline:
		raise BatchAuditError(f"offline image is not cached or has the wrong hash: {path}")
	if sample.size <= 0 or sample.size > MAX_IMAGE_SIZE:
		raise BatchAuditError(f"Winbindex image size is outside the accepted range: {sample.size}")
	contents = fetch_bytes(url, MAX_IMAGE_SIZE)
	actual_hash = hashlib.sha256(contents).hexdigest()
	if actual_hash != sample.sha256:
		raise BatchAuditError(f"image SHA-256 mismatch for {url}: expected {sample.sha256}, got {actual_hash}")
	if len(contents) != sample.size:
		raise BatchAuditError(f"image size mismatch for {url}: expected {sample.size}, got {len(contents)}")
	_atomic_write(path, contents)
	actual_version = SYMBOL_AUDIT.image_version(path)
	if actual_version != sample.version:
		raise BatchAuditError(f"downloaded image version does not match {sample.version.build}.{sample.version.revision}")
	return path, url


def _identity_matches(path: Path, expected: Any) -> bool:
	try:
		actual = SYMBOL_AUDIT.read_pdb_identity(path)
	except SYMBOL_AUDIT.AuditError:
		return False
	return actual.guid == expected.guid and actual.age == expected.age


def ensure_pdb(args: argparse.Namespace, image: Path) -> tuple[Path, str]:
	identity = SYMBOL_AUDIT.read_pe_codeview(image)
	path = args.symbol_cache.resolve() / identity.name / identity.key() / identity.name
	direct_url = pdb_url(args.symbol_server, identity)
	if path.is_file() and _identity_matches(path, identity):
		return path, direct_url
	if args.offline:
		raise BatchAuditError(f"offline PDB is not cached or has the wrong GUID/age: {path}")
	try:
		contents = fetch_bytes(direct_url, MAX_PDB_SIZE)
		_atomic_write(path, contents)
	except BatchAuditError as direct_error:
		compressed_url = pdb_url(args.symbol_server, identity, compressed=True)
		compressed = fetch_bytes(compressed_url, MAX_PDB_SIZE)
		with tempfile.TemporaryDirectory(prefix="openglass-pdb-") as directory:
			root = Path(directory)
			cab = root / (identity.name[:-1] + "_")
			cab.write_bytes(compressed)
			expand = shutil.which("expand.exe") or shutil.which("expand")
			if not expand:
				raise BatchAuditError(f"direct PDB download failed ({direct_error}) and expand.exe is unavailable")
			completed = subprocess.run(
				[expand, str(cab), str(root / identity.name)], capture_output=True, text=True, check=False
			)
			expanded = root / identity.name
			if completed.returncode or not expanded.is_file():
				raise BatchAuditError(
					f"cannot expand {compressed_url}: {completed.stderr.strip() or completed.stdout.strip()}"
				)
			_atomic_write(path, expanded.read_bytes())
	if not _identity_matches(path, identity):
		path.unlink(missing_ok=True)
		raise BatchAuditError(f"downloaded PDB GUID/age does not match image CodeView record: {direct_url}")
	return path, direct_url


def descriptor_failures(descriptors: Iterable[dict[str, Any]], allow_optional_missing: bool) -> list[dict[str, Any]]:
	result = []
	for descriptor in descriptors:
		if descriptor.get("status") not in {"missing", "ambiguous"}:
			continue
		if descriptor.get("requirement") == "required" or not allow_optional_missing:
			result.append({
				"id": descriptor.get("id"), "requirement": descriptor.get("requirement"),
				"status": descriptor.get("status")
			})
	return result


def _sample_metadata(sample: WinbindexSample, filename: str, url: str) -> dict[str, Any]:
	return {
		"version": sample.version.json(), "filename": filename, "sha256": sample.sha256,
		"size": sample.size, "timestamp": sample.timestamp, "virtual_size": sample.virtual_size,
		"symbol_server_key": sample.image_key(), "image_url": url,
		"windows_versions": sample.windows_versions,
	}


def run(args: argparse.Namespace) -> tuple[dict[str, Any], int]:
	index, index_url, index_sha256 = load_index(args)
	samples = discover_samples(index, args.build, args.min_revision, args.max_revision)
	if not samples:
		raise BatchAuditError(f"Winbindex contains no x64 {args.module}.dll samples for build {args.build}")
	filename = "uDWM.dll" if args.module == "udwm" else "dwmcore.dll"
	report_samples = []
	schema_failure_count = 0
	evidence_failure_count = 0
	for position, sample in enumerate(samples, 1):
		url = image_url(args.symbol_server, filename, sample)
		entry: dict[str, Any] = {"inventory": _sample_metadata(sample, filename, url)}
		if args.list_only:
			entry["status"] = "listed"
			report_samples.append(entry)
			continue
		print(
			f"[{position}/{len(samples)}] {sample.version.build}.{sample.version.revision} {sample.sha256[:12]}",
			file=sys.stderr
		)
		try:
			image, _ = ensure_image(args, filename, sample)
			pdb, pdb_download_url = ensure_pdb(args, image)
			audit_args = argparse.Namespace(
				repo=args.repo, architecture=args.architecture, module=args.module,
				version=f"{sample.version.build}.{sample.version.revision}", image=image,
				symbol_path=pdb, dbghelp=args.dbghelp, configuration=args.configuration, stable_id=None,
			)
			audit = SYMBOL_AUDIT.inspect(audit_args)
			if args.stable_ids:
				wanted = set(args.stable_ids)
				audit["descriptors"] = [item for item in audit["descriptors"] if item.get("id") in wanted]
				found = {item.get("id") for item in audit["descriptors"]}
				missing_ids = sorted(wanted - found)
				if missing_ids:
					raise BatchAuditError(f"Symbol stable ID not found: {', '.join(missing_ids)}")
			failures = descriptor_failures(audit["descriptors"], args.allow_optional_missing)
			if audit["evidence"] != "production_candidate" or not audit["pdb"]["paired"]:
				raise BatchAuditError("single-sample audit did not produce paired production evidence")
			entry.update({
				"status": "schema_mismatch" if failures else "passed", "image_path": str(image),
				"pdb_url": pdb_download_url, "audit": audit, "descriptor_failures": failures,
			})
			if failures:
				schema_failure_count += 1
		except (BatchAuditError, SYMBOL_AUDIT.AuditError, OSError) as error:
			entry.update({"status": "evidence_error", "error": str(error)})
			evidence_failure_count += 1
		report_samples.append(entry)
	report = {
		"schema_version": SCHEMA_VERSION,
		"source": {"kind": "winbindex", "url": index_url, "sha256": index_sha256},
		"architecture": args.architecture, "module": args.module, "build": args.build,
		"configuration": args.configuration,
		"policy": {"allow_optional_missing": args.allow_optional_missing, "list_only": args.list_only},
		"summary": {
			"samples": len(samples), "passed": sum(item["status"] == "passed" for item in report_samples),
			"listed": sum(item["status"] == "listed" for item in report_samples),
			"schema_mismatches": schema_failure_count, "evidence_errors": evidence_failure_count,
		},
		"samples": report_samples,
	}
	return report, 2 if evidence_failure_count else 1 if schema_failure_count else 0


def print_text(report: dict[str, Any]) -> None:
	for sample in report["samples"]:
		version = sample["inventory"]["version"]
		label = f"{version['build']}.{version['revision']}"
		line = f"{label}: {sample['status']}"
		if sample.get("descriptor_failures"):
			line += " [" + ", ".join(
				f"{item['id']}={item['status']}" for item in sample["descriptor_failures"]
			) + "]"
		if sample.get("error"):
			line += f" ({sample['error']})"
		print(line)
	summary = report["summary"]
	print(
		f"Samples: {summary['samples']}; passed: {summary['passed']}; listed: {summary['listed']}; "
		f"schema mismatches: {summary['schema_mismatches']}; evidence errors: {summary['evidence_errors']}"
	)


def main(argv: list[str] | None = None) -> int:
	args = parse_args(argv)
	try:
		report, exit_code = run(args)
	except (BatchAuditError, SYMBOL_AUDIT.AuditError) as error:
		print(f"ERROR: {error}", file=sys.stderr)
		return 2
	except Exception as error:
		print(f"INTERNAL ERROR: {error}", file=sys.stderr)
		return 3
	serialized = json.dumps(report, indent=2, ensure_ascii=False)
	if args.output:
		try:
			_atomic_write(args.output.resolve(), (serialized + "\n").encode("utf-8"))
		except OSError as error:
			print(f"ERROR: cannot write report {args.output}: {error}", file=sys.stderr)
			return 2
	if args.format == "json":
		print(serialized)
	else:
		print_text(report)
	return exit_code


if __name__ == "__main__":
	raise SystemExit(main())
