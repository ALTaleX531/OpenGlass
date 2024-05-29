#pragma once
#include "winrt.hpp"
#include "cpprt.hpp"
#include "GaussianBlurEffect.hpp"
#include "CrossFadeEffect.hpp"

namespace OpenGlass::Utils
{
	FORCEINLINE wge::IGraphicsEffectSource CreateBlurredBackdrop(
		const wuc::Compositor& compositor,
		float blurAmount
	)
	{
		if (!blurAmount)
		{
			return wuc::CompositionEffectSourceParameter{ L"Backdrop" };
		}

		auto gaussianBlurEffect{ winrt::make_self<Win2D::GaussianBlurEffect>() };
		gaussianBlurEffect->SetName(L"Blur");
		gaussianBlurEffect->SetBorderMode(D2D1_BORDER_MODE_HARD);
		gaussianBlurEffect->SetBlurAmount(blurAmount);
		gaussianBlurEffect->SetOptimizationMode(D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED);
		gaussianBlurEffect->SetInput(wuc::CompositionEffectSourceParameter{ L"Backdrop" });
		return *gaussianBlurEffect;
	}

	FORCEINLINE wuc::CompositionBrush CreateCrossFadeBrush(
		const wuc::Compositor& compositor,
		const wuc::CompositionBrush& from,
		const wuc::CompositionBrush& to
	)
	{
		auto crossFadeEffect{ winrt::make_self<Win2D::CrossFadeEffect>() };
		crossFadeEffect->SetName(L"Crossfade");
		crossFadeEffect->SetSource(wuc::CompositionEffectSourceParameter{ L"From" });
		crossFadeEffect->SetDestination(wuc::CompositionEffectSourceParameter{ L"To" });
		crossFadeEffect->SetWeight(0);

		auto crossFadeEffectBrush{ compositor.CreateEffectFactory(*crossFadeEffect, {L"Crossfade.Weight"}).CreateBrush() };
		crossFadeEffectBrush.Comment(L"Crossfade");
		crossFadeEffectBrush.SetSourceParameter(L"From", from);
		crossFadeEffectBrush.SetSourceParameter(L"To", to);
		return crossFadeEffectBrush;
	}

	FORCEINLINE wuc::ScalarKeyFrameAnimation CreateCrossFadeAnimation(
		const wuc::Compositor& compositor,
		winrt::Windows::Foundation::TimeSpan const& crossfadeTime
	)
	{
		auto animation{ compositor.CreateScalarKeyFrameAnimation() };
		auto linearEasing{ compositor.CreateLinearEasingFunction() };
		animation.InsertKeyFrame(0.0f, 0.0f, linearEasing);
		animation.InsertKeyFrame(1.0f, 1.0f, linearEasing);
		animation.Duration(crossfadeTime);
		return animation;
	}
}