#include "pch.h"
#include "MsstyleInternals.hpp"
#include "GlassInfra.hpp"
#include "uDwmProjection.hpp"
#include "Shared.hpp"
#include "dwmcoreProjection.hpp"
#include "ReflectionVisual.hpp"
#include "GlassRenderer.hpp"
#include "CaptionTextHandler.hpp"
#include "CustomThemeAtlasLoader.hpp"
#include "BlurSettings.hpp"
#include "SystemSettings.hpp"

using namespace OpenGlass;

HRESULT GlassInfra::UpdateGlassMarker(uDWM::CSolidRectangleVisual* visual, uDWM::CBaseGeometryProxy* geometry, std::bitset<2> status)
{
	winrt::com_ptr<uDWM::CCombinedGeometryProxy> marker;
	RETURN_IF_FAILED(uDWM::ResourceHelper::CreateCombinedGeometry(geometry, nullptr, static_cast<D2D1_COMBINE_MODE>(status.to_ulong()), marker.put()));
	return visual->UpdateClip(marker.get());
}

namespace OpenGlass::GlassInfra
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
			// update accent
			window->OnAccentPolicyUpdated();
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

HRESULT GlassInfra::MyCD2DContext_DestroyDeviceResources(dwmcore::CD2DContext* This)
{
	GlassRenderer::DestroyDeviceResources(This);
	return g_CD2DContext_DestroyDeviceResources_Org(This);
}

HRESULT GlassInfra::MyCGraphicsDeviceManager_ReleaseGraphicsDevice(uDWM::CGraphicsDeviceManager* This)
{
	CaptionTextHandler::DestroyDeviceResources();
	g_reflectionSurface = nullptr;
	return g_CGraphicsDeviceManager_ReleaseGraphicsDevice_Org(This);
}

void GlassInfra::Update(GlassEngine::UpdateType type)
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
	if (type & (GlassEngine::UpdateType::Framework | GlassEngine::UpdateType::Backdrop))
	{
		Shared::g_disableOnBattery = static_cast<bool>(
			GlassEngine::GetDwordFromRegistry(
				L"DisableGlassOnBattery",
				1
			)
		);
		Shared::g_energySaverActive = Util::IsEnergySaverEnabled();
	}
	if (type & GlassEngine::UpdateType::Backdrop)
	{
		Shared::g_uxMitigationFlags = GlassEngine::GetDwordFromRegistry(L"EnableUxMitigation", UxMitigation::DefaultFlags);

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

void GlassInfra::Startup()
{

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CD2DContext_DestroyDeviceResources_Org },
			{ &g_CGraphicsDeviceManager_ReleaseGraphicsDevice_Org }
		}
	);
}

void GlassInfra::Shutdown()
{
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CD2DContext_DestroyDeviceResources_Org },
			{ &g_CGraphicsDeviceManager_ReleaseGraphicsDevice_Org }
		}
	);

}

void GlassInfra::Cleanup()
{
	CReflectionVisual::RemoveAll();
	g_reflectionSurface = nullptr;
}
