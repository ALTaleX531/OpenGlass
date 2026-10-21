"""Read only PE headers, the debug directory and RSDS using HTTP Range."""

from pathlib import Path
import json
import urllib.request
import uuid

import pefile

from tools import audit_symbol_resolution as audit
from tools.file_io import write_json
from tools.pe_image import open_image, require_machine

MAX_METADATA_RANGE = 1024 * 1024


def read_range(url: str, offset: int, size: int) -> bytes:
	# Only headers/debug records are needed; untrusted sizes must not request a full image.
	if size > MAX_METADATA_RANGE:
		raise audit.AuditError(f"PE metadata range exceeds 1 MiB: offset={offset}, size={size}")
	end = offset + size - 1
	request = urllib.request.Request(url, headers={
		"Range": f"bytes={offset}-{end}", "Accept-Encoding": "identity",
		"User-Agent": "OpenGlass-CodeView/1",
	})
	try:
		with urllib.request.urlopen(request, timeout=30) as response:
			# A 200 response would silently download an entire PE. Never fall back to it.
			if response.status != 206 or not response.headers.get("Content-Range", "").startswith(f"bytes {offset}-{end}/"):
				raise audit.AuditError(f"server did not honor HTTP Range {offset}-{end}: {url}")
			data = response.read(size + 1)
			if len(data) != size:
				raise audit.AuditError(f"incomplete HTTP Range {offset}-{end}: {url}")
			return data
	except OSError as error:
		raise audit.AuditError(f"cannot read CodeView range from {url}: {error}") from error


def read_identity(url: str, platform: str = "x64") -> audit.PdbIdentity:
	try:
		def structure(format, offset: int):
			value = pefile.Structure(format)
			value.__unpack__(read_range(url, offset, value.sizeof()))
			return value

		dos = structure(pefile.PE.__IMAGE_DOS_HEADER_format__, 0)
		coff = structure(pefile.PE.__IMAGE_FILE_HEADER_format__, dos.e_lfanew + 4)
		header_size = dos.e_lfanew + 24 + coff.SizeOfOptionalHeader + coff.NumberOfSections * 40
		with open_image(data=read_range(url, 0, header_size)) as image:
			require_machine(image, platform)
			debug = image.OPTIONAL_HEADER.DATA_DIRECTORY[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_DEBUG"]]
			if not debug.VirtualAddress or not debug.Size:
				raise audit.AuditError(f"image has no debug directory: {url}")
			directory = read_range(url, image.get_offset_from_rva(debug.VirtualAddress), debug.Size)
			entry = pefile.Structure(pefile.PE.__IMAGE_DEBUG_DIRECTORY_format__)
			for offset in range(0, len(directory), entry.sizeof()):
				entry.__unpack__(directory[offset:offset + entry.sizeof()])
				if entry.Type == pefile.DEBUG_TYPE["IMAGE_DEBUG_TYPE_CODEVIEW"] and entry.SizeOfData >= 25:
					data = read_range(url, entry.PointerToRawData, entry.SizeOfData)
					if data.startswith(b"RSDS"):
						return audit.decode_codeview(data)
	except (pefile.PEFormatError, IndexError, ValueError) as error:
		raise audit.AuditError(f"cannot parse remote PE CodeView from {url}: {error}") from error
	raise audit.AuditError(f"image has no RSDS CodeView identity: {url}")


def cached_identity(url: str, path: Path, platform: str = "x64") -> audit.PdbIdentity:
	if path.is_file():
		value = json.loads(path.read_text(encoding="utf-8"))
		return audit.PdbIdentity(value["name"], uuid.UUID(value["guid"]), value["age"])
	identity = read_identity(url, platform)
	write_json(path, identity.json())
	return identity
