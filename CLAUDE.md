# OpenGlass (milcomp)

Aero Glass effect utility for Windows 11 26H1+. This is the **milcomp** branch — a Windows.UI.Composition based rewrite that bypasses the removed legacy MIL infrastructure. For older Windows versions (10 1809 through 11 25H2), see the `legacy` branch.

Three components: `OpenGlass.dll` (core/payload), `OpenGlassHost.exe` (service wrapper), `OpenGlassGUI.exe` (wxWidgets frontend).

## Architecture

- **Service-DLL**: `OpenGlassHost.exe` loads `OpenGlass.dll` and calls `ServiceMain` (in `ServiceHandler.cpp`).
- **Injection**: The service logic (`GlassService.cpp`) injects `OpenGlass.dll` into `dwm.exe`. IPC via named pipe `\\.\pipe\Global\OpenGlassHostPipe`.
- **DWM hooks**: Uses `knsoft-slimdetours` for hooking. Glass effect rendering hooks into DWM's Windows.UI.Composition visual objects (such as `CSpriteVisual`) via vtable interception. Reflection uses standard `CSpriteVisual` composition APIs. Legacy D2D rendering paths are retained for the glass compositing pipeline.
- **Projections**: Build-specific offsets in `dwmcoreProjection.Offsets.hpp` and `uDwmProjection.Offsets.hpp`. DComp interop interfaces in `DCompProjection.hpp`. Symbol loading via `SymbolParser.hpp`.
- **Configuration**: Registry `HKCU\SOFTWARE\Microsoft\Windows\DWM` (user) and `HKLM\SOFTWARE\Microsoft\Windows\DWM` (system). UI access in `OpenGlassGUI/RegistryConfig.cpp`.

## Code conventions

- **Indentation**: Tabs. **Braces**: Allman style.
- **Naming**: `PascalCase` classes/namespaces/functions; `m_` member prefix; `g_` global prefix.
- **Error handling**: WIL macros (`THROW_IF_FAILED`, `RETURN_IF_FAILED`, `LOG_IF_FAILED`).
- **Memory**: Injected DLL uses `HeapAlloc` from `OpenGlass::Util::g_processHeap` (overloaded `new`/`delete` in `dllmain.cpp`).
- **Logging**: WIL macros → DbgView.

## Graphics

- **Shaders**: HLSL (`.hlsl`) compiled via D3D11 in `BlurEffect.cpp` and `AeroColorizationEffect.cpp`.
- **Reflection**: `CReflectionVisual` extends uDWM's `CSpriteVisual` via vtable hooking and is inserted into the compositor visual tree. See [ReflectionVisual.hpp](OpenGlass/ReflectionVisual.hpp).
- **Assets**: Supports custom atlas (`CustomThemeAtlas`) and reflection textures.

## GUI

- Split across `MainFrame.Core.cpp`, `MainFrame.State.cpp`, `MainFrame.Tabs.cpp`.
- `MainFrame::NotifySettingsChange` sends `WM_DWMCOLORIZATIONCOLORCHANGED` / `WM_THEMECHANGED`.
- Path validation: `ensureFilePath` in `RegistryConfig.cpp`.

## Build & Debug

- MSBuild + `vcpkg` (deps: `wxwidgets`, `wil`, `knsoft-slimdetours`).
- Emergency exit: `Ctrl`+`Win`+`Shift`+`Q` kills `dwm.exe` (registered in `OpenGlass.cpp`).

## Offset files

Each offset struct has an inline comment documenting the verification function, pseudocode pattern, and cross-references. When re-extracting offsets, read the comment above each struct first.

- [uDwmProjection.Offsets.hpp](OpenGlass/uDwmProjection.Offsets.hpp) — uDWM.dll
- [dwmcoreProjection.Offsets.hpp](OpenGlass/dwmcoreProjection.Offsets.hpp) — dwmcore.dll

## Skills

- **extract-dwmcore-offsets**: Extract all dwmcore.dll offsets from an IDA Pro instance. 7-phase audit. (Needs updating for milcomp/26H1.)
- **extract-udwm-offsets**: Extract all uDWM.dll offsets from an IDA Pro instance. 7-phase audit. (Needs updating for milcomp/26H1.)

## IDA Pro

Multiple dwmcore.dll and uDWM.dll instances (1809 through 26H1) available via `ida-pro-mcp`. Use `list_instances` to see active instances and `select_instance` to switch.

## Codebase stability

Offsets only change at recompile boundaries, not every Windows update. The `milcomp` branch targets 28100+ where MIL infrastructure has been removed.

| DLL codebase | Windows releases |
|---|---|
| 17763 | Windows 10 1809 (minimum supported on legacy) |
| 18362 | Windows 10 1903, 1909 |
| 19041 | Windows 10 2004, 20H2, 21H1, 21H2, 22H2 |
| 20348 | Windows Server 2022 |
| 22000 | Windows 11 21H2 RTM (Build 22000) |
| 22621 | Windows 11 22H2, 23H2 |
| 26100 | Windows 11 24H2, 25H2 |
| 28100 | Windows 11 26H1+ (milcomp only; MIL infrastructure removed) |
