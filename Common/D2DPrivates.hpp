#pragma once
#include <d2d1_1.h>
#include <d3d11_4.h>
#include <dxgi1_5.h>

namespace OpenGlass
{
	const GUID CLSID_D2D1DirectionalBlurKernel{ 0x58EB6E2A, 0x0D779, 0x4B7D, { 0x0AD, 0x39, 0x6F, 0x5A, 0x9F, 0x0C9, 0x0D2, 0x88 } };
	enum D2D1_DIRECTIONALBLURKERNEL_PROP
	{
		D2D1_DIRECTIONALBLURKERNEL_PROP_STANDARD_DEVIATION,
		D2D1_DIRECTIONALBLURKERNEL_PROP_DIRECTION,
		D2D1_DIRECTIONALBLURKERNEL_PROP_KERNEL_RANGE_FACTOR,
		D2D1_DIRECTIONALBLURKERNEL_PROP_OPTIMIZATION_TRANSFORM
	};
	enum D2D1_DIRECTIONALBLURKERNEL_DIRECTION
	{
		D2D1_DIRECTIONALBLURKERNEL_DIRECTION_X,
		D2D1_DIRECTIONALBLURKERNEL_DIRECTION_Y
	};
	enum D2D1_DIRECTIONALBLURKERNEL_OPTIMIZATION_TRANSFORM
	{
		D2D1_DIRECTIONALBLURKERNEL_OPTIMIZATION_TRANSFORM_IDENDITY,
		D2D1_DIRECTIONALBLURKERNEL_OPTIMIZATION_TRANSFORM_SCALE
	};

	const GUID CLSID_D2D1BorderRect{ 0x3CC23214, 0x73AD, 0x450B, { 0x84, 0x36, 0x0AA, 0x81, 0x94, 0x92, 0x7F, 0x0B8 } };
	enum D2D1_BORDERRECT_PROP
	{
		D2D1_BORDERRECT_PROP_RECT,
		D2D1_BORDERRECT_PROP_ENABLE_RECT,
		D2D1_BORDERRECT_PROP_EDGE_MODE_X,
		D2D1_BORDERRECT_PROP_EDGE_MODE_Y,
	};

	struct IDXGISwapChainDWM1;
	struct ID2D1PrivateCompositorBuffer : IUnknown {};
	struct ID2D1PrivateCompositorCommandList : IUnknown {};
	struct ID2D1PrivateCompositorPrimitiveProperties : IUnknown {};
	struct ID2D1PrivateDepthBuffer : IUnknown {};
	struct ID2D1PrivateCompositorRenderer : IUnknown {};
	struct DXGI_PRESENT_MULTIPLANE_OVERLAY;
	enum D2D1_DRAW_COMPOSITOR_COMMAND_LIST_OPTIONS : UINT {};
	enum D2D1_DEVICE_CONTEXT_BATCHING_STATE : UINT {};
	enum D2D1_INK_RENDERING_HINT : UINT {};

	DECLARE_INTERFACE_IID_(ID2D1PrivateCompositorDeviceContext, IUnknown, "2ea67ed7-d42e-4c07-9dd5-a91ea23e01d2")
	{
		STDMETHODV(CreateCompositorCommandList)(ID2D1PrivateCompositorBuffer*, ID2D1PrivateCompositorBuffer*, ID2D1Bitmap**, UINT, ID2D1Bitmap**, UINT, ID2D1PrivateCompositorPrimitiveProperties**, UINT, ID2D1PrivateCompositorCommandList**) PURE;
		STDMETHODV(DrawCompositorCommandList)(ID2D1PrivateCompositorCommandList*, float, D2D_MATRIX_4X4_F const*, D2D1_DRAW_COMPOSITOR_COMMAND_LIST_OPTIONS, UINT) PURE;
		STDMETHODV(CreatePrimitiveProperties)(ID2D1PrivateCompositorPrimitiveProperties**)PURE;
		STDMETHODV(SetPrimitiveColor)(ID2D1PrivateCompositorPrimitiveProperties*, D2D1_COLOR_F const*) PURE;
		STDMETHODV(SetTargetAndDepthBuffer)(ID2D1Image*, ID2D1PrivateDepthBuffer*) PURE;
		STDMETHODV(GetDepthBuffer)(ID2D1PrivateDepthBuffer**) PURE;
		STDMETHODV(SetGuardRect)(ID2D1Bitmap*, RECT const*) PURE;
		STDMETHODV(SetPrimitiveClip)(D2D1_RECT_F const*, D2D1_ANTIALIAS_MODE) PURE;
		STDMETHODV(QueryBatchingState)(D2D1_DEVICE_CONTEXT_BATCHING_STATE*) PURE;
		STDMETHODV(CreateSharedAtlasBitmap)(ID2D1Bitmap1 const*, D2D1_BITMAP_PROPERTIES1 const*, ID2D1Bitmap1**) PURE;
		STDMETHODV(PresentDWM)(IDXGISwapChainDWM1*, UINT, UINT, RECT const*, UINT, DXGI_PRESENT_MULTIPLANE_OVERLAY const*, UINT, IDXGIResource*, UINT) PURE;
		STDMETHODV(PresentMultiplaneOverlay)(IDXGISwapChainDWM1*, UINT, UINT, DXGI_HDR_METADATA_TYPE, void const*, DXGI_PRESENT_MULTIPLANE_OVERLAY const*, UINT) PURE;
		STDMETHODV(DrawCompositorPrimitives)(ID2D1PrivateCompositorRenderer*) PURE;
		STDMETHODV(CreateCompositorCommandListWithBounds)(ID2D1PrivateCompositorBuffer*, ID2D1PrivateCompositorBuffer*, ID2D1Bitmap**, UINT, ID2D1Bitmap**, UINT, ID2D1PrivateCompositorPrimitiveProperties**, UINT, D2D1_RECT_F const*, ID2D1PrivateCompositorCommandList**) PURE;
		STDMETHODV(SetInkRenderingHint)(D2D1_INK_RENDERING_HINT) PURE;
		STDMETHODV(GetInkRenderingHint)(void) PURE;
		STDMETHODV(SignalFence)(ID3D11Fence*, UINT64) PURE;
	};

	interface DX_DECLARE_INTERFACE("e7fda62a-6a94-4f17-9f7c-26a950c74010") ID2D1RegionGeometry : public ID2D1Geometry
	{
		virtual UINT GetRectanglesCount() const = 0;
		virtual void GetRectangles(RECT* buffer, UINT count) const = 0;
	};
}
