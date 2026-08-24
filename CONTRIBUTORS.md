# Contributors

OpenGlass is maintained by [@ALTaleX531](https://github.com/ALTaleX531), with help from developers, researchers, testers, and artists across the Windows customization community.

This file highlights the people whose work is still represented in the project. Git history and source comments remain the detailed record.

## Maintainer

- [@ALTaleX531](https://github.com/ALTaleX531) created AcrylicEverywhere and OpenGlass, and maintains the current Legacy and MILComp implementations. His work includes DWM research, rendering, projections and symbols, hook lifecycle, the service and host, GUI, installer, tests, documentation, and releases.

## Code and features

- [@kfh83](https://github.com/kfh83) helped develop the Windows 7 Aero colorization model, ported it to the Legacy D2D renderer, and improved its settings and documentation. See [PR #14](https://github.com/ALTaleX531/OpenGlass/pull/14) and [PR #37](https://github.com/ALTaleX531/OpenGlass/pull/37).
- [@wiktorwiktor12](https://github.com/wiktorwiktor12) provided major research, implementation help, and testing for Legacy colorization and early button glow work.
- [@tetawaves](https://github.com/tetawaves) contributed Aero Peek and Live Preview fixes, Windows 10 button cloning, Vista and Windows 7 and Windows 8 caption button metrics, and a frame thickness correction. See [PR #121](https://github.com/ALTaleX531/OpenGlass/pull/121), [PR #135](https://github.com/ALTaleX531/OpenGlass/pull/135), and [PR #142](https://github.com/ALTaleX531/OpenGlass/pull/142).
- [@c0redump3d](https://github.com/c0redump3d) created the first Windows 11 caption button glow implementation and its compatibility fixes. The code was later reworked for the current projection and lifecycle system. [@QCQ171-C](https://github.com/QCQ171-C) provided multi-build testing and visual reproductions used to resolve its compatibility and timing problems. See [PR #287](https://github.com/ALTaleX531/OpenGlass/pull/287).
- [@ImSwordQueen](https://github.com/ImSwordQueen) implemented Windows 8 style caption centering. [@nt5point1](https://github.com/nt5point1) also helped develop and validate the idea. See [PR #312](https://github.com/ALTaleX531/OpenGlass/pull/312).
- [@mushui1](https://github.com/mushui1), also known as MuShui, co-developed Windows Server 2022 support and related DWM fixes. See the [support commit](https://github.com/ALTaleX531/OpenGlass/commit/a133752f1368a4d8aa594c91cf201a5596792d8b).
- [@MagicAndre1981](https://github.com/MagicAndre1981) added SourceLink support for PDB debugging. See [PR #157](https://github.com/ALTaleX531/OpenGlass/pull/157).
- [@taiman724](https://github.com/taiman724) contributed a documentation fix in [PR #311](https://github.com/ALTaleX531/OpenGlass/pull/311).
- [@Ingan121](https://github.com/Ingan121) proposed and prototyped multi-session RDP injection in [PR #127](https://github.com/ALTaleX531/OpenGlass/pull/127). The final implementation was rewritten rather than merged directly.

## Research and collaboration

The Aero colorization work grew across AcrylicEverywhere, DWMBlurGlass, and OpenGlass. ALTaleX created the first working recipe. kfh83 ported and refined it. [@TorutheRedFox](https://github.com/TorutheRedFox), [@wackyideas](https://github.com/wackyideas), and [@aubymori](https://github.com/aubymori) helped develop and validate the more accurate additive model. [@Maplespe](https://github.com/Maplespe) supported the work through DWMBlurGlass, and [@ephemeralViolette](https://github.com/ephemeralViolette) provided an early test fork.

Useful history can be found in [DWMBlurGlass PR #222](https://github.com/Maplespe/DWMBlurGlass/pull/222), [DWMBlurGlass PR #269](https://github.com/Maplespe/DWMBlurGlass/pull/269), [OpenGlass PR #14](https://github.com/ALTaleX531/OpenGlass/pull/14), and [Discussion #57](https://github.com/ALTaleX531/OpenGlass/discussions/57).

OpenGlass also builds on ideas and research from glass8 by BigMuscle, [Aero Window Manager](https://github.com/Dulappy/aero-window-manager), and [ADeltaX's Interop Compositor research](https://blog.adeltax.com/interopcompositor-and-coredispatcher/).

## Artwork and community help

- [@aubymori](https://github.com/aubymori) created the official banner in [Discussion #11](https://github.com/ALTaleX531/OpenGlass/discussions/11).
- [@kfh83](https://github.com/kfh83) created the wallpaper used in the banner.

Thanks also to everyone who has shared crash dumps, compatibility results, visual comparisons, translations, testing, and clear bug reports.
