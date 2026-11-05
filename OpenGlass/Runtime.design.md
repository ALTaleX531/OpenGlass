# DWM runtime design contracts

This document describes component responsibilities, rendering flow and lifetime contracts. Exact binary ranges and semantic evidence belong in projection descriptors and code comments. Investigation and acceptance records belong in `.archive/`. Repository-wide architecture, safety and verification rules remain in [AGENTS.md](../AGENTS.md).

## Shared and private ownership

`OpenGlass.cpp`, `GlassEngine.cpp` and `GlassCoverageSet.hpp/.cpp` own the common runtime lifecycle, engine sequencing and coverage storage. Project include paths select architecture-private projection headers. `dwmcoreProjection.hpp` supplies the typed `GlassOcclusionContext`/`IsGlassOccluded` facade. Legacy delegates to `CArrayBasedCoverageSet::IsCovered`, while MILComp delegates to `COcclusionContext::IsOccluded` with `ignoreDeviceTransform=true`. Keep projection types and private rendering implementations architecture-specific.

`GlassKernel.hpp/.cpp` shares window-list traversal, blur expansion, colorization and reflection calculations, including viewport placement, RTL mirroring, parallax and scale. Architecture-private `GlassInfra.hpp/.cpp` owns single-window redraw, hooks, device resources, private window state and the Update/Startup/Shutdown/Cleanup entry points in `OpenGlass::GlassInfra`. Keep settings side effects and lifecycle sequences intact. Consumers include `GlassKernel.hpp` for shared calculations and `GlassInfra.hpp` for architecture integration.

## Engine lifecycle

The three original MILComp `ForceRender` call sites in `GlassEngine.cpp` are a narrow compile-time exception to the shared-business-code rule, and Legacy compiles them out. Update calls it only when `redrawNow` is true, under the DWM lock. Startup calls it after hook installation and lock release. Shutdown calls it after rundown, cleanup and lock release. Keep the lifecycle flow shared without an empty projection adapter or duplicated skeleton.

Hook publication, the first redraw and rundown follow the [DWM safety contract](../AGENTS.md#safety). Keep component sequencing in the shared `GlassEngine` rather than duplicating it in individual handlers.

Custom-theme shutdown preserves the lock-free synchronous `SendMessageW(WM_THEMECHANGED)` before `UnloadTheme`, after removing its delay-IAT hooks and the OpenGlass window procedure. Preserve the remaining component cleanup/redraw order in `GlassEngine::Shutdown`.

## Background copies

Both renderers resolve the current target bitmap through `Util::GetTargetBitmapFromD2DContext`. `Util::GetTextureFromD2DBitmap` obtains the parent texture and selected subresource through `IDXGISurface2::GetResource`, because a subresource surface is a separate object and cannot be queried directly for its parent texture.

`GlassRenderTarget.hpp/.cpp` owns the bitmap, texture, and selected-subresource views. Each architecture's `GlassIntegrity` maintains the current `CGlassRenderTarget` as a stack of function-local records around `DrawVisualTree`. `GlassRenderer` prepares the current record on first Direct3D use, creating views for the selected mip and array slice in the bitmap's typed format. Subsequent glass draws reuse those views, while a target change replaces them. Returning from a nested tree restores the parent record and releases the child references. This scope also applies when SafetyZone is disabled or saves no strips. Direct2D-only draws create no views.

Entering the tree pushes only an empty record because `CDrawingContext` selects its D2D target lazily. `GlassRenderer` resolves the bitmap in `FillGeometry`, after native render-state application, `EnsureBeginDraw`, and the caller's D2D flush. It passes the texture, subresource, RTV, and SRV explicitly to `CD3DGlassRealizer`, which borrows them for that call. The realizer has no dependency on the target record or `GlassIntegrity`.

Direct3D renders only the subresource selected by the current bitmap. Array targets use matching single-slice target and source views. The existing `Texture2D` shaders can read a single-slice array SRV. Shader-visible array length comes from the view. See the [resource-view and shader declaration rules](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#inst_tDCL). The target helper creates an SRV for textures with `D3D11_BIND_SHADER_RESOURCE`. The realizer samples the supplied view directly. A null SRV selects a subresource backdrop copy into an ordinary 2D input. Other slices retain their contents. The realizer clears borrowed bindings before restoring DWM's context state, and returning from the visual tree releases the views and target references. Native DWM view fields are not accessed.

Rendering dimensions belong to the selected subresource. Direct3D derives its extent from the mip index, while Direct2D uses the bitmap's pixel size for capture and scratch resources. Copy boxes, viewports, and sampling coordinates use that same extent.

Direct2D retains shared-atlas sampling when the bitmap and selected drawing path allow it, and the shared bitmap preserves the source subresource. Regional captures copy from that same subresource into an ordinary 2D texture. Capture textures retain the source's hardware-protection setting, keeping protected content in protected resources. Composition stays on the caller's original target with its depth and clip. Texture metadata and the subresource index come from the bitmap when capture or batching needs them.

`CGlassSafetyZoneLayer` saves the four strips around a dirty rectangle before the expanded visual-tree draw, then restores them after the caller flushes that draw. Direct texture copies use the same target subresource in both directions. The two strip buffers inherit target protection and remain in the device-context pool. The target reference and subresource index last from Push to Pop, and failed preparation releases the target reference. Nested draws acquire separate layers.

`BackdropCopy.hpp` declares `CoalesceCopyRectangles`, implemented in `BackdropCopy.cpp` and shared across both realizers. Callers expand, clip, and round source boxes outward to integer texels using the actual occlusion and clipping boundaries. The helper merges touching or overlapping boxes when combined transfer-area and command cost decreases, then selects a single bounding copy when cheaper.

Drawing rectangles describe disjoint coverage in native traversal order. `PlanBands` sums their clipped areas directly. Expansion, outward rounding, and bounding-box merges can produce overlapping work ranges, and merge costs account for the area processed by those ranges.

For Direct2D captures, `CGlassRealizer` uses `GetBlurExpansion()` for sampling bounds and background copies to match dirty-region tracking. It intersects drawing rectangles with drawing bounds, expands them, clips them to the effect sampling bounds, and rounds outward to source texels before coalescing. Sampling bounds are clamped to the geometry bounding box and source bitmap. Crop defines the content region, and Border mirrors that content at its edges. The effect input rectangle and final drawing geometry remain unchanged.

Copies execute on the immediate context ahead of dependent effects. The Direct2D path uses the caller's pre-copy `ID2D1DeviceContext::Flush` to materialize the background before capture. Direct3D command submission follows standard caller and runtime scheduling.

Each Direct3D realizer owns a device-context state object using DWM's single-threaded device contract. After the caller flushes Direct2D, glass drawing switches into that state and restores the previous state on return, including failure returns. Native switching preserves pipeline bindings, constant-buffer ranges and hardware protection. Before switching back, unbind the borrowed output target and input view so the cached state retains only realizer-owned resources. Renderer replacement and device cleanup release the state with the realizer.

## Direct2D effect batching

`CGlassEffectBatch` (`GlassEffectBatch.cpp`) manages scheduling resources for banded blur across four stages using two scratch buffers, while `CustomBlurEffect` computes blur mathematics and shaders. The composition buffer receives Scale/Crop/Border output. Horizontal blur consumes that input and writes to the second buffer. Vertical blur and colorization then reuse the composition buffer, and the result is composed into the original rectangles. The batch holds COM references to graph nodes owned by `CAeroEffect`/`CBlurEffect` and restores connections before subsequent builds or native draws.

The final effect build determines whether to use native drawing or banded batching. Background copies precede drawing, while banded rendering plans stages and manages resources using a snapshot of the effect output, transform, and offset.

- Callers route zero-blur operations to solid-color rendering. The realizer processes nonzero blur and plans bands for multi-rectangle effects with non-unit output scale. Unit-scale output, single rectangles, full-coverage tiles, and plans that retain the original band count use the native path.
- Scratch-stage footprints derive from native kernel required-input queries, cached while blur amount, optimization, and output scale remain unchanged. Banded draws prepare these footprints. Background copies and native draws bypass them. The effect's input rectangle and output offset remain authoritative.
- Non-negative scratch ranges retain absolute coordinates. Integer-translated bitmap views handle negative sample reach and preserve the sampling grid, keeping positive ranges free of additional translation effects.
- Scratch rendering uses a separate pixel-unit context. Composition retains the caller's target, depth and clip, and restores its transform, blend mode and antialiasing state. Scratch targets are detached before parent composition samples them, including on `EndDraw` failure.
- Composition uses the original rectangles under the caller's clip. Geometry and curve masks belong to that final composition.
- Scratch textures allocate on demand to match source format, sample description, miscellaneous flags, and hardware protection settings. Effect changes release graph references while preserving reusable scheduling resources. Context destruction, renderer replacement, and rundown cleanup free these resources.
- Keep copies, passes and composition asynchronous, with command submission owned by the caller/runtime.
- Texture and bitmap metadata are queried directly from owned resources. Planner extents, origins, and kernel-cache keys are stored in scheduling state.
- `CustomBlurEffect::Reset` disconnects inputs while retaining the cached image pointer. Input callers recreate bitmap wrappers to trigger graph reconnection on unchanged frames. Scratch callers reuse compatible wrappers.

## Legacy overlay candidates

Keep swap-chain content that contributes to visible blur in the composed desktop. `GlassIntegrity` selects overlay hooks for the module's verified ABIs. That selection must cover the relevant native candidate paths. Compare clipped candidate bounds with glass coverage in the same device space and at the current depth. Reject only overlapping candidates while blur is enabled. Preserve native overlay eligibility elsewhere.

The occlusion context is borrowed for the synchronous `Compute` call under the DWM lock, restoring the enclosing context on return. Hook installation, removal and resource cleanup remain engine-owned.

## Injection crash protection

The service validates the DWM process and saves its verified handle and injection time in the per-session map before injection and initialization, so startup failures participate in crash-loop protection. Injection waits for initialization to finish. The crash guard requires the tracked process to exit, excluding a failed load while that process is still running. Preserve per-session timing rules, the session-manager shutdown exception, and Abort, Retry, and Ignore behavior.

## Frame demodernizer patches

Each architecture-private frame demodernizer has a distinct shadow patch and a high-contrast patch. The high-contrast patch replaces only the five-byte call, leaving later caller instructions and the AL test intact. Both expected patches must be located and their original bytes validated before applying them through the engine-owned `HookTransaction`.

## Window frame bounds

`GlassFrameIntegrity` corrects visible-frame consumers without changing native content clips. Keep `GetBorderRect`, `GetFrameMargins` and clone-margin storage native: they also supply `CWindowBorder`, client-area and render-target clipping. This component has no `ValidateVisual` replacement.

Where native visible bounds need correction, scope it to `GetActualWindowRect` requests that respect maximized clipping and exclude shadows. Obtain native bounds with shadow exclusion disabled, then exclude side/bottom shadows using actual atlas sizes, preserving the coordinate space and maximized clipping. Other parameter combinations, including caption layout, stay native, as do queries without the required atlas parts. These queries also serve transitions, Peek and capture metadata. Hook selection follows verified module ranges and native entry-point availability. Paths with native atlas-aware metrics retain them.

Recording borders retain native capture/projection selection, corners and DPI handling. Probe the optional recording-border shim without requiring its presence. Resolve its dependent setter only when selecting that branch. Thumbnail corrections apply to full-window live and snapshot representations while the original owner still supplies frame geometry. Preserve native snapshot dimensions and frozen clone fields, as well as client-only, desktop, explicit-source and maximized contracts. Keep distinct metric ABIs in the projections.

Native recording-border notification precedes layout. `UpdateWindowVisuals` supplies intrinsic atlas sizes, and the later `UpdatePinnedParts` realizes them. After that layout succeeds, refresh only a live owner's existing recording-border visual and the affected thumbnail-source metrics. Submit the border sibling immediately because native traversal may already have visited it. Do not recursively validate the top-level window or send full size notifications during ordinary validation. The final unload redraw sends one native size notification after hook removal and theme deactivation to restore existing borders and thumbnails. `deepRedraw` separately clears extra reflection instructions.

## Window appearance state

Appearance consumers use shared `GlassKernel::GetWindowStatus`. `WindowState.hpp` reads `OpenGlass.ActivationOverride` and `OpenGlass.MaximizationOverride` directly with `GetPropW`. A value of 1 forces false and 2 forces true. Zero, absence, and other values retain the native state and the transition-on-maximized rule, though explicit false overrides that transition rule.

Do not enumerate window properties in the DWM update path to distinguish stored zero from absence. Preserve the native active flag supplied by colorization callbacks when no property overrides it. These properties affect appearance only, require an actual redraw to update existing visuals, and do not change geometry or Accent eligibility. Keep native projection wrappers pure and actual maximized-layout checks separate.

Accent owns a separate brush from non-client/client blur. Updating one must not suppress the other's appearance-state refresh.

## Caption helpers

`CaptionMetrics.hpp/.cpp` owns caption-button size calculations, while `CaptionText.hpp/.cpp` owns centering, clone-state operations, glow-parameter and DWrite image-bound calculations. Each architecture retains its handler files, settings, resources, transient visual/size unions and state-map instance.

MILComp applies atlas `CaptionHeight` to native button sizes in its existing non-client layout hook. `CaptionButtons` styles retain their explicit sizes. Do not hook its no-op button size notification or infer it from an adjacent offset slot.

Glow calculation updates the caller's parameters in place, retaining fields unused by the selected mode. The shared engine still coordinates the established lifecycle.

DirectWrite captions can contain explicit line breaks even with word wrapping disabled. Trimming detection reads every line, retaining a stack-only path for ordinary single-line titles. A trimmed line disables caption centering. Each draw recomputes that decision from the current layout. `E_NOT_SUFFICIENT_BUFFER` requests a larger line-metrics array. This is not a caption-rendering failure.

## MILComp Aero Peek

MILComp `GlassReflectionHandler` presents native ordinary and glass-only clone captures with the native timeline, reversal, and cleanup. Glass-only clones exclude non-client and client blur. Do not replace the glass capture with a live blur sheet. `GlassFrameEnhancer` retains ordinary frames and clones glass-only squeegee parts as native nine-grid primitives, because ThemePart::Generate otherwise suppresses their atlas drawing. Keep atlas pixels, opacity, margins, and layout intact without modifying the source primitive or inventing a part ID.

Release both capture brushes and surfaces before resource updates. Native brush reset retains the frozen surface, and its old capture bounds must not pair with a new display rectangle. Attach per-monitor display visuals after the complete native resource update, because temporary resource destruction and vector relocation remove their parent links. Reflection is a separately clipped child outside the captured source. Keep `CAcrylicSheet` independent of this path.

## MILComp drawing bounds

MILComp `CDrawingContext::DrawVisualTree` consumes a 24-byte `TMil3DRect`, including front and back depth. The safety-zone hook expands only its XY rectangle and preserves both depth coordinates, including offscreen visual-surface captures. Never forward a 16-byte `D2D1_RECT_F` in its place.

## MILComp Accent and client blur

MILComp `AccentOverrider` keeps native accent policies and selects the native solid-fill path when overriding blur or acrylic. Keep primary and transition policy and opacity independent across layers. The glass child reproduces the nine-grid hollow center and inherits the native clip. Never replace the native nine-grid brush graph. Native cleanup owns background layers, so detach tracked effects on cleanup, disable, and unload. Redraw must call `OnAccentPolicyUpdated` to switch between native acrylic and the replacement.

Existing `GlassFrameHandler::UpdateWindowVisuals` refreshes Accent markers and reflection slots. Do not add another window-validation hook. `CReflectionVisual` owns NonClient, ClientBlur, and Accent slots, maintaining a child per Accent background layer and adopting native-cloned children before creating new ones. `GlassKernel::GetWindowStatus` and `GlassInfra::UpdateGlassMarker` share status encoding and marker construction. Retain the channel hook for native client-blur commands. Accent enables the native maximized-colorization subscription, matching Legacy. The physical acrylic-predicate shim preserves the verified `SetParentVisible` R8/R9 contract through replacement and rundown.

MILComp's existing `UpdateClientBlur` hook marks the retained native clip only after a successful native update. A NULL blur region selects rectangle or window geometry directly, so it must receive the same marker as region-based blur. Read the clip through the typed `CRectangleVisual` projection and preserve it as the marker input. Native rebuilding reselects the clip on each enabled call, so keep wrapping at that boundary to prevent ordinary refreshes from accumulating markers. Reflection slots and native clip or resource ownership remain separate. Client-blur reflection viewport coordinates use the carrier's scale, because DPI-unaware clients can have a scaled carrier while the top-level window remains at unit scale.

## MILComp button glow

MILComp button glow uses native `CImage` children of the two `CButtonVisual` crossfade nodes, retaining native layout, opacity, and subtree cloning. Track image references without retaining their native owners, including copies in previews or animations, and erase tracking on native container destruction.

`GlowCloneContext` holds the source and copied glow for one synchronous clone. The function-local static context pointer shares state across recursive native calls and restores the enclosing context on return. Retain the copied image globally only after its owner succeeds, ensuring a failed clone leaves no raw image pointer for shutdown to dereference.

Retained images bridge owner deletion after rundown closes. Detach remaining images after hook removal and drain, then release the atlas bitmaps.
