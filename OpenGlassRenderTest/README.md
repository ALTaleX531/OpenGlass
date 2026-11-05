# Glass renderer comparison

OpenGlassRenderTest compares Aero glass rendering across four implementations using identical PNG background input and window-frame geometry. Defaults are blur **3**, radius **6**, BGRA8, and **Speed**.

| Method | Implementation | Background input |
| --- | --- | --- |
| `d2d` | Production `CGlassRealizer` and `CGlassEffectBatch`: input preparation, horizontal blur, vertical blur with colorization, and composition across four stages and two scratch buffers, retaining native draw selection. | Production shared-atlas selection and cost-based regional copies, preserving the source subresource. |
| `rects` | Reference pre-batching D2D: production `CAeroEffect` drawn once per rectangle. | Single-rectangle shared input, `<= 12` regional-copy threshold, and post-copy D3D Flush. |
| `d3d` | Production `CD3DGlassRealizer`: quarter-width horizontal pass, vertical blur, and Aero colorization at fixed **sigma 3**, independent of D2D blur and optimization. | Bitmap-derived views and direct SRV sampling, including selected-slice array views. RT-only targets use production regional copies. |
| `gaussian` | Native D2D Scale (0.5, 0.5) → Crop/Mirror → GaussianBlur (`blurAmount`) → Aero colorization → 2× output transform | Per-rectangle capture matching rects, with copy expansion accounting for 2x full-resolution blur reach |

The executable links production realizers, batching, copy helpers, effects and `CustomBlurEffect`. The pre-batching submission and built-in Gaussian graph stay local in `Renderer.cpp`. Shader binaries compile under its `IntDir` and are embedded as resources. Source, shared-include and build-setting changes regenerate them. Unchanged resources are reused. Test controls remain in this project.

Production D2D and rects use configured blur amounts, whereas production D3D uses a fixed sigma of 3. Gaussian applies the configured standard deviation directly to the half-size image and upsizes 2x, yielding a nominal full-resolution sigma of **2 × blurAmount**. All methods use identical Aero color, afterglow, blur balance, and fallback settings. Reports record method parameters and Gaussian input/output scales.

`OpenGlassRenderTest` loads `OpenGlassRenderTest.png` once through WIC and composites it at native pixel size from the top-left over opaque black, independent of PNG DPI metadata. Target bounds clip larger images, while smaller images leave black outside. Recreating a target for dimension, format, or copy-policy changes reuses the loaded image and prepares a background texture matching the target size and format. All four implementations start every iteration with a copy of this texture. Image loading, background preparation, and reset copies are excluded from both CPU and GPU timing. RT-only targets use a separate sampleable bitmap for the interactive preview, and that copy also stays outside timing.

Window-frame rectangles are generated from `CreateRoundRectRgn` and `GetRegionData` with a client cutout and optional simulated occlusion. Resources reside on the active GPU, with each implementation maintaining its reusable rendering structures. Format changes also reset renderer resources.

## Build and use

Follow the [build prerequisites](../AGENTS.md#build-and-verification), then build `Release|x64`:

```powershell
msbuild OpenGlassRenderTest/OpenGlassRenderTest.vcxproj /m /p:Configuration=Release /p:Platform=x64
Set-Location OpenGlassRenderTest
../Build/x64/Release/common/OpenGlassRenderTest.exe
```

Place `OpenGlassRenderTest.png` in the working directory. Run the commands below from the OpenGlassRenderTest source directory.

| Key | Action |
| --- | --- |
| `1` / `2` / `3` / `4`, Space | Select D2D, D2D rects, D3D, or Gaussian, or cycle implementations. |
| `A` | Cycle implementations every three seconds. |
| `R`, `O` | Cycle corner radius (0, 6, 30) and occlusion levels (0, 1, 2). |
| `F` | Toggle BGRA8 and FP16 render targets. |
| `C` | Use an RT-only texture and a non-sampleable D2D target wrapper to exercise copied input in all four paths. |
| `Q`, `+` / `-` | Cycle optimization modes (Speed, Balanced, Quality) or adjust D2D blur amount. |
| `P`, `S` | Toggle half-pixel coordinate alignment or reset statistics. |

Statistics track CPU and GPU metrics across all four implementations. Configuration adjustments reset warmup cycles and sample accumulators. Letter shortcuts require English keyboard input. FP16 targets display via the BGRA8 preview surface.

## Repeatable measurements

```powershell
../Build/x64/Release/common/OpenGlassRenderTest.exe --benchmark ../.archive/compare.json --width 1920 --height 1080 --blur 3 --radius 6 --frames 120 --warmup 12 --validate
../Build/x64/Release/common/OpenGlassRenderTest.exe --benchmark ../.archive/cpu.json --cpu-only --adapter 1 --format fp16 --blur 30 --copy
```

`--benchmark` runs offscreen benchmarks and emits JSON reports. Methods execute in a reproducible, interleaved order on a single-threaded D3D11 device, matching DWM's device contract. `--frames` sets measured iterations per method. `--warmup` sets discarded initial frames. `--adapter` selects the target DXGI adapter. `--mode d2d|rects|d3d|gaussian` sets the initial interactive implementation. Offscreen `--benchmark` and finite `--capture` run all four implementations. `--help` lists available flags.

- **CPU**: Measures caller `Flush`, renderer execution, and `EndDraw`, encompassing effect configuration, copy passes, batch scheduling, and composition. Background resets, query operations, buffer readbacks, UI overlays, and `Present` calls are excluded. `--cpu-only` disables timestamp queries. Each offscreen four-method round retires outside CPU timing, including CPU-only runs.
- **Target views**: The benchmark prepares the standalone `CGlassRenderTarget` when creating a target and reuses its views during measurements, passing the texture, subresource, RTV, and SRV to the production realizer. DWM instead maintains a record per `DrawVisualTree`, creating views on the first Direct3D glass draw and releasing them upon returning from the tree.
- **GPU span**: Uses D3D11 timestamps around the rendering interval and includes submission and CPU-feed idle gaps. Reports record median, P95, valid sample counts and disjoint/missed counts. Timing statistics use valid samples.
- **Pixels**: `--validate` evaluates full-frame maximum absolute error, RMSE, and changed channel counts comparing production D2D, D3D, and Gaussian against reference D2D rects outside timed passes.
- **Graphics capture**: `--capture <count> --warmup <count>` opens a finite sequence annotated with `D2D`, `D2D rects`, `D3D`, and `Gaussian`. Background reset, UI overlays, and presentation occur outside annotated regions. Use a graphics debugger to collect GPU draw calls within these annotations.

Store capture data and output reports in `.archive/`. Keep GPU adapter, resolution, format, geometry, blur settings, and copy policies consistent during comparative runs. See the [test guide](../OpenGlassTests/README.md#isolation-and-environment-boundaries) for isolated resource and sampling probes. Target-system tests exercise desktop integration, protected targets, HDR presentation, clone displays, and runtime unload within DWM.
