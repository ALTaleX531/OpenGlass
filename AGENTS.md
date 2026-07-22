# OpenGlass agent guide

OpenGlass restores Aero-style glass effects by loading `OpenGlass.dll` into `dwm.exe`. Treat changes to injection, hooks, private DWM layouts, services, and registry writes as system-critical.

## Repository map

- `OpenGlass/`: core DLL, DWM hooks, projections, effects, symbol loading, and service implementation.
- `OpenGlassHost/`: service wrapper that loads the core DLL and starts `ServiceMain`.
- `OpenGlassGUI/`: wxWidgets configuration UI and symbol-download experience.
- `OpenGlassTest/`: interactive GPU/effect benchmark, not a hermetic unit-test suite.
- `Common/`: shared MSBuild configuration; `Scripts/`: Inno Setup packaging.

Runtime flow: the Service Control Manager starts `OpenGlassHost.exe`; the host loads `OpenGlass.dll`; service code validates and injects into `dwm.exe`; a named pipe supplies the target session's HKCU handle; injected hooks read configuration and render the effect. The GUI writes settings immediately and notifies DWM—its Save action confirms current state, while Revert/close may restore captured values.

## Branch architecture

- `legacy` supports the legacy MIL compositor path from Windows 10 build 17763 through Windows 11 builds below 28000. Its projection headers retain historical `OffsetInfo` intervals.
- `milcomp` targets the build 28000+ compositor family through Windows.UI.Composition visual hooks. Its projection surface is smaller and structurally different.

Determine compatibility from the OS build/revision and verified binary capabilities. Marketing labels such as 25H2, 26H1, or 26H2 are not sufficient. Likewise, symbol presence, object layout, and feature availability are separate facts: do not derive one from another or from allocation size alone.

## Build and verification

Requirements are Visual Studio/MSBuild with the v145 C++ toolset, Windows SDK 10.0, vcpkg manifest integration, and the repository's overlay ports. Dependencies are statically linked and declared in `vcpkg.json`.

```powershell
msbuild OpenGlass.slnx /m /restore /p:Configuration=Release /p:Platform=x64
```

Use `Release` for normal local verification. `ReleaseSigned` requires the signing environment defined by the shared signing props and must not be treated as a general developer build. If `ISCC.exe` is on PATH, a Release GUI build packages the installer automatically; pass `/p:OpenGlassInstallerEnabled=false` when packaging is not part of the check.

`OpenGlassTest.exe` is interactive and GPU-dependent. Report whether it was built or manually exercised; do not call it an automated test. Never install the service, inject into DWM, kill DWM, or write registry settings merely to validate an unrelated source or documentation change.

## Editing conventions

- Preserve tabs and Allman braces in C++.
- Use the existing PascalCase, `m_` member, and `g_` global naming conventions.
- Follow existing WIL error-handling patterns.
- Preserve unrelated working-tree changes and avoid broad generated rewrites.
- Treat GUI setting handlers as immediate mutations. Check HKCU/HKLM selection, notification behavior, rollback semantics, and error reporting together.
- Treat the host pipe and user-registry handle transfer as a security boundary. Preserve session and process validation.

## DWM projections and offsets

Projection tables live in `OpenGlass/dwmcoreProjection.Offsets.hpp` and `OpenGlass/uDwmProjection.Offsets.hpp`. Read `OpenGlass/Util.hpp` before changing them.

`OffsetInfo.build` and `.revision` form an exclusive right boundary: an entry applies while the runtime version is before that threshold. A final `{ .build = 0, .revision = 0 }` is the runtime's open-ended fallback and must be last; it does not prove correctness on future unanalyzed builds. Its absence may deliberately mean the member or feature was removed. Do not add a terminal entry merely to satisfy a mechanical pattern.

Use `$maintain-dwm-offsets` from `.agents/skills/maintain-dwm-offsets/` when asked to inspect DWM binaries, compare layouts, audit a projection, or update these tables. The skill defaults to read-only IDA and repository analysis. A production offset change requires explicit authorization and independent semantic evidence from the exact binary; folder names and prior table values are not evidence.

Keep three verification layers distinct: the table linter proves structural invariants, an IDA semantic audit establishes a value for an exact binary, and real-OS validation exercises the injection/rendering path. None substitutes for the next.

Run the structural linter before and after any projection change. Add `--version BUILD.REVISION` to mechanically resolve the active entry and interval:

```powershell
python .agents/skills/maintain-dwm-offsets/scripts/lint_offset_tables.py .
```

## Safety

- Do not modify or save an IDB during an audit-only request.
- Do not infer that a class is removed because one decorated name is absent; check semantic anchors and callers.
- Do not assume nearby flags remain adjacent across builds.
- Do not analyze multiple IDA MCP instances concurrently; instance selection is shared routing state. Reselect and verify the intended module before each query batch.
- Do not publish or claim support for a build until both module projections and the relevant runtime path have been verified.
- Preserve the emergency recovery behavior (`Ctrl`+`Win`+`Shift`+`Q`) when touching runtime code.
