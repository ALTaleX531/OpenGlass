# OpenGlass
A replica of the dead software glass8, also known as the upstream project of [DWMBlurGlass](https://github.com/Maplespe/DWMBlurGlass).  
This software is designed for Windows 10 2004 to Windows 11 23H2, which is currently in its early stages and may have various efficiency issues and bugs.   
> [!IMPORTANT]  
> This software is intended for advanced users only. If you are a beginner and you do not have deeper knowledge of Windows (such as registry editing etc.) you should not install this software.  
> For the average users, you should consider using [DWMBlurGlass](https://github.com/Maplespe/DWMBlurGlass).

> [!WARNING]   
> OpenGlass does NOT support and is NOT intended to support Windows Insider Preview, so if you want to use it in these Windows versions please do it at your own risk.


## Migrate from glass8 to OpenGlass
The following table demonstrates the similarities and differences between these two software.
> [!NOTE]
> 1. Most of the OpenGlass settings are stored only in the `HKCU\SOFTWARE\Microsoft\Windows\DWM`, all registry items stored in HKLM remain constant at runtime.
> 2. All external resources referenced by OpenGlass must ensure that it can be accessed by DWM, so paths like `C:\Users\*` are invalid.
> 3. Instead of reading the registry directly to get the color, OpenGlass uses the udwm internal API to get the calculated color.

| glass8 | Type | Description | OpenGlass | Description | Remarks
| ---- | ---- | ---- | ---- | ---- | ---- |
| EnableLogging | DWORD | 0x0 = disables logging to debug.log (default) / 0x1 = enables verbose logging / logging of donation key messages and fatal errors is always enabled |  |  | **Not implemented** |
| MaxDeviceFeatureLevel | DWORD | Describes the set of features targeted by a Direct3D device. |  |  | **Not supported** |
| DisableGlassOnBattery | DWORD | 0x1 = When your AC power is unplugged and computer is running on the battery then the blur effect will be disabled to decrease power consumption (default) / 0x0 = blur effect won't be disabled on battery |  | 0x01 = When your battery saver is activated then the effect will be disabled to decrease power consumption / 0x0 = blur effect won't be disabled when battery saver is activated | **The default value is 1 instead of 0** |
| DisabledHooks | DWORD | **Undocumented** |  | 0x01 = Disable the hooks of CaptionTextHandler.cpp to enable compatibility with third-party mods | **This registry item is stored in HKLM** |
| ForceD3DMode | DWORD | **Undocumented** |  |  | **Not supported** |
| GeometryCommand | DWORD | **Undocumented** |  |  | **Not supported** |
| GlassOpacity | DWORD | The amount of the opacity of the windows frames (0-100%). |  |  | **OK** |
| ColorizationBlurBalance/ColorizationBlurBalanceInactive | DWORD |  |  |  | **Not supported** |
| ColorizationColorCaption | DWORD | Color used for drawing window titles. Format is 0xBBGGRR. |  |  | **OK** |
| BlurDeviation | DWORD | Standard deviation for Gaussian blur, default = 30 (which means σ = 3.0). Value 0 results in non-blurred transparency. |  |  | **OK** |
| RoundRectRadius | DWORD | The radius of glass geometry, Win8 = 0, Win7 = 12 |  |  | **OK** |
| ColorizationGlassReflectionIntensity | DWORD | The intensity of reflection effect (0-100%). The default value is 0%. |  |  | **OK** |
|  | DWORD | **Not supported** | ColorizationGlassReflectionParallaxIntensity | The parallax intensity of the refection effect (0-100%). The default value is 10%. |  |
| CustomThemeReflection | String | path to PNG file which will be used as overlay image to simulate reflection (Aero stripes) effect |  | If the value does not exist, it will use the resource's bitmap. | **OK** |
| CustomThemeMaterial | String | **Undocumented** |  |  | **Only works for GlassType=Acrylic** |
| MaterialOpacity | DWORD | **Undocumented** |  |  | **Only works for GlassType=Acrylic** |
| CustomThemeAtlas | String | path to PNG file with theme resource (bitmap must have exactly the same layout as msstyle theme you are using!) | CustomThemeMsstyle | path to msstyle file | **CustomThemeAtlas is not supported, use CustomThemeMsstyle instead** |
|  | DWORD | **Not supported** | CustomThemeMsstyleUseDefaults | Color scheme uses the result from GetThemeDefaults |  |
| TextGlowMode | DWORD | Specifies how window caption glow effect will be rendered. 0x0=No glow effect. 0x1=Glow effect loaded from atlas (default). 0x2=Glow effect loaded from atlas and theme opacity is respected. 0x3=Composited glow effect using your theme settings. HIWORD of the value specifies glow size (0 = theme default). | TextGlowSize | The value specifies glow size. The default value is 15. | **TextGlowMode is deprecated, use TextGlowSize instead** |
| EnableBlurBlend | DWORD | Controls how the blurred background and frame colour are composited together (0x0 = default). |  |  | **Not supported** |
| ForceSystemMetrics | DWORD | **Undocumented** |  |  | **Not implemented** |
| GlassSafetyZoneMode | DWORD | **Undocumented** |  |  | **Not supported** |
| CenterCaption | DWORD | **Undocumented** |  |  | **OK** |
|  | DWORD | **Not supported** | GlassLuminosity | The luminosity of Acrylic/Mica effect |  |
|  | DWORD | **Not supported** | GlassType | The type of backdrop effect (0x0-0x4). 0x0=Blur. 0x01=Aero. 0x02=Acrylic. 0x03=Mica. 0x04=Solid. |  |
|  | DWORD | **Not supported** | GlassOverrideBorder | Specifies that the effect should extend to the border. The default value is 0. | **Disabling this option can significantly improve performance** |
|  | DWORD | **Not supported** | GlassCrossFadeTime | The cross fade time for backdrop switching. The default value is 87. |  |
|  | DWORD | **Not supported** | GlassOverrideAccent | Overriding accent with the effect of OpenGlass. The default value is 0. |  |
|  | DWORD | **Not supported** | GlassAdditionalPreScaleAmount | Additional prescaling factor for backdrop input image, the smaller the value the more significant the performance improvement, the lower the quality of the glass. The default value is 90% for Windows 10 but 95% for Windows 11. |  |
|  | DWORD | **Not supported** | EnableOcclusionCulling | Enable occlusion cullling optimization for Windows 10. The default value is 1. | **Outdated** |
|  | DWORD | **Not supported** | EnableEffectInputOptimization | In Windows 10, dwmcore creates a copy of the entire desktop for effect input by default. When this option is on, dwmcore only creates a copy of the desktop in the size of the blurred region. The default value is 1. | **Outdated** |

> [!TIP]  
> Check out the code to discover more details!


## How to reload the configuration
OpenGlass does not provide any GUI nor commands to explicitly reload the configuration, you may consider using glass8's heritage `AeroGlassGUI.exe` to refresh some of the settings.   
However, it's actually quite easy to do so in C/C++.
```c++
PostMessage(FindWindow(TEXT("Dwm"), nullptr), WM_THEMECHANGED, 0, 0);            // refresh part of the settings related to theme
PostMessage(FindWindow(TEXT("Dwm"), nullptr), WM_DWMCOLORIZATIONCHANGED, 0, 0);  // refresh part of the settings related to color/backdrop
```


## Dependencies and References
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