# OpenGlass agent guide

OpenGlass restores Aero-style glass effects by loading `OpenGlass.dll` into `dwm.exe`. Treat changes to injection, hooks, private DWM layouts, services, and registry writes as system-critical.

## Repository map

- `OpenGlass/`: shared core DLL code, two explicit architecture projects, private DWM implementations under `Architecture/`, and independent schemas under `ProjectionSchemas/`.
- `OpenGlassHost/`: service wrapper that loads the core DLL and starts `ServiceMain`.
- `OpenGlassGUI/`: wxWidgets configuration UI, symbol-download experience, and administrator-only DWM/WER diagnostics.
- `OpenGlassRenderTest/`: interactive GPU/effect benchmark, not a hermetic unit-test suite.
- `OpenGlassProjectionTests/`: non-injecting typed registry, resolution, ABI, Layout, and Detour tests.
- `Common/`: shared MSBuild configuration; `Scripts/`: generation, verification, and Inno Setup packaging.

Runtime flow: the Service Control Manager starts `OpenGlassHost.exe`; the host loads `OpenGlass.dll`; service code validates and injects into `dwm.exe`; a named pipe supplies the target session's HKCU handle; injected hooks read configuration and render the effect. The GUI writes appearance settings immediately and notifies DWM—its Save action confirms current state, while Revert/close may restore captured values. Diagnostics actions are separate immediate HKLM operations and are not part of Save/Revert.

## Build architecture

- `OpenGlass.Legacy.vcxproj` builds the Legacy MIL compositor path for Windows 10 build 17763 through Windows 11 builds below 28000.
- `OpenGlass.MILComp.vcxproj` builds the MILComp compositor path for build 28000+ through Windows.UI.Composition visual hooks. Its projection schema and ABI remain independent.
- `OpenGlass.Shared.vcxitems` explicitly lists shared source. Architecture-private code lives under `OpenGlass/Architecture/Legacy/` or `MILComp/`; do not introduce source wildcards or spread architecture-selection `#ifdef`s through shared business code.

Determine compatibility from the OS build/revision and verified binary capabilities. Marketing labels such as 25H2, 26H1, or 26H2 are insufficient. Symbol presence, object layout, and feature availability are also separate facts: do not derive one from another or from allocation size alone.

## Build and verification

Requirements are Visual Studio/MSBuild with the v145 C++ toolset, Windows SDK 10.0, Python 3, vcpkg manifest integration, and the repository's overlay ports. Dependencies are statically linked and declared in `vcpkg.json`.

```powershell
msbuild OpenGlass.slnx /m /restore /p:Configuration=Release /p:Platform=x64
```

Use `Release` for normal local verification. `ReleaseSigned` requires the shared signing environment and is not a general developer build. A Release or ReleaseSigned solution build packages both installers when Inno Setup is available; each successful architecture package also creates `OpenGlassSymbols.<Architecture>.zip` containing the matching DLL, Host, and GUI PDBs. Packaging reports a skip without failing when `ISCC.exe` is absent. Set `OpenGlassInstallerEnabled=false` to suppress packaging explicitly.

The solution builds Host and GUI once into `Build\x64\<Configuration>\common\`, the two DLLs into `legacy\` and `milcomp\`, and uses two explicit Utility projects to package them without coupling installer generation to the GUI. To package only one architecture directly, use:

```powershell
msbuild Scripts/OpenGlass.Packaging.proj /m /p:Configuration=Release /p:Architecture=legacy
msbuild Scripts/OpenGlass.Packaging.proj /m /p:Configuration=Release /p:Architecture=milcomp
python Scripts/archive_pdbs.py . --architecture legacy --configuration Release
```

Python generates projection metadata into `$(IntDir)\Generated\Projection`. Generated files are build artifacts: never edit, copy into the source tree, or commit them. PE size and projected-wrapper machine code may be inspected while developing the projection mechanism, but they are not fixed repository acceptance gates.

`OpenGlassRenderTest.exe` is interactive and GPU-dependent. Report whether it was built or manually exercised; do not call it an automated test. Never install the service, inject into DWM, kill DWM, or write registry settings merely to validate an unrelated source or documentation change.

## Editing conventions

- Preserve tabs and Allman braces in C++.
- Use the existing PascalCase, `m_` member, and `g_` global naming conventions.
- Follow existing WIL error-handling patterns.
- Preserve unrelated working-tree changes and avoid broad generated rewrites.
- Treat GUI setting handlers as immediate mutations. Check HKCU/HKLM selection, notification behavior, rollback semantics, and error reporting together.
- Resolve colorization Override pairs in this exact order: HKCU Override, HKCU base, HKLM Override, HKLM base, default. The GUI and injected DLL must share this ordering; never implement Override by nesting two independently layered registry reads, because that lets an HKLM Override incorrectly shadow an HKCU base value.
- Keep WER diagnostics scoped to `LocalDumps\dwm.exe`; do not silently enable global crash collection. Enabling must write `DumpType=2`, `DumpCount=1`, and an explicit `REG_EXPAND_SZ` folder only after creating that folder. Disabling removes only the per-application key. These actions require an elevated administrator token and remain independent of GUI Save/Revert. The installer owns `{app}\dumps`: provision it without ordinary-user read access, grant the Window Manager identity the access required by WER's DWM crash-collection path, and remove it recursively on uninstall. Never delete a custom dump folder outside `{app}`.
- Treat the host pipe and user-registry handle transfer as a security boundary. Preserve session and process validation.

## DWM projections and offsets

`OpenGlass/ProjectionSchemas/legacy/` and `milcomp/` are the only editable projection inventories. Each architecture has independent `udwm.json` and `dwmcore.json` files containing typed Symbols, projected bindings, exact `UNDNAME_COMPLETE` symbol names, version ranges, Layout cases, fallbacks, and audit notes. Use `notes` only for non-obvious reverse-engineering guidance such as semantic anchors, constructor or xref routes, adjusted-`this`, ABI traps, ICF/inlining ambiguity, and independent cross-checks. Do not duplicate facts already expressed by `symbol_names`, ranges, or ordinary consumer references, and never replace useful guidance with generic migration or PDB provenance. `Scripts/projection_codegen.py` requires `--architecture legacy|milcomp` and emits generated C++ only under the selected project's `$(IntDir)`. The compatibility `*.Offsets.hpp` headers only include generated declarations; they are not a second data source.

Handler code must access private DWM fields through typed projection accessors. Add or correct a schema Layout instead of embedding object arithmetic or padding-based fake layouts in handlers. Codegen deliberately does not pretend to parse arbitrary C++; enforce this architectural boundary in review rather than relying on cast-spelling heuristics. This rule does not cover instruction-pattern navigation, COM vtable slots, or ordinary application-owned storage.

`FieldHandle::address()` and `ref()` preserve the base object's constness. Use `mutable_address()` or `mutable_ref()` only inside a projected class facade whose established API intentionally exposes a mutable private subobject from a const method; never use them directly from handler code to bypass constness.

A Layout case's `until` object is an exclusive right boundary: it applies while the exact module build/revision is before that boundary. An `otherwise: true` case is the explicit open end and must be last; it does not prove correctness on unanalyzed future builds. Without it, later versions resolve to the normal `unsupported` state. Preserve `offset` expressions literally, including negative values and `sizeof` arithmetic; Python must never evaluate them.

Generated `SymbolHandle` and `FieldHandle` values encode only module/index/type. Required Symbols are a global startup prerequisite. Both modules follow `Reset → Collect → Validate → Commit`, and neither publishes slots or variables unless both validate. Optional or version-inactive projected functions must have an ABI-compatible fallback; otherwise inactive calls use the typed FailFast thunk. Do not introduce component feature masks or partial readiness.

Raw Symbols must declare their real function-pointer ABI. Use `BYTE*` with `usage: "code_address"` only for instruction-pattern navigation that intentionally treats a symbol as bytes. If a projected ABI changes across builds, use disjoint descriptors for the same target. The exceptional `abi_compatibility` forms (`discard_return` and `extra_trailing_argument`) require an explicit source type and are compile-time checked; never hide an ABI change by placing incompatible complete names in one descriptor.

After `Freeze`, `FieldHandle::read/ref/address` must directly load the selected module offset and perform address arithmetic; they must not call the registry, scan version cases, validate descriptors, allocate, lock, or throw. The explicit `offset()` API remains a checked cold-path diagnostic. Metadata clarity takes priority over byte packing because descriptor traversal is startup-only.

Every projected wrapper must be `inline` and contain only an `OPENGLASS_MUSTTAIL return Projection::Invoke<&Target>(...)` dispatch. `OPENGLASS_MUSTTAIL` expands to `[[msvc::musttail]]` outside Debug; `/Od` Debug uses a normal debuggable forwarder. Normal Release/LTCG may inline the wrapper into a direct call through the typed slot, but `inline` is not treated as a guarantee. Do not use `__forceinline`: MSVC may still refuse it and emit C4714 warnings. Do not add range checks, `.get()`, fallback logic, runtime instruction patching, or `VirtualProtect` to projected wrappers. A wrapper declaration alone is not a consumer: every projected function must have a real runtime call site or a direct typed Symbol consumer such as a Detour. Remove unused wrappers and descriptors rather than making them needless Required startup gates. Raw symbol access, Detour preparation, and pattern anchors use the checked cold path.

Use `$maintain-dwm-offsets` from `.agents/skills/maintain-dwm-offsets/` when asked to inspect DWM binaries, compare layouts, audit a projection, or update the schemas. The skill defaults to read-only IDA and repository analysis. A production schema change requires explicit authorization and independent semantic evidence from the exact binary; folder names and prior values are not evidence.

Keep three verification layers distinct: static verification combines schema validation with a paired PE/PDB resolution audit; an IDA semantic audit establishes ABI and meaning; and real-OS validation exercises the injection/rendering path. Exact DbgHelp output and uniqueness do not substitute for semantic or runtime evidence.

Use `Scripts/dump_symbols.py --input IMAGE [--output SYMBOL_CACHE] [--grep TEXT]` for quick complete-name discovery directly from an image. The symbol cache defaults to `%TEMP%\symbols`. Its output is convenient schema input, not a production audit report; use `.agents/skills/maintain-dwm-offsets/scripts/audit_symbol_resolution.py` to record PE/PDB identity and IDA to establish semantics.

```powershell
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_layouts.py . --architecture legacy --module udwm --id STABLE_ID --version BUILD.REVISION
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_symbols.py . --architecture legacy
python .agents/skills/maintain-dwm-offsets/scripts/audit_symbol_resolution.py . --architecture legacy --module udwm --version BUILD.REVISION --image PATH_TO_DLL --symbol-path PATH_TO_SYMBOLS --configuration release
python -m unittest discover -s .agents/skills/maintain-dwm-offsets/tests -p "test_*.py"
python Scripts/test_archive_pdbs.py
python Scripts/test_dump_symbols.py
python Scripts/test_projection_codegen.py
msbuild OpenGlassProjectionTests/OpenGlassProjectionTests.vcxproj /m /p:Configuration=Release /p:Platform=x64
msbuild OpenGlass/OpenGlass.Legacy.vcxproj /m /p:Configuration=Release /p:Platform=x64
msbuild OpenGlass/OpenGlass.MILComp.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

## Safety

- Do not modify or save an IDB during an audit-only request.
- Do not infer that a class is removed because one decorated name is absent; check semantic anchors and callers.
- Symbol matching uses exact, unmodified DbgHelp `UNDNAME_COMPLETE` output. Do not restore name-only, substring, decorated-name, or first-match fallbacks.
- Do not assume nearby flags remain adjacent across builds.
- Do not analyze multiple IDA MCP instances concurrently; instance selection is shared routing state. Reselect and verify the intended module before each query batch.
- Do not publish or claim support for a build until both module projections and the relevant runtime path have been verified.
- Preserve the emergency recovery behavior (`Ctrl`+`Win`+`Shift`+`Q`) when touching runtime code.
