---
name: maintain-dwm-offsets
description: Audit, compare, and maintain OpenGlass dwmcore.dll and uDWM.dll projection offsets with IDA evidence. Use when investigating a new Windows DWM binary, checking one projection member or vtable slot, comparing builds or revisions, validating legacy or milcomp offset tables, or preparing evidence-backed changes to dwmcoreProjection.Offsets.hpp or uDwmProjection.Offsets.hpp.
---

# Maintain DWM offsets

Treat an offset as verified only when its semantic role is demonstrated in the selected binary. Do not infer a table from a Windows marketing name, image size, nearby members, or a previous build.

## Select the scope

1. Identify the requested module: `dwmcore.dll`, `uDWM.dll`, or both.
2. Identify the requested depth:
   - **Projection item**: verify only named members or slots and their direct dependencies.
   - **Build comparison**: verify the same items in every selected sample.
   - **Module audit**: enumerate every projection consumed by that module.
   - **Full audit**: audit both modules and all loaded samples. Do this only when explicitly requested.
3. Detect the checked-out branch and the actual projection consumers before using a branch-specific checklist. Read [branches.md](references/branches.md).
4. Run `python .agents/skills/maintain-dwm-offsets/scripts/lint_offset_tables.py .` to inventory and structurally lint the current tables before analysis.

Do not silently expand a focused request into a full audit. Do not modify an IDB or production offset table unless the user explicitly asks for those changes.

## Establish evidence

Read [evidence.md](references/evidence.md) for sample identity, interval semantics, confidence levels, cross-validation, and the report contract. Read [ida-workflow.md](references/ida-workflow.md) before operating IDA through MCP.

For each sample:

1. Record module, architecture, path, hash, PE/file version when available, and PDB identity when available.
2. Treat folder names and labels such as `25H2` or `26H1` as hints until binary metadata corroborates them.
3. After establishing the exact PE build and revision, run the linter with `--version BUILD.REVISION` and record the selected interval and source expression. Do not choose an entry by eye.
4. Locate the semantic accessor, mutator, constructor, dispatcher, producer/consumer pair, or call chain for the requested projection. Use byte patterns and fixed registers only to discover candidates.
5. Derive the byte offset or vtable slot, including any adjusted `this` subobject displacement.
6. Cross-check with an independent function or constructor. If that is unavailable, mark the result provisional.
7. Separate absence of a symbol from absence of a class, member, interface, or capability.

For dwmcore-specific anchors and fallbacks, read [dwmcore.md](references/dwmcore.md). For uDWM-specific anchors and class transitions, read [udwm.md](references/udwm.md). Read only the relevant module reference for a focused request.

## Propose changes safely

Before editing a projection table:

1. Confirm the user requested implementation rather than analysis only.
2. Re-read `OpenGlass/Util.hpp` and the comments immediately above the target table.
3. Express results using right-boundary `OffsetInfo` semantics; never treat `.build` as an introduction marker.
4. Preserve a final `{ .build = 0, .revision = 0 }` only when the projection remains valid for the open-ended interval. A table without that terminal entry may intentionally describe a removed feature.
5. Change only independently verified items. Leave uncertainty in the report rather than guessing a value.
6. Re-run the linter, its unit tests, and the relevant build checks.

The `{ 0, 0 }` terminal is the runtime's open-ended fallback. It is not evidence that the value is correct for an unanalyzed future build.

When preparing a support claim or release, read [release-validation.md](references/release-validation.md). Static lint, a successful build, and an IDA semantic audit do not replace real-OS validation.

## Report

Always return these sections, even for a single projection item:

- **Samples**: module, architecture, hashes, verified version/PDB data, and unverified labels.
- **Linter selection**: requested build/revision, selected entry and interval, or `unsupported`.
- **Evidence**: functions or call chains used and how each value was derived.
- **Findings**: verified values, feature/class presence, and comparison results.
- **Unverified**: missing anchors, ICF ambiguity, inlining ambiguity, or metadata gaps.
- **Suggested table entries**: right-boundary entries or `none`; do not emit edits unless requested.
- **Runtime validation**: `not run`, `passed`, or `failed`; IDA-only work normally reports `not run`.

For an implemented change, also report linter/test/build results and confirm whether any IDB was modified.
