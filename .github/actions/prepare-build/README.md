# Windows dependency reuse

Both Verify and Build use this action. It restores NuGet packages, then tries an
exact cache of the installed vcpkg dependency tree, the pinned vcpkg executable,
and its MSBuild integration scripts. An installed-cache hit skips vcpkg checkout,
bootstrap, registry/tool downloads, ABI calculation and installation. CI disables
MSBuild's automatic vcpkg bootstrap and manifest installation because this action
owns dependency preparation.

The installed cache key includes:

- Runner OS/architecture and image version, MSBuild version, selected MSVC toolset
  and version, Visual Studio installation, resolved SDK include/library paths,
  and PowerShell version.
- Build configuration/platform, vcpkg commit, target triplet and downloaded-tool
  policy.
- `vcpkg.json`, `vcpkg-configuration.json`, overlay ports/triplets, this action's
  implementation, `.github/workflows/build.yml` (including CI environment and
  build arguments) and `Common/CppProject.Configuration.props`.

There are no installed-cache restore prefixes: a different environment or input
must run vcpkg again. The selected Visual Studio installation is also passed to
vcpkg. On a miss, existing binary and download caches remain available, with their
prefix restoration and vcpkg's normal ABI/hash checks. Compatible packages are
reused; changed packages are rebuilt. Compiler tracking remains enabled.

Jobs with the same runner label can receive different image versions during a
GitHub image rollout. Verify and Build then deliberately use different installed
keys; sharing a workflow run does not permit bypassing the environment match.
Subsequent jobs on either image can reuse the corresponding saved tree.

Installed and binary cache primary keys share the complete identity. If a
toolchain or CI setup change invalidates the installed tree, the binary cache
restores through its prefixes and can save any newly built ABI packages under a
new key. An exact hit on an older, immutable binary-cache key must not prevent
that publication. Download caches remain independent of the installed identity;
vcpkg verifies source archives and tools when using them.

The action saves the completed installed tree before compilation and tests.
Build can therefore reuse Verify's dependency preparation, and a later failed
test does not discard that work. No application objects, generated projections,
test results or build outputs are cached. Local builds keep normal manifest
integration and Debug dependencies; this CI triplet remains Release-only with
static libraries and CRT.

GitHub's normal branch access and cache eviction rules still apply. A cache miss
is supported and does not imply failure. Dependency summaries report the installed
cache key/hit, fallback caches and installation time. Compare the `Prepare Windows
build` step duration for each job, and check logs for actual cache hits and skipped
initialization; a cold run and a warm run measure different paths. Keep local
timing reports in ignored `.archive/`, not tracked documentation.
