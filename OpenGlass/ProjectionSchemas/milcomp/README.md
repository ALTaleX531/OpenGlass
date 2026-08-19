# MILComp projection schemas

`udwm.json` and `dwmcore.json` are the only editable projection inventories for the build 28000+ compositor architecture. Preserve their exact DbgHelp `UNDNAME_COMPLETE` names, ranges, offset expressions, and useful reverse-engineering guidance independently from Legacy. Use `notes` only for non-obvious semantic anchors, constructor/xref routes, adjusted-`this`, ABI traps, ICF/inlining ambiguity, or cross-checks; do not restate exact PDB names, ranges, visibility, ordinary consumers, or migration provenance. Stable IDs are diagnostic labels; exact `symbol_names` are the sole matching truth, without name-only, substring, decorated-name, or first-match fallbacks.

Optional top-level `min_inclusive` and `max_exclusive` values bound the whole module inventory. `ModuleRegistry::Freeze` rejects a module version outside an explicitly declared range before symbol collection or hook preparation. MILComp deliberately has no upper bound: a final Layout `otherwise` case carries the last known offset into later revisions so ordinary version-only servicing updates remain usable. This is a compatibility policy, not evidence that an unaudited binary retained the layout; exact Required symbol resolution and the normal all-or-nothing projection validation still apply.

Required top-level `known_builds` records the build families explicitly recognized for that module. Codegen emits this list into `ModuleRegistry`; startup suppresses the new-Windows-version warning only when both `uDWM.dll` and `dwmcore.dll` recognize their current builds. This warning policy is independent of the open-ended module compatibility range and Required projection validation: a known build is not by itself a runtime support claim.

Generate or validate this architecture explicitly:

```powershell
python Scripts/projection_codegen.py --repo . --architecture milcomp --output Cache\ProjectionManual\milcomp
```

Generated C++ belongs under `$(IntDir)` and must not be committed. Runtime metadata uses naturally aligned startup-only tables; after Layout selection, normal `read/ref/address` access is a direct offset load and address calculation with no registry lookup or version scan.

Raw Symbols must retain their exact function-pointer type; a `BYTE*` Symbol is permitted only with explicit `usage: "code_address"` for instruction-pattern navigation. Model projected ABI changes as disjoint typed ranges. The narrowly supported `abi_compatibility` forms are compile-time-checked exceptions for intentionally discarded returns or one extra trailing Win64 argument, not a general ABI escape hatch.

The Symbol validator treats a projected wrapper as a declaration, not a consumer. Every projected function must have a real runtime call site or a direct typed Symbol consumer such as a Detour; delete unused descriptors rather than letting them become Required startup gates.

```powershell
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_layouts.py . --architecture milcomp
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_symbols.py . --architecture milcomp
msbuild OpenGlass\OpenGlass.MILComp.vcxproj /m /p:Configuration=Release /p:Platform=x64
```
