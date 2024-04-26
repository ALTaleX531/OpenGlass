#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "BackdropManager.hpp"
#include "GaussianBlurEffect.hpp"
#include "ColorSourceEffect.hpp"
#include "CompositeEffect.hpp"
#include "AlphaMaskEffect.hpp"
#include "BorderEffect.hpp"

namespace OpenGlass
{
	wuc::CompositionBrush GetBrush(const wuc::Compositor& compositor)
	{
		auto tintColorEffect{ winrt::make_self<Win2D::ColorSourceEffect>() };
		tintColorEffect->SetName(L"TintColor");
		tintColorEffect->SetColor(wu::Color{ 120, 116, 184, 252 });

		auto gaussianBlurEffect{ winrt::make_self<Win2D::GaussianBlurEffect>() };
		gaussianBlurEffect->SetName(L"Blur");
		gaussianBlurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
		gaussianBlurEffect->SetOptimizationMode(D2D1_GAUSSIANBLUR_OPTIMIZATION_QUALITY);
		gaussianBlurEffect->SetBlurAmount(3.f);
		gaussianBlurEffect->SetInput(wuc::CompositionEffectSourceParameter{ L"Backdrop" });

		auto compositeEffect{ winrt::make_self<Win2D::CompositeStepEffect>() };
		compositeEffect->SetCompositeMode(D2D1_COMPOSITE_MODE_SOURCE_OVER);
		compositeEffect->SetName(L"Composite");
		compositeEffect->SetDestination(*gaussianBlurEffect);
		compositeEffect->SetSource(*tintColorEffect);

		auto effectBrush = compositor.CreateEffectFactory(*compositeEffect).CreateBrush();
		effectBrush.SetSourceParameter(L"Backdrop", compositor.CreateBackdropBrush());
		return effectBrush;
	}
}