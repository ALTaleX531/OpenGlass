#include "pch.h"
#include "GlassKernel.hpp"
#include "WindowState.hpp"
#include "GlassInfra.hpp"
#include "Shared.hpp"
#include "GlassRealizer.hpp"
#include "D3DGlassRealizer.hpp"

using namespace OpenGlass;

std::bitset<2> GlassKernel::GetWindowStatus(const uDWM::CTopLevelWindow* window, bool nativeActive)
{
	const auto data = window->GetData();
	const auto hwnd = data ? data->GetHwnd() : nullptr;
	std::bitset<2> status;
	status.set(0, WindowState::Resolve(hwnd, WindowState::ActivationOverrideProperty, nativeActive));
	status.set(1, WindowState::Resolve(hwnd, WindowState::MaximizationOverrideProperty, window->TreatAsMaximized(data)));
	return status;
}

std::bitset<2> GlassKernel::GetWindowStatus(uDWM::CTopLevelWindow* window)
{
	return GetWindowStatus(window, window->TreatAsActiveWindow());
}

D2D1_RECT_F GlassKernel::CalculateReflectionViewport(
	const POINT& offset,
	float parallaxIntensity,
	bool mirrored,
	LONG width,
	const D2D1_SIZE_F& scale
)
{
	const auto screenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
	const auto screenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
	const auto screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	const auto screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	D2D1_RECT_F viewport{};
	LONG left = offset.x;
	if (mirrored)
	{
		left = screenLeft + (screenLeft + screenWidth) - (offset.x + width);
	}
	viewport.left = (screenLeft - left) + ((left + width / 2.f) - (screenLeft + screenWidth / 2.f)) * parallaxIntensity;
	viewport.top = static_cast<float>(screenTop - offset.y);
	viewport.right = viewport.left + static_cast<float>(screenWidth);
	viewport.bottom = viewport.top + static_cast<float>(screenHeight);
	viewport.left /= scale.width;
	viewport.top /= scale.height;
	viewport.right /= scale.width;
	viewport.bottom /= scale.height;
	return viewport;
}

void GlassKernel::RedrawAllTopLevelWindow(bool deepRedraw)
{
	ULONG_PTR desktopID{ 0 };
	Util::GetDesktopID(1, &desktopID);
	const auto windowList = uDWM::CDesktopManager::GetInstance()->GetWindowList()->GetWindowListForDesktop(desktopID);
	for (auto i = windowList->Blink; i != windowList; i = i->Blink)
	{
		GlassInfra::RedrawTopLevelWindow(
			reinterpret_cast<uDWM::CWindowData*>(i)->GetWindow(),
			deepRedraw
		);
	}
}

float GlassKernel::GetBlurExpansion()
{
	if (Shared::IsTransparencyDisabled())
	{
		return 0.f;
	}
	if (Shared::g_useD3DRendering)
	{
		return CD3DGlassRealizer::GetBlurExpansion();
	}

	return CGlassRealizer::GetBlurExpansion(Shared::g_blurAmount);
}

float GlassKernel::GetColorizationOpacity(bool active, bool maximized)
{
	if (active && !maximized)
	{
		return Shared::g_colorizationOpacity;
	}
	else if (active && maximized)
	{
		return Shared::g_colorizationOpacityMaximized;
	}
	else if (!active && !maximized)
	{
		return Shared::g_colorizationOpacityInactive;
	}
	else if (!active && maximized)
	{
		return Shared::g_colorizationOpacityInactiveMaximized;
	}

	return 0.f;
}
D2D1_COLOR_F GlassKernel::GetBaseColor(bool opaque, bool maximized)
{
	D2D1_COLOR_F color{};

	if (Shared::g_opaqueBlendPriority == Shared::OpaqueBlendPriority::Vista)
	{
		if (maximized)
		{
			color = Shared::g_colorizationBaseMaximized;
		}
		else if (opaque)
		{
			color = Shared::g_colorizationBaseOpaque;
		}
		else
		{
			color = Shared::g_colorizationBaseTransparent;
		}
	}
	else
	{
		if (opaque)
		{
			color = Shared::g_colorizationBaseOpaque;
		}
		else if (maximized)
		{
			color = Shared::g_colorizationBaseMaximized;
		}
		else
		{
			color = Shared::g_colorizationBaseTransparent;
		}
	}

	return color;
}
D2D1_COLOR_F GlassKernel::GetSourceColor(bool active)
{
	if (Shared::g_type == Shared::GlassType::Blur)
	{
		const auto& color = active ? Shared::g_color : Shared::g_colorInactive;
		return
		{
			color.r,
			color.g,
			color.b,
			active ? Shared::g_glassOpacity : Shared::g_glassOpacityInactive
		};
	}
	else if (Shared::g_type == Shared::GlassType::Aero)
	{
		return
		{
			Shared::g_color.r,
			Shared::g_color.g,
			Shared::g_color.b,
			1.f
		};
	}

	return {};
}

D2D1_COLOR_F GlassKernel::CRealizedGlassColorizationParameters::GetEffectivescRGBBlendColor(float sdrBoost) const
{
	const auto scRGBColor = Color::sRGBToscRGB(color, 0.f);
	const auto scRGBAfterglow = Color::sRGBToscRGB(afterglow, 0.f);

	if (Shared::g_type == Shared::GlassType::Aero)
	{
		// dwmcore!CCapturedGlassColorizationParameters::GetEffectivescRGBBlendColor (Windows 7)
		D2D1_COLOR_F effectiveBlendColor = {};
		if (Shared::IsGlassFullyOpaque(0.f, blurBalance, afterglowBalance))
		{
			effectiveBlendColor.r = scRGBColor.r * colorBalance;
			effectiveBlendColor.g = scRGBColor.g * colorBalance;
			effectiveBlendColor.b = scRGBColor.b * colorBalance;
			effectiveBlendColor.a = 1.f;
		}
		else
		{
			const auto alpha = std::max(1.f - blurBalance, 0.1f);
			effectiveBlendColor =
			{
				std::clamp(scRGBColor.r * colorBalance + scRGBAfterglow.r * afterglowBalance * 0.6f, 0.f, 1.f),
				std::clamp(scRGBColor.g * colorBalance + scRGBAfterglow.g * afterglowBalance * 0.6f, 0.f, 1.f),
				std::clamp(scRGBColor.b * colorBalance + scRGBAfterglow.b * afterglowBalance * 0.6f, 0.f, 1.f),
				alpha
			};
		}

		if (sdrBoost > 1.f)
		{
			effectiveBlendColor.r *= sdrBoost;
			effectiveBlendColor.g *= sdrBoost;
			effectiveBlendColor.b *= sdrBoost;
		}
		return effectiveBlendColor;
	}

	if (sdrBoost > 1.f)
	{
		return D2D1::ColorF(
			scRGBColor.r * sdrBoost,
			scRGBColor.g * sdrBoost,
			scRGBColor.b * sdrBoost,
			scRGBColor.a
		);
	}
	return scRGBColor;
}
GlassKernel::CRealizedGlassColorizationParameters GlassKernel::RealizeWindowColorization(
	const D2D1_COLOR_F& baseColor,
	const D2D1_COLOR_F& srcColor,
	float colorizationOpacity,
	bool opaque,
	bool livePreview
)
{
	CRealizedGlassColorizationParameters parameters{};

	if (Shared::g_type == Shared::GlassType::Blur)
	{
		parameters.color = srcColor;
		parameters.color.a *= colorizationOpacity;

		parameters.color.r = (1.f - parameters.color.a) * baseColor.r * baseColor.a + parameters.color.r * parameters.color.a;
		parameters.color.g = (1.f - parameters.color.a) * baseColor.g * baseColor.a + parameters.color.g * parameters.color.a;
		parameters.color.b = (1.f - parameters.color.a) * baseColor.b * baseColor.a + parameters.color.b * parameters.color.a;
		parameters.color.a = (1.f - parameters.color.a) * baseColor.a + parameters.color.a;
		if (parameters.color.a)
		{
			parameters.color.r /= parameters.color.a;
			parameters.color.g /= parameters.color.a;
			parameters.color.b /= parameters.color.a;
		}
	}
	else if (Shared::g_type == Shared::GlassType::Aero)
	{
		// uDWM!CGlassColorizationParameters::AdjustWindowColorization (Windows 7)
		parameters.afterglowBalance = Shared::g_afterglowBalance * (1.f - baseColor.a);
		parameters.blurBalance = Shared::g_blurBalance * (1.f - baseColor.a);

		parameters.afterglow = Shared::g_afterglow;
		parameters.color = srcColor;

		float colorBalance = Shared::g_colorBalance * colorizationOpacity;

		parameters.color.r = (1.f - colorBalance) * baseColor.r * baseColor.a + parameters.color.r * colorBalance;
		parameters.color.g = (1.f - colorBalance) * baseColor.g * baseColor.a + parameters.color.g * colorBalance;
		parameters.color.b = (1.f - colorBalance) * baseColor.b * baseColor.a + parameters.color.b * colorBalance;
		parameters.color.a = (1.f - colorBalance) * baseColor.a + parameters.color.a * colorBalance;
		if (parameters.color.a)
		{
			parameters.color.r /= parameters.color.a;
			parameters.color.g /= parameters.color.a;
			parameters.color.b /= parameters.color.a;
		}

		if (opaque)
		{
			parameters.blurBalance = 0.f;
		}
		else if (!livePreview)
		{
			parameters.blurBalance = 1.f - (1.f - parameters.blurBalance) * colorizationOpacity;
		}

		parameters.colorBalance = parameters.color.a;
	}

	return parameters;
}

float GlassKernel::GetAdjustedReflectionIntensity(bool active, bool maximized)
{
	float baseOpacity = 0.f;

	if (active && !maximized)
	{
		baseOpacity = Shared::g_reflectionOpacity;
	}
	else if (active && maximized)
	{
		baseOpacity = Shared::g_reflectionOpacityMaximized;
	}
	else if (!active && !maximized)
	{
		baseOpacity = Shared::g_reflectionOpacityInactive;
	}
	else if (!active && maximized)
	{
		baseOpacity = Shared::g_reflectionOpacityInactiveMaximized;
	}

	return std::clamp(baseOpacity * Shared::g_reflectionIntensity / 0.5f, 0.f, 1.f);
}
