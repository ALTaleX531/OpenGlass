---
name: maintain-dwm-offsets
description: Audit, compare, and maintain OpenGlass dwmcore.dll and uDWM.dll Layout and Symbol schemas with paired PE/PDB identity and IDA evidence. Use when investigating a new Windows DWM binary, checking one projection member, vtable slot, complete symbol name, or hook, comparing exact builds or revisions, validating legacy or milcomp projection metadata, or preparing evidence-backed schema changes.
---

# Maintain DWM projections

Treat a Layout offset or Symbol match as verified only when its semantic role is demonstrated in the selected binary. Do not infer schema data from a Windows marketing name, image size, nearby members, or a previous build.

## Select the scope

1. Identify the requested module: `dwmcore.dll`, `uDWM.dll`, or both.
2. Identify the requested depth:
   - **Projection item**: verify only named schema IDs and their direct dependencies.
   - **Build comparison**: verify the same IDs in every selected sample.
   - **Module audit**: enumerate every schema item consumed by that module.
   - **Full audit**: audit both modules and all loaded samples. Do this only when explicitly requested.
3. Require the DWM architecture: `legacy` or `milcomp`. Confirm its actual consumers and read [architectures.md](references/architectures.md). Never infer architecture from the Git branch, marketing version, or symbol name.
4. Route the stable ID to the matching inventory. For a Layout, run `python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_layouts.py . --architecture legacy|milcomp --module udwm|dwmcore --id STABLE_ID`. For a Symbol or hook, run `python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_symbols.py . --architecture legacy|milcomp --module udwm|dwmcore --id STABLE_ID`; do not pass a Symbol ID to the Layout validator. Run the other validator without `--id` only when that inventory or a direct dependency is also in scope. Each selected module schema is still fully validated.

Do not silently expand a focused request into a full audit. Do not modify an IDB or production schema unless the user explicitly asks for those changes. Files under `$(IntDir)\Generated\Projection` are disposable build artifacts and must never be edited.

## Establish evidence

Read [evidence.md](references/evidence.md) for sample identity, interval semantics, confidence levels, cross-validation, and the report contract. Read [ida-workflow.md](references/ida-workflow.md) before operating IDA through MCP.

For each sample:

1. Record module, architecture, path, hash, PE/file version when available, and PDB identity when available.
2. Treat folder names and labels such as `25H2` or `26H1` as hints until binary metadata corroborates them.
3. For a Layout, after establishing the exact PE build and revision, run the Layout validator with `--version BUILD.REVISION` (or the full `10.0.BUILD.REVISION`) and record the selected schema ID, interval, entry, and raw `offset` expression. This versioned run also performs structural validation, so it replaces rather than duplicates the initial inventory command. Do not choose a case by eye.
4. For a Symbol, run `python Scripts/audit_symbol_resolution.py . --architecture legacy|milcomp --module udwm|dwmcore --version BUILD.REVISION --image PATH_TO_DLL --symbol-path PATH_TO_SYMBOLS [--configuration release|debug] [--dbghelp PATH_TO_DBGHELP] [--id STABLE_ID]`. The default configuration is Release, so Debug-only logical Symbols remain inactive. Record the selected binding index, binding interval, matched complete name, and RVA, plus the selected DbgHelp path, version, and hash and the PDB identity and hash because exact `UNDNAME_COMPLETE` text is resolver-version input. Only a CodeView GUID/age-matched PDB and matching PE version is production name evidence. An unpaired PDB is discovery-only. Discovery can still return a successful process exit when the requested names resolve; automation making a production claim must require `evidence: production_candidate` and `pdb.paired: true`, not merely exit code zero.
   For fast discovery before a schema audit, use `python Scripts/dump_symbols.py --input IMAGE [--output SYMBOL_CACHE] [--grep TEXT]`. The cache defaults to `%TEMP%\symbols`; the command downloads the image-matched public PDB and prints complete names, but does not replace the identity-rich audit report.
   To check every indexed x64 revision of a Windows build, run `python Scripts/audit_winbindex_revisions.py . --architecture legacy|milcomp --module udwm|dwmcore --build BUILD`. The command verifies downloaded image hashes, PE/PDB identity, and exact symbol names; `--list-only` only inventories revisions. The result is a symbol-resolution check, not semantic or runtime evidence.
5. Locate the semantic accessor, mutator, constructor, dispatcher, producer/consumer pair, or call chain for the requested projection. Use byte patterns and fixed registers only to discover candidates.
6. Derive the byte offset, vtable slot, or typed ABI, including any adjusted `this` subobject displacement.
7. Cross-check with an independent function or constructor. If unavailable, mark the result provisional.
8. Separate absence of an exact PDB name match from absence of a class, member, interface, or capability.

For dwmcore-specific anchors and fallbacks, read [dwmcore.md](references/dwmcore.md). For uDWM-specific anchors and class transitions, read [udwm.md](references/udwm.md). Read only the relevant module reference for a focused request.

## Propose changes safely

Before editing a Layout or Symbol schema item:

1. Confirm the user requested implementation rather than analysis only.
2. Re-read `OpenGlass/ProjectionSchemas/<architecture>/README.md`, `OpenGlass/ProjectionHelper.hpp`, and the target item's `notes`. Treat useful reverse-engineering notes as evidence metadata: retain semantic anchors, constructor or xref routes, adjusted-`this`, ABI traps, ICF/inlining ambiguity, and cross-check guidance verbatim unless new semantic evidence explicitly corrects it. Do not add notes that merely restate an exact PDB name, range, visibility, ordinary consumer, or migration provenance.
3. Edit only `OpenGlass/ProjectionSchemas/<architecture>/udwm.json` or `dwmcore.json`. Keep its stable `id`/`name`; never hand-edit generated C++ or add a manual projection array/binding step.
4. Express Layout right boundaries as exact `until: { build, revision }` objects. Use `otherwise: true` only when the Layout remains valid for the open-ended interval. Without it, later versions resolve to `unsupported`; do not equate that mechanically with feature removal.
5. Preserve every `offset` expression as C++ source text, including negative values and `sizeof` arithmetic. Never evaluate or simplify it in Python.
6. Store only the exact, unmodified `UnDecorateSymbolName(..., UNDNAME_COMPLETE)` output in each binding's `symbol_names`. Multiple names within one binding may describe historical aliases only when their typed ABI is identical. Merge versioned entries into one logical Symbol only when module, semantic target, kind/usage, requirement, fallback, and underlying ABI are identical; represent their exact names and non-overlapping intervals as bindings. Keep ABI changes as separate typed logical Symbols even when their intervals are mutually exclusive. Raw Symbols must use their exact function-pointer type; reserve `BYTE*` plus `usage: "code_address"` for instruction-pattern anchors. The only projected compatibility exceptions are explicit, typed `discard_return` and `extra_trailing_argument` logical Symbols verified by codegen; do not use them for reordered or removed arguments. Never use name-only, substring, decorated-name, or first-match fallbacks.
7. A projected Optional or version-inactive function requires an ABI-compatible schema fallback. Declare wrappers `inline` and keep their bodies as pure `OPENGLASS_MUSTTAIL return Projection::Invoke<&Target>(...)` dispatches. Normal Release/LTCG may inline the wrapper. Do not use `__forceinline`: it is not a guarantee and produces C4714 warnings when MSVC refuses it. Musttail is Release-only because MSVC cannot guarantee it under Debug `/Od`. Fallback/range logic belongs in schema bindings and cold commit code. A wrapper is not itself a consumer: require a real runtime call site or a direct typed Symbol consumer such as a Detour, and delete unused wrappers/descriptors instead of retaining unnecessary Required gates.
8. Private DWM fields in handler translation units must be consumed through typed Layout accessors. Add a schema Layout instead of integer-cast offsets, private-pointer arithmetic, or padding-based fake layouts. `address()` and `ref()` preserve constness; reserve `mutable_address()` and `mutable_ref()` for a projected class facade whose established API intentionally returns a mutable private subobject from a const method. Never use them from handler code as a constness bypass. Codegen intentionally does not parse arbitrary C++; inspect relevant consumers during review rather than treating a textual cast scan as proof.
9. Change only independently verified items. Leave uncertainty in the report rather than guessing.
10. Re-run the generator tests, both schema validators and their tests, `OpenGlassProjectionTests`, and a normal Release build. Record PE size changes as diagnostics rather than treating a historical byte count as a permanent correctness condition.

An open-ended Layout case or Symbol binding is runtime behavior, not evidence that it is correct for an unanalyzed future build.

When preparing a support claim or release, read [release-validation.md](references/release-validation.md). Static schema validation, a successful build, and an IDA semantic audit do not replace real-OS validation.

## Report

Always return these sections, even for a single projection item:

- **Samples**: module, architecture, hashes, verified version/PDB data, and unverified labels.
- **Schema selection**: architecture and requested build/revision; for a Layout, the selected schema ID, entry and interval or `unsupported`; for a Symbol, the selected logical ID, binding index and interval, status, matched complete name and RVA, or `inactive` when no binding covers the version. Use `not applicable` when the audited item has no corresponding selection.
- **Evidence**: functions or call chains used and how each value was derived.
- **Findings**: verified values, exact complete-name resolution, feature/class presence, and comparisons.
- **Unverified**: missing anchors, ICF ambiguity, inlining ambiguity, or metadata gaps.
- **Suggested schema**: exact Layout cases or logical Symbol binding/name/range/fallback changes; keep ABI-changing alternatives as separate typed logical Symbols and do not emit edits unless requested.
- **Runtime validation**: `not run`, `passed`, or `failed`; IDA-only work normally reports `not run`.

For an implemented change, also report generator/validator/test/build/audit results and confirm whether any IDB was modified.
