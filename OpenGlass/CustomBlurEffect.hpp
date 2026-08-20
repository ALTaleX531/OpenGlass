#pragma once
#include "cpprt.hpp"
#include "RenderingEffect.hpp"

// this is actually a full mirror implementation of dwmcore!CCustomBlur, 
// in order to produce a high performance blur effect than raw direct2d gaussian blur effect
// dont touch it unless necessary

// `scale down -> crop` is about 25% fater than `crop -> scale down`
// kind of weird but it's true according to my testings

namespace OpenGlass
{
	struct CCustomBlurParams
	{
		float blurAmount; // System compositor GaussianBlurEffect.BlurAmount projection.
		D2D1_GAUSSIANBLUR_OPTIMIZATION optimization;
	};

	class CCustomBlurEffect : public winrt::implements<CCustomBlurEffect, IRenderingEffect, winrt::non_agile, winrt::no_weak_ref>
	{
		bool m_initialized{ false };

		float m_blurAmount{ 0.f };
		D2D1_GAUSSIANBLUR_OPTIMIZATION m_optimization{ D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED };
		D2D1_VECTOR_2F m_prescaleAmount{};
		D2D1_POINT_2F m_offset{};
		D2D1_RECT_F m_imageRectangle{};

		// scale down -> crop -> border -> directional blur x -> directional blur y -> scale up (transform)
		winrt::com_ptr<ID2D1Effect> m_scaleDownEffect{};
		winrt::com_ptr<ID2D1Effect> m_cropInputEffect{};
		winrt::com_ptr<ID2D1Effect> m_borderEffect{};
		winrt::com_ptr<ID2D1Effect> m_directionalBlurXEffect{};
		winrt::com_ptr<ID2D1Effect> m_directionalBlurYEffect{};
		winrt::com_ptr<ID2D1Image> m_inputImage{};

		static const float k_optimizations[16];
		float DetermineOutputScale(
			float size
		);
		HRESULT Initialize(ID2D1DeviceContext* context);
		HRESULT CalculateAndSetEffectParams(ID2D1DeviceContext* context, ID2D1Image* inputImage);
	public:
		HRESULT Build(
			ID2D1DeviceContext* context,
			ID2D1Image* inputImage,
			const D2D1_RECT_F& imageRectangle,
			const void* additionalParams
		) override;
		D2D1_MATRIX_3X2_F GetOutputMatrix() const override;
		D2D1_POINT_2F GetOutputOffset() const override { return m_offset; }
		void GetOutput(ID2D1Image** output) const override;
		void Reset() override;
	};
}
