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
installing that activation's hooks. Hook transaction errors still fail fast;
this capability check does not validate private page layouts.

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
paths, including SecureUxTheme's server when registered.

## Update or remove

1. Before replacing or deleting the DLL, unregister the currently installed copy
   from an elevated 64-bit PowerShell:

   ```powershell
   & "$env:SystemRoot\System32\regsvr32.exe" /u "C:\Program Files\OpenGlassThemeExtension\OpenGlassThemeExtension.dll"
   ```

2. Exit every process that loaded the extension. Closing its window alone may
   leave the host running. Sign out or restart Windows if necessary;
   unregistration does not unload existing objects or pinned hooks.
3. For an update, replace the DLL and register again. For removal,
   delete the files after their hosts have exited. If unregistration fails, retain
   the DLL and resolve the failure before replacing or deleting it.

When upgrading from the earlier three-DLL build, unregister its old core DLL
first and exit all hosts. Then remove the old UI/CPL DLLs, replace the core with
the new single DLL and register it. The three proxy CLSIDs remain unchanged.

If an earlier prototype was installed alongside OpenGlass, unregister using its
**old path** first, then exit its hosts and register the new copy in a separate
directory. Earlier installer records may still remove files from the old
directory when OpenGlass is uninstalled. The new installer does not migrate or
remove old registrations.

## Behavior

The classic color page adds a Windows transparency checkbox and restores the
selected swatch's name. Transparency changes take effect immediately and are
accepted by Save; Cancel, Back, Escape and closing the page restore a pending
transparency change through native navigation. The original intensity range remains 10..85.

The extension writes the two ARGB colors, DWM `AccentColor`, three balance Overrides
and `GlassOpacity` to HKCU. The caller's persistence flag also controls writes to
`AccentColorMenu`. Preview calls write immediately; Cancel relies on Windows
resubmitting the old color and does not restore an arbitrary configuration
snapshot. Existing color Overrides can still mask the base RGB. Windows owns
Automatic color acquisition; the extension does not watch wallpaper changes or
replicate accent palette/history updates.

For configuration, behavior and registration details, see
[Windows theme integration](https://github.com/ALTaleX531/OpenGlass/wiki/Windows-theme-integration).
The [design contract](Design.md)
documents the implementation for developers.
See [LICENSE.txt](../LICENSE.txt) for licensing terms. The extension is unsigned,
including when built in `ReleaseSigned`.
