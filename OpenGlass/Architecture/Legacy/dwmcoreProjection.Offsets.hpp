#pragma once
#include "ProjectionHelper.hpp"

namespace OpenGlass::dwmcore
{
	using namespace DWM;

	struct CArrayBasedCoverageSet;
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
