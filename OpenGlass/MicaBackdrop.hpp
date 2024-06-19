#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "ColorSourceEffect.hpp"
#include "OpacityEffect.hpp"
#include "BlendEffect.hpp"

namespace OpenGlass::MicaBackdrop
{
	wuc::CompositionBrush CreateBrush(
		const wuc::Compositor& compositor,
		const wu::Color& tintColor,
		const wu::Color& luminosityColor,
		float tintOpacity,
		float luminosityOpacity
	)
	{
		if (static_cast<float>(tintColor.A) * tintOpacity == 255.f)
		{
			return compositor.CreateColorBrush(tintColor);
		}

		auto tintColorEffect{ winrt::make_self<Win2D::ColorSourceEffect>() };
		tintColorEffect->SetName(L"TintColor");
		tintColorEffect->SetColor(tintColor);

		auto tintOpacityEffect{ winrt::make_self<Win2D::OpacityEffect>() };
		tintOpacityEffect->SetName(L"TintOpacity");
		tintOpacityEffect->SetOpacity(tintOpacity);
		tintOpacityEffect->SetInput(*tintColorEffect);

		auto luminosityColorEffect{ winrt::make_self<Win2D::ColorSourceEffect>() };
		luminosityColorEffect->SetColor(luminosityColor);

		auto luminosityOpacityEffect{ winrt::make_self<Win2D::OpacityEffect>() };
		luminosityOpacityEffect->SetName(L"LuminosityOpacity");
		luminosityOpacityEffect->SetOpacity(luminosityOpacity);
		luminosityOpacityEffect->SetInput(*luminosityColorEffect);

		auto luminosityBlendEffect{ winrt::make_self<Win2D::BlendEffect>() };
		luminosityBlendEffect->SetBlendMode(D2D1_BLEND_MODE_COLOR);
		luminosityBlendEffect->SetBackground(wuc::CompositionEffectSourceParameter{ L"BlurredWallpaperBackdrop" });
		luminosityBlendEffect->SetForeground(*luminosityOpacityEffect);

		auto colorBlendEffect{ winrt::make_self<Win2D::BlendEffect>() };
		colorBlendEffect->SetBlendMode(D2D1_BLEND_MODE_LUMINOSITY);
		colorBlendEffect->SetBackground(*luminosityBlendEffect);
		colorBlendEffect->SetForeground(*tintOpacityEffect);

		auto effectBrush{ compositor.CreateEffectFactory(*colorBlendEffect).CreateBrush() };
		effectBrush.SetSourceParameter(L"BlurredWallpaperBackdrop", compositor.TryCreateBlurredWallpaperBackdropBrush());

		return effectBrush;
	}
}