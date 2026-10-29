# DWM runtime design contracts

This document records component integration and rendering constraints. Repository-wide architecture, safety and verification rules remain in [AGENTS.md](../AGENTS.md). Read the relevant section before changing its consumers.

## Shared and private ownership

`OpenGlass.cpp`, `GlassEngine.cpp` and `GlassCoverageSet.hpp/.cpp` own the common runtime lifecycle, engine sequencing and coverage storage. Project include paths select architecture-private projection headers. `dwmcoreProjection.hpp` supplies the typed `GlassOcclusionContext`/`IsGlassOccluded` facade. Legacy delegates to `CArrayBasedCoverageSet::IsCovered`, while MILComp delegates to `COcclusionContext::IsOccluded` with `ignoreDeviceTransform=true`. Keep projection types and private rendering implementations architecture-specific.

`GlassKernel.hpp/.cpp` shares window-list traversal, blur expansion, colorization and reflection calculations, including viewport placement, RTL mirroring, parallax and scale. Architecture-private `GlassInfra.hpp/.cpp` owns single-window redraw, hooks, device resources, private window state and the Update/Startup/Shutdown/Cleanup entry points in `OpenGlass::GlassInfra`. Keep settings side effects and lifecycle sequences intact. Consumers include `GlassKernel.hpp` for shared calculations and `GlassInfra.hpp` for architecture integration.

## Engine lifecycle

The three original MILComp `ForceRender` call sites in `GlassEngine.cpp` are a narrow compile-time exception to the shared-business-code rule, and Legacy compiles them out. Update calls it only when `redrawNow` is true, under the DWM lock. Startup calls it after hook installation and lock release. Shutdown calls it after rundown, cleanup and lock release. Keep the lifecycle flow shared without an empty projection adapter or duplicated skeleton.

Hook publication, the first redraw and rundown follow the [DWM safety contract](../AGENTS.md#safety). Keep component sequencing in the shared `GlassEngine` rather than duplicating it in individual handlers.

Custom-theme shutdown preserves the lock-free synchronous `SendMessageW(WM_THEMECHANGED)` before `UnloadTheme`, after removing its delay-IAT hooks and the OpenGlass window procedure. Preserve the remaining component cleanup/redraw order in `GlassEngine::Shutdown`.

## Window frame bounds

Each architecture has its own `GlassFrameIntegrity` to align capture borders, thumbnails, and transitions with the themed frame, independently of the selected glass effect. Margin calculations use `GetActualWindowRect` with `excludeShadowMargins=true` without hooking that function or altering arguments for other callers. The modern `GetBorderRect` hook computes atlas-aware shadow-free bounds directly to avoid recursion through `GetActualWindowRect`, matching the native legacy calculation. Relative coordinates, screen coordinates, and native maximized frame margins remain independent of appearance overrides.

Capture borders on Legacy builds below 26100, including Server 2022, use `GetBorderMargins`. Later capture borders use `GetBorderRect`. Windows 11 uses `GetFrameMargins` for thumbnail and animation margins. They use one consistent frame rectangle for sizes and offsets. Native clone margin snapshots remain frozen and retain native ownership. Shared engine hook transactions and rundown still apply.

Integrity is the outermost node in the existing `ValidateVisual` chain. It records the live owner's frame and inside margins before calling the rest of the chain, compares them only after successful validation, and calls `CWindowData::OnWindowSizeUpdated` if the final frame changed or Windows 11 inside side/bottom margins changed. Inside top margins do not participate in the atlas exclusion, so a top-only change does not send this notification. Clone trees borrowing the owner's data do not notify that owner. The projection border is a sibling and reverse traversal may already have visited it, so it is validated immediately after the notification. MILComp validates the native border container, including its feature-selected child.

Windows 11 `UpdateWindowVisuals` sets the `CWindowBorder` content clip before realizing atlas sizes. Restoring a system-backdrop window can change inside margins and atlas sizes together while leaving the final border rectangle unchanged. Rectangle comparison alone then misses the stale intermediate clip. The existing size notification resets `CWindowBorder::SetBorderRect` after layout, updating the native content clip alongside capture borders and secondary representations.

Loading OpenGlass or changing the theme or atlas uses normal native layout validation. On unload the comparison hook is gone, so the shared window traversal explicitly sends `OnWindowSizeUpdated` in the final redraw, under the existing DWM lock after hook removal and rundown. It does not replay settings notifications or recursively rebuild source windows. There is no per-window map, deferred queue, extra frame request, or new resource ownership or cleanup. `deepRedraw` has the separate purpose of clearing extra reflection instructions on startup and shutdown.

## Window appearance state

Appearance consumers use shared `GlassKernel::GetWindowStatus`. `WindowState.hpp` reads `OpenGlass.ActivationOverride` and `OpenGlass.MaximizationOverride` directly with `GetPropW`. A value of 1 forces false and 2 forces true. Zero, absence, and other values retain the native state and the transition-on-maximized rule, though explicit false overrides that transition rule.

Do not enumerate window properties in the DWM update path to distinguish stored zero from absence. Preserve the native active flag supplied by colorization callbacks when no property overrides it. These properties affect appearance only, require an actual redraw to update existing visuals, and do not change geometry or Accent eligibility. Keep native projection wrappers pure and actual maximized-layout checks separate.

Accent owns a separate brush from non-client/client blur. Updating one must not suppress the other's appearance-state refresh.

## Caption helpers

`CaptionMetrics.hpp/.cpp` owns caption-button size calculations, while `CaptionText.hpp/.cpp` owns centering, clone-state operations, glow-parameter and DWrite image-bound calculations. Each architecture retains its handler files, settings, resources, transient visual/size unions and state-map instance.

MILComp applies atlas `CaptionHeight` to native button sizes in its existing non-client layout hook. `CaptionButtons` styles retain their explicit sizes. Do not hook its no-op button size notification or infer it from an adjacent offset slot.

Glow calculation updates the caller's parameters in place, retaining fields unused by the selected mode. The shared engine still coordinates the established lifecycle.

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
