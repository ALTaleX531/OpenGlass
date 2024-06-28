#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "uDwmProjection.hpp"

namespace OpenGlass::GlassEffectManager
{
	// [Guid("01AA613C-2376-4B95-8A74-B94CA840D4D1")]
	DECLARE_INTERFACE_IID_(IGlassEffect, IUnknown, "01AA613C-2376-4B95-8A74-B94CA840D4D1")
	{
		virtual HRESULT STDMETHODCALLTYPE SetSourceRect(const D2D1_RECT_F& rect) = 0;
		virtual HRESULT STDMETHODCALLTYPE Invalidate(
			ID2D1Bitmap1* backdropBitmap, 
			const D2D1_RECT_F& rect, 
			const D2D1_COLOR_F& color,
			float glassOpacity,
			float blurAmount
		) = 0;
		virtual HRESULT STDMETHODCALLTYPE Render(
			ID2D1Geometry* geometry
		) = 0;
	};

	winrt::com_ptr<IGlassEffect> GetOrCreate(
		dwmcore::CGeometry* geometry, 
		ID2D1DeviceContext* deviceContext = nullptr, 
		bool createIfNecessary = false
	);
	void Remove(dwmcore::CGeometry* geometry);
	void Shutdown();
}