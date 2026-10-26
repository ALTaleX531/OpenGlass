# C++ regression tests

Prerequisites: On a fresh checkout, complete the shared NuGet solution restore before your first standalone tests build as described in [Optional single-project builds](https://github.com/ALTaleX531/OpenGlass/wiki/Building-OpenGlass#optional-single-project-builds), and ensure Python dependencies are installed as described in [Python Dependencies](../tools/README.md#dependencies).

Build and run from the repository root:

```powershell
msbuild OpenGlassTests/OpenGlassTests.vcxproj /m /p:Configuration=Release /p:Platform=x64
.\Build\x64\Release\common\OpenGlassTests.exe
```

Without arguments the executable runs every registered test, as CI does using its configured build platform and configuration. It prints each result and a final count. List names or select one case or one source unit:

```powershell
.\Build\x64\Release\common\OpenGlassTests.exe --list
.\Build\x64\Release\common\OpenGlassTests.exe --test ProjectionRuntime.LogicalSymbolBindings
.\Build\x64\Release\common\OpenGlassTests.exe --suite ProjectionRuntime
```

Names are case-sensitive. `--help` displays usage. Exit codes are 0 for success, 1 for a test failure, and 2 for invalid arguments or an unknown selection. A failed assertion or caught exception is reported against its case, but the remaining selected cases still run.

## Test organization and coverage

Follow [the repository testing policy](../AGENTS.md#testing-policy). Put each tested production cpp/hpp's cases in its dedicated `NameTests.cpp`, with private helpers alongside those cases. For example, `ProjectionRuntimeTests.cpp` tests `OpenGlass/ProjectionRuntime.cpp`, `ProjectionHelperTests.cpp` tests `OpenGlass/ProjectionHelper.hpp`, and `ConfigurationResourcesTests.cpp` tests `OpenGlassGUI/ConfigurationResources.cpp`. `ThemeColorizationTests.cpp` covers `OpenGlassThemeExtension/Colorization.cpp`. This does not require creating a test for every production file.

Behavioral tests cover projection resolution and publication, hook lifetime, effective configuration, preview and recovery, package validation and resources, HWND appearance overrides, Accent eligibility, and representative color calculations. Prefer explicit behavioral cases over copied production tables, repeated implementations of the expected algorithm, or exhaustive combinations that repeat an existing contract test. Catalog tests verify module/GUID/age matching, absent Optional symbols, staging without early publication, and clearing a previously resolved Optional on the next successful commit, while generated-table corruption is verified at the source-validation boundary rather than in C++ runtime tests. Registry tests use isolated keys under `HKCU\Software\OpenGlassTests`.

`Main.cpp` owns only selection, execution, assertion reporting, and the explicit case table. Declare new cases in `TestCases.hpp`, register them with a `Suite.Case` name in that table, and explicitly list new files in the project and filters. `TestHarness.hpp` provides the shared assertion API. Keep reusable data/setup in focused fixtures such as `ProjectionFixture.hpp/.cpp` and `PngFixture.hpp`, and never rely on another case running first. `ProjectionInvokeTests.cpp` contains compile-time ABI assertions, which are checked during every build and have no runtime selection.

List production implementations from other project directories as `_ExternalClCompile`, retaining their per-file options. The shared build target includes them during builds and IntelliSense input computation without showing external files at the project root. Only test-project files belong in this project's filters. Common headers remain visible in the Common shared project.

## Isolation and environment boundaries

These tests run without injecting into DWM. Package and resource fixtures use temporary directories, and registry fixtures use isolated test keys. GPU rendering, desktop appearance, and DWM integration require the separate real-machine verification described in the repository policy.

`CaptionText.DWriteGeometry` checks that long layout boxes, alignment and reading direction do not inflate the shadow bitmap or change text/image origins. It uses DirectWrite without a GPU, but glow pixels and target-DWM clipping still require rendering verification.
