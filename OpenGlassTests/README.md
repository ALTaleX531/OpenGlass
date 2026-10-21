# C++ regression tests

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

Names are case-sensitive. `--help` displays usage. Exit codes are 0 for success, 1 for a test failure, and 2 for invalid arguments or an unknown selection. A failed assertion or caught exception is reported against its case; the remaining selected cases still run.

Follow [the repository testing policy](../AGENTS.md#testing-policy). Put each tested production cpp/hpp's cases in its dedicated `NameTests.cpp`, with private helpers alongside those cases. For example, `ProjectionRuntimeTests.cpp` tests `OpenGlass/ProjectionRuntime.cpp`, `ProjectionHelperTests.cpp` tests `OpenGlass/ProjectionHelper.hpp`, and `ConfigurationResourcesTests.cpp` tests `OpenGlassGUI/ConfigurationResources.cpp`. `ThemeColorizationTests.cpp` covers `OpenGlassThemeExtension/Colorization.cpp`. This does not require creating a test for every production file.

`Main.cpp` owns only selection, execution, assertion reporting, and the explicit case table. Declare new cases in `TestCases.hpp`, register them with a `Suite.Case` name in that table, and explicitly list new files in the project and filters. `TestHarness.hpp` provides the shared assertion API. Keep reusable data/setup in focused fixtures such as `ProjectionFixture.hpp/.cpp` and `PngFixture.hpp`; never rely on another case running first. `ProjectionInvokeTests.cpp` contains compile-time ABI assertions, which are checked during every build and have no runtime selection.

List production implementations from other project directories as `_ExternalClCompile`, retaining their per-file options. The shared build target includes them during builds and IntelliSense input computation without showing external files at the project root. Only test-project files belong in this project's filters. Common headers remain visible in the Common shared project.

These tests run without injecting into DWM. Registry fixtures use unique keys under `HKCU\Software\OpenGlassTests`, and package/resource fixtures use temporary directories. GPU rendering, desktop appearance, and DWM integration require the separate real-machine verification described in the repository policy.
