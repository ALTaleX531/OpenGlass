#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "dwmcoreProjection.hpp"
#include "AeroEffect.hpp"
#include "Buffer2D.hpp"
#include "Shared.hpp"

namespace OpenGlass
{
	class CGlassRealizer
	{
		Shared::GlassType m_glassType{ Shared::GlassType::Invalid };
		winrt::com_ptr<IRenderingEffect> m_glassEffect{ nullptr };
		CBuffer2D m_buffer{};

		bool EnsureGlassEffect(Shared::GlassType type);
	public:
		HRESULT Render(
			ID2D1DeviceContext* context,
			ID2D1Geometry* geometry,
			const D2D1_RECT_F& drawingWorldBounds,
			const std::span<const D2D1_RECT_F>& rectangles,
			const CAeroParams& params,
			Shared::GlassType type
		);
		static float GetBlurExpansion(float blurAmount);
	};
}
