#pragma once
#include "ProjectionHelper.hpp"

// ============================================================================
// uDWM Offset Stability Rules
// ============================================================================
// uDWM.dll is NOT recompiled for every Windows build. Offsets only change at
// recompile boundaries (codebase generations):
//
//   17763 — Windows 10 1809 (minimum supported)
//   18362 — Windows 10 1903, 1909
//   19041 — Windows 10 2004, 20H2, 21H1, 21H2, 22H2
//   20348 — Windows Server 2022
//   22000 — Windows 11 21H2 RTM (Build 22000)
//   22621 — Windows 11 22H2, 23H2
//   26100 — Windows 11 24H2, 25H2
//   28000 — Windows 11 26H1+ (MIL infrastructure removed)
//
// Feature gates:
//   GetSystemBackdropType → 21H2+
//   GetDWriteTextVisual_Index → 22H2+ (appears with CDWriteText)
//   _OnLegacy variants → 22H2+ (CLegacyNonClientBackground introduced)
//   CWindowBorder / GetWindowBorder_Index → 21H2+ (NOT 22H2+)
//   CAnimatedGlassSheet → pre-21H2; replaced by CAcrylicSheet in 21H2+
//
// Interval semantics: until<BUILD, REVISION>() supplies an exclusive RIGHT
// boundary. otherwise() is the explicit open-ended fallback. If otherwise()
// is absent, later versions are unsupported.
// ============================================================================

namespace OpenGlass::uDWM
{
	using namespace DWM;

	struct ACCENT_POLICY;
	struct CAccent;
	struct CBaseGeometryProxy;
	struct CBaseLegacyMilBrushProxy;
	struct CAtlasButton;
	struct CAtlasedImage;
	struct CAtlasedRectsVisual;
	struct CBitmapSource;
	struct CBitmapSourceArray;
	struct CCanvasVisual;
	struct CCompositor;
	struct CDWriteText;
	struct CGlassColorizationResources;
	struct CImage;
	struct CLivePreview;
	struct CRectangleGeometryProxy;
	struct CRenderDataInstruction;
	struct CRenderDataVisual;
	struct CResourceProxy;
	struct CRgnGeometryProxy;
	struct CSolidColorLegacyMilBrushProxy;
	struct CText;
	struct CTimeline;
	struct CTopLevelWindow;
	struct CVisual;
	struct CVisualProxy;
	struct CWindowBorder;
	struct CWindowData;
	struct CWindowList;
	struct IText;
	struct LivePreviewResource;
	struct LivePreviewVisual;
	struct VisualCollection;

} // namespace OpenGlass::uDWM

#include "udwm.Layouts.generated.hpp"
