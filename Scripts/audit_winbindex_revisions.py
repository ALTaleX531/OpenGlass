#!/usr/bin/env python3
"""Check projection symbols against every Winbindex revision of a DWM module."""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import gzip
import hashlib
import io
import json
import os
import re
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time
from typing import Any, Iterable
import urllib.error
import urllib.request

import audit_symbol_resolution as AUDIT



WINBINDEX = "https://winbindex.m417z.com/data/by_filename_compressed"
SYMBOL_SERVER = "https://msdl.microsoft.com/download/symbols"
AMD64 = 0x8664
MAX_INDEX_SIZE = 32 * 1024 * 1024
MAX_UNCOMPRESSED_INDEX_SIZE = 256 * 1024 * 1024
MAX_IMAGE_SIZE = 128 * 1024 * 1024
MAX_PDB_SIZE = 256 * 1024 * 1024
FETCH_ATTEMPTS = 4


class AuditError(Exception):
	pass


@dataclass(frozen=True, order=True)
class Sample:
	version: AUDIT.Version
	sha256: str
	timestamp: int
	virtual_size: int
	size: int
	provenance: tuple[str, ...] = field(default=(), compare=False)

	def image_key(self) -> str:
		return f"{self.timestamp:08X}{self.virtual_size:x}"

	def inventory_record(self, filename: str, index_sha256: str) -> dict[str, Any]:
		return {
			"module": filename,
			"version": {"build": self.version.build, "revision": self.version.revision},
			"sha256": self.sha256,
			"time_date_stamp": self.timestamp,
			"size_of_image": self.virtual_size,
			"file_size": self.size,
			"source_index_sha256": index_sha256,
			"provenance": list(self.provenance),
		}


class SymbolServerIdentityCollision(AuditError):
	def __init__(self, filename: str, sample: Sample, received_sha256: str) -> None:
		self.filename = filename
		self.sample = sample
		self.received_sha256 = received_sha256
		super().__init__(
			f"symbol-server identity collision: expected {sample.sha256}, received {received_sha256}"
		)


@dataclass(frozen=True)
class LoadedIndex:
	entries: dict[str, Any]
	compressed_sha256: str


@dataclass(frozen=True)
class AcquisitionFailure:
	sample: Sample
	detail: str

	def json(self, filename: str) -> dict[str, Any]:
		return {
			"module": filename,
			"version": {"build": self.sample.version.build, "revision": self.sample.version.revision},
			"sha256": self.sample.sha256,
			"error": self.detail,
			"provenance": list(self.sample.provenance),
		}


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
	parser.add_argument("--inventory-output", type=Path)
	parser.add_argument("--image-root", type=Path, action="append", default=[])
	parser.add_argument("--missing-output", type=Path)
	parser.add_argument("--dbghelp", type=Path)
	parser.add_argument("--cache", type=Path)
	return parser.parse_args(argv)


def fetch(url: str, limit: int, *, attempts: int = FETCH_ATTEMPTS) -> bytes:
	request = urllib.request.Request(url, headers={"User-Agent": "OpenGlass-Winbindex-check/2"})
	last_error: Exception | None = None
	for attempt in range(attempts):
		try:
			with urllib.request.urlopen(request, timeout=90) as response:
				length = response.headers.get("Content-Length")
				if length and int(length) > limit:
					raise AuditError(f"download exceeds {limit} bytes: {url}")
				contents = response.read(limit + 1)
			if len(contents) > limit:
				raise AuditError(f"download exceeds {limit} bytes: {url}")
			return contents
		except AuditError:
			raise
		except (OSError, urllib.error.URLError, urllib.error.HTTPError, ValueError) as error:
			last_error = error
			if attempt + 1 < attempts:
				time.sleep(0.5 * (2 ** attempt))
	raise AuditError(f"cannot download {url} after {attempts} attempts: {last_error}")


def _atomic_write(path: Path, contents: bytes) -> None:
	path.parent.mkdir(parents=True, exist_ok=True)
	descriptor, temporary_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
	try:
		with os.fdopen(descriptor, "wb") as stream:
			stream.write(contents)
			stream.flush()
			os.fsync(stream.fileno())
		os.replace(temporary_name, path)
	except Exception:
		try:
			os.unlink(temporary_name)
		except OSError:
			pass
		raise


def load_index(filename: str, cache: Path | None = None) -> LoadedIndex:
	url = f"{WINBINDEX}/{filename.lower()}.json.gz"
	compressed = fetch(url, MAX_INDEX_SIZE)
	digest = hashlib.sha256(compressed).hexdigest()
	if cache is not None:
		cached = cache / "indexes" / filename.lower() / f"{digest}.json.gz"
		if not cached.is_file():
			_atomic_write(cached, compressed)
	try:
		with gzip.GzipFile(fileobj=io.BytesIO(compressed)) as stream:
			contents = stream.read(MAX_UNCOMPRESSED_INDEX_SIZE + 1)
		if len(contents) > MAX_UNCOMPRESSED_INDEX_SIZE:
			raise AuditError("uncompressed Winbindex index is too large")
		index = json.loads(contents)
	except (gzip.BadGzipFile, UnicodeDecodeError, json.JSONDecodeError) as error:
		raise AuditError(f"invalid Winbindex index: {error}") from error
	if not isinstance(index, dict):
		raise AuditError("Winbindex index root is not an object")
	return LoadedIndex(index, digest)


def parse_version(value: Any) -> AUDIT.Version | None:
	if not isinstance(value, str):
		return None
	parts = value.split(" ", 1)[0].split(".")
	if len(parts) != 4 or not all(part.isdigit() for part in parts):
		return None
	return AUDIT.Version(int(parts[2]), int(parts[3]))


def _provenance(entry: dict[str, Any]) -> tuple[str, ...]:
	result: set[str] = set()
	versions = entry.get("windowsVersions")
	if isinstance(versions, dict):
		for product, sources in versions.items():
			if isinstance(sources, dict):
				for source, details in sources.items():
					kbs = {
						match.upper()
						for match in re.findall(r"\bKB\d+\b", json.dumps([source, details], ensure_ascii=False), re.IGNORECASE)
					}
					if kbs:
						result.update(f"{product} | {kb}" for kb in kbs)
					else:
						result.add(f"{product} | {source}")
			elif isinstance(sources, list):
				for source in sources:
					kbs = {match.upper() for match in re.findall(r"\bKB\d+\b", str(source), re.IGNORECASE)}
					result.update(f"{product} | {kb}" for kb in kbs)
					if not kbs:
						result.add(f"{product} | {source}")
			elif sources is not None:
				result.add(f"{product} | {sources}")
	elif isinstance(versions, list):
		result.update(" | ".join(str(item).split(" | ", 2)[:2]) for item in versions)
	return tuple(sorted(result))


def _assembly_versions(value: Any) -> set[AUDIT.Version]:
	result: set[AUDIT.Version] = set()
	if isinstance(value, dict):
		identity = value.get("assemblyIdentity")
		if isinstance(identity, dict):
			version = parse_version(identity.get("version"))
			if version is not None:
				result.add(version)
		for child in value.values():
			result.update(_assembly_versions(child))
	elif isinstance(value, list):
		for child in value:
			result.update(_assembly_versions(child))
	return result


def _sample_version(entry: dict[str, Any], info: dict[str, Any]) -> AUDIT.Version | None:
	version = parse_version(info.get("version"))
	if version is not None:
		return version
	versions = _assembly_versions(entry.get("windowsVersions"))
	return next(iter(versions)) if len(versions) == 1 else None


def _virtual_size(info: dict[str, Any]) -> int | None:
	try:
		if "virtualSize" in info:
			value = int(info["virtualSize"])
		else:
			# Some Winbindex rows omit SizeOfImage but retain the final section's
			# VA/raw pointer and file size. This value is only a symbol-server key
			# candidate; a downloaded PE must still pass hash, machine, version,
			# and header validation before it can enter the catalog.
			file_size = int(info["size"])
			virtual_address = int(info["lastSectionVirtualAddress"])
			raw_pointer = int(info["lastSectionPointerToRawData"])
			value = virtual_address + file_size - raw_pointer
		return value if 0 < value <= MAX_IMAGE_SIZE else None
	except (KeyError, TypeError, ValueError):
		return None


def discover_samples(index: dict[str, Any] | LoadedIndex, args: argparse.Namespace) -> list[Sample]:
	entries = index.entries if isinstance(index, LoadedIndex) else index
	samples: list[Sample] = []
	seen: set[str] = set()
	for digest, entry in entries.items():
		if not isinstance(entry, dict) or not isinstance(entry.get("fileInfo"), dict):
			continue
		info = entry["fileInfo"]
		version = _sample_version(entry, info)
		virtual_size = _virtual_size(info)
		if info.get("machineType") != AMD64 or not version or version.build != args.build or virtual_size is None:
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
				version, sha256, int(info["timestamp"]), virtual_size, int(info["size"]), _provenance(entry)
			))
		except (KeyError, TypeError, ValueError):
			continue
		seen.add(sha256)
	return sorted(samples, key=lambda sample: (sample.version, sample.sha256))


def _pe_machine(path: Path) -> int:
	try:
		with path.open("rb") as stream:
			stream.seek(0x3C)
			pe = struct.unpack("<I", stream.read(4))[0]
			stream.seek(pe)
			if stream.read(4) != b"PE\0\0":
				raise ValueError("missing PE signature")
			return struct.unpack("<H", stream.read(2))[0]
	except (OSError, struct.error, ValueError) as error:
		raise AuditError(f"cannot read PE machine from {path}: {error}") from error


def index_image_roots(roots: Iterable[Path]) -> dict[str, list[Path]]:
	result: dict[str, list[Path]] = {}
	for root in roots:
		if not root.is_dir():
			raise AuditError(f"image root is not a directory: {root}")
		for candidate in root.rglob("*"):
			if candidate.is_file() and candidate.name.lower() in {"udwm.dll", "dwmcore.dll"}:
				digest = AUDIT.sha256_file(candidate).lower()
				result.setdefault(digest, []).append(candidate)
	for paths in result.values():
		paths.sort(key=lambda path: str(path).casefold())
	return result


def _validate_image(path: Path, filename: str, sample: Sample, *, check_name: bool = True) -> None:
	if check_name and path.name.casefold() != filename.casefold():
		raise AuditError(f"candidate module name does not match {filename}: {path}")
	if path.stat().st_size != sample.size:
		raise AuditError(f"candidate byte size does not match inventory: {path}")
	if AUDIT.sha256_file(path).lower() != sample.sha256:
		raise AuditError(f"candidate SHA-256 does not match inventory: {path}")
	if _pe_machine(path) != AMD64:
		raise AuditError(f"candidate machine is not x64: {path}")
	if AUDIT.image_version(path) != sample.version:
		raise AuditError(f"candidate version does not match inventory: {path}")


def ensure_image(
	cache: Path,
	filename: str,
	sample: Sample,
	image_index: dict[str, list[Path]] | None = None,
) -> Path:
	path = cache / "images" / filename.lower() / f"{sample.version.build}.{sample.version.revision}" / sample.sha256 / filename
	if path.is_file():
		try:
			_validate_image(path, filename, sample)
			return path
		except AuditError:
			path.unlink(missing_ok=True)
	if sample.size <= 0 or sample.size > MAX_IMAGE_SIZE:
		raise AuditError(f"invalid image size: {sample.size}")

	details: list[str] = []
	collision_hash: str | None = None
	url = f"{SYMBOL_SERVER}/{filename}/{sample.image_key()}/{filename}"
	try:
		contents = fetch(url, MAX_IMAGE_SIZE)
		actual_hash = hashlib.sha256(contents).hexdigest()
		if actual_hash != sample.sha256:
			collision_hash = actual_hash
		elif len(contents) != sample.size:
			details.append(f"symbol-server size mismatch: expected {sample.size}, received {len(contents)}")
		else:
			path.parent.mkdir(parents=True, exist_ok=True)
			descriptor, temporary_name = tempfile.mkstemp(prefix=filename + ".", suffix=".tmp", dir=path.parent)
			try:
				with os.fdopen(descriptor, "wb") as stream:
					stream.write(contents)
				temporary = Path(temporary_name)
				_validate_image(temporary, filename, sample, check_name=False)
				os.replace(temporary, path)
				return path
			except Exception:
				Path(temporary_name).unlink(missing_ok=True)
				raise
	except (AuditError, OSError) as error:
		details.append(f"symbol-server error: {error}")

	for candidate in (image_index or {}).get(sample.sha256, []):
		try:
			_validate_image(candidate, filename, sample)
			path.parent.mkdir(parents=True, exist_ok=True)
			descriptor, temporary_name = tempfile.mkstemp(prefix=filename + ".", suffix=".tmp", dir=path.parent)
			os.close(descriptor)
			try:
				Path(temporary_name).write_bytes(candidate.read_bytes())
				_validate_image(Path(temporary_name), filename, sample, check_name=False)
				os.replace(temporary_name, path)
				return path
			finally:
				Path(temporary_name).unlink(missing_ok=True)
		except (AuditError, OSError) as error:
			details.append(f"image-root candidate rejected: {error}")
	if collision_hash is not None:
		raise SymbolServerIdentityCollision(filename, sample, collision_hash)
	raise AuditError("; ".join(details) if details else "exact image is unavailable from symbol server and image roots")


def _pdb_paired(actual: AUDIT.PdbIdentity, expected: AUDIT.PdbIdentity) -> bool:
	return actual.guid == expected.guid and actual.age == expected.age


def _publish_pdb_bytes(contents: bytes, pdb: Path, identity: AUDIT.PdbIdentity) -> None:
	descriptor, temporary_name = tempfile.mkstemp(prefix=identity.name + ".", suffix=".tmp", dir=pdb.parent)
	try:
		with os.fdopen(descriptor, "wb") as stream:
			stream.write(contents)
		temporary = Path(temporary_name)
		if not _pdb_paired(AUDIT.read_pdb_identity(temporary), identity):
			raise AuditError("symbol server returned a PDB with the wrong GUID/age")
		os.replace(temporary, pdb)
	except Exception:
		Path(temporary_name).unlink(missing_ok=True)
		raise


def _expand_pdb(contents: bytes, pdb: Path, identity: AUDIT.PdbIdentity) -> None:
	compressed_descriptor, compressed_name = tempfile.mkstemp(
		prefix=identity.name + ".", suffix=".pd_", dir=pdb.parent
	)
	expanded_descriptor, expanded_name = tempfile.mkstemp(
		prefix=identity.name + ".", suffix=".tmp", dir=pdb.parent
	)
	os.close(expanded_descriptor)
	Path(expanded_name).unlink()
	try:
		with os.fdopen(compressed_descriptor, "wb") as stream:
			stream.write(contents)
		expand = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32" / "expand.exe"
		result = subprocess.run(
			[str(expand), compressed_name, expanded_name],
			check=False,
			capture_output=True,
			text=True,
			timeout=120,
		)
		if result.returncode:
			detail = (result.stderr or result.stdout).strip()
			raise AuditError(f"expand.exe rejected the compressed PDB ({result.returncode}){f': {detail}' if detail else ''}")
		expanded = Path(expanded_name)
		if not _pdb_paired(AUDIT.read_pdb_identity(expanded), identity):
			raise AuditError("symbol server returned a compressed PDB with the wrong GUID/age")
		os.replace(expanded, pdb)
	except subprocess.TimeoutExpired as error:
		raise AuditError("expand.exe timed out while decompressing the PDB") from error
	finally:
		Path(compressed_name).unlink(missing_ok=True)
		Path(expanded_name).unlink(missing_ok=True)


def _download_pdb(pdb: Path, identity: AUDIT.PdbIdentity) -> None:
	pdb.parent.mkdir(parents=True, exist_ok=True)
	direct_url = f"{SYMBOL_SERVER}/{identity.name}/{identity.key()}/{identity.name}"
	try:
		_publish_pdb_bytes(fetch(direct_url, MAX_PDB_SIZE), pdb, identity)
		return
	except (AuditError, OSError) as direct_error:
		compressed_name = identity.name[:-1] + "_"
		compressed_url = f"{SYMBOL_SERVER}/{identity.name}/{identity.key()}/{compressed_name}"
		try:
			_expand_pdb(fetch(compressed_url, MAX_PDB_SIZE), pdb, identity)
			return
		except (AuditError, OSError) as compressed_error:
			raise AuditError(
				f"cannot acquire paired PDB; direct={direct_error}; compressed={compressed_error}"
			) from compressed_error


def prime_symbol_cache(image: Path, cache: Path) -> Path:
	identity = AUDIT.read_pe_codeview(image)
	symbol_cache = cache / "symbols"
	pdb = symbol_cache / identity.name / identity.key() / identity.name
	paired = False
	if pdb.is_file():
		try:
			paired = _pdb_paired(AUDIT.read_pdb_identity(pdb), identity)
		except AUDIT.AuditError:
			pass
		if not paired:
			pdb.unlink(missing_ok=True)
	if not paired:
		_download_pdb(pdb, identity)
	actual_pdb, _, paired = AUDIT.find_pdb(pdb, identity)
	if not paired:
		raise AuditError("symbol cache contains a PDB with the wrong GUID/age")
	return actual_pdb


def descriptor_failures(descriptors: list[dict[str, Any]]) -> list[dict[str, Any]]:
	return [item for item in descriptors if item.get("status") in {"missing", "ambiguous"} and item.get("requirement") == "required"]


def write_json(path: Path, value: Any) -> None:
	_atomic_write(path, (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))


def run(args: argparse.Namespace) -> int:
	filename = "uDWM.dll" if args.module == "udwm" else "dwmcore.dll"
	cache = (args.cache or Path(tempfile.gettempdir()) / "openglass-winbindex").resolve()
	loaded = load_index(filename, cache)
	samples = discover_samples(loaded, args)
	if not samples:
		raise AuditError(f"no x64 {filename} samples found for build {args.build}")
	if args.inventory_output:
		write_json(args.inventory_output, {
			"schema_version": 1,
			"module": filename,
			"source_index_sha256": loaded.compressed_sha256,
			"records": [sample.inventory_record(filename, loaded.compressed_sha256) for sample in samples],
		})
	if args.list_only:
		for sample in samples:
			print(f"{sample.version.build}.{sample.version.revision} {sample.sha256}")
		return 0

	image_index = index_image_roots(args.image_root)
	dbghelp = args.dbghelp.resolve() if args.dbghelp else AUDIT.resolve_dbghelp_path(None)
	failures: list[AcquisitionFailure] = []
	resolution_failures = 0
	for position, sample in enumerate(samples, 1):
		label = f"{sample.version.build}.{sample.version.revision}"
		print(f"[{position}/{len(samples)}] {label}", file=sys.stderr)
		try:
			image = ensure_image(cache, filename, sample, image_index)
			pdb = prime_symbol_cache(image, cache)
			audit_args = argparse.Namespace(
				repo=args.repo, architecture=args.architecture, module=args.module, version=label,
				image=image, symbol_path=pdb, dbghelp=dbghelp,
				configuration="release", stable_id=args.stable_id,
			)
			result = AUDIT.inspect(audit_args)
			mismatches = descriptor_failures(result["descriptors"])
			if result["evidence"] != "production_candidate" or mismatches:
				resolution_failures += 1
				detail = ", ".join(f"{item['id']}={item['status']}" for item in mismatches)
				print(f"{label}: failed{f' ({detail})' if detail else ''}")
			else:
				print(f"{label}: passed")
		except (AuditError, AUDIT.AuditError, OSError) as error:
			failures.append(AcquisitionFailure(sample, str(error)))
			print(f"{label}: error ({error})")
	if args.missing_output:
		write_json(args.missing_output, {
			"schema_version": 1,
			"module": filename,
			"records": [failure.json(filename) for failure in failures],
		})
	return 1 if failures or resolution_failures else 0


def main(argv: list[str] | None = None) -> int:
	try:
		return run(parse_args(argv))
	except (AuditError, AUDIT.AuditError, OSError) as error:
		print(f"error: {error}", file=sys.stderr)
		return 2


if __name__ == "__main__":
	raise SystemExit(main())
