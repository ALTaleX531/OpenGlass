#pragma once
#include "AeroEffect.hpp"
#include "Buffer2D.hpp"
#include <iterator>

namespace OpenGlass::RenderTest
{
	enum class Method
	{
		D2D,
		Rects,
		D3D,
		Gaussian
	};
	inline constexpr const char* MethodNames[]{ "d2d", "rects", "d3d", "gaussian" };
	inline constexpr const wchar_t* MethodLabels[]{ L"D2D", L"D2D rects", L"D3D", L"Gaussian" };
	inline constexpr size_t MethodCount = std::size(MethodNames);

	struct Settings
	{
		float blurAmount{ 3.f };
		D2D1_GAUSSIANBLUR_OPTIMIZATION optimization{ D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED };
		bool forceCapture{};

		CAeroParams Parameters() const;
	};

	class GaussianEffect : public winrt::implements<GaussianEffect, IRenderingEffect, winrt::non_agile, winrt::no_weak_ref>
	{
		winrt::com_ptr<ID2D1Factory1> m_factory;
		winrt::com_ptr<ID2D1Effect> m_scaleEffect;
		winrt::com_ptr<ID2D1Effect> m_cropEffect;
		winrt::com_ptr<ID2D1Effect> m_borderEffect;
		winrt::com_ptr<ID2D1Effect> m_gaussianBlurEffect;
		winrt::com_ptr<ID2D1Effect> m_colorizationEffect;
		HRESULT Initialize(ID2D1DeviceContext* context);
	public:
		~GaussianEffect();
		HRESULT Build(ID2D1DeviceContext* context, ID2D1Image* inputImage,
			const D2D1_RECT_F& imageRectangle, const void* additionalParams) override;
		D2D1_MATRIX_3X2_F GetOutputMatrix() const override { return D2D1::Matrix3x2F::Scale(2.f, 2.f); }
		D2D1_POINT_2F GetOutputOffset() const override { return {}; }
		void GetOutput(ID2D1Image** output) const override;
		void Reset() override;
	};

	// Per-rectangle submission shared by the old D2D and built-in Gaussian graphs.
	class RectRenderer
	{
		winrt::com_ptr<IRenderingEffect> m_effect;
		CBuffer2D m_capture;
	public:
		explicit RectRenderer(winrt::com_ptr<IRenderingEffect> effect = winrt::make<CAeroEffect>()) : m_effect(std::move(effect)) {}
		HRESULT Render(ID2D1DeviceContext* context, ID2D1Geometry* geometry,
			const D2D1_RECT_F& bounds, std::span<const D2D1_RECT_F> rectangles, const CAeroParams& params,
			float sourceBlurScale = 1.f);
	};
}
