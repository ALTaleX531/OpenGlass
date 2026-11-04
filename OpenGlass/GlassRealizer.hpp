#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "AeroEffect.hpp"
#include "Buffer2D.hpp"
#include "Shared.hpp"
#include "GlassEffectBatch.hpp"

namespace OpenGlass
{
	class CGlassRealizer
	{
		struct RenderTargetInfo;

		Shared::GlassType m_glassType{ Shared::GlassType::Invalid };
		winrt::com_ptr<IRenderingEffect> m_glassEffect{ nullptr };
		CBuffer2D m_buffer{};
		CGlassEffectBatch m_batch;
		std::vector<D2D1_RECT_U> m_copyRectangles;

		bool EnsureGlassEffect(Shared::GlassType type);
		HRESULT EnsureBuffer(ID2D1DeviceContext* context, const RenderTargetInfo& target);
		void CopyBackdropForEffect(
			const RenderTargetInfo& target,
			const D2D1_RECT_F& bounds,
			const D2D1_RECT_F& samplingBounds,
			std::span<const D2D1_RECT_F> rectangles,
			float expansion
		);
		HRESULT RenderEffect(
			ID2D1DeviceContext* context,
			ID2D1Bitmap1* target,
			const D2D1_RECT_F& bounds,
			std::span<const D2D1_RECT_F> rectangles,
			const D2D1_MATRIX_3X2_F& outputMatrix
		);
	public:
		// GlassRenderer routes zero blur through its solid-color branch.
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
