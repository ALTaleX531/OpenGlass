# Build packaging and installation

## Build outputs

Packaging applies to the Release and ReleaseSigned configurations. When Inno Setup is installed, building the solution packages `OpenGlassSetup.exe`, and ReleaseSigned also outputs `OpenGlassSetup.Signed.exe`. Each installer bundles both architecture DLLs and installs the one matching the detected OS build. Packaging also archives `OpenGlassSymbols.zip` with PDBs for both DWM DLLs, Host and GUI. If `ISCC.exe` is absent, the solution skips installer and symbol packaging without failing. Setting `OpenGlassInstallerEnabled=false` suppresses packaging explicitly.

The developer theme extension remains part of solution and CI builds, requiring no packaging tools. The installer does not contain or manage the extension, and formal Release assets exclude it. The extension remains unsigned in every configuration, including ReleaseSigned. CI uploads the unsigned DLL and matching PDB as a separate developer artifact on every successful Build job, including pull requests and manual runs that have packaging disabled.

The solution builds Host, GUI and the theme extension once into `Build\x64\<Configuration>\common\`, and the two DWM DLLs into `legacy\` and `milcomp\`. The shared Utility project packages installers and symbols without coupling installer generation to the GUI. Direct packaging requires `ISCC.exe` and fails if it is missing:

```powershell
msbuild Scripts/OpenGlass.Packaging.proj /m /p:Configuration=Release
```

See [Windows dependency reuse](../.github/actions/prepare-build/README.md) for shared Verify/Build preparation and cache invalidation, and [the developer extension](../OpenGlassThemeExtension/README.md) for its separately managed installation.

## Project items and filters

Project `.filters` files list only file items owned by that project directory. Cover the complete IDE-visible item set after imports, including `OpenGlass.Shared.vcxitems` files in both DWM projects. A shared project's own `.vcxitems.filters` does not classify consumer views. Keep every visible item in a declared filter, sources (including assembly/shaders) in `Source Files`, and headers in `Header Files`. Keep Legacy/MILComp source/header filters flat. Browse Common files through its shared project instead of importing them into every consumer. Declare externally compiled implementations as `_ExternalClCompile`: `CppProject.props` adds them to `ClCompile` during build/source-input computation, preserving per-file options without creating loose IDE file nodes. Do not add external files to consumer filters or drop their compilation inputs.

## Installation boundaries

Uninstall policy is independent of GUI editing scope. Interactive uninstall provides separate choices for preset packages and configuration. Configuration cleanup targets HKLM and the elevated administrator account's HKCU, including fixed resources in both scopes. When run with alternate administrator credentials, cleanup targets that elevated account and leaves the original launching account's settings intact. Warn users that other profiles' settings remain and may require manual cleanup.

Remove only known OpenGlass values. Never delete the Windows DWM key or enumerate, load, or modify other user hives. Deleting the preset library preserves independent current resources and other user directories. Silent uninstall deletes the elevated account's configuration and HKLM unless `/nodeleteconfig` is specified, and preserves packages unless `/deletepresets` is specified. Always preserve Windows `EnableTransparency`.

Writable runtime data resides outside the binary directory. Runtime and GUI share `%ProgramData%\OpenGlass\symbols` as the default cache, where the installer grants Window Manager write access. The installer owns `%ProgramData%\OpenGlass\dumps`. It provisions this directory without ordinary user read access, grants Window Manager the access required by DWM WER, and removes it recursively on uninstall. Never remove a user-selected custom dump folder.

See [GUI privileges](../OpenGlassGUI/Configuration.design.md#8-privileges-and-installation) for original-user launch, diagnostic actions and preset-drop boundaries.
