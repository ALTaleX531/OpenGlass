#!/usr/bin/env python3
"""Validate OpenGlass projection schemas and generate typed C++ metadata."""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable


SCHEMA_VERSION = 2
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
INVOKE_RE = re.compile(r"Projection\s*::\s*Invoke\s*<\s*(&[^>]+)\s*>")
MUSTTAIL_INVOKE_RE = re.compile(r"OPENGLASS_MUSTTAIL\s+return\s+Projection\s*::\s*Invoke\s*<\s*(&[^>]+)\s*>")
INLINE_INVOKE_RE = re.compile(
    r"\binline\b[^{};]*\{[^{}]*?Projection\s*::\s*Invoke\s*<\s*(&[^>]+)\s*>", re.DOTALL
)
class SchemaError(Exception):
    """A schema or source-completeness invariant failed."""


@dataclass(frozen=True, order=True)
class Version:
    build: int = 0
    revision: int = 0


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--architecture", choices=ARCHITECTURES, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true", help="validate without writing generated files")
    return parser.parse_args(argv)


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


def load_os_constants(repo: Path) -> dict[str, int]:
    path = repo / "OpenGlass" / "OSHelper.hpp"
    try:
        text = strip_non_code(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError) as error:
        raise SchemaError(f"cannot read {path}: {error}") from error
    values: dict[str, int] = {}
    pending = re.findall(r"\b((?:build|revision)_[A-Za-z0-9_]+)\s*=\s*([^,}]+)", text)
    for _ in range(len(pending) + 1):
        changed = False
        for name, expression in pending:
            if name in values:
                continue
            raw = expression.strip()
            if INTEGER_RE.fullmatch(raw):
                values[name] = int(re.sub(r"[uUlL]+$", "", raw), 0)
                changed = True
                continue
            alias = raw.removeprefix("os::")
            if alias in values:
                values[name] = values[alias]
                changed = True
        if not changed:
            break
    unresolved = sorted(name for name, _ in pending if name not in values)
    if unresolved:
        raise SchemaError(f"cannot resolve OS constants in {path}: {', '.join(unresolved)}")
    return values


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


def validate_schema(repo: Path, path: Path, expected_module: str, constants: dict[str, int]) -> dict[str, Any]:
    schema = read_json(path)
    allowed_root = {"schema_version", "module", "namespace", "tag", "symbols", "layouts", "notes"}
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

    symbols = require_list(schema.get("symbols"), f"{path}: symbols")
    symbol_handles: set[str] = set()
    ranges_by_id: dict[str, list[tuple[Version, Version, str]]] = {}
    ranges_by_candidate: dict[str, list[tuple[Version, Version, str]]] = {}
    ranges_by_function_target: dict[str, list[tuple[Version, Version, str, str | None]]] = {}
    ranges_by_variable_target: dict[str, list[tuple[Version, Version, str]]] = {}
    for index, raw in enumerate(symbols):
        context = f"{path}: symbols[{index}]"
        item = require_object(raw, context)
        allowed = {
            "name", "id", "symbol_names", "kind", "type", "target", "requirement",
            "min_inclusive", "max_exclusive", "fallback", "condition", "diagnostic_only", "usage", "notes",
            "abi_compatibility"
        }
        extra = set(item) - allowed
        if extra:
            raise SchemaError(f"{context} has unknown fields: {', '.join(sorted(extra))}")
        require_optional_note(item.get("notes"), f"{context}.notes")
        name = require_string(item.get("name"), f"{context}.name", identifier=True)
        stable_id = require_string(item.get("id"), f"{context}.id")
        candidates = require_list(item.get("symbol_names"), f"{context}.symbol_names")
        if not candidates:
            raise SchemaError(f"{context}.symbol_names must not be empty")
        checked_candidates = [
            require_symbol_name(candidate, f"{context}.symbol_names[{candidate_index}]")
            for candidate_index, candidate in enumerate(candidates)
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
            target = require_string(item.get("target"), f"{context}.target")
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
        if name in symbol_handles:
            raise SchemaError(f"{context}: duplicate C++ handle name {name}")
        symbol_handles.add(name)
        for prior_min, prior_max, prior_name in ranges_by_id.setdefault(stable_id, []):
            if ranges_overlap((minimum, maximum), (prior_min, prior_max)):
                raise SchemaError(f"{context}: stable ID range overlaps {prior_name}")
        ranges_by_id[stable_id].append((minimum, maximum, name))
        if kind == "projected_function":
            normalized_target = normalize_target(target)
            fallback = item.get("fallback")
            for prior_min, prior_max, prior_name, prior_fallback in ranges_by_function_target.setdefault(normalized_target, []):
                if ranges_overlap((minimum, maximum), (prior_min, prior_max)):
                    raise SchemaError(f"{context}: projected target range overlaps {prior_name}")
                if fallback != prior_fallback:
                    raise SchemaError(f"{context}: projected target variants must use the same fallback")
            ranges_by_function_target[normalized_target].append((minimum, maximum, name, fallback))
        elif kind == "projected_variable":
            normalized_target = normalize_target(target)
            for prior_min, prior_max, prior_name in ranges_by_variable_target.setdefault(normalized_target, []):
                if ranges_overlap((minimum, maximum), (prior_min, prior_max)):
                    raise SchemaError(f"{context}: projected variable target range overlaps {prior_name}")
            ranges_by_variable_target[normalized_target].append((minimum, maximum, name))
        for candidate in checked_candidates:
            for prior_min, prior_max, prior_name in ranges_by_candidate.setdefault(candidate, []):
                if ranges_overlap((minimum, maximum), (prior_min, prior_max)):
                    raise SchemaError(
                        f"{context}: complete symbol name overlaps descriptor {prior_name}: {candidate}"
                    )
            ranges_by_candidate[candidate].append((minimum, maximum, name))

    layouts = require_list(schema.get("layouts"), f"{path}: layouts")
    layout_names: set[str] = set()
    layout_ids: set[str] = set()
    for index, raw in enumerate(layouts):
        context = f"{path}: layouts[{index}]"
        item = require_object(raw, context)
        allowed = {"name", "id", "kind", "type", "cases", "notes"}
        extra = set(item) - allowed
        if extra:
            raise SchemaError(f"{context} has unknown fields: {', '.join(sorted(extra))}")
        require_optional_note(item.get("notes"), f"{context}.notes")
        name = require_string(item.get("name"), f"{context}.name", identifier=True)
        stable_id = require_string(item.get("id"), f"{context}.id")
        kind = item.get("kind")
        if kind not in LAYOUT_KINDS:
            raise SchemaError(f"{context}.kind must be field or vtable_slot")
        require_string(item.get("type"), f"{context}.type")
        cases = require_list(item.get("cases"), f"{context}.cases")
        if not cases:
            raise SchemaError(f"{context}.cases must not be empty")
        terminal_seen = False
        prior_resolved: tuple[int, int] | None = None
        for case_index, case_raw in enumerate(cases):
            case_context = f"{context}.cases[{case_index}]"
            case = require_object(case_raw, case_context)
            extra_case = set(case) - {"offset", "until", "otherwise"}
            if extra_case:
                raise SchemaError(f"{case_context} has unknown fields: {', '.join(sorted(extra_case))}")
            require_string(case.get("offset"), f"{case_context}.offset")
            has_until = "until" in case
            has_otherwise = case.get("otherwise") is True
            if has_until == has_otherwise:
                raise SchemaError(f"{case_context} must define exactly one of until or otherwise=true")
            if has_otherwise:
                if case_index + 1 != len(cases):
                    raise SchemaError(f"{case_context}: otherwise must be final")
                terminal_seen = True
                continue
            boundary = require_object(case["until"], f"{case_context}.until")
            if set(boundary) - {"build", "revision"}:
                raise SchemaError(f"{case_context}.until has unknown fields")
            numeric_build = resolve_boundary_expression(
                boundary.get("build"), constants, f"{case_context}.until.build", build=True
            )
            numeric_revision = resolve_boundary_expression(
                boundary.get("revision", "0"), constants, f"{case_context}.until.revision", build=False
            )
            current = (numeric_build, numeric_revision)
            if prior_resolved is not None and current <= prior_resolved:
                raise SchemaError(f"{case_context}: boundaries must be strictly increasing")
            prior_resolved = current
        if terminal_seen and cases[-1].get("otherwise") is not True:
            raise SchemaError(f"{context}: invalid terminal case")
        if name in layout_names or stable_id in layout_ids:
            raise SchemaError(f"{context}: duplicate layout name or stable ID")
        layout_names.add(name)
        layout_ids.add(stable_id)

    schema["symbols"] = sorted(symbols, key=lambda item: (item["id"], version_key(item.get("min_inclusive")), item["name"]))
    schema["layouts"] = sorted(layouts, key=lambda item: (item["id"], item["name"]))
    return schema


def version_key(value: Any) -> tuple[int, int]:
    if value is None:
        return (0, 0)
    return (int(value["build"]), int(value.get("revision", 0)))


class StringPool:
    def __init__(self) -> None:
        self.data = bytearray(b"\0")
        self.offsets: dict[str, int] = {"": 0}

    def add(self, value: str | None) -> int:
        if value is None:
            return 0
        if value in self.offsets:
            return self.offsets[value]
        encoded = value.encode("utf-8") + b"\0"
        offset = len(self.data)
        self.data.extend(encoded)
        self.offsets[value] = offset
        return offset

    def cpp_literal(self) -> str:
        pieces: list[str] = []
        for byte in self.data:
            if byte == 0:
                pieces.append("\\0")
            elif byte == 0x5C:
                pieces.append("\\\\")
            elif byte == 0x22:
                pieces.append('\\"')
            elif 0x20 <= byte < 0x7F:
                pieces.append(chr(byte))
            else:
                pieces.append(f"\\x{byte:02x}")
        return '"' + "".join(pieces) + '"'


def cpp_version(value: Any) -> str:
    if value is None:
        return "{}"
    return f"{{{int(value['build'])}u, {int(value.get('revision', 0))}u}}"


def condition_lines(condition: str | None, lines: Iterable[str]) -> list[str]:
    values = list(lines)
    if condition == "debug":
        return ["#ifdef _DEBUG", *values, "#endif"]
    return values


def require_notes(value: Any, context: str) -> list[str]:
    if value is None:
        return []
    values = value if isinstance(value, list) else [value]
    result: list[str] = []
    for index, note in enumerate(values):
        value = require_string(note, f"{context}[{index}]")
        if MIGRATION_PROVENANCE_RE.match(value):
            raise SchemaError(f"{context}[{index}] must contain durable evidence, not migration provenance")
        result.append(value)
    return result


def require_optional_note(value: Any, context: str) -> str | None:
    if value is None:
        return None
    result = require_string(value, context)
    if MIGRATION_PROVENANCE_RE.match(result):
        raise SchemaError(f"{context} must contain durable evidence, not migration provenance")
    return result


def generated_comment_lines(notes: Any, indent: str = "\t") -> list[str]:
    lines: list[str] = []
    for note in require_notes(notes, "notes"):
        for line in note.splitlines():
            lines.append(f"{indent}//{f' {line}' if line else ''}")
    return lines


def generate_layout_header(schema: dict[str, Any]) -> str:
    namespace = schema["namespace"]
    tag = schema["tag"]
    module = schema["module"]
    lines = [
        "// Generated by Scripts/projection_codegen.py. Do not edit.",
        "#pragma once",
        "",
        f"namespace {namespace}",
        "{",
        f"\tstruct {tag};",
        "\textern Projection::ModuleRegistry g_registry;",
        "}",
        "",
        f"namespace {namespace}::Generated",
        "{",
        f"\textern LONG g_{module}SelectedOffsets[];",
        f"\textern bool g_{module}LayoutSupported[];",
        "}",
        "",
        "namespace OpenGlass::Projection",
        "{",
        f"\ttemplate <> inline ModuleRegistry& RegistryFor<{namespace}::{tag}>() noexcept",
        "\t{",
        f"\t\treturn {namespace}::g_registry;",
        "\t}",
        "",
        f"\ttemplate <> struct LayoutState<{namespace}::{tag}>",
        "\t{",
        "\t\tstatic __forceinline LONG Offset(size_t index) noexcept",
        "\t\t{",
        f"\t\t\treturn {namespace}::Generated::g_{module}SelectedOffsets[index];",
        "\t\t}",
        "",
        "\t\tstatic __forceinline bool IsSupported(size_t index) noexcept",
        "\t\t{",
        f"\t\t\treturn {namespace}::Generated::g_{module}LayoutSupported[index];",
        "\t\t}",
        "\t};",
        "}",
        "",
        f"namespace {namespace}",
        "{",
        "",
    ]
    module_notes = generated_comment_lines(schema.get("notes"))
    if module_notes:
        lines.extend([*module_notes, ""])
    for index, layout in enumerate(schema["layouts"]):
        handle = "VtableSlotHandle" if layout["kind"] == "vtable_slot" else "FieldHandle"
        layout_notes = generated_comment_lines(layout.get("notes"))
        if layout_notes:
            lines.extend(layout_notes)
        lines.append(f"\tinline constexpr Projection::{handle}<{tag}, {index}, {layout['type']}> {layout['name']}{{}};")
        if layout_notes:
            lines.append("")
    lines.extend(["}", ""])
    return "\n".join(lines)


def symbol_cpp_type(symbol: dict[str, Any]) -> str:
    return symbol.get("type") or f"decltype({symbol['target']})"


def generate_symbol_header(schema: dict[str, Any]) -> str:
    namespace = schema["namespace"]
    tag = schema["tag"]
    lines = ["// Generated by Scripts/projection_codegen.py. Do not edit.", "#pragma once", "", f"namespace {namespace}", "{", ""]
    for index, symbol in enumerate(schema["symbols"]):
        declaration = f"\tinline constexpr Projection::SymbolHandle<{tag}, {index}, {symbol_cpp_type(symbol)}> {symbol['name']}{{}};"
        lines.extend(condition_lines(symbol.get("condition"), [declaration]))
    lines.extend(["}", ""])
    return "\n".join(lines)


def requirement_cpp(value: str) -> str:
    return "Required" if value == "required" else "Optional"


def generate_registry_inc(schemas: list[dict[str, Any]]) -> str:
    lines = ["// Generated by Scripts/projection_codegen.py. Do not edit.", ""]
    for schema in schemas:
        module = schema["module"]
        namespace = schema["namespace"]
        tag = schema["tag"]
        pool = StringPool()
        symbol_name_offsets: list[int] = []
        versions: list[tuple[str, str]] = [("0", "0")]
        version_indices: dict[tuple[str, str], int] = {("0", "0"): 0}

        def add_version(build: str, revision: str) -> int:
            key = (build, revision)
            if key not in version_indices:
                version_indices[key] = len(versions)
                versions.append(key)
            return version_indices[key]

        def add_json_version(value: Any) -> int:
            if value is None:
                return 0
            return add_version(f"{int(value['build'])}u", f"{int(value.get('revision', 0))}u")

        symbol_rows: list[str] = []
        binding_rows: list[str] = []
        binding_assertions: list[str] = []
        case_rows: list[str] = []
        case_assertions: list[str] = []
        layout_rows: list[str] = []
        for index, symbol in enumerate(schema["symbols"]):
            first_name_index = len(symbol_name_offsets)
            symbol_name_offsets.extend(pool.add(candidate) for candidate in symbol["symbol_names"])
            id_offset = pool.add(symbol["id"])
            minimum_index = add_json_version(symbol.get("min_inclusive"))
            maximum_index = add_json_version(symbol.get("max_exclusive"))
            requirement = f"Projection::Requirement::{requirement_cpp(symbol['requirement'])}"
            flags = "Projection::SymbolFlags::DebugOnly" if symbol.get("condition") == "debug" else "Projection::SymbolFlags::None"
            symbol_rows.append(
                f"\t{{{id_offset}, {first_name_index}, {len(symbol['symbol_names'])}, {minimum_index}, {maximum_index}, {requirement}, {flags}}},"
            )
            if symbol["kind"].startswith("projected_"):
                target = symbol["target"]
                if symbol["kind"] == "projected_function":
                    fallback = symbol.get("fallback") or f"Projection::ProjectedFailFast<{target}>"
                    storage = f"Projection::ProjectedSlotStorage<{target}>()"
                    fallback_value = f"Projection::ProjectedAddress({fallback})"
                    binding_assertions.append(
                        f"\tstatic_assert(std::is_convertible_v<decltype({fallback}), Projection::projected_abi_t<{target}>>, "
                        f"\"fallback ABI mismatch: {symbol['id']}\");"
                    )
                    compatibility = symbol.get("abi_compatibility")
                    if compatibility == "discard_return":
                        binding_assertions.append(
                            f"\tstatic_assert(Projection::is_discard_return_compatible_v<{symbol['type']}, "
                            f"Projection::projected_abi_t<{target}>>, \"discard-return ABI mismatch: {symbol['id']}\");"
                        )
                    elif compatibility == "extra_trailing_argument":
                        binding_assertions.append(
                            f"\tstatic_assert(Projection::is_extra_trailing_argument_compatible_v<{symbol['type']}, "
                            f"Projection::projected_abi_t<{target}>>, \"trailing-argument ABI mismatch: {symbol['id']}\");"
                        )
                else:
                    storage = f"Projection::ProjectedVariableStorage<{target}>()"
                    fallback_value = "nullptr"
                binding_rows.append(f"\t{{{index}, {storage}, {fallback_value}}},")
        first_case = 0
        for layout in schema["layouts"]:
            for case in layout["cases"]:
                if case.get("otherwise") is True:
                    boundary_index = 0
                else:
                    until = case["until"]
                    boundary_index = add_version(until["build"], until.get("revision", "0"))
                expression = case["offset"]
                case_assertions.append(
                    f"\tstatic_assert(static_cast<LONGLONG>({expression}) >= static_cast<LONGLONG>(INT32_MIN) && "
                    f"static_cast<LONGLONG>({expression}) <= static_cast<LONGLONG>(INT32_MAX), "
                    f"\"layout offset does not fit LONG storage: {layout['id']}\");"
                )
                case_rows.append(f"\t{{static_cast<LONG>({expression}), {boundary_index}}},")
            layout_rows.append(f"\t{{{first_case}, {len(layout['cases'])}}},")
            first_case += len(layout["cases"])

        generated_namespace = f"{namespace}::Generated"
        lines.extend([
            f"namespace {generated_namespace}",
            "{",
            f"\tconstexpr char g_{module}StringPool[] = {pool.cpp_literal()};",
            f"\tconstexpr size_t g_{module}SymbolNameOffsets[] =",
            "\t{",
            *[f"\t\t{offset}," for offset in symbol_name_offsets],
            "\t};",
            f"\tconstexpr Projection::Version g_{module}Versions[] =",
            "\t{",
            *[f"\t\t{{{build}, {revision}}}," for build, revision in versions],
            "\t};",
            f"\tconstexpr Projection::SymbolSpec g_{module}SymbolSpecs[] =",
            "\t{",
            *symbol_rows,
            "\t};",
            f"\tPVOID g_{module}Candidates[std::size(g_{module}SymbolSpecs)]{{}};",
            f"\tPVOID g_{module}Resolved[std::size(g_{module}SymbolSpecs)]{{}};",
            f"\tProjection::ResolutionState g_{module}ResolutionStates[std::size(g_{module}SymbolSpecs)]{{}};",
            *binding_assertions,
            f"\tconst Projection::BindingSpec g_{module}Bindings[] =",
            "\t{",
            *binding_rows,
            "\t};",
            *case_assertions,
            f"\tconstexpr Projection::LayoutCase g_{module}LayoutCases[] =",
            "\t{",
            *case_rows,
            "\t};",
            f"\tconstexpr Projection::LayoutSpec g_{module}LayoutSpecs[] =",
            "\t{",
            *layout_rows,
            "\t};",
            f"\tLONG g_{module}SelectedOffsets[std::size(g_{module}LayoutSpecs)]{{}};",
            f"\tbool g_{module}LayoutSupported[std::size(g_{module}LayoutSpecs)]{{}};",
            "}",
            "",
            f"namespace {namespace}",
            "{",
            f"\tconstinit Projection::ModuleRegistry g_registry{{",
            f"\t\t\"{'uDWM.dll' if module == 'udwm' else 'dwmcore.dll'}\",",
            f"\t\tGenerated::g_{module}StringPool, std::span{{Generated::g_{module}SymbolNameOffsets}}, std::span{{Generated::g_{module}Versions}},",
            f"\t\tstd::span{{Generated::g_{module}SymbolSpecs}},",
            f"\t\tstd::span{{Generated::g_{module}Candidates}}, std::span{{Generated::g_{module}Resolved}},",
            f"\t\tstd::span{{Generated::g_{module}ResolutionStates}}, std::span{{Generated::g_{module}Bindings}},",
            f"\t\tstd::span{{Generated::g_{module}LayoutSpecs}}, std::span{{Generated::g_{module}LayoutCases}},",
            f"\t\tstd::span{{Generated::g_{module}SelectedOffsets}}, std::span{{Generated::g_{module}LayoutSupported}}}};",
            "}",
            "",
        ])
    return "\n".join(lines)


def strip_non_code(text: str) -> str:
    output = list(text)
    index = 0
    state = "code"
    quote = ""
    while index < len(output):
        char = output[index]
        following = output[index + 1] if index + 1 < len(output) else ""
        if state == "code":
            if char == "/" and following == "/":
                output[index] = output[index + 1] = " "
                index += 2
                state = "line"
                continue
            if char == "/" and following == "*":
                output[index] = output[index + 1] = " "
                index += 2
                state = "block"
                continue
            if char in {'"', "'"}:
                quote = char
                output[index] = " "
                state = "literal"
        elif state == "line":
            if char == "\n":
                state = "code"
            else:
                output[index] = " "
        elif state == "block":
            if char == "*" and following == "/":
                output[index] = output[index + 1] = " "
                index += 2
                state = "code"
                continue
            if char != "\n":
                output[index] = " "
        else:
            if char == "\\":
                output[index] = " "
                if index + 1 < len(output):
                    if output[index + 1] != "\n":
                        output[index + 1] = " "
                    index += 2
                    continue
            if char == quote:
                state = "code"
            if char != "\n":
                output[index] = " "
        index += 1
    return "".join(output)


def projection_source(projection_root: Path) -> str:
    source_root = projection_root.parents[1]
    paths = sorted(source_root.glob("*.cpp")) + sorted(source_root.glob("*.hpp")) + sorted(source_root.glob("*.h"))
    paths += sorted(projection_root.rglob("*.cpp")) + sorted(projection_root.rglob("*.hpp")) + sorted(projection_root.rglob("*.h"))
    return "\n".join(strip_non_code(path.read_text(encoding="utf-8-sig")) for path in paths)


def _matching_delimiter(source: str, start: int, opening: str, closing: str) -> int | None:
    depth = 0
    for index in range(start, len(source)):
        if source[index] == opening:
            depth += 1
        elif source[index] == closing:
            depth -= 1
            if depth == 0:
                return index
    return None


def projected_wrapper_is_pure(source: str, match: re.Match[str], target: str) -> bool:
    body_start = source.rfind("{", 0, match.start())
    if body_start < 0:
        return False
    body_end = _matching_delimiter(source, body_start, "{", "}")
    if body_end is None or match.end() > body_end:
        return False

    body = source[body_start + 1:body_end]
    prefix = re.match(
        r"\s*OPENGLASS_MUSTTAIL\s+return\s+Projection\s*::\s*Invoke\s*<\s*(&[^>]+)\s*>\s*",
        body,
    )
    if prefix is None or normalize_target(prefix.group(1)) != target:
        return False
    open_paren = prefix.end()
    if open_paren >= len(body) or body[open_paren] != "(":
        return False
    close_paren = _matching_delimiter(body, open_paren, "(", ")")
    if close_paren is None:
        return False
    return re.fullmatch(r"\s*;\s*", body[close_paren + 1:]) is not None


def projected_function_consumer_counts(source: str, symbol: dict[str, Any]) -> tuple[int, int]:
    target = normalize_target(symbol["target"])
    method = target.removeprefix("&").rsplit("::", 1)[-1]
    masked = list(source)
    invoke_matches = [match for match in INVOKE_RE.finditer(source) if normalize_target(match.group(1)) == target]
    for match in invoke_matches:
        body_start = source.rfind("{", 0, match.start())
        if body_start < 0:
            continue
        body_end = _matching_delimiter(source, body_start, "{", "}")
        if body_end is None:
            continue
        signature_matches = list(re.finditer(rf"\b{re.escape(method)}\s*\(", source[:body_start]))
        wrapper_start = signature_matches[-1].start() if signature_matches else body_start
        masked[wrapper_start:body_end + 1] = " " * (body_end + 1 - wrapper_start)

    without_wrappers = "".join(masked)
    callsite_count = 0
    for match in re.finditer(rf"\b{re.escape(method)}\s*\(", without_wrappers):
        open_paren = without_wrappers.find("(", match.start())
        close_paren = _matching_delimiter(without_wrappers, open_paren, "(", ")")
        if close_paren is None:
            continue
        following = close_paren + 1
        while following < len(without_wrappers) and without_wrappers[following].isspace():
            following += 1
        if following < len(without_wrappers) and without_wrappers[following] == "{":
            continue
        callsite_count += 1

    direct_consumer_count = len(re.findall(rf"\b{re.escape(symbol['name'])}\b", source))
    return callsite_count, direct_consumer_count


def projected_variable_has_consumer(source: str, target: str) -> bool:
    qualified_target = normalize_target(target).removeprefix("&")
    if "::" not in qualified_target:
        return False
    owner, leaf = qualified_target.rsplit("::", 1)
    owner_name = owner.rsplit("::", 1)[-1]
    qualified_use = re.compile(rf"\b{re.escape(owner)}\s*::\s*{re.escape(leaf)}\b")

    owner_declaration = re.compile(rf"\b(?:class|struct)\s+{re.escape(owner_name)}\b[^{{;]*{{")
    has_inline_storage = False
    for match in owner_declaration.finditer(source):
        body_start = source.find("{", match.start(), match.end())
        body_end = _matching_delimiter(source, body_start, "{", "}")
        if body_end is None:
            continue
        body = source[body_start + 1:body_end]
        storage_declaration = re.compile(
            rf"\b(?:inline\s+static|static\s+inline)\b[^;\n]*\b{re.escape(leaf)}\b[^;\n]*;"
        )
        if storage_declaration.search(body) is None:
            continue
        has_inline_storage = True
        if len(re.findall(rf"\b{re.escape(leaf)}\b", body)) > 1:
            return True
    return has_inline_storage and qualified_use.search(source) is not None


def validate_source_completeness(projection_root: Path, schemas: list[dict[str, Any]]) -> None:
    headers = sorted(projection_root.rglob("*.hpp"))
    source = "\n".join(strip_non_code(path.read_text(encoding="utf-8-sig")) for path in headers)
    invoked = [normalize_target(match.group(1)) for match in INVOKE_RE.finditer(source)]
    musttail_invoked = [normalize_target(match.group(1)) for match in MUSTTAIL_INVOKE_RE.finditer(source)]
    inline_invoked = [normalize_target(match.group(1)) for match in INLINE_INVOKE_RE.finditer(source)]
    declared: dict[str, list[dict[str, Any]]] = {}
    for schema in schemas:
        for symbol in schema["symbols"]:
            if symbol["kind"] == "projected_function":
                target = normalize_target(symbol["target"])
                declared.setdefault(target, []).append(symbol)
    for target in sorted(set(invoked) - set(declared)):
        raise SchemaError(f"Projection::Invoke target has no projected_function schema: {target}")
    for target in sorted(set(declared) - set(invoked)):
        if not all(symbol.get("condition") == "debug" for symbol in declared[target]):
            raise SchemaError(f"projected_function schema has no wrapper/direct consumer: {target}")
    for target in set(invoked):
        if invoked.count(target) != 1:
            raise SchemaError(f"projected wrapper target must have exactly one Invoke site: {target}")
        if musttail_invoked.count(target) != 1:
            raise SchemaError(f"projected wrapper must use OPENGLASS_MUSTTAIL immediately before Invoke: {target}")
        if inline_invoked.count(target) != 1:
            raise SchemaError(f"projected wrapper must be declared inline: {target}")
        invoke_match = next(
            match for match in INVOKE_RE.finditer(source)
            if normalize_target(match.group(1)) == target
        )
        if not projected_wrapper_is_pure(source, invoke_match, target):
            raise SchemaError(f"projected wrapper body must contain only the typed Invoke dispatch: {target}")

    all_source = projection_source(projection_root)
    for schema in schemas:
        for layout in schema["layouts"]:
            if not re.search(rf"\b{re.escape(layout['name'])}\b", all_source):
                raise SchemaError(f"Layout schema has no typed source consumer: {layout['name']}")
        for symbol in schema["symbols"]:
            if symbol["kind"] != "raw" or symbol.get("diagnostic_only"):
                continue
            if not re.search(rf"\b{re.escape(symbol['name'])}\b", all_source):
                raise SchemaError(f"raw symbol schema has no direct consumer: {symbol['name']}")
        for symbol in schema["symbols"]:
            if symbol["kind"] != "projected_variable" or symbol.get("diagnostic_only"):
                continue
            target = normalize_target(symbol["target"])
            if not projected_variable_has_consumer(all_source, target):
                raise SchemaError(f"projected_variable schema has no runtime consumer: {target}")
    for target, variants in declared.items():
        symbol = variants[0]
        if all(item.get("condition") == "debug" or item.get("diagnostic_only") for item in variants):
            continue
        callsite_count, direct_consumer_count = projected_function_consumer_counts(all_source, symbol)
        if not callsite_count and not direct_consumer_count:
            raise SchemaError(
                f"projected_function schema has no runtime call site or direct Symbol consumer: {target}"
            )


def atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    encoded = content.encode("utf-8")
    try:
        if path.is_file() and path.read_bytes() == encoded:
            return
        descriptor, temporary_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
        try:
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(encoded)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary_name, path)
        except Exception:
            try:
                os.unlink(temporary_name)
            except OSError:
                pass
            raise
    except OSError as error:
        raise SchemaError(f"cannot atomically write {path}: {error}") from error


def run(repo: Path, architecture: str, output: Path, check: bool) -> None:
    repo = repo.resolve()
    output = output.resolve()
    if not repo.is_dir():
        raise SchemaError(f"repository does not exist: {repo}")
    if architecture not in ARCHITECTURES:
        raise SchemaError(f"unknown DWM architecture: {architecture}")
    projection_root = repo / "OpenGlass" / "Architecture" / ARCHITECTURES[architecture]
    if not projection_root.is_dir():
        raise SchemaError(f"DWM architecture source does not exist: {projection_root}")
    constants = load_os_constants(repo)
    schemas = [
        validate_schema(repo, repo / "OpenGlass" / "ProjectionSchemas" / architecture / f"{module}.json", module, constants)
        for module in MODULES
    ]
    validate_source_completeness(projection_root, schemas)
    generated = {
        "udwm.Layouts.generated.hpp": generate_layout_header(schemas[0]),
        "udwm.Symbols.generated.hpp": generate_symbol_header(schemas[0]),
        "dwmcore.Layouts.generated.hpp": generate_layout_header(schemas[1]),
        "dwmcore.Symbols.generated.hpp": generate_symbol_header(schemas[1]),
        "ProjectionRegistry.generated.inc": generate_registry_inc(schemas),
    }
    if not check:
        for name, content in generated.items():
            atomic_write(output / name, content)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        run(args.repo, args.architecture, args.output, args.check)
    except SchemaError as error:
        print(f"projection_codegen: error: {error}", file=sys.stderr)
        return 1
    except Exception as error:
        print(f"projection_codegen: internal error: {error}", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
