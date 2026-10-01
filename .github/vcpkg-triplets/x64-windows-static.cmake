# Match x64-windows-static, but build only the configuration used by CI.
# This overlay is enabled by the workflow; local Debug builds keep the default.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_PROVIDED_FORTRAN ON)
set(VCPKG_PLATFORM_TOOLSET v145)
set(VCPKG_BUILD_TYPE release)
