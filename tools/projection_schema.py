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

from pygments.lexers import JsonLexer
from pygments.token import Name, Punctuation, String

from tools import projection_target

SCHEMA_VERSION = 5
UINT32_MAX = 0xFFFFFFFF
COMPLETE_NAME_CAPACITY = 64 * 1024
MODULES = ("udwm", "dwmcore")
ARCHITECTURES = {"legacy": "Legacy", "milcomp": "MILComp"}
REQUIREMENTS = {"required", "optional"}
SYMBOL_KINDS = {"raw", "projected_function", "projected_variable"}
ABI_COMPATIBILITIES = {"discard_return", "extra_trailing_argument"}
LAYOUT_KINDS = {"field", "vtable_slot"}

IDENTIFIER_RE = re.compile(r"[A-Za-z_]\w*\Z")
MIGRATION_PROVENANCE_RE = re.compile(r"\bmigrat(?:ed|ion)\s+(?:verbatim|mechanically)\b", re.IGNORECASE)


class SchemaError(Exception):
    """A projection schema or source-completeness invariant failed."""


class VersionSpec(TypedDict):
    build: int
    revision: NotRequired[int]


class SymbolBinding(TypedDict):
    symbol_names: list[str]
    min_inclusive: VersionSpec | None
    max_exclusive: VersionSpec | None


class LayoutCase(TypedDict):
    offset: int
    until: NotRequired[VersionSpec]
    otherwise: NotRequired[bool]


class Symbol(TypedDict):
    name: str
    id: str
    bindings: list[SymbolBinding]
    kind: Literal["raw", "projected_function", "projected_variable"]
    requirement: Literal["required", "optional"]
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
    min_inclusive: NotRequired[VersionSpec | None]
    max_exclusive: NotRequired[VersionSpec | None]
    known_builds: list[int]
    symbols: list[Symbol]
    layouts: list[Layout]
    notes: NotRequired[str | list[str]]


@dataclass(frozen=True, order=True)
class Version:
    build: int = 0
    revision: int = 0

    @classmethod
    def parse(cls, value: str) -> "Version":
        match = re.fullmatch(r"(?:10\.0\.)?([0-9]+)\.([0-9]+)", value)
        if match is None:
            raise SchemaError("--version must use BUILD.REVISION or 10.0.BUILD.REVISION")
        build, revision = map(int, match.groups())
        return parse_version({"build": build, "revision": revision}, "--version", allow_open=False)

    def json(self) -> dict[str, int]:
        return {"build": self.build, "revision": self.revision}


def in_range(version: Version, item: dict[str, Any]) -> bool:
    """Select from validated schema ranges; input validation belongs to the loader."""
    minimum = Version(**item["min_inclusive"]) if item.get("min_inclusive") else Version()
    maximum = Version(**item["max_exclusive"]) if item.get("max_exclusive") else Version()
    return version >= minimum and before(version, maximum)


def select_binding(version: Version, symbol: Symbol, configuration: str) -> tuple[int, SymbolBinding] | None:
    if symbol.get("condition") == "debug" and configuration != "debug":
        return None
    for index, binding in enumerate(symbol["bindings"]):
        if in_range(version, binding):
            return index, binding
    return None


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
        if MIGRATION_PROVENANCE_RE.search(checked):
            raise SchemaError(f"{context}[{index}] must contain durable evidence, not migration provenance")
        result.append(checked)
    return result


def require_optional_note(value: Any, context: str) -> str | None:
    if value is None:
        return None
    result = require_string(value, context)
    if MIGRATION_PROVENANCE_RE.search(result):
        raise SchemaError(f"{context} must contain durable evidence, not migration provenance")
    return result


def parse_uint(value: Any, context: str, *, nonzero: bool = False) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= UINT32_MAX or (nonzero and value == 0):
        raise SchemaError(f"{context} must be a {'nonzero ' if nonzero else ''}unsigned 32-bit integer")
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


def normalize_target(target: str) -> str:
    return re.sub(r"\s+", "", target)


def version_key(value: Any) -> tuple[int, int]:
    if value is None:
        return (0, 0)
    return (int(value["build"]), int(value.get("revision", 0)))


def _validate_symbol_binding(item: dict[str, Any], context: str) -> tuple[Version, Version]:
    allowed = {"symbol_names", "min_inclusive", "max_exclusive"}
    extra = set(item) - allowed
    if extra:
        raise SchemaError(f"{context} has unknown fields: {', '.join(sorted(extra))}")

    candidates = require_list(item.get("symbol_names"), f"{context}.symbol_names")
    if not candidates:
        raise SchemaError(f"{context}.symbol_names must not be empty")
    checked_candidates = [
        require_symbol_name(candidate, f"{context}.symbol_names[{index}]")
        for index, candidate in enumerate(candidates)
    ]
    if len(set(checked_candidates)) != len(checked_candidates):
        raise SchemaError(f"{context}.symbol_names contains a duplicate candidate")

    minimum = parse_version(item.get("min_inclusive"), f"{context}.min_inclusive")
    maximum = parse_version(item.get("max_exclusive"), f"{context}.max_exclusive")
    if maximum.build and not minimum < maximum:
        raise SchemaError(f"{context}: max_exclusive must follow min_inclusive")
    return minimum, maximum


def _validate_symbol(item: dict[str, Any], context: str) -> list[tuple[Version, Version]]:
    allowed = {
        "name", "id", "bindings", "kind", "type", "target", "requirement",
        "fallback", "condition", "diagnostic_only", "usage", "notes", "abi_compatibility",
    }
    extra = set(item) - allowed
    if extra:
        raise SchemaError(f"{context} has unknown fields: {', '.join(sorted(extra))}")

    require_optional_note(item.get("notes"), f"{context}.notes")
    require_string(item.get("id"), f"{context}.id")
    raw_bindings = require_list(item.get("bindings"), f"{context}.bindings")
    if not raw_bindings:
        raise SchemaError(f"{context}.bindings must not be empty")
    ranges: list[tuple[Version, Version]] = []
    for index, raw_binding in enumerate(raw_bindings):
        binding_context = f"{context}.bindings[{index}]"
        binding = require_object(raw_binding, binding_context)
        current_range = _validate_symbol_binding(binding, binding_context)
        if ranges and current_range[0] < ranges[-1][0]:
            raise SchemaError(f"{binding_context}: bindings must be ordered by min_inclusive")
        for prior_range in ranges:
            if ranges_overlap(current_range, prior_range):
                raise SchemaError(f"{binding_context}: binding range overlaps a prior binding")
        ranges.append(current_range)

    kind = require_string(item.get("kind"), f"{context}.kind")
    if kind not in SYMBOL_KINDS:
        raise SchemaError(f"{context}.kind must be one of {', '.join(sorted(SYMBOL_KINDS))}")
    requirement = require_string(item.get("requirement"), f"{context}.requirement")
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
            if require_string(compatibility, f"{context}.abi_compatibility") not in ABI_COMPATIBILITIES:
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
    return ranges


def _validate_symbols(path: Path, symbols: list[dict[str, Any]]) -> list[Symbol]:
    ranges_by_id: dict[str, list[tuple[Version, Version, str]]] = {}
    ranges_by_candidate: dict[str, list[tuple[Version, Version, str]]] = {}
    ranges_by_function_target: dict[str, list[tuple[Version, Version, str, str | None]]] = {}
    ranges_by_variable_target: dict[str, list[tuple[Version, Version, str]]] = {}

    for index, item in enumerate(symbols):
        context = f"{path}: symbols[{index}]"
        ranges = _validate_symbol(item, context)
        name = cast(str, item["name"])
        stable_id = cast(str, item["id"])
        kind = cast(str, item["kind"])

        for minimum, maximum in ranges:
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

        for binding, (minimum, maximum) in zip(cast(list[SymbolBinding], item["bindings"]), ranges):
            for candidate in binding["symbol_names"]:
                for prior_min, prior_max, prior_name in ranges_by_candidate.setdefault(candidate, []):
                    if ranges_overlap((minimum, maximum), (prior_min, prior_max)):
                        raise SchemaError(
                            f"{context}: complete symbol name overlaps descriptor {prior_name}: {candidate}"
                        )
                ranges_by_candidate[candidate].append((minimum, maximum, name))

    return cast(list[Symbol], sorted(
        symbols,
        key=lambda item: (item["id"], version_key(item["bindings"][0].get("min_inclusive")), item["name"]),
    ))


def _validate_layout(item: dict[str, Any], context: str) -> None:
    allowed = {"name", "id", "kind", "type", "cases", "notes"}
    extra = set(item) - allowed
    if extra:
        raise SchemaError(f"{context} has unknown fields: {', '.join(sorted(extra))}")
    require_optional_note(item.get("notes"), f"{context}.notes")
    require_string(item.get("id"), f"{context}.id")
    if require_string(item.get("kind"), f"{context}.kind") not in LAYOUT_KINDS:
        raise SchemaError(f"{context}.kind must be field or vtable_slot")
    require_string(item.get("type"), f"{context}.type")
    cases = require_list(item.get("cases"), f"{context}.cases")
    if not cases:
        raise SchemaError(f"{context}.cases must not be empty")

    prior_boundary: Version | None = None
    for index, raw_case in enumerate(cases):
        case_context = f"{context}.cases[{index}]"
        case = require_object(raw_case, case_context)
        extra_case = set(case) - {"offset", "until", "otherwise"}
        if extra_case:
            raise SchemaError(f"{case_context} has unknown fields: {', '.join(sorted(extra_case))}")
        offset = case.get("offset")
        if isinstance(offset, bool) or not isinstance(offset, int) or not -0x80000000 <= offset <= 0x7FFFFFFF:
            raise SchemaError(f"{case_context}.offset must be a signed 32-bit integer byte offset")
        has_until = "until" in case
        has_otherwise = case.get("otherwise") is True
        if has_until == has_otherwise:
            raise SchemaError(f"{case_context} must define exactly one of until or otherwise=true")
        if has_otherwise:
            if index + 1 != len(cases):
                raise SchemaError(f"{case_context}: otherwise must be final")
            continue

        current = parse_version(case["until"], f"{case_context}.until", allow_open=False)
        if prior_boundary is not None and current <= prior_boundary:
            raise SchemaError(f"{case_context}: boundaries must be strictly increasing")
        prior_boundary = current


def _validate_layouts(path: Path, layouts: list[dict[str, Any]]) -> list[Layout]:
    stable_ids: set[str] = set()
    for index, item in enumerate(layouts):
        context = f"{path}: layouts[{index}]"
        _validate_layout(item, context)
        stable_id = cast(str, item["id"])
        if stable_id in stable_ids:
            raise SchemaError(f"{context}: duplicate layout name or stable ID")
        stable_ids.add(stable_id)
    return cast(list[Layout], sorted(layouts, key=lambda item: (item["id"], item["name"])))


def section_paths(path: Path, section: str, references: Any) -> list[Path]:
    """Resolve the manifest's explicit, non-recursive section file list."""
    return [
        path.parent / require_string(reference, f"{path}: {section}[{index}]")
        for index, reference in enumerate(require_list(references, f"{path}: {section}"))
    ]


def read_section(path: Path, section: str, references: Any) -> list[Any]:
    entries: list[Any] = []
    for source in section_paths(path, section, references):
        fragment = read_json(source)
        if set(fragment) != {section}:
            raise SchemaError(f"{source}: section file must contain only {section}")
        entries.extend(require_list(fragment[section], f"{source}: {section}"))
    return entries


def _platform(item: dict[str, Any], context: str) -> str | None:
    if item.get("platform") is None:
        return None
    value = require_string(item["platform"], f"{context}.platform")
    try:
        return projection_target.normalize_platform(value)
    except ValueError as error:
        raise SchemaError(f"{context}: {error}") from error


SYMBOL_GROUP_FIELDS = {"type", "target", "fallback", "abi_compatibility"}


def _select_group(value: Any, platform: str, context: str) -> dict[str, Any] | None:
    groups = require_list(value, context)
    if not groups:
        raise SchemaError(f"{context} must not be empty")
    selected: dict[str | None, dict[str, Any]] = {}
    for index, raw in enumerate(groups):
        group = require_object(raw, f"{context}[{index}]")
        target = _platform(group, f"{context}[{index}]")
        if target not in (None, platform):
            continue
        if target in selected:
            raise SchemaError(f"{context}: duplicate {target or 'common'} platform group")
        selected[target] = group
    return selected.get(platform, selected.get(None))


def select_descriptors(path: Path, section: str, raw: list[Any], platform: str) -> list[Any]:
    """Select one complete range group per descriptor, then validate its fields."""
    names: set[str] = set()
    selected = []
    ranges_key = "bindings" if section == "symbols" else "cases"
    group_fields = SYMBOL_GROUP_FIELDS if section == "symbols" else {"type"}
    for index, value in enumerate(raw):
        context = f"{path}: {section}[{index}]"
        item = require_object(value, context)
        name = require_string(item.get("name"), f"{context}.name", identifier=True)
        if name in names:
            label = "C++ handle name" if section == "symbols" else "layout name or stable ID"
            raise SchemaError(f"{context}: duplicate {label} {name}")
        names.add(name)
        group = _select_group(item.get(ranges_key), platform, f"{context}.{ranges_key}")
        if group is None:
            continue
        extra = set(group) - {"platform", "ranges"} - group_fields
        if extra:
            raise SchemaError(f"{context}.{ranges_key}: unknown group fields: {', '.join(sorted(extra))}")
        if section == "symbols" and set(item) & SYMBOL_GROUP_FIELDS:
            raise SchemaError(f"{context}: ABI fields belong in the bindings platform group")
        descriptor = dict(item)
        descriptor.update({key: group[key] for key in group_fields if key in group})
        descriptor[ranges_key] = require_list(group.get("ranges"), f"{context}.{ranges_key}.ranges")
        selected.append(descriptor)
    validate = _validate_symbols if section == "symbols" else _validate_layouts
    return validate(path, selected)


def _name_positions(text: str) -> dict[int, int]:
    """Locate name values in the section's descriptor objects, not nested data."""
    positions: dict[int, int] = {}
    depth, index = 0, -1
    is_name = False
    for offset, token, value in JsonLexer().get_tokens_unprocessed(text):
        if token in Punctuation:
            for char in value:
                if char == "{":
                    if depth == 1:
                        index += 1
                    depth += 1
                elif char == "}":
                    depth -= 1
        elif depth == 2:
            if token in Name.Tag:
                is_name = json.loads(value) == "name"
            elif is_name and token in String.Double:
                positions[index] = offset
                is_name = False
    return positions


def section_locations(path: Path, section: str, platform: str = "x64") -> dict[str, tuple[Path, int, int]]:
    """Locate declarations after validate_schema has checked the manifest and sections."""
    result: dict[str, tuple[Path, int, int]] = {}
    platform = projection_target.normalize_platform(platform)
    manifest = read_json(path)
    for reference in manifest[section]:
        source = path.parent / reference
        text = source.read_text(encoding="utf-8")
        positions = _name_positions(text)
        for index, item in enumerate(json.loads(text)[section]):
            ranges_key = "bindings" if section == "symbols" else "cases"
            if _select_group(item[ranges_key], platform, str(source)) is None:
                continue
            position = positions[index]
            result[item["name"]] = (
                source,
                text.count("\n", 0, position) + 1,
                position - text.rfind("\n", 0, position),
            )
    return result


def validate_schema(path: Path, expected_module: str, platform: str = "x64") -> ProjectionSchema:
    """Load, validate, and deterministically order one module schema."""
    schema = read_json(path)
    platform = projection_target.normalize_platform(platform)
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

    schema["symbols"] = select_descriptors(path, "symbols", read_section(path, "symbols", schema.get("symbols")), platform)
    schema["layouts"] = select_descriptors(path, "layouts", read_section(path, "layouts", schema.get("layouts")), platform)
    return cast(ProjectionSchema, schema)
