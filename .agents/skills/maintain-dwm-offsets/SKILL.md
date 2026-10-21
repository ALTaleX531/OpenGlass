---
name: maintain-dwm-offsets
description: Audit, compare, and maintain OpenGlass dwmcore.dll and uDWM.dll Layout and Symbol schemas with paired PE/PDB identity and IDA evidence. Use when investigating a new Windows DWM binary, checking one projection member, vtable slot, complete symbol name, or hook, comparing exact builds or revisions, validating legacy or milcomp projection metadata, or preparing evidence-backed schema changes.
---

# Maintain DWM projections

Treat a Layout offset or Symbol match as verified only when its semantic role is demonstrated in the selected binary. Do not infer schema data from a Windows marketing name, image size, nearby members, or a previous build.

CPU platform is separate from the Legacy/MILComp backend. Tools default to x64;
select `--platform` and record the CPU with every sample. Each descriptor owns
platform groups under `cases`/`bindings`, with null or omitted platform denoting
common. Select one whole group's `ranges` and ABI fields before validation.
Catalogs use the common backend directory unless a platform/backend specialization
exists. Check the PE machine during acquisition and preserve module/GUID/DBI-age pairing. See
[CPU target selection](../../../tools/README.md#cpu-target-selection).

## Select the scope

1. Identify the requested module: `dwmcore.dll`, `uDWM.dll`, or both.
2. Identify the requested depth:
   - **Projection item**: verify only named schema IDs and their direct dependencies.
   - **Build comparison**: verify the same IDs in every selected sample.
   - **Module audit**: enumerate every schema item consumed by that module.
   - **Full audit**: audit both modules and all loaded samples. Do this only when explicitly requested.
3. Require the DWM architecture: `legacy` or `milcomp`. Confirm its actual consumers and read [architectures.md](references/architectures.md). Never infer architecture from the Git branch, marketing version, or symbol name.
4. Route the stable ID to the matching inventory. For a Layout, run `python -m tools.validate_projection_layouts . --architecture legacy|milcomp --module udwm|dwmcore --id STABLE_ID`. For a Symbol or hook, run `python -m tools.validate_projection_symbols . --architecture legacy|milcomp --module udwm|dwmcore --id STABLE_ID`; do not pass a Symbol ID to the Layout validator. Run the other validator without `--id` only when that inventory or a direct dependency is also in scope. Each selected module schema is still fully validated.

Do not silently expand a focused request into a full audit. Do not modify an IDB or production schema unless the user explicitly asks for those changes. Files under `$(IntDir)\Generated\Projection` are disposable build artifacts and must never be edited.

## Establish evidence

Read [evidence.md](references/evidence.md) for sample identity, interval semantics, confidence levels, cross-validation, and the report contract. Read [ida-workflow.md](references/ida-workflow.md) before operating IDA through MCP: discover or adopt the intended session, verify its identity, and pass its explicit `database` ID on every query. An unadopted GUI entry is discovery information, not a usable session ID.

For each sample:

1. Record module, architecture, path, hash, PE/file version when available, and PDB identity when available.
2. Treat folder names and labels such as `25H2` or `26H1` as hints until binary metadata corroborates them.
3. For a Layout, after establishing the exact PE build and revision, run the Layout validator with `--version BUILD.REVISION` (or the full `10.0.BUILD.REVISION`) and record the selected schema ID, interval, entry, and integer byte `offset`. This versioned run also performs structural validation, so it replaces rather than duplicates the initial inventory command. Do not choose a case by eye.
4. For a Symbol, run `python -m tools.audit_symbol_resolution . --architecture legacy|milcomp --module udwm|dwmcore --version BUILD.REVISION --image PATH_TO_DLL --symbol-path PATH_TO_SYMBOLS [--configuration release|debug] [--dbghelp PATH_TO_DBGHELP] [--id STABLE_ID]`. The default configuration is Release, so Debug-only logical Symbols remain inactive. Record the selected binding index, binding interval, matched complete name, and RVA, plus the selected DbgHelp path, version, and hash and the PDB identity and hash because exact `UNDNAME_COMPLETE` text is resolver-version input. Only a CodeView GUID/DBI-age-matched PDB and matching PE version is production name evidence. Both binary-audit commands consume the shared failure list: empty means success. Follow [binary-audit results](../../../tools/README.md#binary-audit-results) for scope, report and exit-code semantics. Failed audits retain discovery reports; exit code zero establishes binary resolution evidence, not ABI semantics or real-OS support.
   For fast discovery before a schema audit, use `python -m tools.dump_symbols --input IMAGE [--output SYMBOL_CACHE] [--grep TEXT]`. The cache defaults to `%TEMP%\symbols`; the command downloads the image-matched public PDB and prints complete names, but does not replace the identity-rich audit report.
   To check every indexed x64 revision of a Windows build, run `python -m tools.audit_winbindex_revisions . --architecture legacy|milcomp --module udwm|dwmcore --build BUILD`. The default reads CodeView using HTTP Range and verifies matching PDB GUID/DBI age and exact names. Add `--binary-audit` explicitly to download complete PEs and verify their hashes, actual versions, RVA bounds and sections; `--list-only` only inventories revisions. Neither mode establishes semantic or runtime evidence.
   Maintain the built-in catalog with `python -m tools.maintain_symbol_catalog collect . --cache CACHE --dbghelp DBGHELP` and `verify .`. Production `collect` discovers x64 Winbindex candidates from each module schema's `known_builds` and uses HTTP Range to obtain CodeView identity and downloads matching Microsoft symbol-server PDBs without a complete PE. Commit only successful records, the two architecture indexes, and their explicitly listed module/build shards. Candidate feeds and failure details stay local; failures default to `CACHE/collection-failures.json`, overridable with `--missing-output`. Do not commit frozen inventories, collision/exclusion reports, derived symbol-server keys, or schema fingerprints. Existing schema-valid records survive omissions from the current feed. PDB acquisition must validate the CodeView GUID/age after either direct `.pdb` download or compressed `.pd_` expansion. Each architecture index owns one exact DbgHelp version/hash for every record; changing resolver requires `collect --replace-resolver`, which clears every existing shard before recollection. Each shard inherits architecture, module, build, and resolver from its index. Within each architecture, module + PDB GUID + age is the unique catalog lookup key across all build shards. Do not persist PE timestamp/size or emit machine/PDB-filename identity checks. Do not store image hashes or image objects in catalog records. PDB hashes and filenames are acquisition/replay evidence only. Catalog collection/replay validates module + PDB GUID + DBI age, exact names and Required completeness without a full PE. Full PE download and version/RVA/section checks require explicit `tools.audit_winbindex_revisions --binary-audit` or direct `tools.audit_symbol_resolution --image`; they remain separate from catalog maintenance. Cache CodeView receipts locally and reject ignored HTTP Range responses without a full-download fallback. Runtime trusts the generated RVA spans after matching the loaded module's CodeView identity; do not repeat key uniqueness, span/index integrity, PE bounds/section checks, or add file-version identity checks. Collection only stages candidates; the common two-module Validate/Commit gate owns Required validation and publication. Each catalog identity owns its Required stable-ID/name/RVA map; absent Optional IDs are unresolved, and codegen emits one RVA per logical Symbol slot. Keep records independent rather than using deltas, shared resolution sets, packed fields, or cross-revision bases. Static verification validates the included records against current bindings; it does not prove exhaustive Winbindex coverage, semantic meaning, or real-OS correctness. See `OpenGlass/SymbolCatalogs/README.md` and [validation ownership](../../../tools/README.md#validation-ownership). Use the shared schema Version/range/binding operations. Source loaders validate persisted inputs; emitters and collection consume validated in-process data without repeating format checks or file hashes. Keep actual PE/PDB pairing, RVA evidence, resolver replacement and replay comparison at their owning boundaries.
5. Locate the semantic accessor, mutator, constructor, dispatcher, producer/consumer pair, or call chain for the requested projection. Use byte patterns and fixed registers only to discover candidates.
6. Derive the byte offset, vtable slot, or typed ABI, including any adjusted `this` subobject displacement.
7. Cross-check with an independent function or constructor. If unavailable, mark the result provisional.
8. Separate absence of an exact PDB name match from absence of a class, member, interface, or capability.

For dwmcore-specific anchors and fallbacks, read [dwmcore.md](references/dwmcore.md). For uDWM-specific anchors and class transitions, read [udwm.md](references/udwm.md). Read only the relevant module reference for a focused request.

## Propose changes safely

Before editing a Layout or Symbol schema item:

1. Confirm the user requested implementation rather than analysis only.
2. Re-read `OpenGlass/ProjectionSchemas/<architecture>/README.md`, `OpenGlass/ProjectionHelper.hpp`, and the target item's `notes`. Treat useful reverse-engineering notes as evidence metadata: retain semantic anchors, constructor or xref routes, adjusted-`this`, ABI traps, ICF/inlining ambiguity, and cross-check guidance verbatim unless new semantic evidence explicitly corrects it. Do not add notes that merely restate an exact PDB name, range, visibility, ordinary consumer, or migration provenance.
3. Use `OpenGlass/ProjectionSchemas/<architecture>/udwm.json` or `dwmcore.json` to locate the explicitly listed section. Edit Symbols under `<module>/symbols/` and Layouts under `<module>/layouts/`, partitioning Layouts primarily by owning class and Symbols by resource or usage category, independently, as specified in `OpenGlass/ProjectionSchemas/README.md`. Keep each descriptor, all version bindings/cases, and its notes together. Keep its stable `id`/`name`; never hand-edit generated C++ or add a manual projection array/binding step. Adding or moving a section also requires updating the manifest and architecture project's explicit `ProjectionSchemaSource` inputs.
4. Express Layout right boundaries as exact `until: { build, revision }` objects using the same JSON integer fields as Symbol bindings: a nonzero unsigned 32-bit `build` and an unsigned 32-bit `revision` that defaults to zero when omitted. Do not use numeric strings or C++ constant names. Use `otherwise: true` only when the Layout remains valid for the open-ended interval. Without it, later versions resolve to `unsupported`; do not equate that mechanically with feature removal.
5. Store every `offset` as a signed 32-bit JSON integer measured in bytes. Preserve negative adjusted-`this` displacements and use byte offsets for vtable entries too. Do not store numeric strings, `sizeof`, `offsetof`, or C++ arithmetic; codegen emits the numbers directly. A representation-only conversion must prove numerical equivalence for the selected architecture, including compiler-derived member offsets, without changing the recorded binary layout.
6. Store only the exact, unmodified `UnDecorateSymbolName(..., UNDNAME_COMPLETE)` output in each binding's `symbol_names`. Multiple names within one binding may describe historical aliases only when their typed ABI is identical. Merge versioned entries into one logical Symbol only when module, semantic target, kind/usage, requirement, fallback, and underlying ABI are identical; represent their exact names and non-overlapping intervals as bindings. Keep ABI changes as separate typed logical Symbols even when their intervals are mutually exclusive. Raw Symbols must use their exact function-pointer type; reserve `BYTE*` plus `usage: "code_address"` for instruction-pattern anchors. The only projected compatibility exceptions are explicit, typed `discard_return` and `extra_trailing_argument` logical Symbols verified by codegen; do not use them for reordered or removed arguments. Never use name-only, substring, decorated-name, or first-match fallbacks.
7. A projected Optional or version-inactive function requires an ABI-compatible schema fallback. Declare wrappers `inline` and keep their bodies as pure `OPENGLASS_MUSTTAIL return Projection::Invoke<&Target>(...)` dispatches. Normal Release/LTCG may inline the wrapper. Do not use `__forceinline`: it is not a guarantee and produces C4714 warnings when MSVC refuses it. Musttail is Release-only because MSVC cannot guarantee it under Debug `/Od`. Fallback/range logic belongs in schema bindings and cold commit code. A wrapper is not itself a consumer: require a real runtime call site or a direct typed Symbol consumer such as a Detour, and delete unused wrappers/descriptors instead of retaining unnecessary Required gates.
8. Private DWM fields in handler translation units must be consumed through typed Layout accessors. Add a schema Layout instead of integer-cast offsets, private-pointer arithmetic, or padding-based fake layouts. `address()` and `ref()` preserve constness; reserve `mutable_address()` and `mutable_ref()` for a projected class facade whose established API intentionally returns a mutable private subobject from a const method. Never use them from handler code as a constness bypass. Codegen intentionally does not parse arbitrary C++; inspect relevant consumers during review rather than treating a textual cast scan as proof.
9. Change only independently verified items. Leave uncertainty in the report rather than guessing.
10. Re-run the generator tests and both schema validators with their tests, then build and run `OpenGlassTests` and build the affected DLL projects in Release. See `AGENTS.md` for commands. Record PE size changes as diagnostics rather than treating a historical byte count as a permanent correctness condition.

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
