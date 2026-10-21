"""CPU target selection, independent of the Legacy/MILComp DWM backend."""

import argparse
from pathlib import Path

# These are binary/tool identities, not declarations of OpenGlass runtime support.
MACHINES = {"x64": 0x8664, "arm64": 0xAA64, "arm64ec": 0xA641, "x86": 0x14C}
WINBINDEX = {
    "x64": "https://winbindex.m417z.com/data/by_filename_compressed",
    "x86": "https://winbindex.m417z.com/data/by_filename_compressed",
    "arm64": "https://m417z.com/winbindex-data-arm64/by_filename_compressed",
    "arm64ec": "https://m417z.com/winbindex-data-arm64/by_filename_compressed",
}


def normalize_platform(value: str) -> str:
    platform = value.lower()
    if platform == "win32":
        platform = "x86"
    if platform not in MACHINES:
        raise ValueError(f"unknown CPU platform: {value}")
    return platform


def add_platform_argument(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--platform", type=normalize_platform, choices=MACHINES, default="x64",
                        help="CPU generation/audit target (default: x64)")


def platform_root(root: Path, platform: str = "x64") -> Path:
    # Preserve existing cache paths; other CPU acquisition receipts stay separate.
    platform = normalize_platform(platform)
    return root if platform == "x64" else root / platform


def schema_path(repo: Path, architecture: str, module: str) -> Path:
    return repo / "OpenGlass/ProjectionSchemas" / architecture / f"{module}.json"


def catalog_root(repo: Path, architecture: str, platform: str = "x64") -> Path:
    root = repo / "OpenGlass/SymbolCatalogs"
    specialized = root / normalize_platform(platform)
    return specialized if (specialized / architecture).is_dir() else root
