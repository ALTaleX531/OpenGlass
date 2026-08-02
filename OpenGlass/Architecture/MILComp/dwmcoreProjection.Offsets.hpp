#pragma once
#include "ProjectionHelper.hpp"

namespace OpenGlass::dwmcore
{
	using namespace DWM;

	struct CArrayBasedCoverageSet;
	struct CD2DContext;
	struct CD3DDevice;
	struct CColorBrush;
	struct CDrawListCache;
	struct CDrawingContext;
	struct CMatrixStack;
	struct CMILMatrix;
	struct COcclusionContext;
	struct CZOrderedRect;
	struct ID2DContextOwner;
	struct IDeviceTarget;
	struct IDeviceTexture;
	struct RenderTargetInfo;

	using CColorBrush_AddOcclusionInformation_t = HRESULT (*)(CColorBrush*, COcclusionContext*, const D2D1_SIZE_F&);
	using CColorBrush_Draw_t = HRESULT (*)(CColorBrush*, CDrawingContext*, const D2D1_SIZE_F&, CDrawListCache*);
	using ID2DContextOwner_GetCurrentZ_t = UINT (*)(const ID2DContextOwner*);
	using ID2DContextOwner_GetCurrentRenderTargetInfo_t = const RenderTargetInfo& (*)(const ID2DContextOwner*);
	using IDeviceTarget_GetRenderTargetView_t = ID3D11RenderTargetView* (*)(const IDeviceTarget*);
	using IDeviceTexture_GetTexture2D_t = ID3D11Texture2D* (*)(const IDeviceTexture*, UINT*);
	using IDeviceTexture_GetShaderResourceView_t = ID3D11ShaderResourceView* (*)(const IDeviceTexture*);

} // namespace OpenGlass::dwmcore

#include "dwmcore.Layouts.generated.hpp"
