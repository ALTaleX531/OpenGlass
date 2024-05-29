#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "ColorSourceEffect.hpp"
#include "OpacityEffect.hpp"
#include "GaussianBlurEffect.hpp"
#include "CompositeEffect.hpp"

namespace OpenGlass::BlurBackdrop
{
	wuc::CompositionBrush CreateBrush(
		const wuc::Compositor& compositor,
		const wu::Color& tintColor,
		float tintOpacity,
		float blurAmount
	)
	{
		if (static_cast<float>(tintColor.A) * tintOpacity == 255.f)
		{
			return compositor.CreateColorBrush(tintColor);
		}

		if (!blurAmount)
		{
			return compositor.CreateColorBrush({ static_cast<BYTE>(static_cast<float>(tintColor.A) * tintOpacity), tintColor.R, tintColor.G, tintColor.B});
		}

		auto gaussianBlurEffect{ winrt::make_self<Win2D::GaussianBlurEffect>() };
		gaussianBlurEffect->SetName(L"Blur");
		gaussianBlurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
		gaussianBlurEffect->SetBlurAmount(blurAmount);
		gaussianBlurEffect->SetOptimizationMode(D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED);
		gaussianBlurEffect->SetInput(wuc::CompositionEffectSourceParameter{ L"Backdrop" });
		if (static_cast<float>(tintColor.A) * tintOpacity == 0.f)
		{
			auto effectBrush{ compositor.CreateEffectFactory(*gaussianBlurEffect).CreateBrush() };
			effectBrush.SetSourceParameter(L"Backdrop", compositor.CreateBackdropBrush());
			return effectBrush;
		}

		auto tintColorEffect{ winrt::make_self<Win2D::ColorSourceEffect>() };
		tintColorEffect->SetName(L"TintColor");
		tintColorEffect->SetColor(tintColor);

		auto tintOpacityEffect{ winrt::make_self<Win2D::OpacityEffect>() };
		tintOpacityEffect->SetName(L"TintOpacity");
		tintOpacityEffect->SetOpacity(tintOpacity);
		tintOpacityEffect->SetInput(*tintColorEffect);

		auto compositeStepEffect{ winrt::make_self<Win2D::CompositeStepEffect>() };
		compositeStepEffect->SetDestination(*gaussianBlurEffect);
		compositeStepEffect->SetSource(*tintOpacityEffect);

		auto effectBrush{ compositor.CreateEffectFactory(*compositeStepEffect).CreateBrush() };
		effectBrush.SetSourceParameter(L"Backdrop", compositor.CreateBackdropBrush());
		return effectBrush;
	}
}