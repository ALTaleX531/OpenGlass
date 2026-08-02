#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "GlassEngine.hpp"

namespace OpenGlass::GlassKernel
{
	inline uDWM::CTopLevelWindow* g_window{ nullptr };
	abi::ICompositionSurface* GetOrCreateReflectionSurface();

	void RedrawAllTopLevelWindow(bool deepRedraw);
	float GetBlurRadius();

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

	bool IsCurrentCVIFullyTransparent();

	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Shutdown();
}
