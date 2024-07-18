#include "pch.h"
#include "GlassFramework.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "GeometryRecorder.hpp"
#include "VisualManager.hpp"
#include "GlassSharedData.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassFramework
{
	HRGN WINAPI MyCreateRoundRectRgn(int x1, int y1, int x2, int y2, int w, int h);
	HRESULT STDMETHODCALLTYPE MyCDrawGeometryInstruction_Create(uDwm::CBaseLegacyMilBrushProxy* brush, uDwm::CBaseGeometryProxy* geometry, uDwm::CDrawGeometryInstruction** instruction);
	HRESULT STDMETHODCALLTYPE MyCTopLevelWindow_UpdateNCAreaBackground(uDwm::CTopLevelWindow* This);
	HRESULT STDMETHODCALLTYPE MyCTopLevelWindow_UpdateClientBlur(uDwm::CTopLevelWindow* This);
	HRESULT STDMETHODCALLTYPE MyCAccent_UpdateAccentPolicy(uDwm::CAccent* This, LPCRECT lprc, uDwm::ACCENT_POLICY* policy, uDwm::CBaseGeometryProxy* geometry);
	HRESULT STDMETHODCALLTYPE MyCAccent__UpdateSolidFill(uDwm::CAccent* This, uDwm::CRenderDataVisual* visual, DWORD color, const D2D1_RECT_F* lprc, float opacity);
	HRESULT STDMETHODCALLTYPE MyCRenderDataVisual_AddInstruction(uDwm::CRenderDataVisual* This, uDwm::CRenderDataInstruction* instruction);
	DWORD STDMETHODCALLTYPE MyCTopLevelWindow_CalculateBackgroundType(uDwm::CTopLevelWindow* This);
	HRESULT STDMETHODCALLTYPE MyCTopLevelWindow_UpdateSystemBackdropVisual(uDwm::CTopLevelWindow* This);
	void STDMETHODCALLTYPE MyCTopLevelWindow_Destructor(uDwm::CTopLevelWindow* This);

	decltype(&MyCreateRoundRectRgn) g_CreateRoundRectRgn_Org{ nullptr };
	decltype(&MyCDrawGeometryInstruction_Create) g_CDrawGeometryInstruction_Create_Org{ nullptr };
	decltype(&MyCTopLevelWindow_UpdateNCAreaBackground) g_CTopLevelWindow_UpdateNCAreaBackground_Org{ nullptr };
	decltype(&MyCTopLevelWindow_UpdateClientBlur) g_CTopLevelWindow_UpdateClientBlur_Org{ nullptr };
	decltype(&MyCAccent_UpdateAccentPolicy) g_CAccent_UpdateAccentPolicy_Org{ nullptr };
	decltype(&MyCAccent__UpdateSolidFill) g_CAccent__UpdateSolidFill_Org{ nullptr };
	decltype(&MyCRenderDataVisual_AddInstruction) g_CRenderDataVisual_AddInstruction_Org{ nullptr };
	decltype(&MyCTopLevelWindow_CalculateBackgroundType) g_CTopLevelWindow_CalculateBackgroundType_Org{ nullptr };
	decltype(&MyCTopLevelWindow_UpdateSystemBackdropVisual) g_CTopLevelWindow_UpdateSystemBackdropVisual_Org{ nullptr };
	decltype(&MyCTopLevelWindow_Destructor) g_CTopLevelWindow_Destructor_Org{ nullptr };

	uDwm::CTopLevelWindow* g_capturedWindow{ nullptr };
	uDwm::CRenderDataVisual* g_accentRenderDataVisual{ nullptr };
	DWORD g_accentState{};
	int g_roundRectRadius{};
}

HRGN WINAPI GlassFramework::MyCreateRoundRectRgn(int x1, int y1, int x2, int y2, int w, int h)
{
	if (g_roundRectRadius == -1)
	{
		return g_CreateRoundRectRgn_Org(x1, y1, x2, y2, w, h);
	}

	return g_CreateRoundRectRgn_Org(x1, y1, x2, y2, g_roundRectRadius, g_roundRectRadius);
}

// restore the blur region set by DwmEnableBlurBehind and make sure the region isn't overlap with the non client region
HRESULT STDMETHODCALLTYPE GlassFramework::MyCDrawGeometryInstruction_Create(uDwm::CBaseLegacyMilBrushProxy* brush, uDwm::CBaseGeometryProxy* geometry, uDwm::CDrawGeometryInstruction** instruction)
{
	if (g_capturedWindow && g_capturedWindow->GetData()->GetHwnd() != uDwm::GetShellWindowForCurrentDesktop())
	{
		HRGN region{ GeometryRecorder::GetRegionFromGeometry(geometry) };
		RECT rgnBox{};
		if (GetRgnBox(region, &rgnBox) != NULLREGION && !IsRectEmpty(&rgnBox))
		{
			winrt::com_ptr<uDwm::CRgnGeometryProxy> rgnGeometry{ nullptr };
			uDwm::ResourceHelper::CreateGeometryFromHRGN(region, rgnGeometry.put());
			winrt::com_ptr<uDwm::CSolidColorLegacyMilBrushProxy> solidBrush{ nullptr };
			RETURN_IF_FAILED(
				uDwm::CDesktopManager::s_pDesktopManagerInstance->GetCompositor()->CreateSolidColorLegacyMilBrushProxy(
					solidBrush.put()
				)
			);
			auto color{ g_capturedWindow->GetTitlebarColorizationParameters()->getArgbcolor() };
			color.a *= 0.99f;
			if (GlassSharedData::g_type == Type::Aero)
				color.r = g_capturedWindow->TreatAsActiveWindow();
			RETURN_IF_FAILED(solidBrush->Update(1.0, color));
			return g_CDrawGeometryInstruction_Create_Org(solidBrush.get(), rgnGeometry.get(), instruction);
		}
	}

	return g_CDrawGeometryInstruction_Create_Org(brush, geometry, instruction);
}

// convert the draw geometry instruction into draw glass instruction
// and make sure the borders are splitted to improve performance
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_UpdateNCAreaBackground(uDwm::CTopLevelWindow* This)
{
	if (!GlassSharedData::IsBackdropAllowed())
	{
		return g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);
	}
	auto data{ This->GetData() };
	if (!data)
	{
		return g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);
	}

	HRESULT hr{ S_OK };

	if (This->HasNonClientBackground())
	{
		GeometryRecorder::BeginCapture();

		DWORD oldSystemBackdropType{ 0 };
		if (os::buildNumber == os::build_w11_21h2)
		{
			oldSystemBackdropType = *reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(data) + 204);
			*reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(data) + 204) = 0;
		}

		hr = g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);

		if (os::buildNumber == os::build_w11_21h2)
		{
			*reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(data) + 204) = oldSystemBackdropType;
		}

		if (SUCCEEDED(hr))
		{
			auto legacyVisualOverride{ VisualManager::GetOrCreateLegacyVisualOverrider(This, true) };
			// the titlebar region has been updated
			// let's update our backdrop region
			if (GeometryRecorder::GetGeometryCount() && legacyVisualOverride)
			{
				auto captionGeometry{ This->GetCaptionGeometry() };
				auto borderGeometry{ This->GetBorderGeometry() };

				HRGN captionRegion{ GeometryRecorder::GetRegionFromGeometry(captionGeometry) };
				HRGN borderRegion{ GeometryRecorder::GetRegionFromGeometry(borderGeometry) };
				
				hr = legacyVisualOverride->UpdateNCBackground(captionRegion, borderRegion);
			}
		}

		GeometryRecorder::EndCapture();
	}
	else
	{
		VisualManager::RemoveLegacyVisualOverrider(This);
		hr = g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);
	}

	return hr;
}

// make the visual of DwmEnableBlurBehind visible
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_UpdateClientBlur(uDwm::CTopLevelWindow* This)
{
	if (!GlassSharedData::IsBackdropAllowed())
	{
		return g_CTopLevelWindow_UpdateClientBlur_Org(This);
	}
	auto data{ This->GetData() };
	if (!data)
	{
		return g_CTopLevelWindow_UpdateClientBlur_Org(This);
	}

	g_capturedWindow = This;
	GeometryRecorder::BeginCapture();
	HRESULT hr{ g_CTopLevelWindow_UpdateClientBlur_Org(This) };
	GeometryRecorder::EndCapture();
	g_capturedWindow = nullptr;

	return hr;
}

// convert accent_state=3 or 4 into 2 and replace its solid rectangle instruction into draw glass instruction
HRESULT STDMETHODCALLTYPE GlassFramework::MyCAccent_UpdateAccentPolicy(uDwm::CAccent* This, LPCRECT lprc, uDwm::ACCENT_POLICY* policy, uDwm::CBaseGeometryProxy* geometry)
{
	if (!GlassSharedData::IsBackdropAllowed())
	{
		return g_CAccent_UpdateAccentPolicy_Org(This, lprc, policy, geometry);
	}
	if (!GlassSharedData::g_overrideAccent)
	{
		return g_CAccent_UpdateAccentPolicy_Org(This, lprc, policy, geometry);
	}

	HRESULT hr{ S_OK };
	auto accentPolicy{ *policy };
	if (accentPolicy.AccentState == 3 || accentPolicy.AccentState == 4)
	{
		accentPolicy.AccentState = 2;
		hr = g_CAccent_UpdateAccentPolicy_Org(This, lprc, &accentPolicy, geometry);
	}
	else
	{
		hr = g_CAccent_UpdateAccentPolicy_Org(This, lprc, policy, geometry);
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE GlassFramework::MyCAccent__UpdateSolidFill(uDwm::CAccent* This, uDwm::CRenderDataVisual* visual, DWORD color, const D2D1_RECT_F* lprc, float opacity)
{
	if (!GlassSharedData::IsBackdropAllowed())
	{
		return g_CAccent__UpdateSolidFill_Org(This, visual, color, lprc, opacity);
	}
	if (!GlassSharedData::g_overrideAccent)
	{
		return g_CAccent__UpdateSolidFill_Org(This, visual, color, lprc, opacity);
	}
	if (!This->GetHwnd())
	{
		return g_CAccent__UpdateSolidFill_Org(This, visual, color, lprc, opacity);
	}
	uDwm::CWindowData* data{ nullptr };
	{
		auto lock{ wil::EnterCriticalSection(uDwm::CDesktopManager::s_csDwmInstance) };
		if (FAILED(uDwm::CDesktopManager::s_pDesktopManagerInstance->GetWindowList()->GetSyncedWindowDataByHwnd(This->GetHwnd(), &data)) || !data)
		{
			return g_CAccent__UpdateSolidFill_Org(This, visual, color, lprc, opacity);
		}
	}
	g_accentState = data->GetAccentPolicy()->AccentState;
	if (g_accentState != 3 && g_accentState != 4)
	{
		return g_CAccent__UpdateSolidFill_Org(This, visual, color, lprc, opacity);
	}

	HRESULT hr{ S_OK };
	g_accentRenderDataVisual = visual;
	hr = g_CAccent__UpdateSolidFill_Org(This, visual, color, lprc, opacity);
	g_accentRenderDataVisual = nullptr;

	return hr;
}

HRESULT STDMETHODCALLTYPE GlassFramework::MyCRenderDataVisual_AddInstruction(uDwm::CRenderDataVisual* This, uDwm::CRenderDataInstruction* instruction)
{
	if (!GlassSharedData::IsBackdropAllowed())
	{
		return g_CRenderDataVisual_AddInstruction_Org(This, instruction);
	}
	if (!GlassSharedData::g_overrideAccent)
	{
		return g_CRenderDataVisual_AddInstruction_Org(This, instruction);
	}
	if (g_accentRenderDataVisual != This)
	{
		return g_CRenderDataVisual_AddInstruction_Org(This, instruction);
	}

	auto drawRectangleInstruction{ reinterpret_cast<uDwm::CSolidRectangleInstruction*>(instruction) };
	auto rectangle{ drawRectangleInstruction->GetRectangle() };
	auto color{ drawRectangleInstruction->GetColor() };
	if (g_accentState == 4 && color.a == 0.f && color.r == 0.f && color.g == 0.f && color.b == 0.f)
	{
		return g_CRenderDataVisual_AddInstruction_Org(This, instruction);
	}

	color.a = 0.99f;
	if (GlassSharedData::g_type == Type::Aero)
		color.r = 1.0f;
	winrt::com_ptr<uDwm::CRgnGeometryProxy> rgnGeometry{ nullptr };
	uDwm::ResourceHelper::CreateGeometryFromHRGN(wil::unique_hrgn{ CreateRectRgn(static_cast<LONG>(rectangle.left), static_cast<LONG>(rectangle.top), static_cast<LONG>(rectangle.right), static_cast<LONG>(rectangle.bottom)) }.get(), rgnGeometry.put());
	winrt::com_ptr<uDwm::CSolidColorLegacyMilBrushProxy> solidBrush{ nullptr };
	RETURN_IF_FAILED(
		uDwm::CDesktopManager::s_pDesktopManagerInstance->GetCompositor()->CreateSolidColorLegacyMilBrushProxy(
			solidBrush.put()
		)
	);
	RETURN_IF_FAILED(solidBrush->Update(1.0, color));
	winrt::com_ptr<uDwm::CDrawGeometryInstruction> drawInstruction{ nullptr };
	RETURN_IF_FAILED(uDwm::CDrawGeometryInstruction::Create(solidBrush.get(), rgnGeometry.get(), drawInstruction.put()));

	return g_CRenderDataVisual_AddInstruction_Org(This, drawInstruction.get());
}

// we trick dwm into thinking the window is using legacy nonclient background
/*
enum class BackgroundType
{
	Legacy,
	Accent,
	SystemBackdrop_BackdropMaterial,
	SystemBackdrop_CaptionAccentColor,
	SystemBackdrop_Default
};
*/
DWORD STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_CalculateBackgroundType(uDwm::CTopLevelWindow* This)
{
	if (!GlassSharedData::IsBackdropAllowed())
	{
		return g_CTopLevelWindow_CalculateBackgroundType_Org(This);
	}

	auto result{ g_CTopLevelWindow_CalculateBackgroundType_Org(This) };
	if (result == 4 || result == 3 || result == 2)
	{
		result = 0;
	}

	return result;
}

// trick dwm into thinking the system backdrop is not exist
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_UpdateSystemBackdropVisual(uDwm::CTopLevelWindow* This)
{
	if (!GlassSharedData::IsBackdropAllowed())
	{
		return g_CTopLevelWindow_UpdateSystemBackdropVisual_Org(This);
	}
	auto data{ This->GetData() };
	if (!data)
	{
		return g_CTopLevelWindow_UpdateSystemBackdropVisual_Org(This);
	}

	HRESULT hr{ S_OK };
	auto oldSystemBackdropType{ *reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(data) + 204) };
	// trick dwm into thinking the window does not enable system backdrop
	*reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(data) + 204) = 0;
	hr = g_CTopLevelWindow_UpdateSystemBackdropVisual_Org(This);
	*reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(data) + 204) = oldSystemBackdropType;

	return hr;
}

// release resources
void STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_Destructor(uDwm::CTopLevelWindow* This)
{
	VisualManager::RemoveLegacyVisualOverrider(This);
	g_CTopLevelWindow_Destructor_Org(This);
}

void GlassFramework::UpdateConfiguration(ConfigurationFramework::UpdateType type)
{
	if (type & ConfigurationFramework::UpdateType::Framework)
	{
		GlassSharedData::g_disableOnBattery = static_cast<bool>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"DisableGlassOnBattery", TRUE));
		GlassSharedData::g_batteryMode = Utils::IsBatterySaverEnabled();
	}
	if (type & ConfigurationFramework::UpdateType::Backdrop)
	{
		GlassSharedData::g_transparencyEnabled = Utils::IsTransparencyEnabled(ConfigurationFramework::GetPersonalizeKey());
		GlassSharedData::g_overrideAccent = static_cast<bool>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"GlassOverrideAccent"));
		g_roundRectRadius = static_cast<int>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"RoundRectRadius"));
	}

	//Seperate keys for now until a way to readd the code that handles setting the actual values in registry is found
	GlassSharedData::g_ColorizationAfterglowBalance = ((float)ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"OG_ColorizationAfterglowBalance",43) / 100);
	GlassSharedData::g_ColorizationBlurBalance = ((float)ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"OG_ColorizationBlurBalance",49) / 100);
	GlassSharedData::g_ColorizationColorBalance = ((float)ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"OG_ColorizationColorBalance",8) / 100);

	//Get the colorizationColor from registry directly for aero glass type, a dwm function could be used, however mods or programs such as AWM hook into this and can
	//cause issues, so the colour is taken directly from registry, which is fine for aero glass (actually better) since inactive and active have the same colour
	DWORD hexColour = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationColor", 0xfffcb874);
	GlassSharedData::g_ColorizationColor = Utils::FromArgb(hexColour);

	auto lock{ wil::EnterCriticalSection(uDwm::CDesktopManager::s_csDwmInstance) };
	if (!GlassSharedData::IsBackdropAllowed())
	{
		VisualManager::ShutdownLegacyVisualOverrider();
	}
	else
	{
		ULONG_PTR desktopID{ 0 };
		Utils::GetDesktopID(1, &desktopID);
		auto windowList{ uDwm::CDesktopManager::s_pDesktopManagerInstance->GetWindowList()->GetWindowListForDesktop(desktopID) };
		for (auto i{ windowList->Blink }; i != windowList; i = i->Blink)
		{
			auto data{ reinterpret_cast<uDwm::CWindowData*>(i) };
			auto hwnd{ data->GetHwnd() };
			if (!hwnd || !IsWindow(hwnd)) { continue; }
			auto window{ data->GetWindow() };
			if (!window) { continue; }

			VisualManager::RedrawTopLevelWindow(window);
			LOG_IF_FAILED(window->ValidateVisual());
		}
	}
}

HRESULT GlassFramework::Startup()
{
	uDwm::GetAddressFromSymbolMap("CDrawGeometryInstruction::Create", g_CDrawGeometryInstruction_Create_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::UpdateNCAreaBackground", g_CTopLevelWindow_UpdateNCAreaBackground_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::UpdateClientBlur", g_CTopLevelWindow_UpdateClientBlur_Org);
	uDwm::GetAddressFromSymbolMap("CAccent::UpdateAccentPolicy", g_CAccent_UpdateAccentPolicy_Org);
	uDwm::GetAddressFromSymbolMap("CAccent::_UpdateSolidFill", g_CAccent__UpdateSolidFill_Org);
	uDwm::GetAddressFromSymbolMap("CRenderDataVisual::AddInstruction", g_CRenderDataVisual_AddInstruction_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::CalculateBackgroundType", g_CTopLevelWindow_CalculateBackgroundType_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::UpdateSystemBackdropVisual", g_CTopLevelWindow_UpdateSystemBackdropVisual_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::~CTopLevelWindow", g_CTopLevelWindow_Destructor_Org);

	g_CreateRoundRectRgn_Org = reinterpret_cast<decltype(g_CreateRoundRectRgn_Org)>(HookHelper::WriteIAT(uDwm::g_moduleHandle, "gdi32.dll", "CreateRoundRectRgn", MyCreateRoundRectRgn));
	
	
	return HookHelper::Detours::Write([]()
	{
		HookHelper::Detours::Attach(&g_CDrawGeometryInstruction_Create_Org, MyCDrawGeometryInstruction_Create);
		HookHelper::Detours::Attach(&g_CTopLevelWindow_UpdateNCAreaBackground_Org, MyCTopLevelWindow_UpdateNCAreaBackground);
		HookHelper::Detours::Attach(&g_CTopLevelWindow_UpdateClientBlur_Org, MyCTopLevelWindow_UpdateClientBlur);
		HookHelper::Detours::Attach(&g_CAccent_UpdateAccentPolicy_Org, MyCAccent_UpdateAccentPolicy);
		HookHelper::Detours::Attach(&g_CAccent__UpdateSolidFill_Org, MyCAccent__UpdateSolidFill);
		HookHelper::Detours::Attach(&g_CRenderDataVisual_AddInstruction_Org, MyCRenderDataVisual_AddInstruction);
		HookHelper::Detours::Attach(&g_CTopLevelWindow_Destructor_Org, MyCTopLevelWindow_Destructor);
		if (os::buildNumber == os::build_w11_21h2)
		{
			HookHelper::Detours::Attach(&g_CTopLevelWindow_UpdateSystemBackdropVisual_Org, MyCTopLevelWindow_UpdateSystemBackdropVisual);
		}
		if (os::buildNumber >= os::build_w11_22h2)
		{
			HookHelper::Detours::Attach(&g_CTopLevelWindow_CalculateBackgroundType_Org, MyCTopLevelWindow_CalculateBackgroundType);
		}
	});
}

void GlassFramework::Shutdown()
{
	HookHelper::Detours::Write([]()
	{
		HookHelper::Detours::Detach(&g_CDrawGeometryInstruction_Create_Org, MyCDrawGeometryInstruction_Create);
		HookHelper::Detours::Detach(&g_CTopLevelWindow_UpdateNCAreaBackground_Org, MyCTopLevelWindow_UpdateNCAreaBackground);
		HookHelper::Detours::Detach(&g_CTopLevelWindow_UpdateClientBlur_Org, MyCTopLevelWindow_UpdateClientBlur);
		HookHelper::Detours::Detach(&g_CAccent_UpdateAccentPolicy_Org, MyCAccent_UpdateAccentPolicy);
		HookHelper::Detours::Detach(&g_CAccent__UpdateSolidFill_Org, MyCAccent__UpdateSolidFill);
		HookHelper::Detours::Detach(&g_CRenderDataVisual_AddInstruction_Org, MyCRenderDataVisual_AddInstruction);
		HookHelper::Detours::Detach(&g_CTopLevelWindow_Destructor_Org, MyCTopLevelWindow_Destructor);
		if (os::buildNumber == os::build_w11_21h2)
		{
			HookHelper::Detours::Detach(&g_CTopLevelWindow_UpdateSystemBackdropVisual_Org, MyCTopLevelWindow_UpdateSystemBackdropVisual);
		}
		if (os::buildNumber >= os::build_w11_22h2)
		{
			HookHelper::Detours::Detach(&g_CTopLevelWindow_CalculateBackgroundType_Org, MyCTopLevelWindow_CalculateBackgroundType);
		}
	});

	if (g_CreateRoundRectRgn_Org)
	{
		HookHelper::WriteIAT(uDwm::g_moduleHandle, "gdi32.dll", "CreateRoundRectRgn", g_CreateRoundRectRgn_Org);
	}

	g_capturedWindow = nullptr;
	VisualManager::ShutdownLegacyVisualOverrider();
	ULONG_PTR desktopID{ 0 };
	Utils::GetDesktopID(1, &desktopID);
	auto windowList{ uDwm::CDesktopManager::s_pDesktopManagerInstance->GetWindowList()->GetWindowListForDesktop(desktopID) };
	for (auto i{ windowList->Blink }; i != windowList; i = i->Blink)
	{
		auto data{ reinterpret_cast<uDwm::CWindowData*>(i) };
		auto hwnd{ data->GetHwnd() };
		if (!hwnd || !IsWindow(hwnd)) { continue; }
		auto window{ data->GetWindow() };
		if (!window) { continue; }

		VisualManager::RedrawTopLevelWindow(window);
		LOG_IF_FAILED(window->ValidateVisual());
	}
}