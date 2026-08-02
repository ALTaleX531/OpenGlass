# MILComp projection schemas

`udwm.json` and `dwmcore.json` are the only editable projection inventories for the build 28000+ compositor architecture. Preserve their exact DbgHelp `UNDNAME_COMPLETE` names, ranges, offset expressions, and useful reverse-engineering guidance independently from Legacy. Use `notes` only for non-obvious semantic anchors, constructor/xref routes, adjusted-`this`, ABI traps, ICF/inlining ambiguity, or cross-checks; do not restate exact PDB names, ranges, visibility, ordinary consumers, or migration provenance. Stable IDs are diagnostic labels; exact `symbol_names` are the sole matching truth, without name-only, substring, decorated-name, or first-match fallbacks.

Generate or validate this architecture explicitly:

```powershell
python Scripts/projection_codegen.py --repo . --architecture milcomp --output Cache\ProjectionManual\milcomp
```

Generated C++ belongs under `$(IntDir)` and must not be committed. Runtime metadata uses naturally aligned startup-only tables; after Layout selection, normal `read/ref/address` access is a direct offset load and address calculation with no registry lookup or version scan.

Raw Symbols must retain their exact function-pointer type; a `BYTE*` Symbol is permitted only with explicit `usage: "code_address"` for instruction-pattern navigation. Model projected ABI changes as disjoint typed ranges. The narrowly supported `abi_compatibility` forms are compile-time-checked exceptions for intentionally discarded returns or one extra trailing Win64 argument, not a general ABI escape hatch.

The Symbol linter treats a projected wrapper as a declaration, not a consumer. Every projected function must have a real runtime call site or a direct typed Symbol consumer such as a Detour; delete unused descriptors rather than letting them become Required startup gates.

```powershell
python .agents/skills/maintain-dwm-offsets/scripts/lint_offset_tables.py . --architecture milcomp
python .agents/skills/maintain-dwm-offsets/scripts/lint_symbol_descriptors.py . --architecture milcomp
msbuild OpenGlass\OpenGlass.MILComp.vcxproj /m /p:Configuration=Release /p:Platform=x64
```
