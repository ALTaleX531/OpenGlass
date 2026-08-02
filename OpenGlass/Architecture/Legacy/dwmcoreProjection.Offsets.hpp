#pragma once
#include "ProjectionHelper.hpp"

// ============================================================================
// dwmcore Offset Stability Rules
// ============================================================================
// dwmcore.dll is recompiled at major build boundaries. Offsets only change at
// these recompile points:
//
//   17763 — Windows 10 1809 (minimum supported)
//   18362 — Windows 10 1903, 1909
//   19041 — Windows 10 2004, 20H2, 21H1, 21H2, 22H2
//   20348 — Windows Server 2022
//   22621 — Windows 11 22H2, 23H2
//   26100 — Windows 11 24H2, 25H2
//   28100 — Windows 11 26H1+ (MIL brush infrastructure partially removed)
//
// 26H1+ Gate: MIL brushes (CImageLegacyMilBrush, CSolidColorLegacyMilBrush,
//   CLegacyMilBrush) are REMOVED. IDrawingContext interface absorbed/removed.
//   COcclusionContext, CDrawingContext core, CD3DDevice still exist.
//   Always gate each class — do NOT assume existence.
//
// Tier 1 — COcclusionContext is the CANARY class: all 6 offsets are the most
// volatile across CUs. If they change, they tend to shift together by the same
// byte delta. Check COcclusionContext FIRST before auditing any other class.
//
// Tier 2 — CDrawingContext WorldTransform shifts between major builds.
//   CRenderData_GetResources grows ~8 bytes per major version.
//
// Tier 3 — Brush family stable within a recompile boundary (but removed 26H1+).
//   CShape vtable slots: consistent across derived classes, stable across
//   builds.
//
// Tier 4 — CD3DDevice/CD2DContext: historically frozen across all Win11 builds.
//
// Interval semantics: until<BUILD, REVISION>() supplies an exclusive RIGHT
// boundary. otherwise() is the explicit open-ended fallback. If otherwise()
// is absent, later versions are unsupported; that does not by itself prove
// the member or owning class was removed.
// ============================================================================

namespace OpenGlass::dwmcore
{
	using namespace DWM;

	struct CArrayBasedCoverageSet;
	struct CCachedVisualImage;
	struct CComposeTop;
	struct CD2DContext;
	struct CD3DSurface;
	struct CD3DDevice;
	struct CDrawingContext;
	struct CFloatResource;
	struct CGeometry;
	struct CImageSource;
	struct CLegacyMilBrush;
	struct CMatrixStack;
	struct CMILMatrix;
	struct COcclusionContext;
	struct CResource;
	struct CShape;
	struct CTreeDirty;
	struct CVisual;
	struct CVisualTree;
	struct CHwndRenderTarget;
	struct CDirtyRegion;
	struct CRegion;
	struct CZOrderedRect;
	struct CZOrderedRect2;
	struct ID2DContextOwner;
	struct IDeviceTarget;
	struct IDeviceTexture;
	struct IDrawingContext;
	struct IRenderTarget;
	struct RenderTargetInfo;

	using IDrawingContext_DrawGeometry_t = HRESULT (*)(IDrawingContext*, CLegacyMilBrush*, CGeometry*);

	using CRenderDataResourceArray = DynArray<CResource*>;

} // namespace OpenGlass::dwmcore

#include "dwmcore.Layouts.generated.hpp"
