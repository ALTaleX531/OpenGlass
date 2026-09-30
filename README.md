![header](assets/banner.png)

# Experience the native Aero Glass interface on Windows 10+

OpenGlass restores the full glass effect to window frames, with control over blur, reflections, colorization, caption rendering, and theme integration.

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/ALTaleX531/OpenGlass)
[![CI](https://github.com/ALTaleX531/OpenGlass/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/ALTaleX531/OpenGlass/actions/workflows/build.yml)

## Supported Windows versions

| Windows version (OS build) | Status |
| --- | --- |
| Windows 10 1809–22H2 (17763–19045) | Stable |
| Windows 11 21H2–25H2 (22000–26200) | Stable |
| Windows 11 26H2 (26300) | Stable |
| Windows 11 26H1 (28000) | Experimental |
| Windows Server 2022 (20348) | Supported |

OpenGlass 3.0.2.3749 adds support for Windows 11 26H2 and fixes the inactive window border regression that followed KB5124010 ([#367](https://github.com/ALTaleX531/OpenGlass/issues/367)).

Windows 11 26H1 uses the MILComp implementation, which is still experimental because some features are not yet implemented.

Only the General Availability releases listed above are supported. Insider builds, other prerelease versions, and Windows Server versions other than 2022 are unsupported and may crash DWM. Compatibility depends on the exact build, revision, and compositor capabilities.

See [Compatibility and DWM architectures](https://github.com/ALTaleX531/OpenGlass/wiki/Compatibility-and-DWM-architectures) for the support policy and how OpenGlass selects its DWM implementation.

## Quick start

1. Download `OpenGlassSetup.exe` from [Releases](https://github.com/ALTaleX531/OpenGlass/releases).
2. Run the installer and open the OpenGlass GUI. It asks for administrator rights because it edits both per-user Windows colorization and system-wide OpenGlass settings. The [configuration reference](https://github.com/ALTaleX531/OpenGlass/wiki/Configuration-and-registry-reference) lists the registry values involved.
3. Adjust the appearance. Changes apply immediately. **Save** keeps the current state, and **Revert** restores the state from before you started editing.

The installer detects your Windows build and installs the matching DWM implementation automatically.

The **Glass colors** page includes Windows Vista and Windows 7 presets. The **Preset packs** page can import, create, apply, and remove immutable [preset ZIPs](https://github.com/ALTaleX531/OpenGlass/wiki/Preset-packages). The official GUI and preset packages manage one system-wide configuration for effects and themes; only the five Windows colorization values and their Override forms are written per user. OpenGlass itself still reads manual and transformation-pack settings from either HKCU or HKLM.

> [!TIP]
> **Emergency Exit:** Hold <kbd>Ctrl</kbd>+<kbd>Win</kbd>+<kbd>Shift</kbd>+<kbd>Q</kbd> to terminate DWM if the system becomes unresponsive.

OpenGlass is aimed at advanced users who are comfortable troubleshooting DWM. For a simpler alternative, consider [DWMBlurGlass](https://github.com/Maplespe/DWMBlurGlass).

## Reporting issues

Report DWM crashes and other bugs in [GitHub Issues](https://github.com/ALTaleX531/OpenGlass/issues/new); reports posted in third-party communities are not tracked. Include the exact Windows build and revision, the OpenGlass version, relevant settings, reproduction steps, and screenshots or recordings.

If glass is unexpectedly opaque, check the GUI's **Diagnostics** tab. For a crash, enable full DWM dumps there and reproduce the problem once. A hang requires a dump captured manually. See [Troubleshooting and crash dumps](https://github.com/ALTaleX531/OpenGlass/wiki/Troubleshooting-and-crash-dumps) for how to collect dumps and what to include in a report.

## Building

```powershell
msbuild OpenGlass.slnx /m /restore /p:Configuration=Release /p:Platform=x64
```

GitHub Actions also builds and tests `main`. Its downloadable `v<version>-unsigned` artifact is an unsigned validation build, not a release or Git tag. See [Building OpenGlass](https://github.com/ALTaleX531/OpenGlass/wiki/Building-OpenGlass) for prerequisites, output paths, packaging, tests, CI behavior, and signing requirements.

## Credits

- [Banner for OpenGlass](https://github.com/ALTaleX531/OpenGlass/discussions/11) by [@aubymori](https://github.com/aubymori), using [metalheart jawn #2](https://www.deviantart.com/kfh83/art/metalheart-jawn-2-1068250045) by [@kfh83](https://github.com/kfh83)
- [[MS-RDPCR2]: Remote Desktop Protocol: Composited Remoting V2](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpcr2)
- [KNSoft.SlimDetours](https://github.com/KNSoft/KNSoft.SlimDetours)
- [VC-LTL](https://github.com/Chuyu-Team/VC-LTL5)
- [Windows Implementation Libraries](https://github.com/Microsoft/wil)
- [libvalinet](https://github.com/valinet/libvalinet), whose symbol download work inspired OpenGlass
- [TranslucentTB](https://github.com/TranslucentTB/TranslucentTB), whose C++ project structure inspired OpenGlass

## Support

OpenGlass is developed in spare time and released under the GPLv3. DWM offers no official extension mechanism, so future Windows updates may break OpenGlass, and ongoing support cannot be guaranteed.

If you find OpenGlass valuable, please consider supporting it on Ko-fi. Donations are voluntary, come with no expectation of anything in return, and must be made as a natural person.

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/altalex531)
