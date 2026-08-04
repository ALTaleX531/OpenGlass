# OpenGlass agent guide

OpenGlass restores Aero-style glass effects by loading `OpenGlass.dll` into `dwm.exe`. Treat changes to injection, hooks, private DWM layouts, services, and registry writes as system-critical.

## Repository map

- `OpenGlass/`: shared core DLL code, two explicit architecture projects, private DWM implementations under `Architecture/`, and independent schemas under `ProjectionSchemas/`.
- `OpenGlassHost/`: service wrapper that loads the core DLL and starts `ServiceMain`.
- `OpenGlassGUI/`: wxWidgets configuration UI, symbol-download experience, and administrator-only DWM/WER diagnostics.
- `OpenGlassRenderTest/`: interactive GPU/effect benchmark, not a hermetic unit-test suite.
- `OpenGlassTests/`: non-injecting unit tests for shared infrastructure, preset packages, registry behavior, projection resolution, ABI, Layout, and Detour logic.
- `Common/`: shared MSBuild configuration; `Scripts/`: generation, verification, and Inno Setup packaging.

Runtime flow: the Service Control Manager starts `OpenGlassHost.exe`; the host loads `OpenGlass.dll`; service code validates and injects into `dwm.exe`; a named pipe supplies the target session's HKCU handle; injected hooks read configuration and render the effect. The GUI elevates immediately while preserving the original interactive user SID, writes appearance settings immediately, and notifies DWM—its Save action confirms current registry state, while Revert/close may restore captured registry values. Diagnostics actions are separate immediate HKLM operations and are not part of Save/Revert.

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
- Treat GUI setting handlers as immediate mutations. The GUI has no editable scope: the typed setting catalog routes the five Windows colorization base names and their Override forms to the original interactive user's hive, and routes every other public OpenGlass setting to HKLM. The GUI and preset packages deliberately model one system-wide OpenGlass configuration; do not add per-user profiles for blur, materials, reflection, themes, hooks, or other OpenGlass behavior. Check notification behavior, `(RegistryScope, SettingId)` backup identity, rollback semantics, and error reporting together.
- Keep the injected DLL's registry lookup independent from the GUI's canonical write policy. Ordinary runtime values always resolve `HKCU → HKLM → default`; Override pairs always resolve `HKCU Override → HKCU base → HKLM Override → HKLM base → default`. This is a long-term compatibility contract for conversion packs and plug-ins, not a temporary migration fallback. Canonical hive enforcement applies only to writes by the official GUI and preset packages. The GUI scans for values in the wrong canonical hive on every startup, stays silent when none exist, and performs a transactional two-hive migration only after confirmation.
- The GUI manifest must remain `asInvoker`. Its bootstrap uses an ACL-protected random named pipe and verifies both process directions, image, session, PID, and the server token SID before trusting the original user identity. Do not replace the target SID with the elevated account when alternate administrator credentials are used.
- Preset packages are ordinary ZIP files with a root `manifest.json` and an optional UTF-8 `LICENSE`. Unless expressly limited by its text, LICENSE is package-wide for all copyrightable content the author is authorized to license; third-party asset terms must be identified in that same file. No LICENSE means the entire package grants no additional rights. Treat the archive as hostile: validate paths, duplicate casing, entry types, count/size/compression ratio, UTF-8, hashes, PNG structure and CRCs, WIC conversion compatibility, and theme-atlas layout grammar before confirmation; reopen and compare the normalized content digest after confirmation. This validation does not claim to pre-decode the complete DEFLATE pixel stream. Deploy atomically to `%ProgramData%\OpenGlass\Presets\<uuid>`, apply a protected read-only ACL, rewrite asset paths, back up both hives, clean the non-canonical hive, and roll back registry plus any newly deployed directory on failure. Applying another package replaces the pending preview without saving or prompting and must retain the original Save/Revert baseline; a failed application restores only the state immediately before that attempt. Catalog entries record whether they are part of the public preset-pack surface and the version in which they were introduced, so internal compatibility values are never exported or replaced and an older package never deletes future settings. Unknown settings and settings outside the current preset surface are retained only in the immutable package digest, omitted from registry writes, and listed as ignored in every import/apply review; a newer nonzero catalog version alone must not make a package unreadable. Save/Revert must never edit deployed or source package content.
- Installer configuration cleanup is deliberately independent of the GUI and preset package canonical-write policy. Interactive uninstall has separate choices for current-user plus HKLM configuration and preset packages. Clean only known OpenGlass values from the current user's HKCU and from HKLM; never enumerate, load, or modify other users' registry hives. The uninstall page must warn that OpenGlass values in other user profiles are preserved and may require manual cleanup. Never delete the entire Windows DWM key or unrelated values. Silent defaults remain delete current-user plus HKLM configuration unless `/nodeleteconfig` is present and preserve packages unless `/deletepresets` is present.
- Keep writable runtime data outside the binary directory. The runtime and GUI share `%ProgramData%\OpenGlass\symbols` as the default symbol cache; the installer grants Window Manager the required write access. WER diagnostics remain scoped to `LocalDumps\dwm.exe`; do not silently enable global crash collection. Enabling must write `DumpType=2`, `DumpCount=1`, and an explicit `REG_EXPAND_SZ` folder only after creating that folder. Disabling removes only the per-application key. These actions require an elevated administrator token and remain independent of GUI Save/Revert. The installer owns `%ProgramData%\OpenGlass\dumps`: provision it without ordinary-user read access, grant the Window Manager identity the access required by WER's DWM crash-collection path, and remove it recursively on uninstall. Never delete a user-selected custom dump folder.
- Treat the host pipe and user-registry handle transfer as a security boundary. Preserve session and process validation.
- The elevated GUI accepts preset ZIP drops from ordinary Explorer by allowing only `WM_DROPFILES`, `WM_COPYDATA`, and the shell's `0x0049` transfer message through the main window's UIPI filter. Keep this exception window-local, and route every dropped path through the same strict ZIP validation used by **Import...**.

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
msbuild OpenGlassTests/OpenGlassTests.vcxproj /m /p:Configuration=Release /p:Platform=x64
msbuild OpenGlass/OpenGlass.Legacy.vcxproj /m /p:Configuration=Release /p:Platform=x64
msbuild OpenGlass/OpenGlass.MILComp.vcxproj /m /p:Configuration=Release /p:Platform=x64
```

## Safety

- Do not modify or save an IDB during an audit-only request.
- Treat hook publication as one lifecycle transaction. Component preparation may resolve symbols, patterns, IAT slots, vtable slots, and original instruction bytes, but must not mutate a target. Queue eager inline, pointer, import, and instruction hooks in the engine-owned `HookTransaction`; do not create component-level transactions or restore hooks by hand.
- Multiple components may detour the same typed projection Symbol. They must share the generated physical dispatcher and form one logical replacement chain; never enqueue independent SlimDetours attachments for the same Symbol address. The transaction deduplicates identical physical dispatcher entries and rejects conflicting entries that reuse one storage location.
- Every replacement must participate in `HookRundown`. Shutdown closes admission before its single lock-free `DwmFlush()`, removes hooks under the DWM lock, releases the lock, and waits for active calls to drain before freeing resources. Never use `SwitchToThread()` as an unload barrier, wait for rundown while holding `s_csDwmInstance`, or call `DwmFlush()` while holding that lock.
- Legacy must not call `ForceRender()`. MILComp may retain its compositor-specific `ForceRender()` calls. Custom-theme shutdown must preserve the lock-free synchronous `SendMessageW(WM_THEMECHANGED)` before `UnloadTheme()` after its delay-IAT hooks and the OpenGlass window procedure have been removed.
- Lifecycle ownership conflicts, unexpected instruction bytes, rundown timeout, and hook transaction failures are FailFast conditions. Unsupported builds and unresolved Required projections remain pre-hook inert conditions.
- Do not infer that a class is removed because one decorated name is absent; check semantic anchors and callers.
- Symbol matching uses exact, unmodified DbgHelp `UNDNAME_COMPLETE` output. Do not restore name-only, substring, decorated-name, or first-match fallbacks.
- Do not assume nearby flags remain adjacent across builds.
- Do not analyze multiple IDA MCP instances concurrently; instance selection is shared routing state. Reselect and verify the intended module before each query batch.
- Do not publish or claim support for a build until both module projections and the relevant runtime path have been verified.
- Preserve the emergency recovery behavior (`Ctrl`+`Win`+`Shift`+`Q`) when touching runtime code.
