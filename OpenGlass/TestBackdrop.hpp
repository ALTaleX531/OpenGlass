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
#include "TintEffect.hpp"
#include "OpacityEffect.hpp"
#include "SaturationEffect.hpp"

namespace OpenGlass
{
	wuc::CompositionBrush GetBrush(const wuc::Compositor& compositor)
	{
		//return compositor.CreateColorBrush({ 255, 116, 184, 252 });
		constexpr float blurAmount{ 3.f };
		constexpr float blurBalance{ 0.49f };
		constexpr float colorBalance{ 0.f };
		constexpr float glowBalance{ 0.43f };
		constexpr wu::Color mainColor{ 255, 116, 184, 252 };
		constexpr wu::Color glowColor{ 255, 116, 184, 252 };

		auto fallbackTintSource{ winrt::make_self<Win2D::ColorSourceEffect>() };
		fallbackTintSource->SetColor(winrt::Windows::UI::Color
		{
			255,
			static_cast<UCHAR>(min(blurBalance + 0.1f, 1.f) * 255.f),
			static_cast<UCHAR>(min(blurBalance + 0.1f, 1.f) * 255.f),
			static_cast<UCHAR>(min(blurBalance + 0.1f, 1.f) * 255.f),
		});

		auto blackOrTransparentSource{ winrt::make_self<Win2D::TintEffect>() };
		blackOrTransparentSource->SetInput(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"Backdrop" });
		blackOrTransparentSource->SetColor(D2D1::ColorF(D2D1::ColorF::Black));

		auto colorEffect{ winrt::make_self<Win2D::ColorSourceEffect>() };
		colorEffect->SetName(L"MainColor");
		colorEffect->SetColor(mainColor);

		auto colorOpacityEffect{ winrt::make_self<Win2D::OpacityEffect>() };
		colorOpacityEffect->SetName(L"MainColorOpacity");
		colorOpacityEffect->SetInput(*colorEffect);
		colorOpacityEffect->SetOpacity(colorBalance);

		auto backdropBalanceEffect{ winrt::make_self<Win2D::OpacityEffect>() };
		backdropBalanceEffect->SetName(L"BlurBalance");
		backdropBalanceEffect->SetOpacity(blurBalance);
		backdropBalanceEffect->SetInput(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"Backdrop" });

		auto actualBackdropEffect{ winrt::make_self<Win2D::CompositeStepEffect>() };
		actualBackdropEffect->SetCompositeMode(D2D1_COMPOSITE_MODE_PLUS);
		actualBackdropEffect->SetDestination(*blackOrTransparentSource);
		actualBackdropEffect->SetSource(*backdropBalanceEffect);

		auto desaturatedBackdrop{ winrt::make_self<Win2D::SaturationEffect>() };
		desaturatedBackdrop->SetSaturation(0.f);
		desaturatedBackdrop->SetInput(winrt::Windows::UI::Composition::CompositionEffectSourceParameter{ L"Backdrop" });

		// make animation feel better...
		auto backdropNotTransparentPromised{ winrt::make_self<Win2D::CompositeStepEffect>() };
		backdropNotTransparentPromised->SetCompositeMode(D2D1_COMPOSITE_MODE_SOURCE_OVER);
		backdropNotTransparentPromised->SetDestination(*fallbackTintSource);
		backdropNotTransparentPromised->SetSource(*desaturatedBackdrop);

		// if the glowColor is black, then it will produce a completely transparent surface
		auto tintEffect{ winrt::make_self<Win2D::TintEffect>() };
		tintEffect->SetInput(*backdropNotTransparentPromised);
		tintEffect->SetColor(winrt::Windows::UI::Color{ static_cast<UCHAR>(static_cast<float>(glowColor.A) * glowBalance), glowColor.R, glowColor.G, glowColor.B });

		auto backdropWithAfterGlow{ winrt::make_self<Win2D::CompositeStepEffect>() };
		backdropWithAfterGlow->SetCompositeMode(D2D1_COMPOSITE_MODE_PLUS);
		backdropWithAfterGlow->SetDestination(*actualBackdropEffect);
		backdropWithAfterGlow->SetSource(*tintEffect);

		auto compositeEffect{ winrt::make_self<Win2D::CompositeStepEffect>() };
		compositeEffect->SetCompositeMode(D2D1_COMPOSITE_MODE_PLUS);
		compositeEffect->SetDestination(*backdropWithAfterGlow);
		compositeEffect->SetSource(*colorOpacityEffect);

		auto gaussianBlurEffect{ winrt::make_self<Win2D::GaussianBlurEffect>() };
		gaussianBlurEffect->SetName(L"Blur");
		gaussianBlurEffect->SetBorderMode(D2D1_BORDER_MODE_SOFT);
		gaussianBlurEffect->SetOptimizationMode(D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED);
		gaussianBlurEffect->SetBlurAmount(blurAmount);
		gaussianBlurEffect->SetInput(*compositeEffect);

		auto effectBrush{ compositor.CreateEffectFactory(*gaussianBlurEffect).CreateBrush() };
		effectBrush.SetSourceParameter(L"Backdrop", compositor.CreateBackdropBrush());

		return effectBrush;
	}
}