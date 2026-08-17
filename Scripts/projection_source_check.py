"""C++ source inspection for projection consumer completeness.

The checks here enforce source-level architectural contracts. They are separate
from JSON validation and from C++ emission because they operate on repository
source text rather than schema structure.
"""

from __future__ import annotations

import re
from pathlib import Path

from projection_schema import INTEGER_RE, ProjectionSchema, SchemaError, Symbol, normalize_target


INVOKE_RE = re.compile(r"Projection\s*::\s*Invoke\s*<\s*(&[^>]+)\s*>")
MUSTTAIL_INVOKE_RE = re.compile(r"OPENGLASS_MUSTTAIL\s+return\s+Projection\s*::\s*Invoke\s*<\s*(&[^>]+)\s*>")
INLINE_INVOKE_RE = re.compile(
    r"\binline\b[^{};]*\{[^{}]*?Projection\s*::\s*Invoke\s*<\s*(&[^>]+)\s*>", re.DOTALL
)
SOURCE_SUFFIXES = {".cpp", ".hpp", ".h"}


def strip_non_code(text: str) -> str:
    """Replace C++ comments and literals with spaces while preserving layout."""
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


def _read_source(path: Path) -> str:
    try:
        return strip_non_code(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeError) as error:
        raise SchemaError(f"cannot read projection source {path}: {error}") from error


def load_os_constants(repo: Path) -> dict[str, int]:
    """Read build and revision constants referenced by Layout boundaries."""
    path = repo / "OpenGlass" / "OSHelper.hpp"
    text = _read_source(path)
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


def projection_source(projection_root: Path) -> str:
    """Return stripped shared and architecture-private OpenGlass source."""
    source_root = projection_root.parents[1]
    paths = sorted(source_root.glob("*.cpp")) + sorted(source_root.glob("*.hpp")) + sorted(source_root.glob("*.h"))
    paths += sorted(projection_root.rglob("*.cpp")) + sorted(projection_root.rglob("*.hpp")) + sorted(projection_root.rglob("*.h"))
    return "\n".join(_read_source(path) for path in paths)


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


def projected_function_consumer_counts(source: str, symbol: Symbol) -> tuple[int, int]:
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


def validate_source_completeness(projection_root: Path, schemas: list[ProjectionSchema]) -> None:
    """Validate wrappers and require a real source consumer for each descriptor."""
    headers = sorted(projection_root.rglob("*.hpp"))
    source = "\n".join(_read_source(path) for path in headers)
    invoked = [normalize_target(match.group(1)) for match in INVOKE_RE.finditer(source)]
    musttail_invoked = [normalize_target(match.group(1)) for match in MUSTTAIL_INVOKE_RE.finditer(source)]
    inline_invoked = [normalize_target(match.group(1)) for match in INLINE_INVOKE_RE.finditer(source)]
    declared: dict[str, list[Symbol]] = {}
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
