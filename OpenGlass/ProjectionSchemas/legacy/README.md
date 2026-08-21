# Projection schemas

`udwm.json` and `dwmcore.json` are the single editable sources for OpenGlass DWM Symbols, projected bindings, exact DbgHelp `UNDNAME_COMPLETE` names, version ranges, Layouts, ABI fallbacks, and reverse-engineering guidance. Use `notes` only for durable, non-obvious guidance: semantic anchors, constructor and xref routes, adjusted-`this` calculations, ABI traps, ICF/inlining ambiguity, and independent cross-checks. Exact PDB names, ranges, visibility, ordinary consumers, and migration provenance belong in their structured fields or source references rather than notes. Do not edit files under `$(IntDir)\Generated\Projection` or duplicate this inventory in C++.

Optional top-level `min_inclusive` and `max_exclusive` values bound the whole module inventory. Legacy explicitly covers `[17763.0, 28000.0)`: `ModuleRegistry::Freeze` rejects versions outside that architecture range before symbol collection or hook preparation. A final Layout `otherwise` case carries the last known offset through later servicing revisions within the range, but never across the build 28000 MILComp architecture boundary.

Required top-level `known_builds` records the build families explicitly recognized for that module. Codegen emits this list into `ModuleRegistry`; startup suppresses the new-Windows-version warning only when both `uDWM.dll` and `dwmcore.dll` recognize their current builds. This warning policy is independent of the module compatibility range and Required projection validation: a known build is not by itself a runtime support claim.

The standard-library-only generator runs as an incremental MSBuild step and can also be invoked directly:

```powershell
python Scripts/projection_codegen.py --repo . --architecture legacy --output Cache\ProjectionManual\Generated\Projection
```

It validates the schema before atomically publishing deterministic output. Offset expressions remain C++ source text and are never evaluated by Python. Layout notes are emitted as adjacent comments in the generated headers while remaining runtime-free metadata. Symbol names are preserved verbatim in a deduplicated NUL string pool; naturally aligned runtime specs and state are indexed rather than self-registering. Stable IDs are diagnostic labels, while `symbol_names` is the sole matching truth. Do not add name-only, substring, decorated-name, or first-match fallback behavior.

Private DWM fields used by handler translation units must have typed Layout accessors. Add a focused Layout descriptor and accessor instead of embedding a byte offset or padding-based fake object in a handler. Codegen validates the structured schema and its projected consumers, but intentionally does not parse arbitrary handler C++; reviewers must enforce that architectural boundary. Instruction-pattern navigation, COM vtable indexing, and application-owned structures remain separate concerns.

Raw Symbols use their exact function-pointer type. Instruction-pattern anchors are the sole exception and must be declared as `BYTE*` with `usage: "code_address"`. Projected ABI changes use disjoint version ranges. When the OpenGlass wrapper intentionally discards an old return value or supplies one extra trailing Win64 argument, declare the exact source `type` plus `abi_compatibility`; codegen proves the supported relationship at compile time. Do not use this mechanism to excuse reordered, removed, or otherwise incompatible arguments.

Each logical Symbol owns one typed slot and a non-empty ordered `bindings` array. A binding contains the exact `symbol_names` and one `[min_inclusive, max_exclusive)` resolution interval. Bindings within a Symbol must not overlap; an uncovered interval makes the Symbol version-inactive rather than unresolved. Merge versioned descriptors only when module, semantic target, kind/usage, requirement, fallback, and underlying ABI are identical. ABI changes remain separate logical Symbols even when their intervals are mutually exclusive.

After startup selects each Layout case, normal `read/ref/address` access is a direct offset load plus address calculation. Descriptor lookup, version selection, validation, allocation, locking, and exceptions remain outside the field-access hot path. The explicit `offset()` API is retained for checked startup diagnostics.

Use the Layout and Symbol schema validators before and after edits. The Symbol validator requires every projected function to have a real wrapper call site or a direct typed Symbol consumer; the wrapper declaration alone does not satisfy completeness. A normal Release lets LTCG inline projected wrappers. Binary size and generated machine code can be inspected when changing the projection mechanism, but they are development diagnostics rather than fixed acceptance thresholds:

```powershell
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_layouts.py . --architecture legacy --version 26100.8972
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_symbols.py . --architecture legacy
msbuild OpenGlass\OpenGlass.Legacy.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

Schema changes establish neither semantic correctness nor OS support by themselves. Use an exact-binary semantic audit and real-OS validation as separate release gates.
