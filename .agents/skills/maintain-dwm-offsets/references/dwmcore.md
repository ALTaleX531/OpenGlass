# dwmcore evidence guide

Start from the target schema item's `notes`; they are routing hints, not proof. Gate each class independently.

## High-value anchors

| Area | Primary evidence | Independent checks and cautions |
|---|---|---|
| `COcclusionContext` | `IsCurrent` comparing a member with the global frame ID | Constructor; optimized-rect paths when `IsCurrent` is absent |
| Legacy MIL brushes | `TryDrawCommandAsDrawList`; legacy brush update/setter functions | Channel update dispatcher; absence only gates brush-related projections |
| `CDrawingContext` | `Create` allocation and vtable assignments | Constructor and interface methods; account for adjusted `this` |
| World transform | `GetWorldTransform3x2` or the active equivalent | Add the interface-subobject displacement to decompiler-relative access |
| `CDirtyRegion` | Optimized-rect and occlusion call paths | A context passed as a parameter is not a stored member |
| `CD3DDevice`/targets | Constructors, interface vtables, target creation | Do not reuse a historical slot solely because neighboring thunks match |

## Semantic gates

- Loss of `TryDrawCommandAsDrawList` or legacy brush symbols supports removal of that brush path, not total dwmcore removal.
- A change in `CDrawingContext::Create` vtable assignments can indicate interface absorption or subobject movement. Verify affected methods individually.
- Object allocation size is supporting context only; it cannot map members by itself.
- Correlated `COcclusionContext` shifts are a useful anomaly check, not independent proof for every member.

## Fallbacks

- If `COcclusionContext::IsCurrent` is absent, inspect `GetOptimizedRect`-style functions for the comparison between a context member and the composition frame ID.
- In that comparison, derive the projection from the `COcclusionContext` side. The `g_pComposition` frame-ID slot is only an anchor and can move independently.
- For pre-template brush builds, use the monolithic `ProcessUpdate`; for template-generated builds, use individual setters plus their dispatcher.
- If a generated brush setter loses its useful name, use the small `CResource::OnPropertyChanged` callee to recover the setter family.
- When a vtable symbol is folded, read constructor assignments and slot targets, then report unresolved ICF instead of selecting a convenient symbol.

## Symbol-loss semantic anchors

- Identify `CArrayBasedCoverageSet::Add` from the `CZOrderedRect` construction, `UpdateDeviceRect`, and `DynArray::AddMultipleAndSet` sequence.
- Identify `CMILMatrix::Multiply` from its 4-by-4 floating-point matrix multiplication loop.
- Identify `CMatrixStack::Push` from the stack write followed by count growth, then use it to route back to the world-transform path.
- Route shared `CDrawingContext` layout through a discoverable `CGlobalDrawingContext::Create` when shape/subdrawing variants have no independent factory.

Treat every algorithmic anchor as candidate navigation and verify it in the exact binary.

## Legacy brush relationships

`CImageLegacyMilBrush` and `CSolidColorLegacyMilBrush` share opacity, float-resource, viewport, and viewbox layout through `CLegacyMilBrush`. Realized color belongs to the solid-color path and requires its own evidence. When `CSolidColorLegacyMilBrush::GetRealizedColor` is inlined, start at `TryDrawCommandAsDrawList`, locate the call path to `CRenderData::DrawSolidColorRectangle`, and interpret the nearby brush read as `D3DCOLORVALUE`; confirm it through another solid-color producer or consumer before accepting it.

## Removal decisions

To end a Layout without an open-ended case, demonstrate that the semantic member/interface is absent from the relevant class or that the consumer path no longer exists. Failure to find one decorated name is insufficient.
