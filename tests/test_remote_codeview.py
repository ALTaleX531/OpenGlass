from __future__ import annotations

import io
import struct
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import uuid

import pefile

from tools import audit_symbol_resolution as audit, remote_codeview, projection_target
from tests import test_audit_symbol_resolution as fixtures


class RemoteCodeViewTests(unittest.TestCase):
	def test_platform_machine_and_receipt_isolation(self) -> None:
		from tools import audit_winbindex_revisions as revisions
		with tempfile.TemporaryDirectory() as directory:
			root = Path(directory)
			image = root / "uDWM.dll"
			expected = {}
			for index, platform in enumerate(("x64", "arm64", "arm64ec"), 1):
				guid = uuid.UUID(int=index)
				fixtures.SymbolResolutionAuditTests._write_pe(image, guid, 0x1A)
				contents = bytearray(image.read_bytes())
				struct.pack_into("<H", contents, struct.unpack_from("<I", contents, 0x3C)[0] + 4, projection_target.MACHINES[platform])
				image.write_bytes(contents)
				expected[platform] = audit.PdbIdentity("uDWM.pdb", guid, 0x1A)
				self.assertEqual(audit.read_pe_codeview(image, platform), expected[platform])
				other = "arm64" if platform == "x64" else "x64"
				with self.assertRaisesRegex(audit.AuditError, "machine does not match"):
					audit.read_pe_codeview(image, other)
				def response(request, timeout):
					start, end = map(int, request.get_header("Range").removeprefix("bytes=").split("-"))
					result = io.BytesIO(contents[start:end + 1])
					result.status = 206
					result.headers = {"Content-Range": f"bytes {start}-{end}/{len(contents)}"}
					return result
				sample = revisions.Sample(audit.Version(100, 7), "1" * 64, 123, 0x2000, len(contents), platform=platform)
				with mock.patch.object(remote_codeview.urllib.request, "urlopen", side_effect=response):
					with self.assertRaisesRegex(audit.AuditError, "machine does not match"):
						remote_codeview.read_identity("https://example.test/uDWM.dll", other)
					self.assertEqual(revisions.sample_codeview(root, "uDWM.dll", sample), expected[platform])
			# Identical PE server keys on different targets retain separate receipts.
			with mock.patch.object(remote_codeview.urllib.request, "urlopen", side_effect=AssertionError("cached receipt should be reused")):
				for platform, identity in expected.items():
					sample = revisions.Sample(audit.Version(100, 7), "1" * 64, 123, 0x2000, 0, platform=platform)
					self.assertEqual(revisions.sample_codeview(root, "uDWM.dll", sample), identity)

	def test_ranges_extract_and_cache_identity_without_reading_the_image(self) -> None:
		guid = uuid.UUID("12345678-1234-5678-90ab-cdef12345678")
		with tempfile.TemporaryDirectory() as directory:
			root = Path(directory)
			image = root / "udwm.dll"
			fixtures.SymbolResolutionAuditTests._write_pe(image, guid, 0x1A)
			contents = image.read_bytes() + b"unused image data" * 1024
			ranges = []
			def response(request, timeout):
				start, end = map(int, request.get_header("Range").removeprefix("bytes=").split("-"))
				ranges.append((start, end))
				result = io.BytesIO(contents[start:end + 1])
				result.status = 206
				result.headers = {"Content-Range": f"bytes {start}-{end}/{len(contents)}"}
				return result
			cache = root / "codeview.json"
			with mock.patch.object(remote_codeview.urllib.request, "urlopen", side_effect=response):
				first = remote_codeview.cached_identity("https://example.test/udwm.dll", cache)
				request_count = len(ranges)
				second = remote_codeview.cached_identity("https://example.test/udwm.dll", cache)
			self.assertEqual(first, audit.PdbIdentity("uDWM.pdb", guid, 0x1A))
			self.assertEqual(second, first)
			self.assertEqual(len(ranges), request_count)
			self.assertLess(sum(end - start + 1 for start, end in ranges), 1024)
			self.assertEqual(first.key(), "123456781234567890ABCDEF123456781a")
			with pefile.PE(data=contents, fast_load=True) as parsed:
				debug_offset = parsed.OPTIONAL_HEADER.DATA_DIRECTORY[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_DEBUG"]].get_file_offset()
			invalid = bytearray(contents)
			struct.pack_into("<I", invalid, debug_offset, 0x7FFFFFFF)
			contents = bytes(invalid)
			with mock.patch.object(remote_codeview.urllib.request, "urlopen", side_effect=response):
				with self.assertRaisesRegex(audit.AuditError, "cannot parse remote PE CodeView"):
					remote_codeview.read_identity("https://example.test/udwm.dll")

	def test_oversized_metadata_is_rejected_before_request(self) -> None:
		with tempfile.TemporaryDirectory() as directory:
			image = Path(directory) / "uDWM.dll"
			fixtures.SymbolResolutionAuditTests._write_pe(image, uuid.UUID(int=1), 1)
			original = image.read_bytes()
			with pefile.PE(data=original, fast_load=True) as parsed:
				sections_offset = parsed.FILE_HEADER.get_file_offset() + 2
				debug = parsed.OPTIONAL_HEADER.DATA_DIRECTORY[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_DEBUG"]]
				debug_size_offset = debug.get_file_offset() + 4
				codeview_size_offset = parsed.get_offset_from_rva(debug.VirtualAddress) + 16
			for field, offset, format, value in (
				("headers", sections_offset, "<H", 0xFFFF),
				("debug directory", debug_size_offset, "<I", remote_codeview.MAX_METADATA_RANGE + 1),
				("CodeView", codeview_size_offset, "<I", remote_codeview.MAX_METADATA_RANGE + 1),
			):
				with self.subTest(field=field):
					contents = bytearray(original)
					struct.pack_into(format, contents, offset, value)
					ranges = []
					def response(request, timeout):
						start, end = map(int, request.get_header("Range").removeprefix("bytes=").split("-"))
						ranges.append((start, end))
						result = io.BytesIO(contents[start:end + 1])
						result.status = 206
						result.headers = {"Content-Range": f"bytes {start}-{end}/{len(contents)}"}
						return result
					with mock.patch.object(remote_codeview.urllib.request, "urlopen", side_effect=response):
						with self.assertRaisesRegex(audit.AuditError, "PE metadata range exceeds"):
							remote_codeview.read_identity("https://example.test/udwm.dll")
					self.assertTrue(all(end - start + 1 <= remote_codeview.MAX_METADATA_RANGE for start, end in ranges))

	def test_ignored_or_incorrect_range_response_never_downloads_a_full_pe(self) -> None:
		for status, content_range in [(200, ""), (206, "bytes 0-3/100")]:
			with self.subTest(status=status, content_range=content_range):
				response = mock.MagicMock()
				response.__enter__.return_value = response
				response.status = status
				response.headers = {"Content-Range": content_range}
				with mock.patch.object(remote_codeview.urllib.request, "urlopen", return_value=response):
					with self.assertRaisesRegex(audit.AuditError, "did not honor HTTP Range"):
						remote_codeview.read_range("https://example.test/udwm.dll", 10, 4)
				response.read.assert_not_called()
