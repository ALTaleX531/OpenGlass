---
name: maintain-dwm-offsets
description: Audit, compare, and maintain OpenGlass dwmcore.dll and uDWM.dll Layout and Symbol schemas with paired PE/PDB identity and IDA evidence. Use when investigating a new Windows DWM binary, checking one projection member, vtable slot, complete symbol name, or hook, comparing exact builds or revisions, validating legacy or milcomp projection metadata, or preparing evidence-backed schema changes.
---

# Maintain DWM projections

Treat a Layout offset or Symbol match as verified only when its semantic role is demonstrated in the selected binary. Do not infer schema data from a Windows marketing name, image size, nearby members, or a previous build.

CPU platform is separate from the Legacy/MILComp backend. Tools default to x64. Select `--platform` when targeting another CPU. Each descriptor owns
platform groups under `cases`/`bindings`, with null or omitted platform denoting
common. Select one whole group's `ranges` and ABI fields before validation.
Catalogs use the common backend directory unless a platform/backend specialization
exists. Check the PE machine during acquisition and preserve module/GUID/DBI-age pairing when using PDBs. See
[CPU target selection](../../../tools/README.md#cpu-target-selection).

## Select the scope

1. Identify the requested module: `dwmcore.dll`, `uDWM.dll`, or both.
2. Identify the requested depth:
   - **Projection item**: verify only named schema IDs and their direct dependencies.
   - **Build comparison**: verify the same IDs in every selected sample.
   - **Module audit**: enumerate every schema item consumed by that module.
   - **Full audit**: audit both modules and all loaded samples. Do this only when explicitly requested.
3. Require the DWM architecture: `legacy` or `milcomp`. Confirm its actual consumers, and read [AGENTS.md#build-architecture](../../../AGENTS.md#build-architecture) and [OpenGlass/ProjectionSchemas/README.md](../../../OpenGlass/ProjectionSchemas/README.md). Never infer architecture from the Git branch, marketing version, or symbol name. Shared projection mechanism changes must verify both backend inventories independently.
4. Route the stable ID to the matching inventory. For a Layout, run `python -m tools.validate_projection_layouts . --architecture legacy --module udwm --id STABLE_ID`. For a Symbol or hook, run `python -m tools.validate_projection_symbols . --architecture legacy --module udwm --id STABLE_ID`. Specify `--architecture milcomp` or `--module dwmcore` when targeting those inventories. Do not pass a Symbol ID to the Layout validator, and run the other validator without `--id` only when that inventory or a direct dependency is also in scope. Each selected module schema is still fully validated.

Do not silently expand a focused request into a full audit. Do not modify an IDB or production schema unless the user explicitly asks for those changes. Files under `$(IntDir)\Generated\Projection` are disposable build artifacts and must never be edited.

## Establish evidence

Read [evidence.md](references/evidence.md) for sample identity, interval semantics, and cross-validation rules. Read [ida-workflow.md](references/ida-workflow.md) before operating IDA through MCP: discover or adopt the intended session, verify its identity, and pass its explicit `database` ID on every query. An unadopted GUI entry is discovery information, not a usable session ID.

For each sample:

1. Verify module (`dwmcore.dll` or `uDWM.dll`), DWM backend (`legacy` or `milcomp`), CPU platform, and actual PE build/revision. Matching PDB GUID/DBI-age pairing is required when resolving Symbols or citing PDB evidence. Layout semantic investigation can use an identified PE without an available PDB. Folder labels and marketing names such as `25H2` or `26H1` are hints only until binary metadata corroborates them.
2. For a Layout, run the Layout validator with `--version BUILD.REVISION` (or `10.0.BUILD.REVISION`). This versioned run selects the applicable case and integer byte `offset`, replacing the initial structural inventory command. Do not choose a case by eye.
3. For a Symbol, run `python -m tools.audit_symbol_resolution . --architecture legacy --module udwm --version BUILD.REVISION --image PATH_TO_DLL --symbol-path PATH_TO_SYMBOLS`. Use `--architecture milcomp` or `--module dwmcore` for those targets. Pass `--configuration debug` to evaluate Debug-only Symbols instead of the Release default, `--dbghelp PATH_TO_DBGHELP` to specify a DbgHelp library, or `--id STABLE_ID` to scope to a single Symbol. The tool reports the selected binding index, interval, matched complete name, RVA, resolver, and PDB identity directly (Symbol audit defaults to Release so Debug-only Symbols are inactive). See [binary-audit results](../../../tools/README.md#binary-audit-results). Matching CodeView GUID/DBI age and PE version are production name evidence, and exit code zero establishes binary resolution, not ABI semantics or real-OS support.
   For fast discovery before a schema audit, `python -m tools.dump_symbols --input IMAGE` downloads the public PDB and prints complete names, but does not replace the paired audit report. You can pass `--output SYMBOL_CACHE` to select a local symbol cache directory or `--grep TEXT` to filter names.
   To check selected CPU revisions of a Windows build (`--platform` defaults to x64, while supported tools can select others), run `python -m tools.audit_winbindex_revisions . --architecture legacy --module udwm --build BUILD`, selecting `--architecture milcomp` or `--module dwmcore` as needed. The default reads CodeView using HTTP Range and verifies matching PDB GUID/DBI age and exact names without downloading full PEs. Add `--binary-audit` explicitly to download complete PEs and verify versions, bounds, and sections, or use `--list-only` to inventory revisions. Neither mode establishes semantic or runtime evidence.
   Read [OpenGlass/SymbolCatalogs/README.md](../../../OpenGlass/SymbolCatalogs/README.md) and [validation ownership](../../../tools/README.md#validation-ownership) before maintaining catalogs. Run `python -m tools.maintain_symbol_catalog collect . --cache CACHE --dbghelp DBGHELP` and `python -m tools.maintain_symbol_catalog verify .`. Catalog resolution verifies exact name/RVA pairs, distinct from semantic, full-PE, or runtime evidence. Full PE acquisition requires explicit `--binary-audit` or a supplied `--image`. Collection uses HTTP Range without falling back to full downloads when Range is ignored. Changing the architecture's DbgHelp resolver identity requires running `python -m tools.maintain_symbol_catalog collect . --cache CACHE --dbghelp DBGHELP --replace-resolver`, which clears and recollects all records for that architecture and writes catalog changes. Keep candidate feeds, reports, and downloads in ignored local storage (`.archive/`).
4. Locate the semantic accessor, mutator, constructor, dispatcher, producer/consumer pair, or call chain. Byte patterns and fixed registers are discovery aids only.
5. Derive the byte offset, vtable slot, or typed ABI, including any adjusted `this` subobject displacement.
6. Cross-check with an independent function or constructor. Uncertainty is not production data, so never write unverified values to schemas.
7. Separate absence of an exact PDB name match from absence of a class, member, interface, or capability.

For dwmcore-specific anchors and fallbacks, read [dwmcore.md](references/dwmcore.md). For uDWM-specific anchors and class transitions, read [udwm.md](references/udwm.md). Read only the relevant module reference for a focused request.

## Propose changes safely

Before editing a Layout or Symbol schema item:

1. Confirm the user requested implementation rather than analysis only.
2. Re-read `OpenGlass/ProjectionSchemas/README.md`, `OpenGlass/ProjectionHelper.hpp`, and the target item's `notes`. Treat useful reverse-engineering notes as evidence metadata: retain semantic anchors, constructor or xref routes, adjusted-`this`, ABI traps, ICF/inlining ambiguity, and cross-check guidance verbatim unless new semantic evidence explicitly corrects it. Do not add notes that merely restate an exact PDB name, range, visibility, ordinary consumer, or migration provenance.
3. Use `OpenGlass/ProjectionSchemas/<architecture>/udwm.json` or `dwmcore.json` to locate the explicitly listed section. Edit Symbols under `<module>/symbols/` and Layouts under `<module>/layouts/`, partitioning Layouts primarily by owning class and Symbols by resource or usage category, independently, as specified in `OpenGlass/ProjectionSchemas/README.md`. Keep each descriptor, all version bindings/cases, and its notes together. Keep its stable `id`/`name`. Never hand-edit generated C++ or add a manual projection array/binding step. Adding or moving a section also requires updating the manifest, the architecture project's explicit `None` items, and `.vcxproj.filters`.
4. Express Layout right boundaries as exact `until: { build, revision }` objects using the same JSON integer fields as Symbol bindings: a nonzero unsigned 32-bit `build` and an unsigned 32-bit `revision` that defaults to zero when omitted. Do not use numeric strings or C++ constant names. Use `otherwise: true` only when the Layout remains valid for the open-ended interval. Without it, later versions resolve to `unsupported`. Do not equate that mechanically with feature removal.
5. Store every `offset` as a signed 32-bit JSON integer measured in bytes. Preserve negative adjusted-`this` displacements and use byte offsets for vtable entries too. Do not store numeric strings, `sizeof`, `offsetof`, or C++ arithmetic, because codegen emits the numbers directly. A representation-only conversion must prove numerical equivalence for the selected architecture, including compiler-derived member offsets, without changing the recorded binary layout.
6. Store only the exact, unmodified `UnDecorateSymbolName(..., UNDNAME_COMPLETE)` output in each binding's `symbol_names`. Multiple names within one binding may describe historical aliases only when their typed ABI is identical. Merge versioned entries into one logical Symbol only when module, semantic target, kind/usage, requirement, fallback, and underlying ABI are identical. Represent their exact names and non-overlapping intervals as bindings. Keep ABI changes as separate typed logical Symbols even when their intervals are mutually exclusive. Raw Symbols must use their exact function-pointer type. Reserve `BYTE*` plus `usage: "code_address"` for instruction-pattern anchors. The only projected compatibility exceptions are explicit, typed `discard_return` and `extra_trailing_argument` logical Symbols verified by codegen. Do not use them for reordered or removed arguments. Never use name-only, substring, decorated-name, or first-match fallbacks.
7. A projected Optional or version-inactive function requires an ABI-compatible schema fallback. Declare wrappers `inline` and keep their bodies as pure `OPENGLASS_MUSTTAIL return Projection::Invoke<&Target>(...)` dispatches. Normal Release/LTCG may inline the wrapper. Do not use `__forceinline`: it is not a guarantee and produces C4714 warnings when MSVC refuses it. Musttail is Release-only because MSVC cannot guarantee it under Debug `/Od`. Fallback/range logic belongs in schema bindings and cold commit code. A wrapper is not itself a consumer: require a real runtime call site or a direct typed Symbol consumer such as a Detour, and delete unused wrappers/descriptors instead of retaining unnecessary Required gates.
8. Private DWM fields in handler translation units must be consumed through typed Layout accessors. Add a schema Layout instead of integer-cast offsets, private-pointer arithmetic, or padding-based fake layouts. `address()` and `ref()` preserve constness. Reserve `mutable_address()` and `mutable_ref()` for a projected class facade whose established API intentionally returns a mutable private subobject from a const method. Never use them from handler code as a constness bypass. Codegen intentionally does not parse arbitrary C++, so inspect relevant consumers during review rather than treating a textual cast scan as proof.
9. Change only independently verified items. Never write unverified schema values.
10. For schema or code changes, run the relevant generator/validator tests and build the affected projects in Release. Refer to [AGENTS.md#testing-policy](../../../AGENTS.md#testing-policy).

An open-ended Layout case or Symbol binding is runtime behavior, not evidence that it is correct for an unanalyzed future build.

Before claiming support for a build, exercise the exact matching OS, GPU, and runtime path. Schema checks, builds, and IDA audits are not substitutes. Perform initial injection only where the operator is present to recover locally or offline and disable the service, never relying solely on a remote connection. Emergency shortcut and service recovery must be verified and available.
