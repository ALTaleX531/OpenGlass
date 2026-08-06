#pragma once
#include "CustomBlurEffect.hpp"
#include "BlurEffect.hpp"
#include "AeroColorizationEffect.hpp"

namespace OpenGlass
{
	struct CAeroParams : CBlurParams
	{
		D2D1_COLOR_F afterglow;
		D2D1_COLOR_F fallback;
		float blurBalance;
	};

	class CAeroEffect : public winrt::implements<CAeroEffect, IRenderingEffect, winrt::non_agile, winrt::no_weak_ref>
	{
		bool m_initialized{ false };

		winrt::com_ptr<IRenderingEffect> m_customBlurEffect{};
		winrt::com_ptr<ID2D1Effect> m_colorizationEffect{};
		winrt::com_ptr<ID2D1Effect> m_outputEffect{};
		inline static winrt::com_ptr<ID2D1Factory1> s_factory{};
		inline static const auto s_customEffectScope = wil::scope_exit([] static
		{
			if (s_factory)
			{
				LOG_IF_FAILED(CAeroColorizationEffect::UnRegister(s_factory.get()));
				s_factory = nullptr;
			}
		});

		HRESULT Initialize(ID2D1DeviceContext* context);
	public:
		HRESULT Build(
			ID2D1DeviceContext* context,
			ID2D1Image* inputImage,
			const D2D1_RECT_F& imageRectangle,
			const void* additionalParams
		) override;
		D2D1_POINT_2F GetOutputOffset() const override { return m_customBlurEffect->GetOutputOffset(); }
		D2D1_MATRIX_3X2_F GetOutputMatrix() const override;
		void GetOutput(ID2D1Image** output) const override;
		void Reset() override;
	};
}
