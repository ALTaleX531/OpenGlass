# Theme extension design

`OpenGlassThemeExtension.dll` owns registration, COM forwarding and one hook lifecycle for the system `themecpl.dll` and `themeui.dll`. `Core.cpp` uses the `OpenGlass` namespace and contains all four COM exports and their CLSIDs. `Core.hpp` declares the shared `OpenGlass::HookType`. Color hooks, DUI hooks and color calculations use `OpenGlass::ColorHooks`, `OpenGlass::DuiHooks` and `OpenGlass::Colorization`, respectively. Hook initialization stays outside `DllMain`. There is no factory cache.

## Registration

Registration creates machine-wide, 64-bit `TreatAs` redirects for ThemeManager2, ThemeManagerShared and the themecpl theme element provider. The extension forwards to the original classes' HKLM `InprocServer32` entries, including SecureUxTheme's proxy when registered. Those original paths remain unchanged. It temporarily impersonates TrustedInstaller to write the protected system registration keys without changing their ACLs.

The extension handles all three routes in one registry transaction without initializing color hooks. Each registration or unregistration operation commits all three routes together, including key creation and deletion. A failure or process termination before commit leaves the previous registration intact. A foreign `TreatAs` is rejected. Unregistration deletes the extension's proxy CLSID subtrees and removes only redirects that still point to those proxies, preserving unrelated data under the original system CLSIDs and their `InprocServer32` entries.

## COM forwarding and class activation

`DllGetClassObject` maps the three proxy CLSIDs to their original classes, reads the original class's HKLM `InprocServer32` registration, loads that server and pins it for the process lifetime. It calls the server's `DllGetClassObject` with the original CLSID, then initializes both hook modules in a transaction before returning the server's result to COM. Failure to load DUI70, resolve its required exports or locate the required imports leaves this activation's hooks uninstalled and preserves the successful native factory result. This preserves forwarding through SecureUxTheme's server when registered. The theme element route passes `HookType::CPL` for `themecpl.dll`, while the two theme manager routes pass `HookType::UI` for `themeui.dll`.

The pin in `DllGetClassObject` retains the loaded downstream COM server after
the local `wil::unique_hmodule` releases its `LoadLibraryExW` reference on return.
The returned factory and its objects can outlive this function, and their COM
reference counts do not replace the loader reference for a manually loaded DLL.
The same function separately pins `OpenGlassThemeExtension.dll` so the installed
IAT replacements remain valid. The extension's
`DllCanUnloadNow` returning `S_FALSE` does not retain the downstream server.
Both pins use [`GET_MODULE_HANDLE_EX_FLAG_PIN`](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandleexw),
which keeps the selected module loaded until process termination.

## Hook preparation and error handling

`DllGetClassObject` directly constructs the `HookHelper::HookTransaction`, calls
`DuiHooks::Prepare(type)` and `ColorHooks::Prepare(type)`, then queues the returned
hooks and commits. For hook setup, each module exposes only `Prepare`, which
resolves its required exports and imports and returns a pointer to its retained `ImportHook`. Already
installed hooks return `nullptr`, and DUI also returns `nullptr` for `HookType::UI`.
Preparation errors throw. Neither import-preparation function creates nor commits
a transaction, and Core queues nothing until both calls have returned successfully.
The DUI provider's per-page vtable installation and removal use their own
transactions during the page lifetime, as described below.

Preparation uses `GetModuleHandleW` to find the corresponding loaded system module. A third-party server must have loaded that module by the time its `DllGetClassObject` returns. Each module's ordinal imports for `dwmapi!127`, `dwmapi!131` and `uxtheme!122` are prepared together using `HookHelper::ImportHook`. The inspected binaries use normal imports in `themecpl.dll` and delay imports in `themeui.dll`, and preparation selects the corresponding directory. CPL also prepares the normal `api-ms-win-core-com-l1-1-0!CoCreateInstance` import and the `dui70!XResourceProvider::Create` delay import. All are published by the outer transaction. `ImportHook::Prepare` returns a failed HRESULT for a missing module, import directory/table or requested entry, ambiguous matches and unsupported VA-based delay imports. The extension throws on these failures, while the DWM callers retain their fail-fast handling. Invalid hook inventories and lifecycle conflicts remain programming errors and fail fast in the shared helper.

DUI module loading and export resolution run inside `DuiHooks::Prepare`. Both preparation calls must succeed before any hooks are queued, so a missing color import cannot leave a pending DUI hook or change the installation state. The transaction is still in its preparation phase, and SlimDetours starts only at `Commit`. `DirectUI::InitializeProjection` throws on DLL, export or pinning failure, and it does not call FailFast. The hook-initialization block in `DllGetClassObject` catches and logs preparation exceptions, skipping installation while returning the already obtained native factory and its successful HRESULT. The original-server lookup, loading and activation retain their error-return behavior. Both loader pins precede the native factory call, so a pinning failure cannot strand an already returned interface. An empty transaction can be destroyed without `Commit`, as its destructor releases the transaction lock and clears the current transaction. Leaving queued hooks without committing remains a fail-fast condition.

Both modules use the same three ordinary replacement functions, with no template
instantiations or `HookRundown` thunks. Each has its own `ModuleHooks` entry: the
three `_Org` fields initially hold the replacement addresses, and installation
replaces them with the previous IAT values. The replacements never call these
saved pointers. Each `ModuleHooks` entry owns its `ImportHook`, while `DuiHooks` retains
its import hook separately. These objects outlive the preparation calls because
`HookTransaction::Apply` queues pointers to them for the later outer `Commit`.
CPL's additional `CoCreateInstance` replacement uses the typed
`g_CoCreateInstance_Org` pointer declared in `ColorHooks.Shell.hpp`. It initially
holds the replacement address and receives the previous normal-IAT value during
installation. The replacement forwards through that saved pointer.

## Module and hook lifetime

Each color `ModuleHooks` entry keeps its actual module name in `m_moduleName`.
Both modules use `ImportHook::IsInstalled()` to skip installed hooks, and there are no separate initialization flags. Preparation and queuing leave that state false, and the transaction changes it when applying the hook during `Commit`. The outer
transaction's lock covers preparation, state reads and installation, so the
existing ordinary `bool` in `ImportHook` suffices. Core prepares each component
once per transaction. Repeated COM activation returns null pointers for installed
hooks and can commit an empty transaction.

Export or import resolution failure leaves the installation state unchanged and
queues nothing, allowing a later activation to retry. Hooks already installed
for another route remain installed. Actual hook transaction and ownership
failures still fail fast. This capability check does
not validate the private page layout below.

The extension is pinned for the process lifetime and `DllCanUnloadNow` returns
`S_FALSE`. Its replacements do not use rundown. The module import hooks and
their callbacks remain installed for the process lifetime. Each color page's
copied handler vtable is removed before the page or its provider is destroyed.
All three proxy classes are registered to the extension's own module path. Registration and unregistration do not initialize or remove hooks.

## Delay-import originals

`ImportHook` saves each IAT slot's current value without resolving delay imports
first, so a saved original can still be an unresolved thunk. In the inspected
`themeui.dll` and `ntdll.dll` **10.0.26100.9549**, the thunk calls
`__delayLoadHelper2`, which delegates to `ResolveDelayLoadedAPI`. The resolver
returns the current slot value immediately when it points outside the importing
module's image. An installed replacement in `OpenGlassThemeExtension.dll` meets that
condition. Calling the saved unresolved thunk would therefore jump back to the
hook and recurse, rather than republishing the native export over the hook.
The protected resolver's batch processing also skips other slots already pointing
outside that image and preserves them during writeback.

This is a limitation of the shared hook helper's saved-original contract. A
pointer saved after resolution can call the real export, but the helper does not
guarantee that state. The extension avoids this dependency because none of its
three replacements calls a saved original or the native color setter directly.
The additional DUI replacement forwards other resource IDs through the real
`XResourceProvider::Create` export resolved from `dui70.dll`, never its saved
delay-IAT value. DUI70 is also pinned while its function pointers are retained.
The separate `CoCreateInstance` replacement forwards through a saved normal-IAT
entry, so it does not call an unresolved delay thunk.

## Classic color page

### Provider and resource projection

`DuiHooks.cpp` intercepts only resource **1002** in themecpl's
`XResourceProvider::Create` import. It supplies a small `IXProviderCP` implementation
with the three native callbacks: `CreateDUICP`, `CreateParserCP`, `DestroyCP`.
This interface is not `IUnknown`. Its ABI and the six `IElementListener` slots
were checked against the x64 10.0.26100.9549 binaries.

`ColorPage.xml` retains the modern UIFILE's page classes, control IDs and
intensity range **10..85**, adding the transparency checkbox and selected-swatch
name. `OpenGlassThemeExtension.rc` embeds it and the added UI strings in the
extension. The provider uses DUI70's exported `DUIXmlParser` methods with the extension as the XML and
default resource module, retaining themecpl as the registered-class module.
Native strings and unthemed expando images explicitly select `library(themecpl.dll)`
in their `resstr`/`resbmp` expressions. The original COM
factory still registers `GlassColorCplPage`, `SwatchSelector`, `HSBSlider` and
the other native classes before the provider creates the tree.

This hook replaces the main page only. Native code's separate parsers for
`ColorSwatch` and high-contrast row templates continue to read the system UIFILE, so editing those templates in the extension's XML does not replace that path.
Conversely, hooking only themecpl's `SetXMLFromResource` import does not replace
the main page load inside DUI70. A resource-redirection alternative must reach
that load and separately obtain the created controls to attach their listeners, because replacing the XML alone does not restore the removed native checkbox handler.
There is no runtime symbol lookup or instruction scan. The required decorated
DUI70 exports, callback vtables and the native navigation layout described below
are private ABI dependencies.

`DuiProjection.hpp` declares the used native methods on `Element`, `Selector`,
`Value`, `DUIXmlParser` and `XResourceProvider`. These are opaque object facades,
not declarations of their field layouts. Each inline method uses the same
`OPENGLASS_MUSTTAIL return Projection::Invoke` dispatch as the DWM projections, and the common invocation and ABI helpers live in `Common/ProjectionInvoke.hpp`.
`DuiProjection.cpp` resolves the exact export names into typed slots and pins
DUI70 before hook installation. Calls do not resolve exports or check module
state. `DuiHooks.cpp` contains the provider and hook behavior, using ordinary
method calls rather than a separate `g_dui` function-pointer table.

### Localization and accessibility

The provider attaches itself as a listener to `EnableGlassCheckBox`, `SwatchSelector` and `ColorSwatchLabelName`. A selector `SelectionProp` change copies the selected element's localized `AccName` into the visible name label, including the native Automatic swatch. No selection displays **Customized**. The prefix and name occupy a left-aligned flow row below the swatches, and the name is not a full-width centered element. The **Current color:** prefix, Customized fallback, checkbox text, accessibility descriptions and default-action text use `OpenGlassThemeExtension.rc`'s English (`en-US`), Simplified Chinese (`zh-CN`) and Traditional Chinese (`zh-TW`) `STRINGTABLE` resources. Windows selects the strings through its normal UI-language fallback, and the shared UIFILE is language-neutral. Labels and action descriptions with corresponding Windows 7 `themecpl.dll.mui` resources retain that wording. **Color selector** is a literal in the Windows 7 UIFILE without a corresponding MUI string, so the extension supplies its translations. The resource script uses UTF-8. Additional languages belong in resource tables, not XML or C++.

The XML's `accdefaction` stores the action description exposed through `IAccessible::get_accDefaultAction`, whose [contract requires localization](https://learn.microsoft.com/en-us/windows/win32/api/oleacc/nf-oleacc-iaccessible-get_accdefaultaction). DUI parses it as a string without an enum lookup. `accDoDefaultAction` dispatches the control's default action independently of this description, so **click** and **press** use string resources. DUI role, state and layout tokens remain unchanged. Developer diagnostics, such as a missing decorated export name, remain English. `resource.h` declares their IDs, which the XML references numerically. The XML's initial Customized label and the listener's no-selection fallback share `IDS_CUSTOM_COLOR`. The latter uses `LoadStringW`'s resource pointer, relying on `Common/CppProject.props`'s `NullTerminateStrings` setting. Existing system strings and swatch names retain their native localization. No extension DLL filename is embedded in the XML, so changing the build target name does not affect lookup.

### Transparency preview and preference handling

The checkbox initially reads the Windows `EnableTransparency` preference through
`Util::IsTransparencyEnabled()`. Only a successful DWORD read with a nonzero value
counts as enabled, while missing, invalid or unreadable values count as disabled.
On `SelectedProp` changes, it directly writes
`EnableTransparency` as `REG_DWORD` 0 or 1 under
`HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize`, using
`wil::reg::set_value_dword_nothrow` and the shared `Util::g_personalizeKeyPath`.

After a successful write, the checkbox listener calls the extension's getter
and setter with `skipPersistence = TRUE` to recalculate balances using the current
color/opacity. The setter passes the current DWM window and
`DwmSettings::UpdateType::Backdrop` to the shared `DwmSettings::NotifyChange`
helper. It sends the `WM_SETTINGCHANGE / ImmersiveColorSet` notification with
`SendMessageTimeoutW` and a one-second timeout.
`SendNotifyMessageW` rejects this cross-process string message with
`ERROR_MESSAGE_SYNC_ONLY`.
`_SetTransparency` only writes the preference and returns `S_FALSE` when it is
already equal, so the listener refreshes only on `S_OK`. A failed registry
write restores the checkbox to the current Windows preference. Later color-write
failures are logged, and they do not roll back an already successful system preference
change.

This checkbox always edits Windows `EnableTransparency`. The GUI's `--no-system-transparency` option changes only the GUI's mapping of `ColorizationOpaqueBlend`, and it does not affect the extension. The extension does not write or delete either layer's `ColorizationOpaqueBlend`. The existing opaque-blend and power-saver conditions still affect effective OpenGlass transparency. It does not change Automatic mode or the intensity range. The provider stores only a boolean transparency checkpoint, not the raw registry type, bytes or absence. Neither preview nor rollback deletes `EnableTransparency`. If an initially missing or invalid preference is enabled and then cancelled, rollback writes DWORD 0, and a previously enabled noncanonical DWORD is restored as 1 after a pending disable. Unchanged effective preferences skip the write. Pending changes are derived from the checkbox's selection. Returning the checkbox to its initial value clears the pending change. A page whose checkbox was never changed does not restore an unrelated external change. `glassAttribute` is not used to carry checkbox state: the modern native page retains only ARGB/mode and constructs fresh setter parameters with that field zero. See [Operation semantics](#operation-semantics) for the interaction between Save/Cancel and persistence.

### Native navigation and teardown

The namespace-level replacements join `CGlassColorCplPage::HandleOK` and
`HandleNavigateAway`. `HandleOK` updates the bound provider's checkpoint to the
current checkbox selection before forwarding to the original. Both the Save
button and Enter on a swatch dispatch through this method. Save therefore accepts
the transparency change immediately, even if native navigation subsequently
fails. `HandleNavigateAway` restores transparency and the checkbox when its
selection differs from the checkpoint, then calls the original color handler
so its balance calculation sees the restored preference. This rollback does not
explicitly call the color getter or setter. Its `SetSelected` notification sees
the already restored preference and skips the checkbox listener's refresh.
The replacements do not read the native revert or color-dirty fields, so transparency-only changes participate without making the native color dirty. Cancel, Escape,
Back, closing and high-contrast page switching share the native navigation path.

`CreateDUICP` finds `WindowColorCPL` through `FindDescendent` and obtains its
native handler. Each provider copies its seven-slot vtable into a
`CColorCplPageHandlerVtable` structure with named, typed function pointers:
`HandleOK`, `HandleCancel`, `HandleApply`, `HandleInput`, `HandleLayoutInit`,
`HandleNavigateAway` and `PerformInitTasks`. Only `HandleOK` and
`HandleNavigateAway` are replaced, while the other entries remain native. In the
inspected glass page, the empty `HandleApply` and `PerformInitTasks` implementations
share an address, whereas the accessible-color page's table distinguishes these slots.

The provider stores its back-pointer immediately after the copied function table.
One `PointerHookBase` replaces that instance's handler vtable pointer in a
`HookTransaction`, retaining its original table for forwarding and restoration.
Each replacement obtains the provider from its own handler's copied table.
There is no thread-local binding or shared-vtable mutation, so multiple pages
on the same thread retain their own providers and transparency checkpoints.
The Windows transparency preference itself remains shared between pages.

The following layout was independently inspected in x64 themecpl
**10.0.22621.4745** and **10.0.26100.9549**:

| Item | Location | Semantic evidence |
| --- | --- | --- |
| `CColorCplPageHandler` subobject | `WindowColorCPL + 0xD0` | Constructor and `CColorCplPage::_Setup` agree on the adjusted handler pointer |
| `HandleOK` | Handler vtable slot 0 | Save-button events and Enter on a swatch dispatch through this slot |
| `HandleNavigateAway` | Handler vtable slot 5 | `OnNavigateAway` and `_Setup` dispatch through this slot |

These offsets are explicit private-layout assumptions, not a general promise for
other binaries. Only `HandleOK` advances the checkpoint after page creation, while ordinary navigation restores it, including when `_Setup` reuses the page while
changing high-contrast mode. A failed transparency restore leaves the checkpoint
intact and still calls the original to perform its color rollback before returning
the error. Native callers may ignore the HRESULT, and the extension does not add a
navigation veto or durable recovery.

The provider owns its listeners for the page lifetime. It detaches
listeners in `DestroyCP` because native `FrameProvider` can destroy the provider
before the element tree. Element detach callbacks clear the corresponding
pointers when the tree is destroyed first. A listener on `WindowColorCPL` restores
the original handler vtable pointer through a removal transaction when detached.
`DestroyCP` removes this listener before releasing the copied table, and native `Element::OnDestroy` also detaches it before destroying the handler. Both teardown
orders therefore restore the original vtable before either owner is freed.
Teardown does not restore transparency, which remains part of native navigation.

## Activation and special exports

<a id="opening-a-theme-file"></a>
<a id="active-setup-and-dllinstall"></a>
<a id="dll-and-activation-boundary"></a>

The extension exports `DllGetClassObject`, `DllCanUnloadNow`, `DllRegisterServer`, and `DllUnregisterServer`. Windows continues to load original system DLLs directly for special entry points, specifically `themecpl!OpenThemeActionW` and `themeui!DllInstall`. These activation boundaries reflect inspected x64 **10.0.26100.9549** binaries and do not establish universal compatibility across other Windows versions or builds.

When opening a theme file, Windows invokes `themecpl!OpenThemeActionW`, which creates `ThemeManager2` through COM. That activation reaches the extension's registered `TreatAs` proxy, which initializes the `themeui.dll` hooks before returning the native class factory. Directly loading `themecpl.dll` does not bypass this interception, and the extension requires neither a forwarded `OpenThemeActionW` export nor activation of the separate CPL theme element proxy.

In contrast, calling `themeui!DllInstall` (including the `/UserInstall` and `/InstallVS` branches) constructs internal class factories directly without invoking COM. These direct construction paths bypass proxy activation and do not install hooks. If hooks were already initialized earlier within the same process by prior COM activation, they may still intercept subsequent color operations, but this mechanism cannot guarantee coverage for setup or logon execution. The extension does not export `DllInstall` and does not support `regsvr32 /i`.

A single extension DLL manages all three proxy routes, forwarding class factory requests while maintaining separate IAT state for normal imports in `themecpl.dll` and delay imports in `themeui.dll`.

## Color parameters

### Shared reads and opaque-blend decision

Runtime DWORD reads are shared with both renderers and GUI diagnostics through
`Common/DwmSettings.hpp`. Callers supply the target user's and machine's open DWM keys, and the helper owns no handles or cached state. Failed typed reads fall
through to the next layer. Balance reads use the shared Override precedence,
while the two Windows base colors remain fixed HKCU state.

The input to `DwmpSetColorizationParameters` is eight DWORDs on the inspected
10.0.26100.9549 binaries: Color, Afterglow, ColorBalance, AfterglowBalance,
BlurBalance, EnableWindowColorization, GlassAttribute, Flags. The two ARGBs and
the last three fields are retained. `EnableWindowColorization` is not used as
the opaque-blend switch, as the extension reads `ColorizationOpaqueBlend` using
HKCU, then HKLM, then zero, matching the runtime's ordinary lookup order.
Each setter calls `Util::IsTransparencyEnabled()` from `Common/SystemSettings.hpp`
to read `EnableTransparency` from
`HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize`, treating
missing, invalid or unreadable values as disabled. The same header provides the
`HKEY` overload used by both renderers and GUI diagnostics. `DisableGlassOnBattery` follows
HKCU, then HKLM, then one. The shared `DwmSettings::IsTransparencyDisabled`
predicate selects the opaque formula when
`ColorizationOpaqueBlend` is nonzero, `EnableTransparency` is zero, or power saver
is active while `DisableGlassOnBattery` is nonzero.

Each setter also calls `Util::IsEnergySaverEnabled()` from the same shared header.
`DisableGlassOnBattery` controls whether the result affects the opaque formula,
not whether the query runs.
The helper synchronously calls `NtPowerInformation(GetPowerSettingValue)` for
`GUID_POWER_SAVING_STATUS` first.
A nonzero result immediately counts as active, and a zero result also queries
`GUID_ENERGY_SAVER_STATUS`, so modern Standard mode is recognized even when the
legacy status is off. For the second query, `STATUS_INVALID_PARAMETER` (unknown
GUID) or `STATUS_NOT_SUPPORTED` means no additional saver state is available. Other query failures are logged and return false, matching the renderers.
The DWORD is compared with `ENERGY_SAVER_OFF`, covering both Standard and High
Savings without an OS-version threshold.

These settings return a total payload length followed by three packed
`{ ULONG ChangeStamp; ULONG ValueLength; DWORD Value; }` entries. The shared helper
retains the full output buffer and reads the first entry directly. For these
two status GUIDs, the kernel's `PopEsPublishState` / `PopEsPublishStateV2` writes
the same current state to AC and DC through `PopSetPowerSettingValueAcDc`, so
no power-source query or entry selection is needed. This does not generalize
to arbitrary power-policy settings. `CallNtPowerInformation` is not used because
its wrapper rejects this information class. There is no power notification,
cached flag or initialization wait. Power changes alone do not rewrite balances, so the next intercepted setter queries the current state.

### Color setters and persistence

The Win7 balance formula is shared with the GUI in `Common/Colorization.hpp`.
The full input alpha is used. The formula's scalar is saturated at the ends of
the valid weight curve (10..100 for opaque, 5..125 for transparent) before unsigned
conversion, keeping all three weights within 0..100 and their sum at 100. This
also fixes negative-weight wrapping in the GUI's opaque calculation at high
intensity. Both stored ARGBs are unchanged, including alpha 0 and 255. The old
modern-Windows balances are discarded.

Every setter call immediately writes these HKCU DWM values:

- `ColorizationColor`, `ColorizationAfterglow`,
- `AccentColor`, using the incoming Color RGB as opaque COLORREF (`0xFFBBGGRR`),
- `ColorizationColorBalanceOverride`, `ColorizationAfterglowBalanceOverride`, `ColorizationBlurBalanceOverride`,
- `GlassOpacity`, calculated as `(alpha * 100 + 127) / 255`, shared with the GUI.

After completing the DWM writes, `WriteColorization` writes the same opaque COLORREF to the following value when the setter's `skipPersistence` argument is zero: `HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Accent\AccentColorMenu`. A nonzero argument leaves that preference untouched. (See [Operation semantics](#operation-semantics) for caller usage of `skipPersistence`.) This extension deliberately does not consult color policies for this write. It does not modify the policies.

The three base balances, `EnableWindowColorization` and `ColorizationGlassAttribute`
are neither written nor removed. Vista glass reads `GlassOpacity`, while Windows 7
glass uses the balance Overrides. Existing `GlassOpacityInactive` remains independent.

After writing, the setter sends the existing best-effort targeted
`WM_SETTINGCHANGE / ImmersiveColorSet` refresh to the DWM notification window when
present, using a synchronous send with a one-second timeout, then returns `S_OK`.
The notification carries no color snapshot, as DWM reads the current registry state.
Both OpenGlass architectures handle this notification
by updating the backdrop. It invokes neither the saved original nor the actual
dwmapi setter export, and notification failures are logged without failing the setter.

### Getter and caller compatibility

The getter reads the two Windows ARGBs from HKCU, balances in runtime order
(HKCU Override, HKLM Override, HKCU base, HKLM base, default zero), and `GlassOpacity`
in ordinary HKCU, HKLM, default 63 order. Opacity and balances are clamped to 0..100.
The returned Color alpha reflects effective `GlassOpacity`, allowing the classic
panel to read GUI intensity changes. If the stored alpha already rounds to that
percentage, its exact byte is retained, but otherwise the GUI's percentage-to-alpha
conversion is used. Afterglow retains its independent ARGB. The getter requires the two Windows colors. It does not require or create either of the unused keys, but reads their existing values when present and defaults both fields to zero.

On the inspected binaries, themecpl's page getter, global color getter and color
icon update, plus themeui's global color getter, reinterpret alpha `FF` using
`((ColorBalance - 10) * .75 / 100 + .1)`. For that alpha alone, the hook returns
ColorBalance 130 so these consumers reconstruct 255. This is a caller compatibility
encoding, never an OpenGlass weight: the setter always recalculates the three
balances before writing. The remaining getter consumers either use only Color
(themeui thumbnail) or replace colors/balances before setting them (themecpl's
accessibility page and themeui's setter). The getter makes no registry writes.

`SetUserColorPreference` returns `S_OK` without invoking uxtheme. Both inspected
color callers invoke it after their DWM setter, and allowing it to run would issue
another DWM write and replace the just-calculated parameters. Accent synchronization
uses the explicit writes above, and palette and history side effects are not replicated.

## Automatic color acquisition

`ColorHooks.Shell.cpp` intercepts only themecpl's `CoCreateInstance`
requests for `CLSID_DesktopWallpaper` with `IDesktopWallpaperPrivate`
(`{C182461F-DFAC-4375-AB6E-4CC45AA7F9CC}`). The hook calls the saved
`g_CoCreateInstance_Org` entry and wraps the returned interface in an independent
forwarding object. Other classes and requested interfaces pass
through unchanged. Themeui's COM imports are not hooked.

The private interface extends `IDesktopWallpaper` with slots 19..24:
`SetWallpaperNoGP`, `PaintWallpaperThumbnail`, `ComputeImageColorizationColor`,
`GetMonitorNumber`, `Flush` and `PaintWallpaperBitmapFromShellTask`. This ordering
was inspected in shell32 **10.0.17763.737** and **10.0.26100.9549**. The newer
`IDesktopWallpaperPrivate2` adds methods after this prefix. The proxy implements
the base private IID only, plus `IDesktopWallpaper`, `IPersistStream`, `IPersist`
and `IUnknown`. These interfaces retain one proxy identity. Wallpaper methods
and persistence forward to the original object: themecpl's `CWallpaperCore`
also activates the private interface and queries `IPersistStream` to capture
and restore its desktop-background checkpoint.

Only `ComputeImageColorizationColor` changes behavior:

1. Call the native method to obtain its ARGB and preserve its alpha.
2. Use `Common/ShellColorRefresh.hpp` to synchronously send the same forced
   `UpdateWallpaperTransition(14, 0xFFFFFFFF)` request used by OpenGlassGUI.
3. Read the resulting HKCU DWM `ColorizationColor` and return its RGB combined
   with the native alpha. Native COM, Shell request and registry-read failures
   propagate as HRESULTs, and no unadjusted-color fallback is reported as success.

The Shell request requires a running Shell window and retains the shared
five-second timeout. It asks Windows to compute and apply its current automatic
accent, including Windows' policy and feature-dependent processing, and it is not
a read-only color calculation. The extension does not reproduce the luminance
cap or select an algorithm by OS version. In the inspected 1809 Shell, the raw
image RGB is transformed with HLS luminance 55, while in 26100.9549 this step depends
on feature 41486951. Reading the applied value follows the running Shell's choice.

The native color page sets `AutoColorization` before calling this method. Both
`_SetPreviewColorizationColor` and Cancel-to-Automatic in `HandleNavigateAway`
use its result as page state before calling the hooked DWM setter. Returning
the final RGB here keeps that state consistent without a separate color
checkpoint or a correction after the setter. The existing setter then writes
the native alpha, recalculated balances, opacity and accent values as usual.

Preserving that alpha retains themecpl's native Automatic intensity. The GUI
also queries the native method when its Automatic swatch is explicitly selected.
Both use the original byte for Windows 7 balances and separately round it to
`GlassOpacity`, and neither derives balances by converting the rounded percentage
back to alpha. The private interface declaration is shared in
`Common/DesktopWallpaper.hpp`. Editing layer, transparency and other independent
settings can still produce different rendered appearances.

The detailed calculation formula, normal bounds (125..175), fallback retention, caching and percentage rounding are documented in the shared GUI contract. See [native automatic alpha and later wallpapers](../OpenGlassGUI/Configuration.design.md#native-automatic-alpha-and-later-wallpapers). The extension preserves whatever byte the native method returns without clamping to the normal calculation range.

Later Shell-driven wallpaper changes are outside this proxy. The Shell's
`CDesktopBrowser::_SetDWMAndImmersiveColor` RGB conversion loses the original
alpha, so its final registry alpha is not a replacement source. Existing
`GlassOpacity` and balance Overrides retain the last explicit strength.
See [native alpha and later wallpapers](../OpenGlassGUI/Configuration.design.md#native-automatic-alpha-and-later-wallpapers)
for the boundary a future community modification would need to address.

The native wallpaper class factory can return its shared object. Shell's own
forced-refresh handler calls the same private method on that object, so mutating
its vtable would also intercept Shell's internal calculation. This proxy owns
separate COM storage and leaves the original vtables intact, so no recursion flag,
thread-local binding or extra hook transaction is needed.

## Operation semantics

Manual preview, Automatic, Save and any Cancel setter all immediately write the
DWM values, including `AccentColor`. The caller's `skipPersistence` flag controls
only the extra `AccentColorMenu` write: zero writes it, nonzero leaves it alone.
In the inspected themecpl, manual previews pass 1, Automatic previews pass 0, and
Save/Cancel can also pass 0. Thus the flag does not identify Save exclusively, so Automatic previews and Cancel can also update the persistent accent preference.
This differs from the GUI's own `AccentColorMenu` synchronization, which waits
until Save. Both paths still use the forced Shell request for Automatic, and that
request can independently change the Windows accent preference.

The setter and getter keep their temporary data and registry handles local to
each call. They neither use the mutable `ModuleHooks` state nor call saved
originals, and have no replacement-level reentrancy flag or per-call lock.
The initialization guard only prevents repeated hook installation. Registry
values remain shared external state: multi-value reads and writes are not atomic
and are not serialized across callers.

The classic panel captures its original color and calls the setter again on Cancel, which naturally restores the color, derived balances, opacity and DWM
`AccentColor` without an extension journal, and synchronizes `AccentColorMenu`
when that call passes zero. It does not restore arbitrary previous custom balances,
independent afterglow, raw value types, or absent values. The provider restores a
pending Windows transparency preview at the start of native `HandleNavigateAway`,
before its color rollback. This only restores the preference and checkbox, and any subsequent native color setter calculates balances with the restored
preference. No original color or arbitrary balance snapshot is added by that checkpoint.
A failed multi-value write can leave completed writes in place. Failures
are returned to the caller, and notification failures alone do not fail the operation.
No HKLM values or existing color Overrides are removed. Thus an existing color
Override can still mask the incoming base RGB. The three user balance Overrides
intentionally supersede machine balance Overrides and remain until changed or
removed by the user.

Automatic uses the Shell's applied RGB through the COM proxy above. The extension
does not watch wallpaper changes or intercept uxtheme writes in other processes. TreatAs
covers the subsequent COM activation in `OpenThemeActionW`, whereas the direct construction
paths in `DllInstall` do not initialize the extension, as described above.
Developers build the extension from source as a single unsigned DLL. Users own
registration and the process lifetime: unregister before replacing or removing
the DLL, exit all hosts that loaded the extension, then replace it
and register again if needed. Closing a window alone may leave its host running, and unregistration does not unload pinned hooks. See [README.md](README.md) for the
manual `regsvr32` commands. Build/unit and import-slot checks do not establish real desktop behavior, so manual testing of Settings, classic color sliders, Cancel and SecureUxTheme
coexistence is still required for this hook stage.
