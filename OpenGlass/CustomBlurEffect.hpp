#pragma once
#include "pch.h"
#include "framework.hpp"
#include "cpprt.hpp"

// this is actually a full mirror implementation of dwmcore!CCustomBlur, 
// in order to produce a high performance blur effect than raw direct2d gaussian blur effect
// no reason for touching it

// i have to apologize to all other contributors since i completely refactor this part of the code
namespace OpenGlass
{
	const GUID CLSID_D2D1DirectionalBlurKernel{ 0x58EB6E2A, 0x0D779, 0x4B7D, { 0x0AD, 0x39, 0x6F, 0x5A, 0x9F, 0x0C9, 0x0D2, 0x88} };
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

	// [Guid("01AA613C-2376-4B95-8A74-B94CA840D4D1")]
	DECLARE_INTERFACE_IID_(ICustomBlurEffect, IUnknown, "01AA613C-2376-4B95-8A74-B94CA840D4D1")
	{
		virtual HRESULT STDMETHODCALLTYPE SetInput(
			ID2D1DeviceContext* context,
			ID2D1Image* inputImage,
			const D2D1_RECT_F& imageRectangle,
			const D2D1_RECT_F& imageBounds,
			float blurAmount
		) = 0;
		virtual ID2D1Image* STDMETHODCALLTYPE GetOutput() const = 0;
		virtual void STDMETHODCALLTYPE Reset() = 0;
	};

	class CCustomBlurEffect : public winrt::implements<CCustomBlurEffect, ICustomBlurEffect>
	{
		bool m_initialized{ false };

		float m_blurAmount{ 9.f };
		D2D1_RECT_F m_imageRectangle{};
		ID2D1Image* m_effectInput{ nullptr };
		winrt::com_ptr<ID2D1Image> m_effectOutput{ nullptr };

		winrt::com_ptr<ID2D1Effect> m_cropInputEffect{};
		winrt::com_ptr<ID2D1Effect> m_scaleDownEffect{};
		winrt::com_ptr<ID2D1Effect> m_borderEffect{};
		winrt::com_ptr<ID2D1Effect> m_directionalBlurXEffect{};
		winrt::com_ptr<ID2D1Effect> m_directionalBlurYEffect{};
		winrt::com_ptr<ID2D1Effect> m_scaleUpEffect{};

		static const float k_optimizations[16];
		static float DetermineOutputScale(float size, float blurAmount);
		HRESULT Initialize(ID2D1DeviceContext* context);
	public:
		HRESULT STDMETHODCALLTYPE SetInput(
			ID2D1DeviceContext* context,
			ID2D1Image* inputImage,
			const D2D1_RECT_F& imageRectangle,
			const D2D1_RECT_F& imageBounds,
			float blurAmount
		) override;
		ID2D1Image* STDMETHODCALLTYPE GetOutput() const override { return m_effectOutput.get(); }
		void STDMETHODCALLTYPE Reset() override;
	};
}