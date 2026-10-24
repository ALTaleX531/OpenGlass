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

OpenGlass supports only the listed x64 General Availability releases, with experimental support for Windows 11 26H1. Insider builds are not supported. See [Compatibility and DWM architectures](https://github.com/ALTaleX531/OpenGlass/wiki/Compatibility-and-DWM-architectures) for details.

## Quick start

1. Download `OpenGlassSetup.Signed.exe` from [Releases](https://github.com/ALTaleX531/OpenGlass/releases).
2. Run the installer, then open the configuration window from the Start menu shortcut **Configure OpenGlass** (or check **Launch OpenGlass GUI** on the installation finish page). It requests administrator rights.
3. Adjust the appearance. Settings are written immediately. **Save** accepts the changes, while **Revert** or closing the GUI undoes pending changes.

Starting in v3.1.0, the configuration window opens in the shared HKLM view, which sets ordinary defaults for the computer. Personal HKCU settings override ordinary settings for that account. See the [configuration guide](https://github.com/ALTaleX531/OpenGlass/wiki/Configuration-and-registry-reference) to choose or switch views, and see the [preset library guide](https://github.com/ALTaleX531/OpenGlass/wiki/Preset-packages) for saved looks and recovery behavior.

> [!TIP]
> **Emergency Exit:** Hold <kbd>Ctrl</kbd>+<kbd>Win</kbd>+<kbd>Shift</kbd>+<kbd>Q</kbd> to terminate DWM if the system becomes unresponsive.

OpenGlass is a specialized implementation focused on Aero Glass, using aggressive optimizations for that purpose and primarily targeting advanced users. Readers who simply want a translucent desktop appearance should consider [DWMBlurGlass](https://github.com/Maplespe/DWMBlurGlass).

## Windows theme integration for developers

The theme extension connects Windows theme application and the classic color panel to OpenGlass. It is intended for developers who build and customize it from source. The installer does not include or manage it. See [Windows theme integration](https://github.com/ALTaleX531/OpenGlass/wiki/Windows-theme-integration) for building, manual installation and behavior.

## Reporting issues

Report bugs in [GitHub Issues](https://github.com/ALTaleX531/OpenGlass/issues/new), with your exact Windows build, OpenGlass version and reproduction steps. For crashes or opaque glass, start with the configuration window's **Diagnostics** tab and the [troubleshooting guide](https://github.com/ALTaleX531/OpenGlass/wiki/Troubleshooting-and-crash-dumps).

## Building

For build prerequisites, dependencies, and CI caching details, see [Building OpenGlass](https://github.com/ALTaleX531/OpenGlass/wiki/Building-OpenGlass), [Python setup](tools/README.md#dependencies), and [Windows dependency reuse](.github/actions/prepare-build/README.md).

```powershell
python -m pip install -r requirements.txt
msbuild OpenGlass.slnx /m /restore /p:RestorePackagesConfig=true /p:Configuration=Release /p:Platform=x64
```

See the [Changelog](https://github.com/ALTaleX531/OpenGlass/wiki/Changelog) for technical version changes. CI artifacts are unsigned validation builds, not releases, and the developer theme extension DLL and PDB are uploaded separately.

## Credits

- GUI icon: The glass-window artwork is from [AeroShotCRE](https://github.com/Cvolton/AeroShotCRE) and is credited to RUvlad1, and the registry artwork is from Microsoft's Windows 11 Registry Editor. See the [assets guide](OpenGlassGUI/Assets/README.md) for source and license details.
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
