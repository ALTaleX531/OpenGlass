"""Read complete PE headers using pefile at the binary-input boundary."""

from collections.abc import Iterator
from contextlib import contextmanager
from pathlib import Path

import pefile

from tools import projection_target


def require_machine(image: pefile.PE, platform: str) -> None:
    if image.FILE_HEADER.Machine != projection_target.MACHINES[platform]:
        raise pefile.PEFormatError(f"image machine does not match {platform}")


@contextmanager
def open_image(path: Path | None = None, *, data: bytes | None = None) -> Iterator[pefile.PE]:
    with pefile.PE(str(path) if path is not None else None, data=data, fast_load=True) as image:
        # pefile can return guessed headers or a partial section list with warnings.
        if (image.OPTIONAL_HEADER.Magic not in {pefile.OPTIONAL_HEADER_MAGIC_PE, pefile.OPTIONAL_HEADER_MAGIC_PE_PLUS}
                or image.FILE_HEADER.SizeOfOptionalHeader < 60):
            raise pefile.PEFormatError("invalid optional header")
        if len(image.sections) != image.FILE_HEADER.NumberOfSections:
            raise pefile.PEFormatError("incomplete PE section table")
        yield image
