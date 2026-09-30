# Legacy projection schemas

`udwm.json` and `dwmcore.json` are the only editable sources for OpenGlass's DWM Symbols, projected bindings, exact DbgHelp `UNDNAME_COMPLETE` names, version ranges, Layouts, ABI fallbacks, and reverse-engineering guidance. Do not edit the generated files under `$(IntDir)\Generated\Projection`, and do not duplicate this inventory in C++.

Use `notes` only for lasting, non-obvious guidance: semantic anchors, constructor and xref routes, adjusted-`this` calculations, ABI traps, ICF/inlining ambiguity, and independent cross-checks. Exact PDB names, ranges, visibility, ordinary consumers, and migration provenance belong in structured fields or source references, not in notes.

The optional top-level `min_inclusive` and `max_exclusive` values bound a module's whole inventory. Legacy declares `[17763.0, 28000.0)`, and `ModuleRegistry::Freeze` rejects any version outside that range before symbol collection or hook preparation. A final Layout `otherwise` case carries the last known offset through later servicing revisions within the range, but never across the build 28000 boundary into MILComp.

The required top-level `known_builds` list records the build families a module explicitly recognizes. Codegen emits it into `ModuleRegistry`, and startup skips the new-Windows-version warning only when both `uDWM.dll` and `dwmcore.dll` recognize their current builds. This warning is independent of the module range and Required projection validation; a known build is not by itself a claim of runtime support.

The generator uses only the Python standard library and runs as an incremental MSBuild step. To run it by hand from the repository root, writing into the default x64 Release intermediate directory:

```powershell
python Scripts/projection_codegen.py --repo . --architecture legacy --output Cache\OpenGlass.Legacy\x64\Release\Generated\Projection
```

Add `--check` to validate without writing files. The generator validates the schema first, then atomically replaces each changed output file. Offset expressions remain C++ source text that Python never evaluates. Layout notes become comments in the generated headers and add no runtime data. Symbol names are stored verbatim in a deduplicated NUL string pool, and naturally aligned runtime specs and state are indexed rather than self-registering. Stable IDs are diagnostic labels; `symbol_names` alone decides matching. Do not add name-only, substring, decorated-name, or first-match fallbacks.

Handlers must reach private DWM fields through typed Layout accessors. Add a focused Layout descriptor and accessor rather than embedding a byte offset or a padding-based fake object in a handler. Codegen validates the schema and its projected consumers but deliberately does not parse arbitrary handler C++, so reviewers must enforce this boundary. The rule does not apply to instruction-pattern navigation, COM vtable indexing, or application-owned structures.

Raw Symbols use their exact function-pointer type. The one exception is an instruction-pattern anchor, which must be declared as `BYTE*` with `usage: "code_address"`. Model projected ABI changes with disjoint version ranges. If the OpenGlass wrapper intentionally discards an old return value or supplies one extra trailing Win64 argument, declare the exact source `type` plus `abi_compatibility`; the generated code asserts the relationship at compile time. Do not use this mechanism to cover reordered, removed, or otherwise incompatible arguments.

Each logical Symbol owns one typed slot and a non-empty, ordered `bindings` array. A binding holds the exact `symbol_names` and one `[min_inclusive, max_exclusive)` resolution interval. Bindings within a Symbol must not overlap, and a version that no binding covers makes the Symbol version-inactive rather than unresolved. Merge versioned descriptors into one Symbol only when module, semantic target, kind/usage, requirement, fallback, and underlying ABI are all identical. If the ABI changes, keep separate logical Symbols even when their intervals don't overlap.

Once startup has selected each Layout case, normal `read/ref/address` access is a direct offset load plus address arithmetic. Descriptor lookup, version selection, validation, allocation, locking, and exceptions stay out of the field-access hot path. The checked `offset()` API remains available for startup diagnostics.

Run the Layout and Symbol schema validators before and after edits. The Symbol validator requires every projected function to have a real wrapper call site or a direct typed Symbol consumer; declaring the wrapper is not enough. A normal Release build lets LTCG inline projected wrappers. When you change the projection mechanism, binary size and generated machine code are worth inspecting, but they are development diagnostics, not fixed acceptance thresholds:

```powershell
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_layouts.py . --architecture legacy --version 26100.8972
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_symbols.py . --architecture legacy
msbuild OpenGlass\OpenGlass.Legacy.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

Schema changes alone establish neither semantic correctness nor OS support. Treat an exact-binary semantic audit and real-OS validation as separate release gates.
