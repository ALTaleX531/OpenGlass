#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "uDwmProjection.hpp"
#include "Shared.hpp"

namespace OpenGlass
{
	// [Guid("01AA613C-2376-4B95-8A74-B94CA840D4D1")]
	DECLARE_INTERFACE_IID_(IGlassEffect, IUnknown, "01AA613C-2376-4B95-8A74-B94CA840D4D1")
	{
		virtual void STDMETHODCALLTYPE SetGlassRenderingParameters(
			const D2D1_COLOR_F& color,
			const D2D1_COLOR_F& afterglow,
			float glassOpacity,
			float blurAmount,
			float colorBalance,
			float afterglowBalance,
			float blurBalance,
			Shared::Type type
		) = 0;
		virtual HRESULT STDMETHODCALLTYPE Render(
			ID2D1DeviceContext* context,
			ID2D1Geometry* geometry,
			const D2D1_RECT_F& clipWorldBounds,
			bool normalDesktopRender
		) = 0;
	};

	namespace GlassEffectFactory
	{
		winrt::com_ptr<IGlassEffect> GetOrCreate(
			dwmcore::CGeometry* geometry,
			bool createIfNecessary = false
		);
		void Remove(dwmcore::CGeometry* geometry);
		void Shutdown();
	}
}