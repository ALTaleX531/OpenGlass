# OpenGlass theme extension

This developer extension connects Windows theme application and the classic color
panel to OpenGlass. Developers can build and customize it from source or use the
separate unsigned CI artifact containing the DLL and matching PDB. It remains
part of solution and CI builds and is not included in formal Release assets.
See [CI builds](https://github.com/ALTaleX531/OpenGlass/wiki/Building-OpenGlass#automated-ci)
for artifact names and retention.
It requires OpenGlass for the glass effects.
The main installer does not include, activate, update or remove this extension.

The color hooks were investigated on Windows binaries **10.0.26100.9549**. The
core's compatibility table does not establish extension compatibility on other
builds. This is a developer prototype with manually managed registration
and process lifetime.

If DUI70 cannot be loaded, a required DUI export is unavailable or the required
imports cannot be resolved, activation returns the original COM factory without
installing that activation's hooks. Hook transaction errors still fail fast, and this capability check does not validate private page layouts.

## Build and customize

Build [OpenGlassThemeExtension.vcxproj](OpenGlassThemeExtension.vcxproj) with the `Release|x64` configuration. See [Building OpenGlass](https://github.com/ALTaleX531/OpenGlass/wiki/Building-OpenGlass) for build commands. To customize the output filename, change the project's `TargetName`. Because registration uses the DLL's own full path, register the renamed DLL directly.

## Install

1. Copy the DLL to a fixed, administrator-protected
   directory, for example `C:\Program Files\OpenGlassThemeExtension`. Keep it
   outside the main OpenGlass installation directory.
2. From an **elevated 64-bit PowerShell**, run the following, adjusting the path
   and filename for your build:

   ```powershell
   & "$env:SystemRoot\System32\regsvr32.exe" "C:\Program Files\OpenGlassThemeExtension\OpenGlassThemeExtension.dll"
   ```

3. Restart Settings and any personalization hosts to use the new registration.
   Add `--scope=hkcu` to the OpenGlass GUI shortcut's command line so its ordinary
   edits use the same user layer as the extension.

The single DLL (`OpenGlassThemeExtension.dll` in the default build) registers all three
COM routes to its own path and dispatches activation by proxy CLSID. Registration uses
machine-wide 64-bit `TreatAs` redirects and preserves the original COM server
paths, including SecureUxTheme's server when registered. The extension does not export `DllInstall`, and `regsvr32 /i` is not supported.

## Update or remove

1. Before replacing, moving, or deleting the DLL, successfully unregister the
   currently installed copy at its old path from an elevated 64-bit PowerShell:

   ```powershell
   & "$env:SystemRoot\System32\regsvr32.exe" /u "C:\Program Files\OpenGlassThemeExtension\OpenGlassThemeExtension.dll"
   ```

2. Exit every process that loaded the extension. Closing its window alone may
   leave the host running. Sign out or restart Windows if necessary, as unregistration does not unload existing objects or pinned hooks.
3. For an update, replace the DLL and register again. If moving to another
   fixed administrator-protected directory, move the DLL and register again from
   its final path. For removal, delete the files after their hosts have exited.
   If unregistration fails, keep the DLL at its registered path until resolved
   before any replacement, movement, or deletion.

When upgrading from the earlier three-DLL build, unregister its old core DLL
first and exit all hosts. Then remove the old UI/CPL DLLs, replace the core with
the new single DLL and register it. The three proxy CLSIDs remain unchanged.

If an earlier prototype was installed alongside OpenGlass, unregister using its
**old path** first, then exit its hosts and register the new copy in a separate
directory. Earlier installer records may still remove files from the old
directory when OpenGlass is uninstalled. The new installer does not migrate or
remove old registrations.

## Behavior

The classic color page retains the native 10..85 intensity range and adds a Windows
transparency checkbox. Selecting a swatch displays its localized name below the swatches, while unselected custom colors display Customized.

The transparency checkbox directly modifies the current-user Windows transparency
setting. The OpenGlass GUI option `--no-system-transparency` does not affect the
extension, and active energy saver or opaque blend settings can still prevent
transparency. Previews update immediately. Clicking Save accepts transparency changes
immediately, even if subsequent native page navigation fails. Cancelling, pressing
Escape, navigating Back, or closing the page restores only the initial boolean
transparency state (initially missing or invalid values count as disabled and restore
as disabled, never deleting the value, without preserving original raw types or absence).
An untouched checkbox leaves external transparency modifications alone. Restoration
failures provide no durable retry guarantee.

Cancelling color changes restores the previous color and recalculates glass strength, but it does not restore arbitrary custom balances, independent afterglow, or raw registry
snapshots. If a multi-value write fails partially, completed values remain written.
Existing color Overrides can mask incoming base colors. Unlike the GUI, which
synchronizes the Windows menu accent preference only on Save, Automatic previews and
Cancel operations may update it immediately.

Automatic color acquisition requires a running Windows Shell and uses the Shell's
final RGB, which system policy may alter over raw wallpaper colors, reporting Shell
request and color-read failures. The request has a five-second timeout. Subsequent wallpaper changes retain the last explicit
OpenGlass strength rather than recalculating balances.

For internal architecture, COM forwarding, and hook implementation details, see
[Design.md](Design.md). See [LICENSE.txt](../LICENSE.txt) for licensing terms. The
extension is unsigned, including when built in `ReleaseSigned`.
