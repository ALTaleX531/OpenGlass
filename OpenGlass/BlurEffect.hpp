#pragma once
#include "CustomBlurEffect.hpp"

namespace OpenGlass
{
	// [Guid("78E29503-85D2-4250-87BD-95572A988899")]
	DECLARE_INTERFACE_IID_(IBlurEffect, IUnknown, "78E29503-85D2-4250-87BD-95572A988899")
	{
		virtual HRESULT STDMETHODCALLTYPE SetInput(
			ID2D1DeviceContext* context,
			ID2D1Image* inputImage,
			const D2D1_RECT_F& imageRectangle,
			const D2D1_RECT_F& imageBounds,
			float blurAmount,
			const D2D1_COLOR_F& color,
			float opacity
		) = 0;
		virtual ID2D1Image* STDMETHODCALLTYPE GetOutput() const = 0;
		virtual void STDMETHODCALLTYPE Reset() = 0;
	};

	class CBlurEffect : public winrt::implements<CBlurEffect, IBlurEffect>
	{
		bool m_initialized{ false };

		winrt::com_ptr<ICustomBlurEffect> m_customBlurEffect{ nullptr };

		winrt::com_ptr<ID2D1Effect> m_compositeEffect{ nullptr };
		winrt::com_ptr<ID2D1Effect> m_colorEffect{ nullptr };
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
			float opacity
		) override;
		ID2D1Image* STDMETHODCALLTYPE GetOutput() const override { return m_effectOutput.get(); }
		void STDMETHODCALLTYPE Reset() override;
	};
}