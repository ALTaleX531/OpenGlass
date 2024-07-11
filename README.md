![header](https://private-user-images.githubusercontent.com/44238627/346738551-f507e3e8-a4bd-4edd-8386-e221650c6ca9.png?jwt=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3MjA2NzUyMjQsIm5iZiI6MTcyMDY3NDkyNCwicGF0aCI6Ii80NDIzODYyNy8zNDY3Mzg1NTEtZjUwN2UzZTgtYTRiZC00ZWRkLTgzODYtZTIyMTY1MGM2Y2E5LnBuZz9YLUFtei1BbGdvcml0aG09QVdTNC1ITUFDLVNIQTI1NiZYLUFtei1DcmVkZW50aWFsPUFLSUFWQ09EWUxTQTUzUFFLNFpBJTJGMjAyNDA3MTElMkZ1cy1lYXN0LTElMkZzMyUyRmF3czRfcmVxdWVzdCZYLUFtei1EYXRlPTIwMjQwNzExVDA1MTUyNFomWC1BbXotRXhwaXJlcz0zMDAmWC1BbXotU2lnbmF0dXJlPTNjM2RlYTM4NmFjZmU3ZjYzNGYwN2RlYmEzMmIxMmU2MDAyZGVlMTNiZDczYTNmZmU1MmNjODM2MDZmN2RiZGImWC1BbXotU2lnbmVkSGVhZGVycz1ob3N0JmFjdG9yX2lkPTAma2V5X2lkPTAmcmVwb19pZD0wIn0.tp-HZisbys41FBErjnLndfemjmxGWzmBL0XcFvJzNXY)
# OpenGlass
A replica of the dead software glass8, also known as the upstream project of [DWMBlurGlass](https://github.com/Maplespe/DWMBlurGlass).  

This branch does not rely on `dcomp` and `Windows.UI.Composition`, so it is very limited in functionality and it is not unusual to encounter all kinds of bugs, but its performance is much better than the master branch. Currently this branch ONLY supports Windows 10 22H2.
> [!IMPORTANT]  
> This software is intended for advanced users only. If you are a beginner and you do not have deeper knowledge of Windows (such as registry editing etc.) you should not install this software.  
> For the average users, you should consider using [DWMBlurGlass](https://github.com/Maplespe/DWMBlurGlass).

> [!WARNING]   
> OpenGlass does NOT support and is NOT intended to support Windows Insider Preview, so if you want to use it in these Windows versions please do it at your own risk.
## How to use this software
1. Extract the files from the Release page to `C:\`, but please don't put them in `C:\Users\*`, otherwise OpenGlass won't work properly.
2. Run `install.bat` as administrator, this will create a scheduled task for you to run the OpenGlass helper process on boot, which will monitor and automatically inject its component into Dwm.
3. Run `startup.bat` as administrator, this will run the helper process manually.
4. When you use it for the first time or just after updating your system, OpenGlass will try to download the symbol files and you will see its download progress bar in the taskbar, but please don't close it and be patient for about 15s. When the symbol files are ready, enjoy!
5. When you want to stop using OpenGlass or update the version of OpenGlass, running `shutdown.bat` will remove the effects of OpenGlass for you and exit the helper process. At this time, you can either replace the OpenGlass files or continue to run `uninstall.bat` and manually delete the remaining files to complete the uninstallation.

## Documentation
The legacy branch can use most of the features of the master branch. The following table lists the difference with master branch. The legacy branch uses the colors stored by `CTopLevelWindow`, so you can change the color settings using AWM without problems. 

- `Not implemented` means that this feature has not yet been implemented but is possible in the future.
- `Not supported` means that this feature is impossible to be implemented in current structure.

| master branch | Type | Description | legacy branch | Description | Remarks
| ---- | ---- | ---- | ---- | ---- | ---- |
| RoundRectRadius | DWORD | The radius of glass geometry, Win8 = 0, Win7 = 12 |  | Rounded corners are not anti-aliased. | **OK** |
| CustomThemeMaterial | String | **Undocumented** |  |  | **Not implemented** |
| MaterialOpacity | DWORD | **Undocumented** |  |  | **Not implemented** |
| GlassLuminosity | DWORD | The luminosity of Acrylic/Mica effect |  |  | **Not implemented** |
| GlassType | DWORD | The type of backdrop effect (0x0-0x4). 0x0=Blur. 0x01=Aero. 0x02=Acrylic. 0x03=Mica. 0x04=Solid. |  | Only 0x0 is implemented. | **OK** |
| GlassOverrideBorder | DWORD | Specifies that the effect should extend to the border. The default value is 0. |  | The glass will override the border by default. | **Not implemented** |
| GlassCrossFadeTime | DWORD | The cross fade time for backdrop switching. The default value is 87. |  |  | **Not supported** |
| GlassOverrideAccent | DWORD | Overriding accent with the effect of OpenGlass. The default value is 0. |  | Some windows are overwritten resulting in full transparency. And the behavior of the overrides makes a difference. | **OK** |
| GlassAdditionalPreScaleAmount | DWORD | Additional prescaling factor for backdrop input image, the smaller the value the more significant the performance improvement, the lower the quality of the glass. The default value is 90% for Windows 10 but 95% for Windows 11. |  |  | **Not implemented** |
| ForceAccentColorization | DWORD | When this option is on, OpenGlass will always uses the colors from `AccentColor` and `AccentColorInactive`, which will ignore all the system settings related to color. You can turn it on when there exists third-party softwares that break the auto-coloring. |  |  | **Not implemented** |
| GlassCrossFadeEasingFunction | DWORD | The easing function for cross fade animation. 0x0=Linear. 0x1=CubicBezier. The Default value is 0. |  |  | **Not supported** |

> [!TIP]  
> Check out the code to discover more details!


## How to reload the configuration
OpenGlass does not provide any GUI nor commands to explicitly reload the configuration, you may consider using glass8's heritage `AeroGlassGUI.exe` to refresh some of the settings.   
However, it's actually quite easy to do so in C/C++.
```c++
PostMessage(FindWindow(TEXT("Dwm"), nullptr), WM_THEMECHANGED, 0, 0);            // refresh part of the settings related to theme
PostMessage(FindWindow(TEXT("Dwm"), nullptr), WM_DWMCOLORIZATIONCHANGED, 0, 0);  // refresh part of the settings related to color/backdrop
```

## About compatibility
- OpenGlass cannot be used with DWMBlurGlass as they belong to the same type of software.
- OpenGlass does not support rendering borders that are included in accents, so some softwares that turn on borders for accents such as TranslucentFlyouts will not be able to display borders.
- OpenGlass can be used with [AWM](https://github.com/Dulappy/aero-window-manager), but `CustomThemeMsstyle` will not be available and you need to be careful about the order in which you execute and stop.
    - **Good Example😘**:  

        execute OpenGlass  
        execute AWM  
        ...  
        stop AWM  
        stop OpenGlass  

        (The first to execute should be stopped at last.)
    - **Bad Example😭**:  
        execute OpenGlass  
        execute AWM  
        ...  
        stop OpenGlass  
        stop AWM  

        In this case you are likely to get a crash of DWM or the other software working abnormally.
- OpenGlass can be used with most windhawk mods, but some may have compatibility issues, especially community or personally written mods that can't be found in the windhawk marketplace.
- It is not necessary to use the DComp Unlocker mod with OpenGlass, as OpenGlass already has this feature built-in.

## Dependencies and References
### [Banner for OpenGlass](https://github.com/ALTaleX531/OpenGlass/discussions/11)
Provided by [@aubymori](https://github.com/aubymori)  
Wallpaper: [metalheart jawn](https://www.deviantart.com/kfh83/art/metalheart-jawn-2-1068250045) #2 by [@kfh83](https://github.com/kfh83)
### [Microsoft Research Detours Package](https://github.com/microsoft/Detours)  
Detours is a software package for monitoring and instrumenting API calls on Windows.  
### [VC-LTL - An elegant way to compile lighter binaries.](https://github.com/Chuyu-Team/VC-LTL5)  
VC-LTL is an open source CRT library based on the MS VCRT that reduce program binary size and say goodbye to Microsoft runtime DLLs, such as msvcr120.dll, api-ms-win-crt-time-l1-1-0.dll and other dependencies.  
### [Windows Implementation Libraries (WIL)](https://github.com/Microsoft/wil)  
The Windows Implementation Libraries (WIL) is a header-only C++ library created to make life easier for developers on Windows through readable type-safe C++ interfaces for common Windows coding patterns.  
### [Interop Compositor](https://blog.adeltax.com/interopcompositor-and-coredispatcher/)
Saved me some decompiling and reverse engineering time thanks to ADeltaX's blog!
### [Win32Acrylic](https://github.com/ALTaleX531/Win32Acrylic)
Win2D sucks!
### [AcrylicEverywhere](https://github.com/ALTaleX531/AcrylicEverywhere)
The predecessor of this project.
### [Loading Visual Styles Per-Application](https://winclassic.net/thread/2178/loading-visual-styles-application)
It is possible for an application to load a style on a per-application basis using an undocumented API.