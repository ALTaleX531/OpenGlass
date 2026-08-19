"""Projection schema types and validation.

This module validates JSON data only. It does not inspect C++ sources, generate
C++, or write files.
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Literal, NotRequired, TypedDict, cast


SCHEMA_VERSION = 3
UINT32_MAX = 0xFFFFFFFF
COMPLETE_NAME_CAPACITY = 64 * 1024
MODULES = ("udwm", "dwmcore")
ARCHITECTURES = {"legacy": "Legacy", "milcomp": "MILComp"}
REQUIREMENTS = {"required", "optional"}
SYMBOL_KINDS = {"raw", "projected_function", "projected_variable"}
ABI_COMPATIBILITIES = {"discard_return", "extra_trailing_argument"}
LAYOUT_KINDS = {"field", "vtable_slot"}

IDENTIFIER_RE = re.compile(r"[A-Za-z_]\w*\Z")
INTEGER_RE = re.compile(r"(?:0[xX][0-9A-Fa-f]+|[0-9]+)[uUlL]*\Z")
MIGRATION_PROVENANCE_RE = re.compile(r"Migrated\s+mechanically\s+from\b")


class SchemaError(Exception):
    """A projection schema or source-completeness invariant failed."""


class SymbolVersion(TypedDict):
    build: int
    revision: NotRequired[int]


class LayoutBoundary(TypedDict):
    build: str
    revision: NotRequired[str]


class LayoutCase(TypedDict):
    offset: str
    until: NotRequired[LayoutBoundary]
    otherwise: NotRequired[bool]


class Symbol(TypedDict):
    name: str
    id: str
    symbol_names: list[str]
    kind: Literal["raw", "projected_function", "projected_variable"]
    requirement: Literal["required", "optional"]
    min_inclusive: SymbolVersion | None
    max_exclusive: SymbolVersion | None
    type: NotRequired[str]
    target: NotRequired[str]
    fallback: NotRequired[str]
    condition: NotRequired[Literal["debug"]]
    diagnostic_only: NotRequired[bool]
    usage: NotRequired[Literal["code_address"]]
    notes: NotRequired[str]
    abi_compatibility: NotRequired[Literal["discard_return", "extra_trailing_argument"]]


class Layout(TypedDict):
    name: str
    id: str
    kind: Literal["field", "vtable_slot"]
    type: str
    cases: list[LayoutCase]
    notes: NotRequired[str]


class ProjectionSchema(TypedDict):
    schema_version: int
    module: Literal["udwm", "dwmcore"]
    namespace: str
    tag: str
    min_inclusive: NotRequired[SymbolVersion | None]
    max_exclusive: NotRequired[SymbolVersion | None]
    known_builds: list[int]
    symbols: list[Symbol]
    layouts: list[Layout]
    notes: NotRequired[str | list[str]]


@dataclass(frozen=True, order=True)
class Version:
    build: int = 0
    revision: int = 0


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise SchemaError(f"cannot read {path}: {error}") from error
    if not isinstance(value, dict):
        raise SchemaError(f"{path}: schema root must be an object")
    return value


def require_object(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise SchemaError(f"{context} must be an object")
    return value


def require_list(value: Any, context: str) -> list[Any]:
    if not isinstance(value, list):
        raise SchemaError(f"{context} must be an array")
    return value


def require_string(value: Any, context: str, *, identifier: bool = False) -> str:
    if not isinstance(value, str) or not value:
        raise SchemaError(f"{context} must be a non-empty string")
    if identifier and not IDENTIFIER_RE.fullmatch(value):
        raise SchemaError(f"{context} must be a C++ identifier")
    return value


def require_symbol_name(value: Any, context: str) -> str:
    result = require_string(value, context)
    if any(ord(character) < 0x20 or ord(character) > 0x7E for character in result):
        raise SchemaError(f"{context} must contain printable ASCII without NUL or control characters")
    if len(result.encode("ascii")) + 1 > COMPLETE_NAME_CAPACITY:
        raise SchemaError(f"{context} exceeds the DbgHelp complete-name buffer capacity")
    return result


def require_bool(value: Any, context: str) -> bool:
    if not isinstance(value, bool):
        raise SchemaError(f"{context} must be a boolean")
    return value


def require_notes(value: Any, context: str) -> list[str]:
    if value is None:
        return []
    values = value if isinstance(value, list) else [value]
    result: list[str] = []
    for index, note in enumerate(values):
        checked = require_string(note, f"{context}[{index}]")
        if MIGRATION_PROVENANCE_RE.match(checked):
            raise SchemaError(f"{context}[{index}] must contain durable evidence, not migration provenance")
        result.append(checked)
    return result


def require_optional_note(value: Any, context: str) -> str | None:
    if value is None:
        return None
    result = require_string(value, context)
    if MIGRATION_PROVENANCE_RE.match(result):
        raise SchemaError(f"{context} must contain durable evidence, not migration provenance")
    return result


def parse_uint(value: Any, context: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= UINT32_MAX:
        raise SchemaError(f"{context} must be an unsigned 32-bit integer")
    return value


def parse_version(value: Any, context: str, *, allow_open: bool = True) -> Version:
    if value is None and allow_open:
        return Version()
    item = require_object(value, context)
    unknown = set(item) - {"build", "revision"}
    if unknown:
        raise SchemaError(f"{context} has unknown fields: {', '.join(sorted(unknown))}")
    build = parse_uint(item.get("build"), f"{context}.build")
    revision = parse_uint(item.get("revision", 0), f"{context}.revision")
    if not build:
        raise SchemaError(f"{context}.build must be nonzero")
    return Version(build, revision)


def before(left: Version, right: Version) -> bool:
    return not right.build or left < right


def ranges_overlap(left: tuple[Version, Version], right: tuple[Version, Version]) -> bool:
    return before(left[0], right[1]) and before(right[0], left[1])


def validate_boundary_expression(expression: Any, constants: dict[str, int], context: str, *, build: bool) -> str:
    value = require_string(expression, context).strip()
    if INTEGER_RE.fullmatch(value):
        parsed = int(re.sub(r"[uUlL]+$", "", value), 0)
        if parsed > UINT32_MAX or (build and not parsed):
            raise SchemaError(f"{context} is outside the supported unsigned range")
        return value
    name = value.removeprefix("os::")
    prefix = "build_" if build else "revision_"
    if not name.startswith(prefix) or name not in constants:
        raise SchemaError(f"{context} references unknown {prefix[:-1]} constant: {value}")
    return value


def resolve_boundary_expression(expression: Any, constants: dict[str, int], context: str, *, build: bool) -> int:
    value = validate_boundary_expression(expression, constants, context, build=build)
    if INTEGER_RE.fullmatch(value):
        return int(re.sub(r"[uUlL]+$", "", value), 0)
    return constants[value.removeprefix("os::")]


def normalize_target(target: str) -> str:
    return re.sub(r"\s+", "", target)


def version_key(value: Any) -> tuple[int, int]:
    if value is None:
        return (0, 0)
    return (int(value["build"]), int(value.get("revision", 0)))


def _validate_symbol(item: dict[str, Any], context: str) -> tuple[Version, Version]:
    allowed = {
        "name", "id", "symbol_names", "kind", "type", "target", "requirement",
        "min_inclusive", "max_exclusive", "fallback", "condition", "diagnostic_only", "usage", "notes",
        "abi_compatibility",
    }
    extra = set(item) - allowed
    if extra:
        raise SchemaError(f"{context} has unknown fields: {', '.join(sorted(extra))}")

    require_optional_note(item.get("notes"), f"{context}.notes")
    require_string(item.get("name"), f"{context}.name", identifier=True)
    require_string(item.get("id"), f"{context}.id")
    candidates = require_list(item.get("symbol_names"), f"{context}.symbol_names")
    if not candidates:
        raise SchemaError(f"{context}.symbol_names must not be empty")
    checked_candidates = [
        require_symbol_name(candidate, f"{context}.symbol_names[{index}]")
        for index, candidate in enumerate(candidates)
    ]
    if len(set(checked_candidates)) != len(checked_candidates):
        raise SchemaError(f"{context}.symbol_names contains a duplicate candidate")

    kind = require_string(item.get("kind"), f"{context}.kind")
    if kind not in SYMBOL_KINDS:
        raise SchemaError(f"{context}.kind must be one of {', '.join(sorted(SYMBOL_KINDS))}")
    requirement = item.get("requirement")
    if requirement not in REQUIREMENTS:
        raise SchemaError(f"{context}.requirement must be required or optional")

    if kind == "raw":
        raw_type = require_string(item.get("type"), f"{context}.type")
        usage = item.get("usage")
        if raw_type in {"PVOID", "void*", "LPVOID"}:
            raise SchemaError(f"{context}.type must preserve the raw symbol's typed ABI")
        if usage not in (None, "code_address"):
            raise SchemaError(f"{context}.usage only supports code_address")
        if raw_type == "BYTE*" and usage != "code_address":
            raise SchemaError(f"{context}: BYTE* raw symbols must declare usage code_address")
        if usage == "code_address" and raw_type != "BYTE*":
            raise SchemaError(f"{context}: code_address raw symbols must use BYTE*")
        if item.get("target") is not None or item.get("fallback") is not None:
            raise SchemaError(f"{context}: raw symbols cannot define target or fallback")
        if item.get("abi_compatibility") is not None:
            raise SchemaError(f"{context}: raw symbols cannot define abi_compatibility")
    else:
        if item.get("usage") is not None:
            raise SchemaError(f"{context}: projected symbols cannot define usage")
        require_string(item.get("target"), f"{context}.target")
        compatibility = item.get("abi_compatibility")
        if kind == "projected_variable":
            if item.get("type") is not None:
                raise SchemaError(f"{context}: projected variables derive their type from target")
            if compatibility is not None:
                raise SchemaError(f"{context}: projected variables cannot define abi_compatibility")
            if item.get("fallback") is not None:
                raise SchemaError(f"{context}: projected variables cannot define fallback")
        elif compatibility is None:
            if item.get("type") is not None:
                raise SchemaError(f"{context}: projected functions derive their type from target unless ABI compatibility is explicit")
        else:
            if compatibility not in ABI_COMPATIBILITIES:
                raise SchemaError(
                    f"{context}.abi_compatibility must be one of {', '.join(sorted(ABI_COMPATIBILITIES))}"
                )
            require_string(item.get("type"), f"{context}.type")
        if requirement == "optional" and kind == "projected_function" and not item.get("fallback"):
            raise SchemaError(f"{context}: optional projected functions require an ABI fallback")

    if item.get("fallback") is not None:
        require_string(item["fallback"], f"{context}.fallback")
    condition = item.get("condition")
    if condition not in (None, "debug"):
        raise SchemaError(f"{context}.condition only supports debug")
    if "diagnostic_only" in item:
        require_bool(item["diagnostic_only"], f"{context}.diagnostic_only")

    minimum = parse_version(item.get("min_inclusive"), f"{context}.min_inclusive")
    maximum = parse_version(item.get("max_exclusive"), f"{context}.max_exclusive")
    if maximum.build and not minimum < maximum:
        raise SchemaError(f"{context}: max_exclusive must follow min_inclusive")
    return minimum, maximum


def _validate_symbols(path: Path, raw_symbols: Any) -> list[Symbol]:
    symbols = require_list(raw_symbols, f"{path}: symbols")
    symbol_handles: set[str] = set()
    ranges_by_id: dict[str, list[tuple[Version, Version, str]]] = {}
    ranges_by_candidate: dict[str, list[tuple[Version, Version, str]]] = {}
    ranges_by_function_target: dict[str, list[tuple[Version, Version, str, str | None]]] = {}
    ranges_by_variable_target: dict[str, list[tuple[Version, Version, str]]] = {}

    for index, raw in enumerate(symbols):
        context = f"{path}: symbols[{index}]"
        item = require_object(raw, context)
        minimum, maximum = _validate_symbol(item, context)
        name = cast(str, item["name"])
        stable_id = cast(str, item["id"])
        kind = cast(str, item["kind"])

        if name in symbol_handles:
            raise SchemaError(f"{context}: duplicate C++ handle name {name}")
        symbol_handles.add(name)

        for prior_min, prior_max, prior_name in ranges_by_id.setdefault(stable_id, []):
            if ranges_overlap((minimum, maximum), (prior_min, prior_max)):
                raise SchemaError(f"{context}: stable ID range overlaps {prior_name}")
        ranges_by_id[stable_id].append((minimum, maximum, name))

        if kind == "projected_function":
            target = normalize_target(cast(str, item["target"]))
            fallback = cast(str | None, item.get("fallback"))
            for prior_min, prior_max, prior_name, prior_fallback in ranges_by_function_target.setdefault(target, []):
                if ranges_overlap((minimum, maximum), (prior_min, prior_max)):
                    raise SchemaError(f"{context}: projected target range overlaps {prior_name}")
                if fallback != prior_fallback:
                    raise SchemaError(f"{context}: projected target variants must use the same fallback")
            ranges_by_function_target[target].append((minimum, maximum, name, fallback))
        elif kind == "projected_variable":
            target = normalize_target(cast(str, item["target"]))
            for prior_min, prior_max, prior_name in ranges_by_variable_target.setdefault(target, []):
                if ranges_overlap((minimum, maximum), (prior_min, prior_max)):
                    raise SchemaError(f"{context}: projected variable target range overlaps {prior_name}")
            ranges_by_variable_target[target].append((minimum, maximum, name))

        for candidate in cast(list[str], item["symbol_names"]):
            for prior_min, prior_max, prior_name in ranges_by_candidate.setdefault(candidate, []):
                if ranges_overlap((minimum, maximum), (prior_min, prior_max)):
                    raise SchemaError(
                        f"{context}: complete symbol name overlaps descriptor {prior_name}: {candidate}"
                    )
            ranges_by_candidate[candidate].append((minimum, maximum, name))

    return cast(list[Symbol], sorted(symbols, key=lambda item: (item["id"], version_key(item.get("min_inclusive")), item["name"])))


def _validate_layout(item: dict[str, Any], context: str, constants: dict[str, int]) -> None:
    allowed = {"name", "id", "kind", "type", "cases", "notes"}
    extra = set(item) - allowed
    if extra:
        raise SchemaError(f"{context} has unknown fields: {', '.join(sorted(extra))}")
    require_optional_note(item.get("notes"), f"{context}.notes")
    require_string(item.get("name"), f"{context}.name", identifier=True)
    require_string(item.get("id"), f"{context}.id")
    if item.get("kind") not in LAYOUT_KINDS:
        raise SchemaError(f"{context}.kind must be field or vtable_slot")
    require_string(item.get("type"), f"{context}.type")
    cases = require_list(item.get("cases"), f"{context}.cases")
    if not cases:
        raise SchemaError(f"{context}.cases must not be empty")

    prior_boundary: tuple[int, int] | None = None
    for index, raw_case in enumerate(cases):
        case_context = f"{context}.cases[{index}]"
        case = require_object(raw_case, case_context)
        extra_case = set(case) - {"offset", "until", "otherwise"}
        if extra_case:
            raise SchemaError(f"{case_context} has unknown fields: {', '.join(sorted(extra_case))}")
        require_string(case.get("offset"), f"{case_context}.offset")
        has_until = "until" in case
        has_otherwise = case.get("otherwise") is True
        if has_until == has_otherwise:
            raise SchemaError(f"{case_context} must define exactly one of until or otherwise=true")
        if has_otherwise:
            if index + 1 != len(cases):
                raise SchemaError(f"{case_context}: otherwise must be final")
            continue

        boundary = require_object(case["until"], f"{case_context}.until")
        if set(boundary) - {"build", "revision"}:
            raise SchemaError(f"{case_context}.until has unknown fields")
        current = (
            resolve_boundary_expression(boundary.get("build"), constants, f"{case_context}.until.build", build=True),
            resolve_boundary_expression(boundary.get("revision", "0"), constants, f"{case_context}.until.revision", build=False),
        )
        if prior_boundary is not None and current <= prior_boundary:
            raise SchemaError(f"{case_context}: boundaries must be strictly increasing")
        prior_boundary = current


def _validate_layouts(path: Path, raw_layouts: Any, constants: dict[str, int]) -> list[Layout]:
    layouts = require_list(raw_layouts, f"{path}: layouts")
    names: set[str] = set()
    stable_ids: set[str] = set()
    for index, raw in enumerate(layouts):
        context = f"{path}: layouts[{index}]"
        item = require_object(raw, context)
        _validate_layout(item, context, constants)
        name = cast(str, item["name"])
        stable_id = cast(str, item["id"])
        if name in names or stable_id in stable_ids:
            raise SchemaError(f"{context}: duplicate layout name or stable ID")
        names.add(name)
        stable_ids.add(stable_id)
    return cast(list[Layout], sorted(layouts, key=lambda item: (item["id"], item["name"])))


def validate_schema(path: Path, expected_module: str, constants: dict[str, int]) -> ProjectionSchema:
    """Load, validate, and deterministically order one module schema."""
    schema = read_json(path)
    allowed_root = {
        "schema_version", "module", "namespace", "tag", "min_inclusive", "max_exclusive",
        "known_builds", "symbols", "layouts", "notes",
    }
    unknown = set(schema) - allowed_root
    if unknown:
        raise SchemaError(f"{path}: unknown root fields: {', '.join(sorted(unknown))}")
    if schema.get("schema_version") != SCHEMA_VERSION:
        raise SchemaError(f"{path}: schema_version must be {SCHEMA_VERSION}")
    if schema.get("module") != expected_module:
        raise SchemaError(f"{path}: module must be {expected_module}")
    require_string(schema.get("namespace"), f"{path}: namespace")
    require_string(schema.get("tag"), f"{path}: tag", identifier=True)
    require_notes(schema.get("notes"), f"{path}: notes")
    minimum = parse_version(schema.get("min_inclusive"), f"{path}: min_inclusive")
    maximum = parse_version(schema.get("max_exclusive"), f"{path}: max_exclusive")
    if maximum.build and not minimum < maximum:
        raise SchemaError(f"{path}: max_exclusive must follow min_inclusive")
    raw_known_builds = require_list(schema.get("known_builds"), f"{path}: known_builds")
    if not raw_known_builds:
        raise SchemaError(f"{path}: known_builds must not be empty")
    known_builds = [
        parse_uint(value, f"{path}: known_builds[{index}]")
        for index, value in enumerate(raw_known_builds)
    ]
    if any(not build for build in known_builds):
        raise SchemaError(f"{path}: known_builds must contain nonzero builds")
    if any(current <= prior for prior, current in zip(known_builds, known_builds[1:])):
        raise SchemaError(f"{path}: known_builds must be strictly increasing")
    for build in known_builds:
        build_maximum = Version(build + 1, 0) if build < UINT32_MAX else Version()
        if not ranges_overlap((Version(build, 0), build_maximum), (minimum, maximum)):
            raise SchemaError(f"{path}: known build {build} is outside the module version range")
    schema["known_builds"] = known_builds

    schema["symbols"] = _validate_symbols(path, schema.get("symbols"))
    schema["layouts"] = _validate_layouts(path, schema.get("layouts"), constants)
    return cast(ProjectionSchema, schema)
