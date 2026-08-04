![header](assets/banner.png)

# Experience native look of Aero Glass interface on Windows 10+

This utility returns the full glass effect to the window frame like [glass8](http://www.msfn.org/board/forum/180-aero-glass-for-windows-8/), but with deeper control over blur, reflections, and theme integration.

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/ALTaleX531/OpenGlass)

## Supported Windows versions

- Windows 10 build 17763 (1809) through build 19045 (22H2)
- Windows 11 builds below 28000 with the Legacy package, including build 26200 (25H2)
- Windows 11 builds 28000 and later with the experimental MILComp package
- Windows Server 2022

> [!IMPORTANT]
> The Legacy DLL does **not** support build 28000 or later because the legacy MIL compositor path it hooks is no longer available. See [#260](https://github.com/ALTaleX531/OpenGlass/issues/260).
>
> For build 28000 and later, use `OpenGlassSetup.MILComp.exe` rather than the Legacy installer.

Windows release names are not a linear compatibility scale. Microsoft describes Windows 11 26H1/build 28000 as a platform release for selected new devices rather than an in-place feature update from 24H2 or 25H2, and Windows Insider development can use parallel build trains. OpenGlass therefore evaluates three separate facts:

1. the marketing release name (for user-facing context);
2. the exact OS build and revision (for selecting an offset interval); and
3. the compositor capabilities present in the actual DWM binaries (for selecting a DLL architecture).

Use the build number and verified compositor path to choose an architecture; do not select one from `26H1` or `26H2` alone. See Microsoft's [Windows 11 release information](https://learn.microsoft.com/en-us/windows/release-health/windows11-release-information), [build 28000 announcement](https://blogs.windows.com/windows-insider/2025/11/07/announcing-windows-11-insider-preview-build-28000-canary-channel/), and [parallel 26H1 Insider build trains](https://blogs.windows.com/windows-insider/2026/06/08/announcing-new-builds-8-june-2026-2/) for the upstream release model.

> [!NOTE]
> OpenGlass only supports Windows builds from the General Availability channel. Builds from other channels (such as Canary, Dev, Release Preview and Beta) and Windows Server versions other than 2022 are **NOT supported**. Running on unsupported builds can crash DWM.

## DWM architectures

One source tree builds two architecture-specific `OpenGlass.dll` files:

| Build target | Target | Architecture | Status |
|--------------|--------|--------------|--------|
| `OpenGlass.Legacy.vcxproj` | Builds 17763 through 27999, including Windows 11 25H2/build 26200 | MIL compositor draw stream hooks | Stable |
| `OpenGlass.MILComp.vcxproj` | Builds 28000 and later | Windows.UI.Composition visual hooks | Experimental |

**Key differences**:
- `legacy` relies on the legacy MIL compositor path and is limited to builds below 28000.
- `milcomp` bypasses that removed path by working at the Windows.UI.Composition level for builds 28000 and later.

Choose MILComp when the actual OS build is 28000 or later. Otherwise, use Legacy. Marketing release labels are explanatory only.

> [!CAUTION]
> The MILComp architecture remains experimental. Injection and rendering have been validated on build 28000 and 28100 samples; virtual-desktop preview, symbol failure, download retry, and DWM recovery paths have also been exercised. These results apply only to the tested binaries, and unverified revisions remain unsupported.

## Who should use OpenGlass?

OpenGlass is for advanced users who are comfortable editing the Windows registry and troubleshooting DWM. If you want a simpler option, try [DWMBlurGlass](https://github.com/Maplespe/DWMBlurGlass).

## Quick start

### Getting started

1. **Installation**: Download the latest Inno Setup package from [Releases](https://github.com/ALTaleX531/OpenGlass/releases) and follow the installer.
2. **Configuration**: Use the OpenGlass GUI or edit the [registry](#configuration) directly (manual registry edits require restarting OpenGlass or changing system color settings to apply). The GUI always requests administrator elevation at startup because one page can contain both per-user Windows colors and system-wide OpenGlass settings.
3. **Reference**: Review the release notes and source code to stay informed about behavior changes and technical updates.

The GUI's **Glass colors** page includes the original Windows Vista and Windows 7 color presets. The displayed family follows **Glass type**, but changing the type does not apply a preset. The compact controls mirror the classic control panel: select a swatch, enable or disable transparency, and adjust color intensity. **Detailed colorization settings** contains active/inactive colors and Windows 7 composition parameters, while **Advanced colorization settings** contains low-level blending controls. Reflection opacity variants on **Theme** and text color overrides on **Appearance** are also collapsed by default. Changes apply immediately, and **Revert** restores the values from before editing. Windows 7 composition values use the documented [`dwm_colorization_calculator`](https://github.com/ALTaleX531/dwm_colorization_calculator) conversion.

The **Preset packs** page imports, creates, applies, and removes immutable OpenGlass preset packages stored as ordinary `.zip` archives. A preset pack represents one system-wide OpenGlass configuration; only the five Windows colorization values and their Override forms remain independent for each user. Multi-user profiles for blur, materials, reflection, themes, hooks, or other OpenGlass behavior are intentionally not supported by the GUI or package format. Installed packages include a color preview in the library. Use **Import...** or drop one or more ZIPs anywhere on the GUI. A single dropped package offers **Import only** or **Import and apply**; a multi-file drop validates and imports the complete batch without changing the current configuration. **Create from current preview...** captures the settings currently applied by the GUI, including unsaved changes, so Save is not required before authoring a package. Creating the ZIP does not save those changes or alter the Revert baseline. The creation dialog can install the new ZIP immediately without applying it and remembers the previous author and license inputs until the GUI exits. **Reset all settings...** deletes the explicit preset-pack values so a package author can begin from default and inherited values; the reset applies immediately and remains reversible until Save. A package replaces the complete configuration catalog version it declares; it is never merged with the current configuration. Applying another package replaces the current unsaved preview without an extra save/discard prompt, so packages can be tried in sequence; **Save** accepts the latest result and **Revert** restores the configuration from before the trial. Applied settings remain editable, but Save/Revert modify only the registry and never rewrite the ZIP or its deployed copy.

> [!TIP]
> **Emergency Exit**: Long press <kbd>Ctrl</kbd>+<kbd>Win</kbd>+<kbd>Shift</kbd>+<kbd>Q</kbd> to immediately terminate DWM if the system becomes unresponsive.

### Reporting issues

If you encounter crashes or technical bugs:

1. **Collect Dumps**: Open the elevated OpenGlass GUI, select the **Diagnostics** tab, choose a dump folder, and select **Enable full dumps**. The default is `%ProgramData%\OpenGlass\dumps`. This creates the per-application `HKLM\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\dwm.exe` configuration with `DumpType=2` and `DumpCount=1`; **Disable dumps** removes that per-application configuration. The installer protects the default folder for WER collection of DWM crashes and administrator access, and removes it with its contents during uninstall. System-wide WER settings may still apply independently. See Microsoft's [WER guidelines](https://learn.microsoft.com/en-us/windows/win32/wer/collecting-user-mode-dumps) for the underlying behavior and folder-permission requirements.
2. **Submit a Report**: Open a GitHub issue with your **Windows build**, **registry settings**, **steps to reproduce**, and **visual evidence** (screenshots/recordings) if necessary.

## Configuration

**Methods**: Use the OpenGlass GUI for convenience, or edit the registry directly for advanced control. The executable manifest remains `asInvoker`; an unelevated bootstrap instance immediately relaunches itself with `runas`, and canceling UAC exits the GUI. The old editable HKCU/HKLM scope page no longer exists. The elevated instance retains the original interactive user's SID, even if different administrator credentials are supplied, so user colorization values do not accidentally enter the administrator's profile.

When the GUI detects settings stored outside its canonical locations, it offers **Migrate and continue** or **Exit and migrate later**. Migration preserves the old effective values, explains that non-color settings become shared by all users, and backs up both hives. If no misplaced values exist, the check is silent; a failed or declined migration never enters the editor.

OpenGlass GUI configuration is system-wide by design, apart from Windows colorization. This intentionally favors one clear machine configuration over independent OpenGlass profiles for multiple Windows users.

**Canonical registry locations**:

| Canonical hive | Settings |
| --- | --- |
| Original interactive user's `HKEY_USERS\<SID>\SOFTWARE\Microsoft\Windows\DWM` | `ColorizationColor`, `ColorizationAfterglow`, `ColorizationColorBalance`, `ColorizationAfterglowBalance`, and `ColorizationBlurBalance`, including each `Override` form |
| `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\DWM` | Every other public OpenGlass setting in this document |

> [!IMPORTANT]
> The canonical table above applies only to writes performed by the official OpenGlass GUI and preset packages. It does not deprecate or restrict the DLL's registry lookup behavior. For third-party transformation packs and plug-ins, `HKCU → HKLM` remains a long-term compatibility contract rather than a temporary migration fallback.

**Key inheritance**: Missing keys use predefined defaults. Variants (for example, `XXXInactive` and `XXXMaximized`) inherit from their base key if not explicitly set. The injected DLL resolves ordinary values as `HKCU → HKLM → default`, while Override pairs resolve `HKCU Override → HKCU base → HKLM Override → HKLM base → default`. Explicit GUI edits create the per-user Override form. The reset button removes stored copies from both hives and returns to the inherited value or default.

Built-in Vista presets write `ColorizationColorOverride` and derive `GlassOpacity` from the preset alpha. Built-in Windows 7 presets write color, afterglow, and the three composition-parameter Override values using the current opaque-blend state. Presets do not change opaque blend, reflection intensity, inactive colors, or advanced blending settings.

**Enable transparency** is the inverse presentation of `ColorizationOpaqueBlend`. For Vista glass, **Color intensity** edits `GlassOpacity`; for Windows 7 glass it edits the color and afterglow alpha and recalculates the three composition balances. Both historical sliders use the classic 10–85% range.

## Preset ZIP packages

An OpenGlass preset is a standard ZIP recognized by a root `manifest.json`; no custom filename extension is used. The recommended filename is `<sanitized-name>-<uuid>.zip`. Schema version 2 contains:

```text
manifest.json
[LICENSE]
assets/
  theme-atlas.png
  theme-atlas.png.layout
  reflection.png
  material.png
```

```json
{
  "schema_version": 2,
  "catalog_version": 1,
  "uuid": "00112233-4455-6677-8899-aabbccddeeff",
  "name": "Preset name",
  "description": "Optional preset description",
  "author": { "name": "Author", "homepage": "https://example.com" },
  "license": { "file": "LICENSE" },
  "settings": {},
  "assets": {}
}
```

The abbreviated empty objects above show the metadata shape only; a valid package must contain every preset-pack setting defined by its catalog version, using a typed value, an in-package asset reference, or `null` to delete that value during Replace. Settings unknown to the current OpenGlass build, including implementation-specific compatibility values, are never applied or exported. They do not invalidate an otherwise compatible package: the review dialog lists them explicitly as ignored, allowing older builds to inspect and apply the subset they understand from packages with a newer catalog version.

The manifest contains a canonical lowercase UUID, package name, optional description, an author name and absolute HTTP(S) homepage, the complete typed preset-pack catalog for its declared catalog version, and SHA-256/size records for included assets. The `description` field remains present but may be an empty string. `license` may be `null`; otherwise a non-empty UTF-8 `LICENSE` is included and OpenGlass derives a short display name from an SPDX identifier, a recognized license text, or its title.

Unless its text expressly limits the scope, the root `LICENSE` applies to the entire preset package—all copyrightable manifest content, package selection and arrangement, images, and layout files—but only to material the author is authorized to license. Authors who include third-party assets under different terms must identify those assets, copyright holders, and terms inside the same `LICENSE`; schema version 2 does not model separate per-asset licenses. If no LICENSE is provided, OpenGlass presents the entire package as **all rights reserved by default**: local application does not imply permission to modify or redistribute any package content. Author identity and licensing authority are displayed as **unverified**, and OpenGlass does not establish a signature or trust chain. When a future GUI reads an older catalog, Replace affects only settings introduced by that catalog version and leaves newer settings untouched.

Package assets replace original absolute paths. Import deploys verified content read-only under `%ProgramData%\OpenGlass\Presets\<uuid>\` and rewrites canonical asset settings to that location. The deployed ACL grants Full access only to SYSTEM and Administrators and Read/Execute to Users and Window Manager. A UUID is an immutable content identity: identical content is reused, while different content with the same UUID is rejected. Editing metadata, LICENSE, settings, or an asset requires a new UUID.

Import rejects path traversal, alternate data streams, case-insensitive duplicate names, links or non-regular entries, excessive paths, malformed/truncated archives, abnormal compression ratios, size/count limits, hash mismatches, invalid theme-atlas layouts, and images that fail bounded PNG structure/CRC checks or WIC conversion setup. The same validators protect external atlas, reflection, and material files before the injected DLL publishes them to rendering state. PNG validation deliberately does not traverse the complete DEFLATE pixel stream with `CopyPixels`; actual pixel decoding still occurs when the resource is rendered. Preset packages accept no script, shader, DLL, executable, or network asset. Applying a package requires confirmation of the complete set/delete diff, sensitive settings, graphics decoded by `dwm.exe`, and restart-required values; OpenGlass never restarts DWM automatically.

Removing a deployed package is refused while a canonical asset setting references it. Interactive uninstall presents one options page with two independent choices. **Delete current-user and machine configuration** is selected by default, while **Delete installed OpenGlass preset packages** is not. Configuration cleanup removes only known OpenGlass values from the uninstalling user's HKCU and from HKLM; settings in other Windows user profiles are deliberately preserved, and the uninstall page warns that those values may be removed manually while signed in as the corresponding user. Cleanup remains independent of the GUI and preset package canonical-write policy. Silent uninstall keeps the same defaults: `/nodeleteconfig` preserves current-user and machine configuration, while `/deletepresets` removes `%ProgramData%\OpenGlass\Presets`.

## Registry reference

### Colorization settings

| Key Name | Type | Description |
| -------- | ---- | ----------- |
| ColorizationColor(Override)<br>ColorizationColorInactive<br>ColorizationAfterglow(Override) | DWORD | ARGB color used for the glass effect, alpha channel is ignored.<br><br>ℹ️ `ColorizationColorInactive` is only used when `GlassType` = 0x0<br>ℹ️ `ColorizationAfterglow(Override)` is only used when `GlassType` = 0x1 |
| ColorizationColorBalance(Override)<br>ColorizationAfterglowBalance(Override)<br>ColorizationBlurBalance(Override) | DWORD | Composition parameters for Windows 7 Aero effect shader.<br><br>ℹ️ Only used when `GlassType` = 0x1 |
| GlassOpacity<br>GlassOpacityInactive | DWORD | The intensity of the color (0-100%). Default value is 63%.<br><br>ℹ️ Only used when `GlassType` = 0x0 |
| ColorizationColorCaption<br>ColorizationColorCaptionInactive<br>ColorizationColorCaptionMaximized<br>ColorizationColorCaptionInactiveMaximized | DWORD | Color used for drawing window titles. Format is 0xBBGGRR.<br><br><ul><li>0xFFFFFFFF = Determined by the system</li><li>0xFFFFFFFE = Read the `TEXTCOLOR` property from the current theme to obtain them.</li><li>0xFFFFFFFD = Automatically select the appropriate text colors based on `GlassType`. (default)</li></ul> |
| ColorizationOpaqueBlend | DWORD | Controls the transparency of glass effect (default = 0). |
| ColorizationBaseTransparent<br>ColorizationBaseMaximized<br>ColorizationBaseOpaque | DWORD | ARGB base color used for color blending. <br><br><ul><li>0xFFFFFFFE = Automatically select the appropriate base color based on `GlassType` (default).</li><li>0xFFFFFFFF = Read the `COLORIZATIONCOLOR` property from the current theme to obtain them.</li></ul> |
| ColorizationOpaqueBlendPriority | DWORD | Behavior of choosing opaque blend base color. <br><br><ul><li>0x0 = Windows Vista.</li><li>0x1 = Windows 7.</li><li>0xFFFFFFFF = Automatically select the appropriate behavior based on `GlassType` (default).</li></ul>ℹ️ For Windows Vista, `ColorizationBaseMaximized` is preferred, whereas for Windows 7 it is `ColorizationBaseOpaque`. |
| ColorizationOpacity<br>ColorizationOpacityInactive<br>ColorizationOpacityMaximized<br>ColorizationOpacityInactiveMaximized | DWORD | (Additional) factors applied to glass color blending. (0%-100%). <br><br><ul><li>0xFFFFFFFE = Automatically select the appropriate factors based on `GlassType`. (default).</li><li>0xFFFFFFFF = Read the `COLORIZATIONOPACITY` property from the current theme to obtain them.</li></ul> |

### Glass settings

| Key Name | Type | Description |
| -------- | ---- | ----------- |
| GlassType | DWORD | The type of glass effect. <br><br><ul><li>0x0 = Windows Vista style blur (default).</li><li>0x1 = Windows 7 style blur.</li></ul> |
| GlassOverrideAccent | DWORD | Overrides accent blur surfaces with OpenGlass glass effects (e.g. the win10 taskbar). Default is 0. |
| CustomThemeReflection | String | Path to file with texture that is stretched over whole desktop and rendered above glass regions (default is Aero Glass Win7 reflection texture) |
| ColorizationGlassReflectionIntensity | DWORD | The overall multiplier applied to the intensity of reflection effect (0-100%). Default value is 0%.<br><br>opacity = base_opacity * intensity * 2 |
| ColorizationGlassReflectionOpacity<br>ColorizationGlassReflectionOpacityInactive<br>ColorizationGlassReflectionOpacityMaximized<br>ColorizationGlassReflectionOpacityInactiveMaximized | DWORD | The base opacity of reflection effect (0-100%). <br><br><ul><li>0xFFFFFFFE = Automatically select the appropriate factors based on `GlassType`. (default)</li><li>0xFFFFFFFF = Read the `OPACITY` property of `SQUEEGEREFLECTIONMAP` from the current theme to obtain them.</li></ul> |
| ColorizationGlassReflectionParallaxIntensity | DWORD | The parallax intensity of the reflection effect (e.g. when moving the windows side to side). Default value is 13%. |
| ColorizationGlassReflectionPolicy | DWORD | Controls where reflections should be rendered (default = 0xFFFFFFFF). <br><br><ul><li>Titlebar = 1<<0</li><li>Aero Peek = 1<<2</li><li>Aero Snap = 1<<3 (ℹ️ Only effective in Win10)</li><li>Render everywhere if possible = 0xFFFFFFFF</li></ul> |
| BlurDeviation | DWORD | Standard deviation for gaussian blur, default = 30 (which means σ = 3.0) <br>Value 0 results in non-blurred transparency.<br><br>ℹ️ Only effective when `UseDirect3DRendering` = 0x0 |
| BlurOptimization | DWORD | Quality of gaussian blur<br><br><ul><li>0x0 = Speed first (default)</li><li>0x1 = Balance</li><li>0x2 = Quality first</li></ul>  |
| RoundRectRadius | DWORD | The radius of glass geometry (default = 0), Win8=0, Win7=6 |
| CustomThemeMaterial | String | Path to file with texture that is rendered (tiled) above glass regions (default is Acrylic noise texture) |
| MaterialOpacity | DWORD | opacity of material texture (default = 0) |
| UseDirect3DRendering | DWORD | Set 1 to use d3d11 as glass renderer backend, and the blur radius is hardcoded to 3. (default = 0) |

### Theme settings

| Key Name | Type | Description |
| -------- | ---- | ----------- |
| CaptionButtons | DWORD | Changes caption buttons sizes, icon left margin and the opacity of the button glyphs.<br><br><ul><li>0x0 = Vanilla style (default)</li><li>0x1 = Windows Vista style</li><li>0x2 = Windows 7 style</li><li>0x3 = Windows 8 style</li></ul> |
| CenterCaption | DWORD | Controls how title bar text is aligned.<br><br><ul><li>0x0 = Keeps it on the left (default)</li><li>0x1 = Regular centering</li><li>0x2 = Windows 8 style centering</li></ul> |
| TextGlowMode | DWORD | Specifies how window caption glow effect will be rendered <br><br><ul><li>0x0 = No glow effect</li><li>0x1 = Glow effect loaded from atlas (default)</li><li>0x2 = Glow effect loaded from atlas and theme opacity is respected</li><li>0x3 = Composited glow effect using your theme settings HIWORD of the value specifies glow size (0 = theme default)</li></ul> |
| CustomThemeAtlas | String | Path to PNG file with theme resource (bitmap must have exactly the same layout as msstyle theme you are using!). <br><br>💡 OpenGlass also looks for a `.layout` file with the same name (e.g., `theme.png.layout`) to determine the layout of the atlas. |
| DisableModernBorders | DWORD | Disable modern rounded window borders. <br><br><ul><li>0x0 = Enable modern borders (default)</li><li>0x1 = Disable modern borders</li></ul><br>ℹ️ Only effective in Win11 |

### Advanced settings

These settings are intended for `HKLM` and should only be modified if necessary.

> [!CAUTION]
> Do not modify this section unless you fully understand the impact.


| Key Name | Type | Description |
| -------- | ---- | ----------- |
| DisableGlassOnBattery | DWORD | <ul><li>0x1 = When energy saver is on then the glass effect will be opaque to decrease energy consumption (default)</li><li>0x0 = glass effect won't be opaque on energy saver</li></ul> |
| DisabledHooks | DWORD | Controls which architecture-specific handler hooks are disabled, which also controls feature availability. Implementations live under [Legacy](OpenGlass/Architecture/Legacy/) and [MILComp](OpenGlass/Architecture/MILComp/). <br><br><ul><li>0x0 = No hooks are disabled (default)</li><li>0x1 = Disables CaptionTextHandler hooks</li><li>0x2 = Disables AccentOverrider hooks</li><li>0x4 = Disables GlassFrameHandler hooks</li><li>0x8 = Disables GlassReflectionHandler hooks</li><li>0x10 = Disables CaptionMetricsTweaker hooks</li></ul><br>⚠️ Should only be used to maintain compatibility with third-party applications. |
| GlassSafetyZoneMode | DWORD | Set 0 to disable glass safety zone. (default = 1) |

## Credits

### [Banner for OpenGlass](https://github.com/ALTaleX531/OpenGlass/discussions/11)

Provided by [@aubymori](https://github.com/aubymori).
Wallpaper: [metalheart jawn #2](https://www.deviantart.com/kfh83/art/metalheart-jawn-2-1068250045) by [@kfh83](https://github.com/kfh83).

### [[MS-RDPCR2]: Remote Desktop Protocol: Composited Remoting V2](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpcr2)

Specifies the Remote Desktop Protocol: Composited Remoting V2, which displays the contents of the Windows-based desktop running on one machine on a second machine connected to the first via a network.

### [KNSoft.SlimDetours](https://github.com/KNSoft/KNSoft.SlimDetours)

SlimDetours is an improved Windows API hooking library base on Microsoft Detours.

### [VC-LTL - An elegant way to compile lighter binaries.](https://github.com/Chuyu-Team/VC-LTL5)

VC-LTL is an open source CRT library based on the MS VCRT that reduce program binary size and say goodbye to Microsoft runtime DLLs, such as msvcr120.dll, api-ms-win-crt-time-l1-1-0.dll and other dependencies.

### [Windows Implementation Libraries (WIL)](https://github.com/Microsoft/wil)

The Windows Implementation Libraries (WIL) is a header-only C++ library created to make life easier for developers on Windows through readable type-safe C++ interfaces for common Windows coding patterns.

### [libvalinet](https://github.com/valinet/libvalinet)

A header-only collection of generic implementations shared between multiple projects.

OpenGlass borrowed its symbol download feature.

### [TranslucentTB](https://github.com/TranslucentTB/TranslucentTB)

A lightweight utility that makes the Windows taskbar translucent/transparent.

OpenGlass borrowed its C++ project structure.

## Development build

### Prerequisites

- **Visual Studio 2026** with C++ desktop development workload
- **vcpkg**: main C++ dependencies (wxwidgets, wil, knsoft-slimdetours) are pulled via vcpkg. Auxiliary packages (VC-LTL, SourceLink) come from NuGet.

### Building

1. Run `vcpkg integrate install` so dependencies are picked up automatically by MSBuild.
2. Open `OpenGlass.slnx` in Visual Studio.
3. Select the `Release` configuration and build either `OpenGlass.Legacy` or `OpenGlass.MILComp`; Host and GUI are shared and build once.

> [!TIP]
> **Inno Setup is not required for normal builds.** A Release solution build automatically generates both installers when `ISCC.exe` is available and otherwise reports that packaging was skipped. Each successful package also creates `OpenGlassSymbols.Legacy.zip` or `OpenGlassSymbols.MILComp.zip` beside the installers, containing the matching `OpenGlass.pdb`, `OpenGlassHost.pdb`, and `OpenGlassGUI.pdb`. The `Scripts/OpenGlass.Packaging.proj` target remains available for explicitly packaging only `/p:Architecture=legacy` or `/p:Architecture=milcomp`.

### ReleaseSigned configuration

Official releases use the `ReleaseSigned` configuration. This configuration uses macros to prevent digital signature from being abused. **Do not mix `ReleaseSigned` binaries with plain `Release` binaries**. Signed and unsigned components are not compatible. If you compile with the plain `Release` configuration, some features may behave differently when loaded alongside official signed DLLs.

## Maintaining DWM projections

OpenGlass describes the private `dwmcore.dll` and `uDWM.dll` ABI with generated typed Symbol and Layout handles backed by startup-time module registries. Layout offsets and required symbols can change between Windows builds and must be audited against the exact binaries. Legacy and MILComp schemas remain independent.

For quick signature discovery, `Scripts/dump_symbols.py` asks DbgHelp to download the PDB matched to an input image and prints exact `UNDNAME_COMPLETE` names:

```powershell
python Scripts/dump_symbols.py --input "$env:WINDIR\System32\dwmcore.dll" --grep "COcclusionContext::PreSubgraph"
```

Symbols are cached under `%TEMP%\symbols` by default; pass `--output PATH` to override it. Add `--rva` when an image-relative address is useful. This is a discovery convenience; use the paired audit tool and IDA semantic analysis before changing a production descriptor.

### Agent-assisted audit

The repository provides the explicit `$maintain-dwm-offsets` skill for general coding agents. It supports focused projection checks, cross-build comparisons, per-module audits, and explicitly requested full audits. Select `legacy` or `milcomp` explicitly; schema `notes` contain the reverse-engineering routing evidence.

**Setup**: Connect the agent to IDA Pro through [ida-pro-mcp](https://github.com/mrexodia/ida-pro-mcp). The repository declares this capability dependency but does not commit machine-specific MCP ports or launch configuration.

1. Open the exact DLL in IDA Pro and wait for auto-analysis to finish.
2. Invoke `$maintain-dwm-offsets` with the module, samples, and desired audit scope.
3. Review the report's schema-selected interval, active Required/Optional state, sample identity, semantic evidence, findings, unverified items, runtime-validation status, and suggested descriptor changes.
4. Apply changes to the matching Layout or Symbol descriptor only after the value, complete PDB name, and typed ABI have the required independent evidence.

Run both read-only schema validators before and after editing descriptors:

```powershell
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_layouts.py . --architecture legacy --version BUILD.REVISION
python .agents/skills/maintain-dwm-offsets/scripts/validate_projection_symbols.py . --architecture legacy
python .agents/skills/maintain-dwm-offsets/scripts/audit_symbol_resolution.py . --architecture legacy --module udwm --version BUILD.REVISION --image PATH_TO_DLL --symbol-path PATH_TO_SYMBOLS --configuration release
```

The Layout validator preserves offset expressions as source text, resolves exact exclusive-right-boundary intervals, and reports an intentional `unsupported` result when no case applies. The Symbol validator checks stable IDs, module ownership, ranges, requirements, complete-name candidates, signature variants, and consumer references. The resolution auditor additionally verifies the image/PDB GUID and age before resolving exact DbgHelp `UNDNAME_COMPLETE` output, records the DbgHelp identity, and excludes Debug-only descriptors unless `--configuration debug` is requested.

Keep three verification layers separate: static verification combines schema validation with paired PE/PDB resolution auditing, an IDA semantic audit establishes ABI and meaning, and real-OS validation exercises service injection, recovery, and rendering. A successful static audit is not a release-readiness claim; use the skill's real-OS checklist before claiming support for a new layout interval.

### Manual update

If IDA Pro is not available, use another disassembler to follow the semantic anchors named beside each descriptor. Do not infer a Layout merely from a neighboring member, or infer a Symbol's ABI from a short name or the first same-name result.

## Support

OpenGlass is developed in my free time and distributed under the GPLv3 license.

As DWM does not officially support extensibility, OpenGlass relies on undocumented techniques. While designed for stability and performance, future Windows updates may cause breakage. Efforts will be made to maintain functionality, but continuous support cannot be guaranteed.

If you find OpenGlass valuable, please consider supporting the project via Ko-fi. By donating, you agree that:

- Your donation is voluntary without expectation of consideration.
- You donate as a natural person.

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/altalex531)
