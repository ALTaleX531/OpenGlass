#include "pch.h"
#include "MsstyleInternals.hpp"
#include "GlassKernel.hpp"
#include "uDWMProjection.hpp"
#include "Shared.hpp"
#include "dwmcoreProjection.hpp"
#include "ReflectionVisual.hpp"
#include "GlassRenderer.hpp"
#include "CaptionTextHandler.hpp"
#include "CustomThemeAtlasLoader.hpp"
#include "GlassRealizer.hpp"
#include "D3DGlassRealizer.hpp"
#include "BlurSettings.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassKernel
{
	HRESULT MyCD2DContext_DestroyDeviceResources(dwmcore::CD2DContext* This);
	HRESULT MyCGraphicsDeviceManager_ReleaseGraphicsDevice(uDWM::CGraphicsDeviceManager* This);

	Projection::Detour<dwmcore::Symbol_CD2DContext_DestroyDeviceResources, &MyCD2DContext_DestroyDeviceResources> g_CD2DContext_DestroyDeviceResources_Org{};
	Projection::Detour<uDWM::Symbol_CGraphicsDeviceManager_ReleaseGraphicsDevice, &MyCGraphicsDeviceManager_ReleaseGraphicsDevice> g_CGraphicsDeviceManager_ReleaseGraphicsDevice_Org{};

	void RedrawTopLevelWindow(uDWM::CTopLevelWindow* window, bool deepRedraw)
	{
		if (window)
		{
			if (const auto dwriteTextVisual = window->GetDWriteTextVisual(); dwriteTextVisual)
			{
				dwriteTextVisual->SetDirtyFlags(0x2); // device lost/update layout
				dwriteTextVisual->SetDirtyFlags(0x4); // update offset
				dwriteTextVisual->SetDirtyFlags(0x200); // update dwrite objects
			}
			if (deepRedraw)
			{
				// clear extra reflection draw geometry command
				if (const auto legacyVisual = window->GetLegacyVisual(); legacyVisual)
				{
					legacyVisual->ClearAll();
				}
			}
			// update nc background
			window->SetDirtyFlags(0x20000);
			// update window visuals
			window->SetDirtyFlags(0x40000);
			// update blur behind
			window->OnBlurBehindUpdated();
			// update system backdrop
			window->OnSystemBackdropUpdated();
		}
	}

	winrt::com_ptr<abi::ICompositionSurface> g_reflectionSurface{ nullptr };
	abi::ICompositionSurface* GetOrCreateReflectionSurface()
	{
		if (!g_reflectionSurface)
		{
			LOG_IF_FAILED(CReflectionVisual::CreateSurface(g_reflectionSurface.put()));
		}

		return g_reflectionSurface.get();
	}
}

HRESULT GlassKernel::MyCD2DContext_DestroyDeviceResources(dwmcore::CD2DContext* This)
{
	GlassRenderer::DestroyDeviceResources(This);
	return g_CD2DContext_DestroyDeviceResources_Org(This);
}

HRESULT GlassKernel::MyCGraphicsDeviceManager_ReleaseGraphicsDevice(uDWM::CGraphicsDeviceManager* This)
{
	CaptionTextHandler::DestroyDeviceResources();
	g_reflectionSurface = nullptr;
	return g_CGraphicsDeviceManager_ReleaseGraphicsDevice_Org(This);
}

void GlassKernel::RedrawAllTopLevelWindow(bool deepRedraw)
{
	ULONG_PTR desktopID{ 0 };
	Util::GetDesktopID(1, &desktopID);
	const auto windowList = uDWM::CDesktopManager::GetInstance()->GetWindowList()->GetWindowListForDesktop(desktopID);
	for (auto i = windowList->Blink; i != windowList; i = i->Blink)
	{
		RedrawTopLevelWindow(
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
				std::clamp((scRGBColor.r * colorBalance + scRGBAfterglow.r * afterglowBalance * 0.6f) / alpha, 0.f, 1.f),
				std::clamp((scRGBColor.g * colorBalance + scRGBAfterglow.g * afterglowBalance * 0.6f) / alpha, 0.f, 1.f),
				std::clamp((scRGBColor.b * colorBalance + scRGBAfterglow.b * afterglowBalance * 0.6f) / alpha, 0.f, 1.f),
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

void GlassKernel::Update(GlassEngine::UpdateType type)
{
	if (type & GlassEngine::UpdateType::Theme)
	{
		Shared::g_textGlowMode = GlassEngine::GetDwordFromRegistry(L"TextGlowMode", 1);

		WCHAR reflectionTexturePath[MAX_PATH]{};
		GlassEngine::GetStringFromRegistry(L"CustomThemeReflection", reflectionTexturePath);
		PathUnquoteSpacesW(reflectionTexturePath);
		Shared::g_reflectionTexturePath.assign(reflectionTexturePath);

		g_reflectionSurface = nullptr;
	}
	if (type & GlassEngine::UpdateType::Framework)
	{
		Shared::g_disableOnBattery = static_cast<bool>(
			GlassEngine::GetDwordFromRegistry(
				L"DisableGlassOnBattery",
				1
			)
		);
	}
	if (type & GlassEngine::UpdateType::Backdrop)
	{
		Shared::g_type = static_cast<Shared::GlassType>(std::clamp(GlassEngine::GetDwordFromRegistry(L"GlassType", 0), 0ul, 1ul));
		Shared::g_transparencyEnabled = GlassEngine::GetPersonalizeKey() ? Util::IsTransparencyEnabled(GlassEngine::GetPersonalizeKey()) : true;

		DWORD value = 0;
		Shared::g_reflectionIntensity = std::clamp(static_cast<float>(GlassEngine::GetDwordFromRegistry(L"ColorizationGlassReflectionIntensity")) / 100.f, 0.f, 1.f);

		Shared::g_reflectionParallaxIntensity = std::clamp(static_cast<float>(GlassEngine::GetDwordFromRegistry(L"ColorizationGlassReflectionParallaxIntensity", 13)) / 100.f, 0.f, 1.f);
		Shared::g_reflectionPolicy = static_cast<Shared::ReflectionPolicy>(GlassEngine::GetDwordFromRegistry(L"ColorizationGlassReflectionPolicy", 0xFFFFFFFF));
		Shared::g_blurAmount = BlurSettings::DecodeBlurAmount(GlassEngine::GetDwordFromRegistry(L"BlurDeviation", BlurSettings::DefaultEncodedDeviation));
		Shared::g_blurOptimization = static_cast<D2D1_GAUSSIANBLUR_OPTIMIZATION>(std::clamp(GlassEngine::GetDwordFromRegistry(L"BlurOptimization", 0), 0ul, 2ul));
		Shared::g_roundRectRadius = static_cast<int>(GlassEngine::GetDwordFromRegistry(L"RoundRectRadius"));

		value = GlassEngine::GetOverridableDwordFromRegistry(L"ColorizationColor", L"ColorizationColorOverride", 0xFFFFFFFF);
		Shared::g_color = Color::FromArgb(value);
		Shared::g_colorInactive = Color::FromArgb(GlassEngine::GetDwordFromRegistry(L"ColorizationColorInactive", value));

		value = GlassEngine::GetDwordFromRegistry(L"ColorizationOpaqueBlendPriority", 0xFFFFFFFF);
		Shared::g_opaqueBlend = static_cast<int>(GlassEngine::GetDwordFromRegistry(L"ColorizationOpaqueBlend"));
		if (value == 0xFFFFFFFF)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				value = 0;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				value = 1;
			}
		}
		Shared::g_opaqueBlendPriority = static_cast<Shared::OpaqueBlendPriority>(std::clamp(value, 0ul, 1ul));

		Shared::g_useD3DRendering = static_cast<bool>(GlassEngine::GetDwordFromRegistry(L"UseDirect3DRendering", 0));
	}
}

void GlassKernel::Startup()
{

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CD2DContext_DestroyDeviceResources_Org },
			{ &g_CGraphicsDeviceManager_ReleaseGraphicsDevice_Org }
		},
		true
	);
}

void GlassKernel::Shutdown()
{
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CD2DContext_DestroyDeviceResources_Org },
			{ &g_CGraphicsDeviceManager_ReleaseGraphicsDevice_Org }
		},
		false
	);

}

void GlassKernel::Cleanup()
{
	CReflectionVisual::RemoveAll();
	g_reflectionSurface = nullptr;
}
