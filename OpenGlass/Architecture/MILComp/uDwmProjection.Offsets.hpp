#pragma once
#include "ProjectionHelper.hpp"

namespace OpenGlass::uDWM
{
	using namespace DWM;

	struct ACCENT_POLICY;
	struct CAccent;
	struct CBaseGeometryProxy;
	struct CBitmapSource;
	struct CCompositor;
	struct CContainerVisual;
	struct CGlassColorizationResources;
	struct CImage;
	struct CLegacyNonClientBackground;
	struct CLivePreview;
	struct CLivePreviewTimeline;
	struct LivePreviewBrush;
	struct LivePreviewVisual;
	struct LivePreviewResource;
	struct CNineGridVisual;
	struct CRectangleVisual;
	struct CResourceProxy;
	struct CSolidRectangleVisual;
	struct CTopLevelWindow;
	struct CVisual;
	struct CVisualProxy;
	struct CWindowBorder;
	struct CWindowData;
	struct CWindowList;
	struct CDWriteText;
	struct IText;
	struct VisualCollection;

	using CVisual_GetTransformParent_t = CVisual* (*)(const CVisual*);

} // namespace OpenGlass::uDWM

#include "udwm.Layouts.generated.hpp"
