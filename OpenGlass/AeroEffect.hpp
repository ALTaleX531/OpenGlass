#pragma once
#include "CustomBlurEffect.hpp"

namespace OpenGlass
{
	// [Guid("60419F4A-0BAB-4EAA-A5A6-C7E05BB3FAC4")]
	DECLARE_INTERFACE_IID_(IAeroEffect, IUnknown, "60419F4A-0BAB-4EAA-A5A6-C7E05BB3FAC4")
	{
		virtual HRESULT STDMETHODCALLTYPE SetInput(
			ID2D1DeviceContext* context,
			ID2D1Image* inputImage,
			const D2D1_RECT_F& imageRectangle,
			const D2D1_RECT_F& imageBounds,
			float blurAmount,
			const D2D1_COLOR_F& color,
			const D2D1_COLOR_F& afterglowColor,
			float colorBalance,
			float afterglowBalance,
			float blurBalance
		) = 0;
		virtual ID2D1Image* STDMETHODCALLTYPE GetOutput() const = 0;
		virtual void STDMETHODCALLTYPE Reset() = 0;
	};

	class CAeroEffect : public winrt::implements<CAeroEffect, IAeroEffect>
	{
		bool m_initialized{ false };

		winrt::com_ptr<ICustomBlurEffect> m_customBlurEffect{ nullptr };

		winrt::com_ptr<ID2D1Effect> m_fallbackColorEffect{ nullptr };
		winrt::com_ptr<ID2D1Effect> m_colorEffect{ nullptr };
		winrt::com_ptr<ID2D1Effect> m_tintEffect{ nullptr };
		winrt::com_ptr<ID2D1Effect> m_desaturationEffect{ nullptr };
		winrt::com_ptr<ID2D1Effect> m_innerCompositeEffect{ nullptr };
		winrt::com_ptr<ID2D1Effect> m_afterglowBalanceEffect{ nullptr };
		winrt::com_ptr<ID2D1Effect> m_blurBalanceEffect{ nullptr };
		winrt::com_ptr<ID2D1Effect> m_compositeEffect{ nullptr };
		winrt::com_ptr<ID2D1Effect> m_compositeEffect2{ nullptr };
		winrt::com_ptr<ID2D1Image> m_effectOutput{ nullptr };

		HRESULT Initialize(ID2D1DeviceContext* context);
	public:
		HRESULT STDMETHODCALLTYPE SetInput(
			ID2D1DeviceContext* context,
			ID2D1Image* inputImage,
			const D2D1_RECT_F& imageRectangle,
			const D2D1_RECT_F& imageBounds,
			float blurAmount,
			const D2D1_COLOR_F& color,
			const D2D1_COLOR_F& afterglowColor,
			float colorBalance,
			float afterglowBalance,
			float blurBalance
		) override;
		ID2D1Image* STDMETHODCALLTYPE GetOutput() const override { return m_effectOutput.get(); }
		void STDMETHODCALLTYPE Reset() override;
	};
}