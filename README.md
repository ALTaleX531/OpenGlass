![header](assets/banner.png)

# Experience the native Aero Glass interface on Windows 10+

OpenGlass restores the full glass effect to window frames, with control over blur, reflections, colorization, caption rendering, and theme integration.

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/ALTaleX531/OpenGlass)
[![CI](https://github.com/ALTaleX531/OpenGlass/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/ALTaleX531/OpenGlass/actions/workflows/build.yml)

## Supported Windows versions

| Windows build | Status |
| --- | --- |
| Windows 10 build 17763 through 19045 | Stable |
| Windows 11 builds below 28000, including build 26200 | Stable |
| Windows 11 build 28000 and later | Experimental |
| Windows Server 2022 | Supported |

> [!IMPORTANT]
> `OpenGlassSetup.exe` automatically installs the correct DWM implementation for the detected Windows build. There is no architecture choice to make.

Only General Availability Windows builds are supported. Insider and preview builds, and Windows Server versions other than 2022, are unsupported and may crash DWM. Compatibility depends on the exact build, revision, and verified compositor capabilities.

See [Compatibility and DWM architectures](https://github.com/ALTaleX531/OpenGlass/wiki/Compatibility-and-DWM-architectures) for the complete support policy and explanation of parallel Windows build trains.

## Quick start

1. Download `OpenGlassSetup.exe` from [Releases](https://github.com/ALTaleX531/OpenGlass/releases).
2. Install OpenGlass and open its GUI. The GUI requests administrator elevation because it manages both per-user Windows colorization and system-wide OpenGlass settings. The [configuration reference](https://github.com/ALTaleX531/OpenGlass/wiki/Configuration-and-registry-reference) documents the corresponding registry values.
3. Adjust the appearance. Changes apply immediately; **Save** accepts the current state and **Revert** restores the state captured before editing.

The **Glass colors** page includes Windows Vista and Windows 7 presets. The **Preset packs** page can import, create, apply, and remove immutable [preset ZIPs](https://github.com/ALTaleX531/OpenGlass/wiki/Preset-packages). OpenGlass intentionally uses one system-wide configuration for effects and themes; only the five Windows colorization values and their Override forms remain per-user.

> [!TIP]
> **Emergency Exit:** Long-press <kbd>Ctrl</kbd>+<kbd>Win</kbd>+<kbd>Shift</kbd>+<kbd>Q</kbd> to terminate DWM if the system becomes unresponsive.

OpenGlass is intended for advanced users who are comfortable troubleshooting DWM. For a simpler alternative, consider [DWMBlurGlass](https://github.com/Maplespe/DWMBlurGlass).

## Reporting issues

For unexpectedly opaque glass, first check the GUI's **Diagnostics** tab, which reports the Windows transparency setting, opaque-blend setting, effective power mode, and battery-saver policy. For a DWM crash or hang, enable full DWM dumps there and reproduce the problem once. Include the exact Windows build and revision, OpenGlass version, registry settings, reproduction steps, screenshots or recordings, and a dump when a crash occurred. See [Troubleshooting and crash dumps](https://github.com/ALTaleX531/OpenGlass/wiki/Troubleshooting-and-crash-dumps).

## Building

```powershell
msbuild OpenGlass.slnx /m /restore /p:Configuration=Release /p:Platform=x64
```

The `main` branch is also built and tested by GitHub Actions. Its downloadable `v<version>-unsigned` artifact is an unsigned validation build, not a release or Git tag. See [Building OpenGlass](https://github.com/ALTaleX531/OpenGlass/wiki/Building-OpenGlass) for prerequisites, output paths, packaging, tests, CI behavior, and signing requirements.

## Credits

- [Banner for OpenGlass](https://github.com/ALTaleX531/OpenGlass/discussions/11) by [@aubymori](https://github.com/aubymori), using [metalheart jawn #2](https://www.deviantart.com/kfh83/art/metalheart-jawn-2-1068250045) by [@kfh83](https://github.com/kfh83)
- [[MS-RDPCR2]: Remote Desktop Protocol: Composited Remoting V2](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpcr2)
- [KNSoft.SlimDetours](https://github.com/KNSoft/KNSoft.SlimDetours)
- [VC-LTL](https://github.com/Chuyu-Team/VC-LTL5)
- [Windows Implementation Libraries](https://github.com/Microsoft/wil)
- [libvalinet](https://github.com/valinet/libvalinet), whose symbol download work inspired OpenGlass
- [TranslucentTB](https://github.com/TranslucentTB/TranslucentTB), whose C++ project structure inspired OpenGlass

## Support

OpenGlass is developed in free time and distributed under the GPLv3 license. DWM does not officially support extensibility, so future Windows updates may cause breakage and continuous support cannot be guaranteed.

If you find OpenGlass valuable, please consider supporting the project via Ko-fi. Donations are voluntary, carry no expectation of consideration, and must be made as a natural person.

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/altalex531)
