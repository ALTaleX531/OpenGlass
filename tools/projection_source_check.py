"""C++ source inspection for projection consumer completeness.

The checks here enforce source-level architectural contracts. They are separate
from JSON validation and from C++ emission because they operate on repository
source text rather than schema structure.
"""

from __future__ import annotations

import re
from collections import Counter
from io import StringIO
from pathlib import Path

from pcpp import Action, OutputDirective, Preprocessor
from pygments.lexers import CppLexer
from pygments.token import Comment, String

from tools.projection_schema import ProjectionSchema, SchemaError, Symbol, normalize_target
from tools.projection_target import normalize_platform


INVOKE_RE = re.compile(r"Projection\s*::\s*Invoke\s*<\s*(&[^>]+)\s*>")
MUSTTAIL_INVOKE_RE = re.compile(r"OPENGLASS_MUSTTAIL\s+return\s+Projection\s*::\s*Invoke\s*<\s*(&[^>]+)\s*>")
INLINE_INVOKE_RE = re.compile(
    r"\binline\b[^{};]*\{[^{}]*?Projection\s*::\s*Invoke\s*<\s*(&[^>]+)\s*>", re.DOTALL
)


def strip_non_code(text: str, *, preserve_characters: bool = False) -> str:
    """Mask comments/literals with Pygments, preserving source positions."""
    # Mask directive bodies too; CPU branch selection follows lexical masking.
    # CppLexer needs a newline to recognize // comments at EOF.
    lexical_source = re.sub(r"(?m)^([ \t]*)#", r"\1 ", text + "\n")
    output = list(text + "\n")
    for start, token, value in CppLexer().get_tokens_unprocessed(lexical_source):
        if token in Comment or token in String and not (preserve_characters and token in String.Char):
            output[start:start + len(value)] = ["\n" if char == "\n" else " " for char in value]
    return "".join(output[:-1])


class _TargetSource(Preprocessor):
    """Select compiler/Windows SDK CPU branches; keep other macros lexical."""

    CPU_MACROS = {
        "x64": {"_M_X64": 100, "_M_AMD64": 100, "_AMD64_": ""},
        "arm64": {"_M_ARM64": 1, "_ARM64_": ""},
        "arm64ec": {"_M_ARM64EC": 1, "_M_X64": 100, "_M_AMD64": 100, "_AMD64_": "", "_ARM64EC_": ""},
        "x86": {"_M_IX86": 600, "_X86_": ""},
    }
    TARGET_MACROS = {name for macros in CPU_MACROS.values() for name in macros} | {"_WIN32", "_WIN64"}

    def __init__(self, platform: str):
        super().__init__()
        self.line_directive = None
        self.auto_pragma_once_enabled = False
        self.macros.clear()
        self.expand_linemacro = self.expand_filemacro = self.expand_countermacro = False
        for name, value in self.CPU_MACROS[platform].items():
            self.define(f"{name} {value}")
        self.define("_WIN32 1")
        if platform != "x86":
            self.define("_WIN64 1")

    def on_directive_handle(self, directive, toks, ifpassthru, precedingtoks):
        if directive.value not in {"if", "ifdef", "ifndef", "elif", "else", "endif"}:
            raise OutputDirective(Action.IgnoreAndPassThrough)
        return True

    def on_unknown_macro_in_defined_expr(self, tok):
        return False if tok.value in self.TARGET_MACROS else None

    def on_unknown_macro_in_expr(self, ident):
        return 0 if ident in self.TARGET_MACROS else None

    def on_unknown_macro_function_in_expr(self, ident):
        return None

    def evalexpr(self, tokens):
        result, unresolved = super().evalexpr(tokens)
        if unresolved is not None:
            # pcpp marks any unknown macro as partial, even in a short-circuited
            # operand. Its evaluator can still prove the resulting constant.
            try:
                return self.evaluator(unresolved).value(), None
            except (SyntaxError, ArithmeticError):
                pass
        return result, unresolved

    def on_directive_unknown(self, directive, toks, ifpassthru, precedingtoks):
        return None

    def on_error(self, file, line, msg):
        raise SchemaError(f"{file}:{line}: {msg}")


def _read_source(path: Path, platform: str) -> str:
    try:
        source = strip_non_code(path.read_text(encoding="utf-8-sig"), preserve_characters=True)
    except (OSError, UnicodeError) as error:
        raise SchemaError(f"cannot read projection source {path}: {error}") from error
    processor = _TargetSource(platform)
    processor.parse(source, source=str(path))
    output = StringIO()
    processor.write(output)
    return strip_non_code(output.getvalue())


def projection_sources(projection_root: Path, platform: str = "x64") -> tuple[str, str]:
    """Read each source once and return projection headers and all consumer source."""
    source_root = projection_root.parents[1]
    headers = sorted(projection_root.rglob("*.hpp"))
    paths = sorted(source_root.glob("*.cpp")) + sorted(source_root.glob("*.hpp")) + sorted(source_root.glob("*.h"))
    paths += sorted(projection_root.rglob("*.cpp")) + headers + sorted(projection_root.rglob("*.h"))
    platform = normalize_platform(platform)
    sources = {path: _read_source(path, platform) for path in paths}
    return "\n".join(sources[path] for path in headers), "\n".join(sources.values())


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


def validate_source_completeness(projection_root: Path, schemas: list[ProjectionSchema], platform: str = "x64") -> str:
    """Validate descriptor consumers and return the inspected, stripped source."""
    source, all_source = projection_sources(projection_root, platform)
    invoked: dict[str, list[re.Match[str]]] = {}
    for match in INVOKE_RE.finditer(source):
        invoked.setdefault(normalize_target(match.group(1)), []).append(match)
    musttail_invoked = Counter(normalize_target(match.group(1)) for match in MUSTTAIL_INVOKE_RE.finditer(source))
    inline_invoked = Counter(normalize_target(match.group(1)) for match in INLINE_INVOKE_RE.finditer(source))
    declared: dict[str, list[Symbol]] = {}
    for schema in schemas:
        for symbol in schema["symbols"]:
            if symbol["kind"] == "projected_function":
                target = normalize_target(symbol["target"])
                declared.setdefault(target, []).append(symbol)

    for target in sorted(set(invoked) - set(declared)):
        raise SchemaError(f"Projection::Invoke target has no projected_function schema: {target}")
    for target in sorted(invoked):
        if len(invoked[target]) != 1:
            raise SchemaError(f"projected wrapper target must have exactly one Invoke site: {target}")
        if musttail_invoked[target] != 1:
            raise SchemaError(f"projected wrapper must use OPENGLASS_MUSTTAIL immediately before Invoke: {target}")
        if inline_invoked[target] != 1:
            raise SchemaError(f"projected wrapper must be declared inline: {target}")
        invoke_match = invoked[target][0]
        if not projected_wrapper_is_pure(source, invoke_match, target):
            raise SchemaError(f"projected wrapper body must contain only the typed Invoke dispatch: {target}")

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
        if all(item.get("condition") == "debug" or item.get("diagnostic_only") for item in variants):
            continue
        for symbol in variants:
            callsite_count, direct_consumer_count = projected_function_consumer_counts(all_source, symbol)
            if callsite_count or direct_consumer_count:
                break
        else:
            raise SchemaError(
                f"projected_function schema has no runtime call site or direct Symbol consumer: {target}"
            )
    return all_source
