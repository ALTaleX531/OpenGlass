"""Publish tool output without exposing a partially written file."""

import json
import os
from contextlib import contextmanager
from collections.abc import Iterator
from pathlib import Path
import tempfile
from typing import Any


@contextmanager
def atomic_output(path: Path) -> Iterator[Path]:
    """Publish the staged file only after its writer/validator completes."""
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
    os.close(descriptor)
    temporary = Path(temporary_name)
    try:
        yield temporary
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def write_bytes(path: Path, contents: bytes) -> None:
    if path.is_file() and path.read_bytes() == contents:
        return
    with atomic_output(path) as temporary:
        temporary.write_bytes(contents)


def write_json(path: Path, value: Any) -> None:
    write_bytes(path, (json.dumps(value, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
