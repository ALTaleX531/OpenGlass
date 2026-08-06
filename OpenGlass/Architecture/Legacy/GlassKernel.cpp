#include "pch.h"
#include "MsstyleInternals.hpp"
#include "GlassKernel.hpp"
#include "uDWMProjection.hpp"
#include "Shared.hpp"
#include "dwmcoreProjection.hpp"
#include "GlassEffectBrush.hpp"
#include "GlassReflectionBrush.hpp"
#include "GlassRenderer.hpp"
#include "GlassIntegrity.hpp"
#include "CaptionTextHandler.hpp"
#include "CustomThemeAtlasLoader.hpp"
#include "GlassRealizer.hpp"
#include "D3DGlassRealizer.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassKernel
{
	HRGN WINAPI MyCreateRoundRectRgn(int x1, int y1, int x2, int y2, int w, int h);
	HRGN WINAPI MyCreateRectRgn(int x1, int y1, int x2, int y2);
	HRGN WINAPI MyExtCreateRegion(const XFORM* lpx, DWORD nCount, const RGNDATA* lpData);

	HRESULT MyIDCompositionDesktopDevice_WaitForCommitCompletion(IDCompositionDesktopDevice* This);
	HRESULT MyCD2DContext_DestroyDeviceResources(dwmcore::CD2DContext* This);
	HRESULT MyCDesktopManager_ReleaseDXGIAdapter(uDWM::CDesktopManager* This);
	HRESULT MyCGraphicsDeviceManager_ReleaseGraphicsDevice(PVOID This);
	HRESULT MyCTopLevelWindow_EnsureImages_Pre_W10_1903(uDWM::IDwmChannel* channel);
	HRESULT MyCTopLevelWindow_EnsureImages_At_Least_W10_1903();

	decltype(&MyCreateRoundRectRgn) g_CreateRoundRectRgn_Org{ nullptr };
	decltype(&MyCreateRectRgn) g_CreateRectRgn_Org{ nullptr };
	decltype(&MyExtCreateRegion) g_ExtCreateRegion_Org{ nullptr };
	HookHelper::ImportHook g_gdiImportHooks;

	decltype(&MyIDCompositionDesktopDevice_WaitForCommitCompletion) g_IDCompositionDesktopDevice_WaitForCommitCompletion_Org{ nullptr };
	decltype(&MyIDCompositionDesktopDevice_WaitForCommitCompletion)* g_IDCompositionDesktopDevice_WaitForCommitCompletion_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyIDCompositionDesktopDevice_WaitForCommitCompletion> g_IDCompositionDesktopDevice_WaitForCommitCompletion_Hook;
	Projection::Detour<dwmcore::Symbol_CD2DContext_DestroyDeviceResources, decltype(&MyCD2DContext_DestroyDeviceResources)> g_CD2DContext_DestroyDeviceResources_Org{};
	Projection::Detour<uDWM::Symbol_CDesktopManager_ReleaseDXGIAdapter, decltype(&MyCDesktopManager_ReleaseDXGIAdapter)> g_CDesktopManager_ReleaseDXGIAdapter_Org{};
	Projection::Detour<uDWM::Symbol_CGraphicsDeviceManager_ReleaseGraphicsDevice, decltype(&MyCGraphicsDeviceManager_ReleaseGraphicsDevice)> g_CGraphicsDeviceManager_ReleaseGraphicsDevice_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_EnsureImages_Pre_18362, decltype(&MyCTopLevelWindow_EnsureImages_Pre_W10_1903)> g_CTopLevelWindow_EnsureImages_Pre_W10_1903_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_EnsureImages_18362, decltype(&MyCTopLevelWindow_EnsureImages_At_Least_W10_1903)> g_CTopLevelWindow_EnsureImages_At_Least_W10_1903_Org{};

	void RedrawTopLevelWindow(uDWM::CTopLevelWindow* window, bool deepRedraw)
	{
		if (window)
		{
			if (uDWM::g_versionInfo.build >= os::build_w11_22h2)
			{
				if (const auto dwriteTextVisual = window->GetDWriteTextVisual(); dwriteTextVisual)
				{
					// update text offset
					dwriteTextVisual->SetDirtyFlags(8);
					// update text color
					dwriteTextVisual->SetDirtyFlags(0x1000);
				}
			}
			else
			{
				// update text
				window->SetDirtyFlags(0x10000u);
			}
			if (deepRedraw)
			{
				// clear extra reflection draw geometry command
				if (const auto legacyVisual = window->GetLegacyVisual(); legacyVisual)
				{
					legacyVisual->ClearInstructions();
				}
			}
			// update accent
			window->OnAccentPolicyUpdated();
			// update nc background
			window->SetDirtyFlags(0x100000u);
			// update window visuals
			window->SetDirtyFlags(0x1000000u);
			// update blur behind
			window->OnBlurBehindUpdated();
			// update system backdrop
			if (uDWM::g_versionInfo.build >= os::build_w11_21h2)
			{
				window->SetDirtyFlags(0x4000u);
			}
		}
	}

	void ApplyCornerRadiusToWindowFrames(int radius)
	{
		if (uDWM::g_versionInfo.build >= os::build_w11_24h2)
		{
			return;
		}
		if (radius == -1)
		{
			return;
		}

		for (int i = 0; i < 6; i++)
		{
			// tool window frames should always have square corners
			if (i == 4 || i == 5)
			{
				uDWM::CTopLevelWindow::GetWindowFrames()[i]->GetCornerRadius() = 0;
			}
			else
			{
				uDWM::CTopLevelWindow::GetWindowFrames()[i]->GetCornerRadius() = radius;
			}
		}
	}
}

HRGN WINAPI GlassKernel::MyCreateRoundRectRgn(int x1, int y1, int x2, int y2, int w, int h)
{
	if (Shared::g_roundRectRadius == -1)
	{
		return g_CreateRoundRectRgn_Org(x1, y1, x2, y2, w, h);
	}

	g_diameter = w;
	g_roundedBounds = { x1, y1, x2, y2 };

	if (uDWM::g_versionInfo.build >= os::build_w11_24h2)
	{
		g_diameter = g_window && g_window->IsToolWindow() ? 0 : Shared::g_roundRectRadius * 2;
		// windows 11 25h2 gdi bug mitigation
		if (g_diameter)
		{
			return g_CreateRoundRectRgn_Org(x1, y1, x2, y2, g_diameter, g_diameter);
		}
		else
		{
			return g_CreateRectRgn_Org(x1, y1, x2, y2);
		}
	}
	return g_CreateRoundRectRgn_Org(x1, y1, x2, y2, w, h);
}
HRGN WINAPI GlassKernel::MyCreateRectRgn(int x1, int y1, int x2, int y2)
{
	if (x1 == 0 && y1 == 0 && x2 == 0 && y2 == 0)
	{
		return g_CreateRectRgn_Org(x1, y1, x2, y2);
	}

	if (g_redirectFirstCreateRectRgnCall)
	{
		if (g_redirectFirstCreateRectRgnCall.value() == true)
		{
			g_redirectFirstCreateRectRgnCall = false;
			return MyCreateRoundRectRgn(x1, y1, x2, y2, 0, 0);
		}
	}
	else
	{
		return MyCreateRoundRectRgn(x1, y1, x2, y2, 0, 0);
	}

	return g_CreateRectRgn_Org(x1, y1, x2, y2);
}

HRGN WINAPI GlassKernel::MyExtCreateRegion(const XFORM* lpx, DWORD nCount, const RGNDATA* lpData)
{
	const auto rectangles = reinterpret_cast<LPRECT>(const_cast<RGNDATA*>(lpData)->Buffer);
	if (lpData->rdh.nCount == 4)
	{
		// impl for glass8 .layout propertie DontDeflateInactiveFrameGeometry
		const auto dpi = static_cast<LONG>(uDWM::CDesktopManager::GetInstance()->GetDPIValue());
		if (
			Shared::g_dontDeflateInactiveFrameGeometry &&
			rectangles[0].top == dpi
		)
		{
			// top
			rectangles[0].top -= dpi;
			rectangles[0].left -= dpi;
			rectangles[0].right += dpi;
			// left
			rectangles[1].left -= dpi;
			// right
			rectangles[2].right += dpi;
			// bottom
			rectangles[3].bottom += dpi;
			rectangles[3].left -= dpi;
			rectangles[3].right += dpi;
		}
	}

	// glass8: KNOWN WIN10 LIMITATION: 
	//     - due to the way how Win10 composes windows frames, 
	//       it is not possible to fully apply corner radius
	// but i fixed it
	//
	// g_roundedBounds could be empty if window is maximized
	if (lpData->rdh.nCount == 1 && g_diameter && !IsRectEmpty(&g_roundedBounds))
	{
		const auto roundedRgnScope = wil::scope_exit([] { g_diameter = 0; g_roundedBounds = {}; });
		const HRGN captionRgn = g_ExtCreateRegion_Org(lpx, nCount, lpData);
		if (!captionRgn)
		{
			return captionRgn;
		}
		const wil::unique_hrgn roundedRgn{ g_CreateRoundRectRgn_Org(g_roundedBounds.left, g_roundedBounds.top, g_roundedBounds.right, g_roundedBounds.bottom, g_diameter, g_diameter) };
		if (!roundedRgn)
		{
			return captionRgn;
		}
		CombineRgn(captionRgn, captionRgn, roundedRgn.get(), RGN_AND);

		return captionRgn;
	}
	return g_ExtCreateRegion_Org(lpx, nCount, lpData);
}

HRESULT GlassKernel::MyIDCompositionDesktopDevice_WaitForCommitCompletion([[maybe_unused]] IDCompositionDesktopDevice* This)
{
	return S_OK;
}

HRESULT GlassKernel::MyCD2DContext_DestroyDeviceResources(dwmcore::CD2DContext* This)
{
	GlassRenderer::DestroyDeviceResources(This);
	GlassIntegrity::DestroyDeviceResources(This);
	return g_CD2DContext_DestroyDeviceResources_Org(This);
}

HRESULT GlassKernel::MyCDesktopManager_ReleaseDXGIAdapter(uDWM::CDesktopManager* This)
{
	CaptionTextHandler::DestroyDeviceResources();
	return g_CDesktopManager_ReleaseDXGIAdapter_Org(This);
}

HRESULT GlassKernel::MyCGraphicsDeviceManager_ReleaseGraphicsDevice(PVOID This)
{
	CaptionTextHandler::DestroyDeviceResources();
	return g_CGraphicsDeviceManager_ReleaseGraphicsDevice_Org(This);
}

HRESULT GlassKernel::MyCTopLevelWindow_EnsureImages_Pre_W10_1903(uDWM::IDwmChannel* channel)
{
	const auto result = g_CTopLevelWindow_EnsureImages_Pre_W10_1903_Org(channel);
	ApplyCornerRadiusToWindowFrames(Shared::g_roundRectRadius);
	return result;
}
HRESULT GlassKernel::MyCTopLevelWindow_EnsureImages_At_Least_W10_1903()
{
	const auto result = g_CTopLevelWindow_EnsureImages_At_Least_W10_1903_Org();
	ApplyCornerRadiusToWindowFrames(Shared::g_roundRectRadius);
	return result;
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

float GlassKernel::GetBlurRadius()
{
	if (Shared::IsTransparencyDisabled())
	{
		return 0.f;
	}
	if (Shared::g_useD3DRendering)
	{
		return CD3DGlassRealizer::GetBlurRadius(Shared::g_blurAmount);
	}

	return CGlassRealizer::GetBlurRadius(Shared::g_blurAmount);
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
		Shared::g_blurAmount = std::clamp(static_cast<float>(GlassEngine::GetDwordFromRegistry(L"BlurDeviation", 30)) / 10.f * 3.f, 0.f, 250.f);
		Shared::g_blurOptimization = static_cast<D2D1_GAUSSIANBLUR_OPTIMIZATION>(std::clamp(GlassEngine::GetDwordFromRegistry(L"BlurOptimization", 0), 0ul, 2ul));
		Shared::g_roundRectRadius = static_cast<int>(GlassEngine::GetDwordFromRegistry(L"RoundRectRadius"));
		ApplyCornerRadiusToWindowFrames(Shared::g_roundRectRadius);
		
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
	winrt::com_ptr<IDCompositionDesktopDevice> dcompDevice{ nullptr };
	FAIL_FAST_IF_FAILED_MSG(uDWM::CDesktopManager::GetInstance()->GetInteropCompositorDCompDevicePartner()->QueryInterface(dcompDevice.put()), "Unable to obtain IDCompositionDesktopDevice");
	g_IDCompositionDesktopDevice_WaitForCommitCompletion_Org_Address = reinterpret_cast<decltype(g_IDCompositionDesktopDevice_WaitForCommitCompletion_Org_Address)>(&(HookHelper::get_vftable_from(dcompDevice.get())[4]));
	g_IDCompositionDesktopDevice_WaitForCommitCompletion_Hook.Prepare(
		g_IDCompositionDesktopDevice_WaitForCommitCompletion_Org_Address,
		&g_IDCompositionDesktopDevice_WaitForCommitCompletion_Org
	);
	HookHelper::GetCurrentHookTransaction().Apply(g_IDCompositionDesktopDevice_WaitForCommitCompletion_Hook);

	const auto build_before_w11_24h2 = uDWM::g_versionInfo.build < os::build_w11_24h2;
	const auto build_before_server_2022 = uDWM::g_versionInfo.build < os::build_server_2022;
	g_CreateRoundRectRgn_Org = CreateRoundRectRgn;
	g_gdiImportHooks.Prepare(
		uDWM::g_moduleHandle,
		std::initializer_list<HookHelper::ImportDllDetourInfo>
		{
			{
				"gdi32.dll",
				std::initializer_list<HookHelper::ImportFunctionDetourInfo>
				{
					HookHelper::MakeImportDetour<&MyCreateRoundRectRgn>("CreateRoundRectRgn", &g_CreateRoundRectRgn_Org, build_before_w11_24h2),
					HookHelper::MakeImportDetour<&MyCreateRectRgn>("CreateRectRgn", &g_CreateRectRgn_Org, !build_before_w11_24h2),
					HookHelper::MakeImportDetour<&MyExtCreateRegion>("ExtCreateRegion", &g_ExtCreateRegion_Org)
				}
			}
		}
	);
	HookHelper::GetCurrentHookTransaction().Apply(g_gdiImportHooks);

	const auto build_before_w10_1903 = uDWM::g_versionInfo.build < os::build_w10_1903;
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CD2DContext_DestroyDeviceResources_Org, &MyCD2DContext_DestroyDeviceResources },
			{ &g_CDesktopManager_ReleaseDXGIAdapter_Org, &MyCDesktopManager_ReleaseDXGIAdapter, build_before_server_2022 },
			{ &g_CGraphicsDeviceManager_ReleaseGraphicsDevice_Org, &MyCGraphicsDeviceManager_ReleaseGraphicsDevice, !build_before_server_2022 },
			{ &g_CTopLevelWindow_EnsureImages_Pre_W10_1903_Org, &MyCTopLevelWindow_EnsureImages_Pre_W10_1903, build_before_w10_1903 },
			{ &g_CTopLevelWindow_EnsureImages_At_Least_W10_1903_Org, &MyCTopLevelWindow_EnsureImages_At_Least_W10_1903, build_before_w11_24h2 && !build_before_w10_1903 }
		},
		true
	);
}

void GlassKernel::Shutdown()
{
	const auto build_before_w10_1903 = uDWM::g_versionInfo.build < os::build_w10_1903;
	const auto build_before_w11_24h2 = uDWM::g_versionInfo.build < os::build_w11_24h2;
	const auto build_before_server_2022 = uDWM::g_versionInfo.build < os::build_server_2022;
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CD2DContext_DestroyDeviceResources_Org, &MyCD2DContext_DestroyDeviceResources },
			{ &g_CDesktopManager_ReleaseDXGIAdapter_Org, &MyCDesktopManager_ReleaseDXGIAdapter, build_before_server_2022 },
			{ &g_CGraphicsDeviceManager_ReleaseGraphicsDevice_Org, &MyCGraphicsDeviceManager_ReleaseGraphicsDevice, !build_before_server_2022 },
			{ &g_CTopLevelWindow_EnsureImages_Pre_W10_1903_Org, &MyCTopLevelWindow_EnsureImages_Pre_W10_1903, build_before_w10_1903 },
			{ &g_CTopLevelWindow_EnsureImages_At_Least_W10_1903_Org, &MyCTopLevelWindow_EnsureImages_At_Least_W10_1903, build_before_w11_24h2 && !build_before_w10_1903 }
		},
		false
	);

	HookHelper::GetCurrentHookTransaction().Apply(g_gdiImportHooks);

	HookHelper::GetCurrentHookTransaction().Apply(g_IDCompositionDesktopDevice_WaitForCommitCompletion_Hook);
	
}

void GlassKernel::Cleanup()
{
	ApplyCornerRadiusToWindowFrames(0);
	GlassReflectionBrush::RemoveAll();
	GlassEffectBrush::RemoveAll();
}
