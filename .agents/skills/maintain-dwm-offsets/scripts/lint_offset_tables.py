#!/usr/bin/env python3
"""Lint OpenGlass projection tables and resolve their build intervals."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any


UINT32_MAX = 0xFFFFFFFF
PROJECTION_FILES = (
    "OpenGlass/dwmcoreProjection.Offsets.hpp",
    "OpenGlass/uDwmProjection.Offsets.hpp",
)
CONSUMER_FILES = (
    "OpenGlass/dwmcoreProjection.hpp",
    "OpenGlass/uDwmProjection.hpp",
)
STRUCT_RE = re.compile(r"\bstruct\s+([A-Za-z_]\w*_Offsets)\b")
ENTRY_RE = re.compile(r"\bUtil\s*::\s*OffsetInfo\b")
ARRAY_RE = re.compile(r"\bstd\s*::\s*array\b")
CONSUMER_RE = re.compile(r"\bPointerExecuteUnsafe\s*<\s*([A-Za-z_]\w*_Offsets)\b")
BOUNDARY_NAME_RE = re.compile(r"os::(build_[A-Za-z0-9_]+|revision_[A-Za-z0-9_]+)\Z")
FIELD_NAME_RE = re.compile(r"\.([A-Za-z_]\w*)\s*=\s*")
VERSION_RE = re.compile(r"([0-9]+)\.([0-9]+)\Z")


class InputError(Exception):
    """An invalid command-line input or repository layout."""


class ParseError(Exception):
    """A source construct cannot be parsed completely."""


@dataclass(frozen=True)
class Version:
    build: int
    revision: int

    def json(self) -> dict[str, int]:
        return {"build": self.build, "revision": self.revision}


@dataclass
class Finding:
    severity: str
    file: str
    line: int
    column: int
    table: str | None
    entry: int | None
    message: str


@dataclass
class Entry:
    index: int
    offset_expression: str
    build_expression: str
    revision_expression: str
    boundary: Version | None
    start: int


@dataclass
class Table:
    file: str
    name: str
    start: int
    entries_data: list[Entry] = field(default_factory=list)
    valid: bool = True
    has_consumer: bool = False


@dataclass
class Source:
    relative: str
    text: str
    masked: str

    def location(self, offset: int) -> tuple[int, int]:
        line = self.text.count("\n", 0, offset) + 1
        last_newline = self.text.rfind("\n", 0, offset)
        return line, offset - last_newline


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("repo", nargs="?", default=".", help="repository or branch snapshot root")
    parser.add_argument("--format", choices=("text", "json"), default="text")
    parser.add_argument("--version", metavar="BUILD.REVISION")
    return parser.parse_args(argv)


def parse_requested_version(raw: str | None) -> Version | None:
    if raw is None:
        return None
    match = VERSION_RE.fullmatch(raw)
    if not match:
        raise InputError("--version must use two decimal components: BUILD.REVISION")
    build, revision = (int(part) for part in match.groups())
    if build > UINT32_MAX or revision > UINT32_MAX:
        raise InputError("--version components must fit in an unsigned 32-bit integer")
    return Version(build, revision)


def mask_non_code(text: str) -> str:
    """Replace comments and literals with spaces while preserving offsets/newlines."""
    chars = list(text)
    index = 0
    state = "code"
    quote = ""
    while index < len(chars):
        char = chars[index]
        next_char = chars[index + 1] if index + 1 < len(chars) else ""
        if state == "code":
            if char == "/" and next_char == "/":
                chars[index] = chars[index + 1] = " "
                index += 2
                state = "line_comment"
                continue
            if char == "/" and next_char == "*":
                chars[index] = chars[index + 1] = " "
                index += 2
                state = "block_comment"
                continue
            if char in ('"', "'"):
                quote = char
                chars[index] = " "
                index += 1
                state = "literal"
                continue
        elif state == "line_comment":
            if char == "\n":
                state = "code"
            else:
                chars[index] = " "
            index += 1
            continue
        elif state == "block_comment":
            if char == "*" and next_char == "/":
                chars[index] = chars[index + 1] = " "
                index += 2
                state = "code"
                continue
            if char != "\n":
                chars[index] = " "
            index += 1
            continue
        elif state == "literal":
            if char == "\\":
                chars[index] = " "
                if index + 1 < len(chars):
                    if chars[index + 1] != "\n":
                        chars[index + 1] = " "
                    index += 2
                    continue
            if char == quote:
                state = "code"
            if char != "\n":
                chars[index] = " "
            index += 1
            continue
        index += 1
    if state in ("block_comment", "literal"):
        raise ParseError(f"unterminated {state.replace('_', ' ')}")
    return "".join(chars)


def matching_delimiter(masked: str, start: int) -> int:
    pairs = {"{": "}", "(": ")", "[": "]", "<": ">"}
    opener = masked[start]
    closer = pairs.get(opener)
    if closer is None:
        raise ParseError(f"expected opening delimiter at offset {start}")
    depth = 0
    for index in range(start, len(masked)):
        if masked[index] == opener:
            depth += 1
        elif masked[index] == closer:
            depth -= 1
            if depth == 0:
                return index
    raise ParseError(f"unbalanced {opener}{closer} delimiter")


def split_top_level(masked: str, start: int, end: int, delimiter: str = ",") -> list[tuple[int, int]]:
    ranges: list[tuple[int, int]] = []
    stack: list[str] = []
    pairs = {"{": "}", "(": ")", "[": "]"}
    reverse = {value: key for key, value in pairs.items()}
    part_start = start
    for index in range(start, end):
        char = masked[index]
        if char in pairs:
            stack.append(char)
        elif char in reverse:
            if not stack or stack[-1] != reverse[char]:
                raise ParseError(f"unbalanced delimiter at offset {index}")
            stack.pop()
        elif char == delimiter and not stack:
            ranges.append((part_start, index))
            part_start = index + 1
    if stack:
        raise ParseError("unbalanced delimiter in initializer")
    ranges.append((part_start, end))
    return ranges


def parse_uint_literal(raw: str) -> int | None:
    value = raw.strip()
    if not re.fullmatch(r"(?:0[xX][0-9A-Fa-f]+|[0-9]+)[uUlL]*", value):
        return None
    suffix_free = re.sub(r"[uUlL]+$", "", value)
    parsed = int(suffix_free, 16 if suffix_free.lower().startswith("0x") else 10)
    return parsed if parsed <= UINT32_MAX else None


def parse_enum(repo: Path, enum_name: str, prefix: str) -> tuple[dict[str, int], list[str]]:
    path = repo / "OpenGlass" / "OSHelper.hpp"
    try:
        text = path.read_text(encoding="utf-8-sig")
        masked = mask_non_code(text)
    except (OSError, UnicodeError, ParseError) as error:
        raise InputError(f"cannot read {path}: {error}") from error
    match = re.search(rf"\benum\s+{re.escape(enum_name)}\s*:\s*ULONG\s*\{{", masked)
    if not match:
        raise InputError(f"cannot locate enum {enum_name} in {path}")
    body_start = masked.find("{", match.start())
    try:
        body_end = matching_delimiter(masked, body_start)
        pieces = split_top_level(masked, body_start + 1, body_end)
    except ParseError as error:
        raise InputError(f"cannot parse enum {enum_name}: {error}") from error
    values: dict[str, int] = {}
    errors: list[str] = []
    for start, end in pieces:
        item = text[start:end].strip()
        if not item:
            continue
        item_match = re.fullmatch(r"([A-Za-z_]\w*)\s*=\s*(.+)", item, re.DOTALL)
        if not item_match or not item_match.group(1).startswith(prefix):
            errors.append(f"unsupported {enum_name} item: {item}")
            continue
        name, expression = item_match.groups()
        value = parse_uint_literal(expression)
        if value is None:
            alias = re.fullmatch(r"(?:os::)?([A-Za-z_]\w*)", expression.strip())
            value = values.get(alias.group(1)) if alias else None
        if value is None:
            errors.append(f"unsupported {enum_name} value for {name}: {expression.strip()}")
        else:
            values[name] = value
    return values, errors


def resolve_boundary(expression: str, values: dict[str, int]) -> int | None:
    literal = parse_uint_literal(expression)
    if literal is not None:
        return literal
    match = BOUNDARY_NAME_RE.fullmatch(expression.strip())
    return values.get(match.group(1)) if match else None


def finding(source: Source, severity: str, offset: int, message: str,
            table: str | None = None, entry: int | None = None) -> Finding:
    line, column = source.location(offset)
    return Finding(severity, source.relative, line, column, table, entry, message)


def parse_entry(source: Source, table: Table, token_start: int, brace_start: int,
                brace_end: int, values: dict[str, int], index: int) -> tuple[Entry | None, list[Finding]]:
    findings: list[Finding] = []
    fields: dict[str, str] = {}
    try:
        pieces = split_top_level(source.masked, brace_start + 1, brace_end)
    except ParseError as error:
        return None, [finding(source, "error", token_start, str(error), table.name, index)]
    for start, end in pieces:
        if not source.masked[start:end].strip():
            continue
        field_start = start
        while field_start < end and source.masked[field_start].isspace():
            field_start += 1
        match = FIELD_NAME_RE.match(source.masked, field_start, end)
        if not match:
            findings.append(finding(source, "error", field_start, "entry field is not a designated initializer", table.name, index))
            continue
        name = match.group(1)
        value = source.text[match.end():end].strip()
        if name in fields:
            findings.append(finding(source, "error", field_start, f"duplicate field .{name}", table.name, index))
        elif name not in {"offset", "build", "revision"}:
            findings.append(finding(source, "error", field_start, f"unexpected field .{name}", table.name, index))
        else:
            fields[name] = value
    missing = sorted({"offset", "build", "revision"} - fields.keys())
    if missing:
        findings.append(finding(source, "error", token_start, "missing field(s): " + ", ".join(f".{name}" for name in missing), table.name, index))
    if not fields.get("offset", "").strip():
        findings.append(finding(source, "error", token_start, ".offset must not be empty", table.name, index))
    if findings:
        return None, findings
    build = resolve_boundary(fields["build"], values)
    revision = resolve_boundary(fields["revision"], values)
    if build is None or revision is None:
        findings.append(finding(
            source, "error", token_start,
            f"boundary cannot be resolved safely: {fields['build']}, {fields['revision']}",
            table.name, index,
        ))
        return None, findings
    return Entry(index, fields["offset"], fields["build"], fields["revision"],
                 Version(build, revision), token_start), findings


def parse_projection(source: Source, values: dict[str, int]) -> tuple[list[Table], list[Finding]]:
    tables: list[Table] = []
    findings: list[Finding] = []
    claimed_entries: set[int] = set()
    for struct_match in STRUCT_RE.finditer(source.masked):
        name = struct_match.group(1)
        brace_start = source.masked.find("{", struct_match.end())
        semicolon = source.masked.find(";", struct_match.end())
        table = Table(source.relative, name, struct_match.start())
        tables.append(table)
        if brace_start < 0 or (semicolon >= 0 and semicolon < brace_start):
            findings.append(finding(source, "error", struct_match.start(), "offset struct has no body", name))
            table.valid = False
            continue
        try:
            brace_end = matching_delimiter(source.masked, brace_start)
        except ParseError as error:
            findings.append(finding(source, "error", brace_start, str(error), name))
            table.valid = False
            continue
        after_struct = brace_end + 1
        while after_struct < len(source.masked) and source.masked[after_struct].isspace():
            after_struct += 1
        if after_struct >= len(source.masked) or source.masked[after_struct] != ";":
            findings.append(finding(source, "error", brace_end, "offset struct body is not followed by a semicolon", name))
            table.valid = False

        arrays = list(ARRAY_RE.finditer(source.masked, brace_start + 1, brace_end))
        if len(arrays) != 1:
            findings.append(finding(source, "error", struct_match.start(), f"offset table must contain exactly one std::array initializer; found {len(arrays)}", name))
            table.valid = False
            array_start, array_end = brace_start + 1, brace_end
        else:
            array_brace = source.masked.find("{", arrays[0].end(), brace_end)
            if array_brace < 0 or source.masked[arrays[0].end():array_brace].strip():
                findings.append(finding(source, "error", arrays[0].start(), "std::array must use a direct braced initializer", name))
                table.valid = False
                array_start, array_end = brace_start + 1, brace_end
            else:
                try:
                    array_end = matching_delimiter(source.masked, array_brace)
                    if array_end > brace_end:
                        raise ParseError("std::array initializer extends outside the offset struct")
                    array_start = array_brace + 1
                except ParseError as error:
                    findings.append(finding(source, "error", array_brace, str(error), name))
                    table.valid = False
                    array_start, array_end = brace_start + 1, brace_end

        entry_matches = list(ENTRY_RE.finditer(source.masked, array_start, array_end))
        entry_index = 0
        for entry_match in entry_matches:
            entry_brace = source.masked.find("{", entry_match.end(), brace_end)
            if entry_brace < 0:
                findings.append(finding(source, "error", entry_match.start(), "OffsetInfo has no initializer", name))
                table.valid = False
                continue
            between = source.masked[entry_match.end():entry_brace]
            if between.strip():
                findings.append(finding(source, "error", entry_match.start(), "unexpected tokens before OffsetInfo initializer", name))
                table.valid = False
                continue
            try:
                entry_end = matching_delimiter(source.masked, entry_brace)
                if entry_end > array_end:
                    raise ParseError("OffsetInfo initializer extends outside std::array")
            except ParseError as error:
                findings.append(finding(source, "error", entry_brace, str(error), name))
                table.valid = False
                continue
            entry_index += 1
            claimed_entries.add(entry_match.start())
            entry, entry_findings = parse_entry(
                source, table, entry_match.start(), entry_brace, entry_end, values, entry_index
            )
            findings.extend(entry_findings)
            if entry is None:
                table.valid = False
            else:
                table.entries_data.append(entry)
        if not entry_matches:
            findings.append(finding(source, "error", struct_match.start(), "table contains no OffsetInfo entries", name))
            table.valid = False
        for stray in ENTRY_RE.finditer(source.masked, brace_start + 1, brace_end):
            if stray.start() not in claimed_entries:
                findings.append(finding(source, "error", stray.start(), "OffsetInfo initializer is outside the table's std::array", name))
                table.valid = False
        validate_boundaries(source, table, findings)

    for entry_match in ENTRY_RE.finditer(source.masked):
        after = entry_match.end()
        while after < len(source.masked) and source.masked[after].isspace():
            after += 1
        if after < len(source.masked) and source.masked[after] == "{" and entry_match.start() not in claimed_entries:
            findings.append(finding(source, "error", entry_match.start(), "OffsetInfo initializer is outside a parsed offset table"))
    return tables, findings


def validate_boundaries(source: Source, table: Table, findings: list[Finding]) -> None:
    previous: Version | None = None
    seen: set[Version] = set()
    terminal_count = 0
    for entry in table.entries_data:
        assert entry.boundary is not None
        boundary = entry.boundary
        if boundary.build == 0:
            terminal_count += 1
            if boundary.revision != 0:
                findings.append(finding(source, "error", entry.start, "terminal build 0 must use revision 0", table.name, entry.index))
                table.valid = False
            if entry.index != len(table.entries_data):
                findings.append(finding(source, "error", entry.start, "terminal entry is not final", table.name, entry.index))
                table.valid = False
            continue
        if boundary in seen:
            findings.append(finding(source, "error", entry.start, f"duplicate boundary {boundary.build}.{boundary.revision}", table.name, entry.index))
            table.valid = False
        if previous is not None and not version_before(previous, boundary):
            findings.append(finding(source, "error", entry.start, f"boundary {boundary.build}.{boundary.revision} does not follow {previous.build}.{previous.revision}", table.name, entry.index))
            table.valid = False
        seen.add(boundary)
        previous = boundary
    if terminal_count > 1:
        findings.append(finding(source, "error", table.start, "table contains multiple terminal entries", table.name))
        table.valid = False


def version_before(runtime: Version, boundary: Version) -> bool:
    if boundary.build == 0:
        return True
    if boundary.revision == 0:
        return runtime.build < boundary.build
    return runtime.build < boundary.build or (
        runtime.build == boundary.build and runtime.revision < boundary.revision
    )


def select_entry(table: Table, requested: Version) -> dict[str, Any]:
    if not table.valid:
        return {"status": "invalid_table", "entry": None}
    left: Version | None = None
    for entry in table.entries_data:
        assert entry.boundary is not None
        if version_before(requested, entry.boundary):
            right = None if entry.boundary.build == 0 else entry.boundary.json()
            return {
                "status": "matched",
                "entry": entry.index,
                "offset_expression": entry.offset_expression,
                "interval": {"left": left.json() if left else None, "right": right},
            }
        left = entry.boundary
    return {"status": "unsupported", "entry": None}


def classify(sources: list[Source]) -> str:
    combined = "\n".join(source.masked for source in sources)
    if re.search(r"\bstruct\s+CVisual_GetScale_Offsets\b", combined):
        return "milcomp"
    if "TryDrawCommandAsDrawList" in combined or "CVisual_GetScale_MilSizeD_Offsets" in combined:
        return "legacy"
    return "unknown"


def load_source(repo: Path, relative: str) -> Source:
    path = repo / Path(relative)
    if not path.is_file():
        raise InputError(f"missing required file: {path}")
    try:
        text = path.read_text(encoding="utf-8-sig")
        return Source(relative, text, mask_non_code(text))
    except (OSError, UnicodeError, ParseError) as error:
        raise InputError(f"cannot read {path}: {error}") from error


def inspect_repo(repo: Path, requested: Version | None) -> dict[str, Any]:
    build_values, build_errors = parse_enum(repo, "os_build", "build_")
    revision_values, revision_errors = parse_enum(repo, "os_revision", "revision_")
    values = {**build_values, **revision_values}
    sources = [load_source(repo, relative) for relative in PROJECTION_FILES]
    all_tables: list[Table] = []
    findings: list[Finding] = []
    for source in sources:
        tables, source_findings = parse_projection(source, values)
        all_tables.extend(tables)
        findings.extend(source_findings)
    for message in build_errors + revision_errors:
        source = load_source(repo, "OpenGlass/OSHelper.hpp")
        findings.append(finding(source, "error", 0, message))

    definitions: dict[str, Table] = {}
    for table in all_tables:
        if table.name in definitions:
            source = next(item for item in sources if item.relative == table.file)
            findings.append(finding(source, "error", table.start, f"duplicate table definition; first defined in {definitions[table.name].file}", table.name))
            table.valid = False
            definitions[table.name].valid = False
        else:
            definitions[table.name] = table

    consumers: set[str] = set()
    for relative in CONSUMER_FILES:
        source = load_source(repo, relative)
        for match in CONSUMER_RE.finditer(source.masked):
            name = match.group(1)
            consumers.add(name)
            if name not in definitions:
                findings.append(finding(source, "error", match.start(), "consumer has no offset table definition", name))
    for table in all_tables:
        table.has_consumer = table.name in consumers
        if not table.has_consumer:
            source = next(item for item in sources if item.relative == table.file)
            findings.append(finding(source, "warning", table.start, "offset table has no PointerExecuteUnsafe consumer", table.name))

    tables_json: list[dict[str, Any]] = []
    for table in all_tables:
        item: dict[str, Any] = {
            "file": table.file,
            "name": table.name,
            "entries": len(table.entries_data),
            "terminal": bool(table.entries_data and table.entries_data[-1].boundary == Version(0, 0)),
            "has_consumer": table.has_consumer,
        }
        if requested is not None:
            item["selection"] = select_entry(table, requested)
        tables_json.append(item)

    errors = sum(item.severity == "error" for item in findings)
    warnings = sum(item.severity == "warning" for item in findings)
    summary: dict[str, Any] = {
        "files": len(sources),
        "tables": len(all_tables),
        "entries": sum(len(table.entries_data) for table in all_tables),
        "open_ended_tables": sum(bool(table.entries_data and table.entries_data[-1].boundary == Version(0, 0)) for table in all_tables),
        "errors": errors,
        "warnings": warnings,
    }
    if requested is not None:
        summary["matched_tables"] = sum(item["selection"]["status"] == "matched" for item in tables_json)
        summary["unsupported_tables"] = sum(item["selection"]["status"] == "unsupported" for item in tables_json)
        summary["invalid_tables"] = sum(item["selection"]["status"] == "invalid_table" for item in tables_json)
    return {
        "schema_version": 1,
        "status": "error" if errors else "ok",
        "branch_shape": classify(sources),
        "requested_version": requested.json() if requested else None,
        "tables": tables_json,
        "findings": [asdict(item) for item in findings],
        "summary": summary,
    }


def print_text(result: dict[str, Any]) -> None:
    summary = result["summary"]
    print(f"Branch shape: {result['branch_shape']}")
    if result["requested_version"]:
        version = result["requested_version"]
        print(f"Requested version: {version['build']}.{version['revision']}")
    print(f"Projection files: {summary['files']}")
    print(f"Tables: {summary['tables']}  entries: {summary['entries']}  open-ended: {summary['open_ended_tables']}")
    if result["requested_version"]:
        print(f"Matched: {summary['matched_tables']}  unsupported: {summary['unsupported_tables']}  invalid: {summary['invalid_tables']}")
        for table in result["tables"]:
            selection = table["selection"]
            if selection["status"] == "matched":
                print(f"MATCH: {table['name']} entry {selection['entry']}: {selection['offset_expression']}")
            else:
                print(f"{selection['status'].upper()}: {table['name']}")
    print(f"Errors: {summary['errors']}  warnings: {summary['warnings']}")
    for item in result["findings"]:
        location = f"{item['file']}:{item['line']}:{item['column']}"
        context = f":{item['table']}" if item["table"] else ""
        if item["entry"] is not None:
            context += f":entry {item['entry']}"
        print(f"{item['severity'].upper()}: {location}{context}: {item['message']}")


def error_result(message: str) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "status": "input_error",
        "branch_shape": None,
        "requested_version": None,
        "tables": [],
        "findings": [],
        "summary": {},
        "error": message,
    }


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        requested = parse_requested_version(args.version)
        repo = Path(args.repo).resolve()
        if not repo.is_dir():
            raise InputError(f"repository path is not a directory: {repo}")
        result = inspect_repo(repo, requested)
    except InputError as error:
        if args.format == "json":
            print(json.dumps(error_result(str(error)), indent=2, ensure_ascii=False))
        else:
            print(f"ERROR: {error}", file=sys.stderr)
        return 2
    except Exception as error:  # A stable CLI must not leak an implementation traceback.
        if args.format == "json":
            result = error_result(f"internal error: {error}")
            result["status"] = "internal_error"
            print(json.dumps(result, indent=2, ensure_ascii=False))
        else:
            print(f"INTERNAL ERROR: {error}", file=sys.stderr)
        return 3

    if args.format == "json":
        print(json.dumps(result, indent=2, ensure_ascii=False))
    else:
        print_text(result)
    return 1 if result["summary"]["errors"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
