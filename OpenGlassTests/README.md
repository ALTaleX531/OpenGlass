# Local C++ test workspace

Use `OpenGlassTests` to investigate changes and exercise focused cases locally before pushing. Maintain cases around current development needs, without a long-term regression-coverage or product-quality guarantee. Actual behavior on the target system remains the primary acceptance criterion for runtime changes.

Prerequisites: On a fresh checkout, complete the shared NuGet solution restore before your first standalone tests build as described in [Optional single-project builds](https://github.com/ALTaleX531/OpenGlass/wiki/Building-OpenGlass#optional-single-project-builds), and ensure Python dependencies are installed as described in [Python Dependencies](../tools/README.md#dependencies).

Build and run from the repository root:

```powershell
msbuild OpenGlassTests/OpenGlassTests.vcxproj /m /p:Configuration=Release /p:Platform=x64
.\Build\x64\Release\common\OpenGlassTests.exe
```

Without arguments the executable runs every registered test and prints each result and a final count. CI compiles the project as part of the solution but does not execute it. List names or select one case or one source unit:

```powershell
.\Build\x64\Release\common\OpenGlassTests.exe --list
.\Build\x64\Release\common\OpenGlassTests.exe --test ProjectionRuntime.LogicalSymbolBindings
.\Build\x64\Release\common\OpenGlassTests.exe --suite ProjectionRuntime
```

Names are case-sensitive. `--help` displays usage. Exit codes are 0 for success, 1 for a test failure, and 2 for invalid arguments or an unknown selection. A failed assertion or caught exception is reported against its case, but the remaining selected cases still run.

## Test organization and coverage

Follow [the repository testing policy](../AGENTS.md#testing-policy). Put each tested production cpp/hpp's cases in its dedicated `NameTests.cpp`, with private helpers alongside those cases. For example, `ProjectionRuntimeTests.cpp` tests `OpenGlass/ProjectionRuntime.cpp`, `ProjectionHelperTests.cpp` tests `OpenGlass/ProjectionHelper.hpp`, and `ConfigurationResourcesTests.cpp` tests `OpenGlassGUI/ConfigurationResources.cpp`. `ThemeColorizationTests.cpp` covers `OpenGlassThemeExtension/Colorization.cpp`. This does not require creating a test for every production file.

Behavioral tests cover projection resolution and publication, hook lifetime, effective configuration, preview and recovery, package validation and resources, HWND appearance overrides, Accent eligibility, and representative color calculations. Prefer explicit behavioral cases over copied production tables, repeated implementations of the expected algorithm, or exhaustive combinations that repeat an existing contract test. Catalog tests verify module/GUID/age matching, staging without early publication, and clearing a previously resolved Optional on the next successful commit, while generated-table corruption is verified at the source-validation boundary rather than in C++ runtime tests. Unresolved Optional Symbols permit commit and return null from `try_get()`. `get()` still requires a resolved address. Registry tests use isolated keys under `HKCU\Software\OpenGlassTests`.

`Main.cpp` owns only selection, execution, assertion reporting, and the explicit case table. Declare new cases in `TestCases.hpp`, register them with a `Suite.Case` name in that table, and explicitly list new files in the project and filters. `TestHarness.hpp` provides the shared assertion API. Keep reusable data/setup in focused fixtures such as `ProjectionFixture.hpp/.cpp` and `PngFixture.hpp`, and never rely on another case running first. `ProjectionInvokeTests.cpp` contains compile-time ABI assertions, which are checked during every build and have no runtime selection.

List production implementations from other project directories as `_ExternalClCompile`, retaining their per-file options. The shared build target includes them during builds and IntelliSense input computation without showing external files at the project root. Only test-project files belong in this project's filters. Common headers remain visible in the Common shared project.

## Isolation and environment boundaries

These tests run without injecting into DWM. Package and resource fixtures use temporary directories, and registry fixtures use isolated test keys. GPU rendering, desktop appearance, and DWM integration require the separate real-machine verification described in the repository policy.

Verify background copy coalescing and Direct2D four-stage batching using GPU probes linked to production realizers. Test `CGlassRealizer` with nonzero blur values alongside caller solid-color routing. Validate source box clipping, blur expansion, and cost-based copy merging against full-source references and prior capture states, quantifying accepted kernel-tail differences. Test variations across geometry complexities, occlusion states, formats (BGRA8/FP16), quality presets, coordinate alignments, and private clip regions. Verify two-scratch buffer reuse, stage footprint caching, graph reconnection across parameter changes, and native draw fallback paths. Validate context state restoration, format and dimension transitions, resource reuse lifecycles, and asynchronous cleanup of pending commands. Confirm compliance with the [batching contract](../OpenGlass/Runtime.design.md#direct2d-effect-batching) across shared inputs and non-sampleable targets. Use [OpenGlassRenderTest](../OpenGlassRenderTest/README.md) in default Speed mode to measure CPU time, GPU timestamp spans, and pixel differences across production D2D, reference D2D rects, production D3D, and built-in D2D Scale(0.5)+GaussianBlur. Store local test fixtures and capture artifacts in `.archive/`. Target-system tests exercise live desktop integration within DWM.

For texture-backed targets, compare the selected slice and mip with an equivalent 2D target and check that other subresources retain their contents. Exercise shared and copied inputs, SafetyZone restoration, protection transitions, nested target lifetimes, and state restoration after renderer failure.

`CaptionText.DWriteGeometry` checks that long layout boxes, alignment and reading direction do not inflate single-line or multiline shadow bitmaps or change text/image origins. It uses DirectWrite without a GPU, but glow pixels and target-DWM clipping still require rendering verification.

`CaptionText.DWriteTrimming` covers empty, single-line and multiline captions, including LF/CRLF, trailing line breaks, RTL, trimming on either line and removal of trimming after widening. It uses real DirectWrite layouts to regress the caption crash caused by a one-line metrics buffer.

`WindowFrameBounds.AtlasShadow` covers asymmetric shadow exclusion on a negative-coordinate monitor, unchanged visible bounds when restored frame margins and atlas sizes grow together, and preservation of the native maximized clip. Recording-border submission, thumbnail updates, transitions and clone behavior still require target-DWM verification.
