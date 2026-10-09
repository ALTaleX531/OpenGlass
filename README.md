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

Only the General Availability releases listed above are supported. Insider builds and other unlisted versions are unsupported. Windows 11 26H1 remains experimental. See [Compatibility and DWM architectures](https://github.com/ALTaleX531/OpenGlass/wiki/Compatibility-and-DWM-architectures) for details.

## Quick start

1. Download `OpenGlassSetup.exe` from [Releases](https://github.com/ALTaleX531/OpenGlass/releases).
2. Run the installer, which selects the matching DWM implementation, then open the GUI. It requests administrator rights.
3. Adjust the appearance. Settings are written immediately; **Save** accepts the changes, while **Revert** or closing the GUI undoes pending changes.

To display changes made outside the GUI, hold <kbd>Shift</kbd> while opening **More**, then choose **Sync**. Save or Revert any pending changes first. Sync reloads settings, the preset library and diagnostics; it never saves or reverts implicitly.

See the [configuration guide](https://github.com/ALTaleX531/OpenGlass/wiki/Configuration-and-registry-reference) and [preset library guide](https://github.com/ALTaleX531/OpenGlass/wiki/Preset-packages) for editing scopes, color options, presets and recovery behavior.

> [!TIP]
> **Emergency Exit:** Hold <kbd>Ctrl</kbd>+<kbd>Win</kbd>+<kbd>Shift</kbd>+<kbd>Q</kbd> to terminate DWM if the system becomes unresponsive.

OpenGlass is aimed at advanced users who are comfortable troubleshooting DWM. For a simpler alternative, consider [DWMBlurGlass](https://github.com/Maplespe/DWMBlurGlass).

## Reporting issues

Report bugs in [GitHub Issues](https://github.com/ALTaleX531/OpenGlass/issues/new), with your exact Windows build, OpenGlass version and reproduction steps. For crashes or opaque glass, start with **Diagnostics** and the [troubleshooting guide](https://github.com/ALTaleX531/OpenGlass/wiki/Troubleshooting-and-crash-dumps).

## Building

```powershell
msbuild OpenGlass.slnx /m /restore /p:Configuration=Release /p:Platform=x64
```

See [Building OpenGlass](https://github.com/ALTaleX531/OpenGlass/wiki/Building-OpenGlass) for prerequisites, tests and packaging, and the [Changelog](https://github.com/ALTaleX531/OpenGlass/wiki/Changelog) for technical version changes. CI artifacts are unsigned validation builds, not releases.

## Credits

- GUI icon: adapted from Microsoft's [Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons), Copyright (c) 2020 Microsoft Corporation, under the [MIT License](OpenGlassGUI/Assets/FluentIcons.LICENSE.txt). [Sources and modifications](OpenGlassGUI/Assets/README.md).
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
