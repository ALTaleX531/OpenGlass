# wxWidgets 3.3.4 overlay

This is the vcpkg wxWidgets port from the repository's registry baseline
`13465a7b726f171350defa5368b901a52f7a6af5`, updated to the official `v3.3.4` tag.
The upstream port's MIT notice is retained in `VCPKG-LICENSE.txt`.
Keep the registry baseline and unrelated dependency versions unchanged. Source
archives use pinned SHA-512 hashes. Lexilla follows the submodule commit recorded
by wxWidgets 3.3.4, while Scintilla retains the release's unchanged submodule commit.

The Wayland setup-header fix is already in 3.3.4 and is no longer patched. The
SDL2 integration patch follows the new SDL3/SDL2 selection block. The existing
vcpkg installation, relocation, dependency-export and static-runtime integration
remain in place. The overlay adds [refresh-system-colour-policy.patch](refresh-system-colour-policy.patch) to refresh uxtheme's cached colour policy before checking light or dark mode.

Installed MSW packages omit upstream `build.cfg` build records because they retain original compiler flags and machine-specific paths. MSBuild and CMake consumers use installed headers, libraries and package exports rather than these records. CI dependency source-path trimming and vcpkg's normal absolute-path checks remain enabled.

The GUI uses the 3.3.4 native system-colour notification path. See [the configuration contract](../../OpenGlassGUI/Configuration.design.md#gui-system-colour-notifications) for notification handling and the read-only swatch refresh. Upstream changes improve dark mode and notification timing, but they do not guarantee flicker-free repainting on every Windows version.
