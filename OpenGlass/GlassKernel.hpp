#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include <bitset>

namespace OpenGlass::uDWM
{
	struct CTopLevelWindow;
}

namespace OpenGlass::GlassKernel
{
	std::bitset<2> GetWindowStatus(const uDWM::CTopLevelWindow* window, bool nativeActive);
	std::bitset<2> GetWindowStatus(uDWM::CTopLevelWindow* window);
	void RedrawAllTopLevelWindow(bool deepRedraw);
	float GetBlurExpansion();

	struct CRealizedGlassColorizationParameters
	{
		D2D1_COLOR_F color;
		D2D1_COLOR_F afterglow;
		float colorBalance;
		float afterglowBalance;
		float blurBalance;

		D2D1_COLOR_F GetEffectivescRGBBlendColor(float sdrBoost) const;
	};
	float GetColorizationOpacity(bool active, bool maximized);
	D2D1_COLOR_F GetBaseColor(bool opaque, bool maximized);
	D2D1_COLOR_F GetSourceColor(bool active);
	CRealizedGlassColorizationParameters RealizeWindowColorization(
		const D2D1_COLOR_F& baseColor,
		const D2D1_COLOR_F& srcColor,
		float colorizationOpacity,
		bool opaque,
		bool livePreview
	);
	float GetAdjustedReflectionIntensity(bool active, bool maximized);
	D2D1_RECT_F CalculateReflectionViewport(
		const POINT& offset,
		float parallaxIntensity,
		bool mirrored,
		LONG width,
		const D2D1_SIZE_F& scale
	);
}
