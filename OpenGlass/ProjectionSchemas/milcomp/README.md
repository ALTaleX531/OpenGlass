# MILComp projection schemas

`udwm.json` and `dwmcore.json` are the only editable projection inventories for the build 28000+ compositor architecture. Keep their exact DbgHelp `UNDNAME_COMPLETE` names, ranges, offset expressions, and reverse-engineering guidance independent of Legacy's. Use `notes` only for non-obvious semantic anchors, constructor/xref routes, adjusted-`this` calculations, ABI traps, ICF/inlining ambiguity, and cross-checks; do not repeat exact PDB names, ranges, visibility, ordinary consumers, or migration provenance there. Stable IDs are diagnostic labels; exact `symbol_names` alone decide matching, with no name-only, substring, decorated-name, or first-match fallbacks.

The optional top-level `min_inclusive` and `max_exclusive` values bound a module's whole inventory, and `ModuleRegistry::Freeze` rejects a version outside a declared range before symbol collection or hook preparation. MILComp deliberately has no upper bound: a final Layout `otherwise` case carries the last known offset into later versions. That does not verify the layout of an unaudited binary; exact Required symbol resolution and all-or-nothing projection validation still apply.

The required top-level `known_builds` list records the build families a module explicitly recognizes. Codegen emits it into `ModuleRegistry`, and startup skips the new-Windows-version warning only when both `uDWM.dll` and `dwmcore.dll` recognize their current builds. This warning is independent of the open-ended module range and Required projection validation; a known build is not by itself a claim of runtime support.

To generate this architecture into the default x64 Release intermediate directory, run the generator from the repository root:

```powershell
python Scripts/projection_codegen.py --repo . --architecture milcomp --output Cache\OpenGlass.MILComp\x64\Release\Generated\Projection
```

Add `--check` to validate without writing files. Generated C++ belongs under `$(IntDir)` and must not be committed. Runtime metadata lives in naturally aligned, startup-only tables. After Layout selection, normal `read/ref/address` access is a direct offset load plus address arithmetic, with no registry lookup or version scan.

Raw Symbols must keep their exact function-pointer type. A `BYTE*` Symbol is allowed only with an explicit `usage: "code_address"`, for instruction-pattern navigation. Model projected ABI changes as disjoint typed ranges. The `abi_compatibility` forms are narrow, compile-time-checked exceptions for an intentionally discarded return value or one extra trailing Win64 argument, not a general ABI escape hatch.

Each logical Symbol owns one typed slot and a non-empty, ordered `bindings` array. A binding holds the exact `symbol_names` and one `[min_inclusive, max_exclusive)` resolution interval. Bindings within a Symbol must not overlap, and a version that no binding covers makes the Symbol version-inactive rather than unresolved. Merge versioned descriptors into one Symbol only when module, semantic target, kind/usage, requirement, fallback, and underlying ABI are all identical. If the ABI changes, keep separate logical Symbols even when their intervals don't overlap.

The Symbol validator treats a projected wrapper as a declaration, not a consumer. Every projected function needs a real runtime call site or a direct typed Symbol consumer, such as a Detour. Delete unused descriptors instead of letting them become Required startup gates.

```powershell
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_layouts.py . --architecture milcomp
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_symbols.py . --architecture milcomp
msbuild OpenGlass\OpenGlass.MILComp.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

Schema changes alone establish neither semantic correctness nor OS support. Exact-binary semantic audits and real-OS validation are separate release gates.
