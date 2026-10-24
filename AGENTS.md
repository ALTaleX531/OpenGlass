# OpenGlass agent guide

OpenGlass restores Aero-style glass effects by loading `OpenGlass.dll` into `dwm.exe`. Changes to injection, hooks, private DWM layouts, services and registry writes are system-critical. Preserve established safety contracts without adding speculative safeguards.

## Repository map

- `OpenGlass/`: core DLL, shared runtime, two architecture projects and independent projection inventories.
- `OpenGlassHost/`: service wrapper that loads the DLL and starts `ServiceMain`.
- `OpenGlassGUI/`: wxWidgets configuration UI, symbol downloads and DWM diagnostics.
- `OpenGlassThemeExtension/`: separately registered Windows theme integration with an independent [hook lifetime](OpenGlassThemeExtension/Design.md#module-and-hook-lifetime).
- `Common/`: cross-module C/C++ code and shared MSBuild configuration.
- `OpenGlassTests/`: non-injecting C++ regression tests.
- `OpenGlassRenderTest/`: interactive GPU/effect benchmark, not an automated test suite.
- `tools/` and `tests/`: Python tools and their tests.
- `Scripts/`: Inno Setup and MSBuild packaging.

The service validates and injects the core DLL into DWM, then transfers the target session's user-registry handles through an authenticated pipe. Injected hooks read settings and render effects. The GUI edits settings and requests redraws under its separate configuration and privilege contracts.

## Build architecture

- `OpenGlass.Legacy.vcxproj` owns the Legacy compositor path for builds `[17763, 28000)`, while `OpenGlass.MILComp.vcxproj` owns the MILComp path for build 28000+. Their private projections and ABI remain independent.
- Keep runtime lifecycle and engine sequencing shared in `OpenGlass.cpp` and `GlassEngine.cpp`. `GlassKernel` owns shared calculations, while architecture-private `GlassInfra` owns hooks, private state and device integration. Preserve settings side effects and startup and shutdown order.
- Project include paths select architecture-private headers. Share functional helpers while keeping replacement entry points, hook storage, ABI selection, state and hook lifecycle in each architecture. Do not force a common hook skeleton through adapters or duplicate the shared engine.
- `OpenGlass.Shared.vcxitems` explicitly lists shared files. Do not introduce source wildcards or spread architecture-selection `#ifdef`s through shared business code. Keep backend rendering calls confined to the engine's documented guards.
- Put cross-module code in `Common/`, DWM-only shared code in `OpenGlass/`, and GUI-only code in `OpenGlassGUI/`. Test access alone does not justify moving production code. Keep cohesive helpers together and shared files independent of module-private headers.

Determine compatibility from exact OS and module build numbers and revisions, plus verified binary capabilities. Symbol presence, layout and feature availability are separate facts. Marketing names, nearby fields and allocation sizes are insufficient evidence. Do not claim support until both module projections and the relevant runtime path have been verified.

Read [Runtime.design.md](OpenGlass/Runtime.design.md) before changing component integration, rendering ownership or lifecycle details.

## Build and verification

Use Visual Studio/MSBuild with the v145 toolset, Windows SDK 10.0, Python 3.11+, vcpkg manifest integration and repository overlay ports. C++ dependencies are static and declared in `vcpkg.json`. Install pinned Python dependencies with the interpreter used for generation:

```powershell
python -m pip install -r requirements.txt
msbuild OpenGlass.slnx /m /restore /p:RestorePackagesConfig=true /p:Configuration=Release /p:Platform=x64
```

Use `Release` for local verification. `ReleaseSigned` requires the shared signing environment. Generated projections belong only under the selected project's `$(IntDir)\Generated\Projection`. Never edit, copy into the source tree or commit them. PE size and wrapper machine code are investigation aids, not fixed acceptance gates.

CI Verify runs Python checks and non-injecting C++ tests before Build compiles the solution and uploads artifacts. Keep dependency preparation shared and invalidate caches when toolchain or dependency inputs change. See [dependency reuse](.github/actions/prepare-build/README.md) and [packaging](Scripts/README.md), including optional installer tooling and the separate unsigned developer extension artifact.

Report whether `OpenGlassRenderTest` was built or manually exercised. Never install the service, inject into DWM, kill DWM or write registry settings merely to verify unrelated source or documentation changes.

## Testing policy

- Organize `OpenGlassTests/` by the production cpp/hpp under test, using a dedicated `NameTests.cpp` for each tested unit. Keep the runner and shared fixtures separate from test implementations. The executable runs all tests by default. Preserve `--list`, `--test Suite.Case`, and `--suite Suite` selection. Every runtime case must run independently. See [the C++ test guide](OpenGlassTests/README.md).
- Prioritize permanent automated regression tests for code that is frequently changed and frequently used. Cover meaningful behavior, shared contracts, and demonstrated regressions. Do not pursue blanket coverage or tests that merely mirror implementation details.
- Keep rarely changed, rarely used code at minimal useful coverage. When changing it, write focused temporary tests for that change as needed instead of automatically growing the permanent suite. Retain a new test permanently when repeated use or a recurring regression justifies its maintenance cost.
- When the changed behavior requires a real machine, target OS, GPU, desktop session, or DWM integration, create a minimal temporary test demo after implementation and exercise the actual changed path on a suitable test machine. Use a concrete expected result and record the observed result. Builds, mocks, schema checks, and non-injecting tests do not substitute for this validation. If the required environment is unavailable, report the path as not run and identify what remains to be exercised.
- Keep temporary test code, demos, fixtures, binaries, and results in ignored `.archive/`, outside permanent test discovery and CI. Promote only reusable tests into `tests/` or `OpenGlassTests/`. Do not accumulate one-off verification scaffolding in production code or tracked test suites.
- Match verification to the change. Run the relevant tests and required checks, and broaden testing when a failure or unresolved concern warrants it. Documentation-only edits normally need consistency checks, not a full build or real-machine demo. Existing CI and release verification requirements still apply to their respective workflows.

## Simplicity and defensive programming

- Reject speculative defensive programming. Prefer simple, direct, efficient code that follows established contracts. Do not design or implement defensive measures merely because something could theoretically go wrong.
- Locks, synchronization, checks, boundary-condition handling, duplicate validation, fallbacks, retries, recovery states, and similar safeguards require all three: substantial concrete evidence of a reachable failure in the actual supported execution path, consequences serious enough to warrant intervention, and a clear corrective benefit that outweighs code complexity, runtime cost, and maintenance burden. Without that evidence and justification, omit the measure and continue with the simplest correct implementation. A hypothetical race, imagined misuse, or the label "system-critical" alone is insufficient.
- Rely on established ownership, lifetime, threading, and input contracts inside their boundaries. Perform necessary validation at the actual trust or lifecycle boundary and avoid repeating it downstream. When a safeguard meets the criteria above, use the narrowest effective measure and explain the proven failure, consequence, and benefit in the relevant design rationale or code comment.
- Preserve explicit security, hostile-input, transaction and hook-lifecycle contracts in this guide and its linked design documents. This policy does not authorize removing those protections indiscriminately or extending them into unrelated paths. Additional safeguards still require the evidence and benefit above.

## Editing conventions

- Keep this guide focused on repository-wide architecture, workflow and norms. Put component implementation rationale and individual regression details beside the code or in focused documentation, with a link here when relevant.
- Preserve tabs, Allman braces, PascalCase, `m_` members, `g_` globals and existing WIL error handling in C++.
- Name replacements `My<Class>_<NativeFunction>` and original-entry storage `g_<Class>_<NativeFunction>_Org`, preserving native leading underscores, for example `MyCLivePreview__FadeOutToGlass`. Keep C++, assembly and projection names consistent.
- Preserve unrelated working-tree changes and avoid broad generated rewrites. Update affected build inputs, filters, tests and focused documentation together. The ignored `.wiki/` is an independent Git repository. Inspect relevant pages and keep its commits and pushes separate.
- Keep architecture-private declarations beside implementations. Shared consumers select them through project include paths, without forwarding headers.
- Keep project items and filters explicit, unique and complete after imports, covering shared imports in both DWM consumers. Classify only project-owned files. Keep architecture source and header filters flat, with sources, assembly and shaders in `Source Files` and headers in `Header Files`. Use `_ExternalClCompile` for external implementations and browse Common through its shared project. See [project-item details](Scripts/README.md#project-items-and-filters).
- Consolidate unpushed format changes into at most one `schema_version` increment per schema relative to its last pushed format, and reuse an already incremented version.
- Keep Python production code in `tools`, tests in `tests/`, and invoke commands as `python -m tools.MODULE` from the root. Use explicit imports without `sys.path` hacks or dynamic source loading. Skills invoke shared tools instead of duplicating validators. Run the full suite with `python -m unittest discover -s tests -p "test_*.py"`.
- Follow [validation ownership](tools/README.md#validation-ownership): loaders own format and integrity, emitters trust normalized data, and acquisition and audits own binary evidence. Do not revalidate internal reports, rehash downstream or reload freshly validated output merely to repeat checks.
- Use mature Python libraries when they meaningfully replace substantial custom work, and prefer the standard library where already concise. Update dependencies, setup, CI and documentation together.
- Keep machine-specific paths, local audit and review reports, diagnostic exports and acceptance records out of Git. Store them in ignored `.archive/` and track reusable design contracts instead.

### GUI configuration

Read and follow [Configuration.design.md](OpenGlassGUI/Configuration.design.md) before GUI changes. Preserve selected-layer editing, immediate preview, Save and Revert boundaries and original interactive-user identity across elevation. Never delete Windows `EnableTransparency`. Administrative operations require entry-point privilege checks as well as appropriate control state, while read-only Diagnostics must remain available.

### Presets and Windows colors

Read and follow [Presets.md](OpenGlassGUI/Presets.md). Treat preset ZIPs as untrusted and preserve path, ratio and payload validation for every import route. Keep editing-layer values, effective presets, Windows accent state and preview checkpoints distinct. Preserve each operation's recovery guarantees and known attribution and source terms without claiming legal determinations.

### Installation, diagnostics and process boundaries

- Preserve session and process validation for host-pipe and registry-handle transfer and authenticated original-user elevation. User operations target that verified user, while machine operations target HKLM.
- Configuration cleanup targets known OpenGlass values in HKLM and the elevated uninstaller account's HKCU, plus both scopes' fixed resources. Deleting only the preset library preserves current configuration resources. Never enumerate, load, or modify other users' hives, or delete the Windows DWM key. Preserve other accounts' settings and resources, as well as user-selected custom dump folders. See [installation boundaries](Scripts/README.md#installation-boundaries).
- Keep writable runtime data outside the binary directory. WER remains opt-in and scoped to `LocalDumps\dwm.exe`, with protected installer-owned dumps and no global crash collection. Preserve administrative action boundaries and separation from GUI Save and Revert.
- Keep the preset-drop UIPI exception window-local and limited to required transfer messages. Drops use the same strict validation as Import. See [the privilege contract](OpenGlassGUI/Configuration.design.md#8-privileges-and-installation).

## DWM projections and offsets

Read [the projection contract](OpenGlass/ProjectionSchemas/README.md) and [catalog contract](OpenGlass/SymbolCatalogs/README.md) before changing their mechanisms or inventories.

- Maintain independent backend inventories. Select CPU groups before validating ranges and ABIs, and keep generation, catalog selection and caches consistent. Tool CPU targets do not expand solution or runtime support.
- Access private DWM fields through typed Layout accessors, preserving constness. Add or correct Layouts instead of embedding object arithmetic or padding-based fake layouts. Keep projection wrappers pure. Fallback and range logic belongs in metadata and cold resolution paths.
- Match exact, unmodified DbgHelp `UNDNAME_COMPLETE` names and declare real typed ABIs. Never use name-only, substring, decorated-name or first-match fallbacks, or merge incompatible ABIs into one logical Symbol. Retain useful semantic evidence notes.
- Descriptor integrity belongs to generation. Runtime trusts internal tables while validating actual module version and resolution. Both modules follow `Reset → Collect → Validate → Commit`. Publish neither unless both validate. Active Required Symbols are global prerequisites, without component masks or partial readiness.
- Use [$maintain-dwm-offsets](.agents/skills/maintain-dwm-offsets/SKILL.md) for binary, layout and projection audits and schema maintenance. Audits default to read-only. Production schema changes require user authorization and independent semantic evidence from the exact binary.
- Keep structural and binary verification, IDA semantic evidence and real-OS validation distinct. Exact symbol resolution alone proves neither ABI semantics nor rendering behavior.

## Safety

- Publish DWM hooks through the engine-owned `HookTransaction`. Preparation may resolve targets and original bytes but must not mutate them. Queue eager inline, pointer, import and instruction hooks together. Do not create component transactions or restore hooks by hand. End installation before the first redraw so lazy hooks can use independent transactions, and keep that redraw under the DWM lock.
- Hooks sharing one typed Symbol share its generated physical dispatcher and logical replacement chain. Never attach independent SlimDetours hooks to the same Symbol address. Retain transaction deduplication and ownership-conflict detection.
- Every DWM replacement participates in `HookRundown`. Shutdown closes admission before its single lock-free `DwmFlush`, removes hooks under the DWM lock, releases the lock and drains active calls before freeing resources. Never wait for rundown or call `DwmFlush` while holding `s_csDwmInstance`, or use `SwitchToThread` as an unload barrier.
- Legacy must not call `ForceRender`. Preserve custom-theme notification and unload ordering documented in [runtime lifecycle](OpenGlass/Runtime.design.md#engine-lifecycle).
- Ownership conflicts, unexpected instruction bytes, rundown timeout and hook-transaction failure are FailFast conditions. Unsupported builds and unresolved active Required projections remain inert before hooks are installed.
- A correct function-pointer ABI alone does not prove detour safety. Audit every direct caller and intervening wrapper for private register or EFLAGS dependencies on the exact binary. Preserve verified dependencies in the narrow physical dispatcher through replacement and rundown. Do not disguise them as signature changes or generalize across builds.
- IDA audit requests must not modify or save databases. Use explicit verified session IDs, serialize multi-sample audits, and never save or close a pre-existing user session as cleanup. Follow the skill's session workflow.
- Preserve emergency recovery (`Ctrl`+`Win`+`Shift`+`Q`) when touching runtime code.
