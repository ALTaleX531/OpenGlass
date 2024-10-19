#include "pch.h"
#include "GlassFramework.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "GeometryRecorder.hpp"
#include "VisualManager.hpp"
#include "Shared.hpp"

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

	void STDMETHODCALLTYPE MyCAnimatedGlassSheet_OnRectUpdated(uDwm::CAnimatedGlassSheet* This, LPCRECT lprc);
	void STDMETHODCALLTYPE MyCAnimatedGlassSheet_Destructor(uDwm::CAnimatedGlassSheet* This);

	HRESULT STDMETHODCALLTYPE MyCLivePreview__UpdateGlassVisual(uDwm::CLivePreview* This);
	HRESULT STDMETHODCALLTYPE MyCTopLevelWindow_CloneVisualTreeForLivePreview(
		uDwm::CTopLevelWindow* This, 
		bool cloneForReflection, 
		bool reserved1, 
		bool reserved2, 
		uDwm::CTopLevelWindow** cloned
	);

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

	decltype(&MyCAnimatedGlassSheet_OnRectUpdated) g_CAnimatedGlassSheet_OnRectUpdated_Org{ nullptr };
	decltype(&MyCAnimatedGlassSheet_Destructor) g_CAnimatedGlassSheet_Destructor_Org{ nullptr };

	decltype(&MyCLivePreview__UpdateGlassVisual) g_CLivePreview__UpdateGlassVisual_Org{ nullptr };
	decltype(&MyCTopLevelWindow_CloneVisualTreeForLivePreview) g_CTopLevelWindow_CloneVisualTreeForLivePreview_Org{ nullptr };

	uDwm::CTopLevelWindow* g_capturedWindow{ nullptr };
	uDwm::CRenderDataVisual* g_accentRenderDataVisual{ nullptr };
	DWORD g_accentState{};

	UINT g_dwOverlayTestMode{};
	winrt::com_ptr<IDCompositionVisual2> g_hackVisual{ nullptr };
}

HRGN WINAPI GlassFramework::MyCreateRoundRectRgn(int x1, int y1, int x2, int y2, int w, int h)
{
	if (Shared::g_roundRectRadius == -1)
	{
		return g_CreateRoundRectRgn_Org(x1, y1, x2, y2, w, h);
	}

	return g_CreateRoundRectRgn_Org(x1, y1, x2, y2, Shared::g_roundRectRadius, Shared::g_roundRectRadius);
}

// restore the blur region set by DwmEnableBlurBehind and make sure the region isn't overlap with the non client region
HRESULT STDMETHODCALLTYPE GlassFramework::MyCDrawGeometryInstruction_Create(uDwm::CBaseLegacyMilBrushProxy* brush, uDwm::CBaseGeometryProxy* geometry, uDwm::CDrawGeometryInstruction** instruction)
{
	if (g_capturedWindow && g_capturedWindow->GetData()->GetHwnd() != uDwm::GetShellWindowForCurrentDesktop())
	{
		auto color =
			Shared::g_forceAccentColorization ?
			dwmcore::Convert_D2D1_COLOR_F_sRGB_To_D2D1_COLOR_F_scRGB(g_capturedWindow->TreatAsActiveWindow() ? Shared::g_accentColor : Shared::g_accentColorInactive) :
			g_capturedWindow->GetTitlebarColorizationParameters()->getArgbcolor();
		color.a = g_capturedWindow->TreatAsActiveWindow() ? 0.5f : 0.0f;
		RETURN_IF_FAILED(reinterpret_cast<uDwm::CSolidColorLegacyMilBrushProxy*>(brush)->Update(1.0, color));
	}

	return g_CDrawGeometryInstruction_Create_Org(brush, geometry, instruction);
}

// convert the draw geometry instruction into draw glass instruction
// and make sure the borders are splitted to improve performance
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_UpdateNCAreaBackground(uDwm::CTopLevelWindow* This)
{
	if (!Shared::IsBackdropAllowed())
	{
		return g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);
	}
	auto data = This->GetData();
	if (!data)
	{
		return g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);
	}

	HRESULT hr{ S_OK };

	if (auto brush = This->GetClientBlurVisualBrush(); brush)
	{
		auto color =
			Shared::g_forceAccentColorization ?
			dwmcore::Convert_D2D1_COLOR_F_sRGB_To_D2D1_COLOR_F_scRGB(This->TreatAsActiveWindow() ? Shared::g_accentColor : Shared::g_accentColorInactive) :
			This->GetTitlebarColorizationParameters()->getArgbcolor();
		color.a = This->TreatAsActiveWindow() ? 0.5f : 0.0f;
		LOG_IF_FAILED(brush->Update(1.0, color));
	}
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
			auto legacyVisualOverride = VisualManager::LegacyVisualOverrider::GetOrCreate(This, true);
			// the titlebar region has been updated
			// let's update our backdrop region
			if (GeometryRecorder::GetGeometryCount() && legacyVisualOverride)
			{
				auto captionGeometry = This->GetCaptionGeometry();
				auto borderGeometry = This->GetBorderGeometry();

				HRGN captionRegion{ GeometryRecorder::GetRegionFromGeometry(captionGeometry) };
				HRGN borderRegion{ GeometryRecorder::GetRegionFromGeometry(borderGeometry) };
				
				hr = legacyVisualOverride->UpdateNCBackground(captionRegion, borderRegion);
			}
		}

		GeometryRecorder::EndCapture();
	}
	else
	{
		VisualManager::LegacyVisualOverrider::Remove(This);
		hr = g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);
	}

	return hr;
}

// make the visual of DwmEnableBlurBehind visible
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_UpdateClientBlur(uDwm::CTopLevelWindow* This)
{
	if (!Shared::IsBackdropAllowed())
	{
		return g_CTopLevelWindow_UpdateClientBlur_Org(This);
	}
	auto data = This->GetData();
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
	if (!Shared::IsBackdropAllowed())
	{
		return g_CAccent_UpdateAccentPolicy_Org(This, lprc, policy, geometry);
	}
	if (!Shared::g_overrideAccent)
	{
		return g_CAccent_UpdateAccentPolicy_Org(This, lprc, policy, geometry);
	}

	HRESULT hr{ S_OK };
	auto accentPolicy = *policy;
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
	if (!Shared::IsBackdropAllowed())
	{
		return g_CAccent__UpdateSolidFill_Org(This, visual, color, lprc, opacity);
	}
	if (!Shared::g_overrideAccent)
	{
		return g_CAccent__UpdateSolidFill_Org(This, visual, color, lprc, opacity);
	}
	if (!This->GetHwnd())
	{
		return g_CAccent__UpdateSolidFill_Org(This, visual, color, lprc, opacity);
	}
	uDwm::CWindowData* data{ nullptr };
	{
		auto lock = wil::EnterCriticalSection(uDwm::CDesktopManager::s_csDwmInstance);
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
	if (!Shared::IsBackdropAllowed())
	{
		return g_CRenderDataVisual_AddInstruction_Org(This, instruction);
	}
	if (!Shared::g_overrideAccent)
	{
		return g_CRenderDataVisual_AddInstruction_Org(This, instruction);
	}
	if (g_accentRenderDataVisual != This)
	{
		return g_CRenderDataVisual_AddInstruction_Org(This, instruction);
	}

	auto drawRectangleInstruction = reinterpret_cast<uDwm::CSolidRectangleInstruction*>(instruction);
	auto rectangle = drawRectangleInstruction->GetRectangle();
	auto color = drawRectangleInstruction->GetColor();
	if (g_accentState == 4 && color.a == 0.f && color.r == 0.f && color.g == 0.f && color.b == 0.f)
	{
		return g_CRenderDataVisual_AddInstruction_Org(This, instruction);
	}

	color.a = 0.5f;

	winrt::com_ptr<uDwm::CRgnGeometryProxy> rgnGeometry{ nullptr };
	RETURN_IF_FAILED(
		uDwm::ResourceHelper::CreateGeometryFromHRGN(
			wil::unique_hrgn
			{ 
				CreateRectRgn(
					static_cast<LONG>(rectangle.left), 
					static_cast<LONG>(rectangle.top), 
					static_cast<LONG>(rectangle.right), 
					static_cast<LONG>(rectangle.bottom)
				) 
			}.get(), 
			rgnGeometry.put()
		)
	);
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
	if (!Shared::IsBackdropAllowed())
	{
		return g_CTopLevelWindow_CalculateBackgroundType_Org(This);
	}

	auto result = g_CTopLevelWindow_CalculateBackgroundType_Org(This);
	if (result == 4 || result == 3 || result == 2)
	{
		result = 0;
	}

	return result;
}

// trick dwm into thinking the system backdrop is not exist
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_UpdateSystemBackdropVisual(uDwm::CTopLevelWindow* This)
{
	if (!Shared::IsBackdropAllowed())
	{
		return g_CTopLevelWindow_UpdateSystemBackdropVisual_Org(This);
	}
	auto data = This->GetData();
	if (!data)
	{
		return g_CTopLevelWindow_UpdateSystemBackdropVisual_Org(This);
	}

	HRESULT hr{ S_OK };
	auto oldSystemBackdropType = *reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(data) + 204);
	// trick dwm into thinking the window does not enable system backdrop
	*reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(data) + 204) = 0;
	hr = g_CTopLevelWindow_UpdateSystemBackdropVisual_Org(This);
	*reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(data) + 204) = oldSystemBackdropType;

	return hr;
}

// release resources
void STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_Destructor(uDwm::CTopLevelWindow* This)
{
	VisualManager::LegacyVisualOverrider::Remove(This);
	return g_CTopLevelWindow_Destructor_Org(This);
}

void STDMETHODCALLTYPE GlassFramework::MyCAnimatedGlassSheet_OnRectUpdated(uDwm::CAnimatedGlassSheet* This, LPCRECT lprc)
{
	if (auto sheetOverrider = VisualManager::AnimatedGlassSheetOverrider::GetOrCreate(This, true); sheetOverrider)
	{
		LOG_IF_FAILED(sheetOverrider->OnRectUpdated(lprc));
	}

	return g_CAnimatedGlassSheet_OnRectUpdated_Org(This, lprc);
}
void STDMETHODCALLTYPE GlassFramework::MyCAnimatedGlassSheet_Destructor(uDwm::CAnimatedGlassSheet* This)
{
	VisualManager::AnimatedGlassSheetOverrider::Remove(This);
	return g_CAnimatedGlassSheet_Destructor_Org(This);
}

// reserved
HRESULT STDMETHODCALLTYPE GlassFramework::MyCLivePreview__UpdateGlassVisual(uDwm::CLivePreview* This)
{
	return g_CLivePreview__UpdateGlassVisual_Org(This);
}
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_CloneVisualTreeForLivePreview(
	uDwm::CTopLevelWindow* This,
	bool cloneForReflection,
	bool reserved1,
	bool reserved2,
	uDwm::CTopLevelWindow** cloned
)
{
	auto hr = g_CTopLevelWindow_CloneVisualTreeForLivePreview_Org(
		This,
		cloneForReflection,
		reserved1,
		reserved2,
		cloned
	);

	if (SUCCEEDED(hr) && cloneForReflection)
	{
		winrt::com_ptr<uDwm::CCanvasVisual> visual{ nullptr };
		RETURN_IF_FAILED(
			uDwm::CCanvasVisual::Create(
				visual.put()
			)
		);
		winrt::com_ptr<uDwm::CSolidColorLegacyMilBrushProxy> brush{ nullptr };
		RETURN_IF_FAILED(
			uDwm::CDesktopManager::s_pDesktopManagerInstance->GetCompositor()->CreateSolidColorLegacyMilBrushProxy(
				brush.put()
			)
		);
		RETURN_IF_FAILED(brush->Update(1.0, D2D1::ColorF(0x000000, 0.25f)));

		RECT windowRect{};
		This->GetActualWindowRect(&windowRect, true, true, false);

		wil::unique_hrgn region
		{ 
			CreateRoundRectRgn(
				windowRect.left,
				windowRect.top,
				windowRect.right,
				windowRect.bottom,
				Shared::g_roundRectRadius,
				Shared::g_roundRectRadius
			) 
		};
		RETURN_LAST_ERROR_IF_NULL(region);

		winrt::com_ptr<uDwm::CRgnGeometryProxy> geometry{ nullptr };
		RETURN_IF_FAILED(
			uDwm::ResourceHelper::CreateGeometryFromHRGN(
				region.get(),
				geometry.put()
			)
		);
		winrt::com_ptr<uDwm::CDrawGeometryInstruction> instruction{ nullptr };
		RETURN_IF_FAILED(
			uDwm::CDrawGeometryInstruction::Create(
				brush.get(),
				geometry.get(),
				instruction.put()
			)
		);
		RETURN_IF_FAILED(visual->AddInstruction(instruction.get()));
		RETURN_IF_FAILED(
			(*cloned)->GetNonClientVisual()->GetVisualCollection()->InsertRelative(
				visual.get(),
				nullptr,
				false,
				true
			)
		);
	};

	return hr;
}

void GlassFramework::UpdateConfiguration(ConfigurationFramework::UpdateType type)
{
	if (type & ConfigurationFramework::UpdateType::Framework)
	{
		Shared::g_disableOnBattery = static_cast<bool>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"DisableGlassOnBattery", TRUE));
		Shared::g_batteryMode = Utils::IsBatterySaverEnabled();
	}
	if (type & ConfigurationFramework::UpdateType::Backdrop)
	{
		Shared::g_transparencyEnabled = Utils::IsTransparencyEnabled(ConfigurationFramework::GetPersonalizeKey());
		Shared::g_enableGeometryMerging = static_cast<bool>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"EnableGeometryMerging"));
		Shared::g_overrideAccent = static_cast<bool>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"GlassOverrideAccent"));
		Shared::g_roundRectRadius = static_cast<int>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"RoundRectRadius"));

		if (Shared::g_forceAccentColorization = static_cast<bool>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ForceAccentColorization")); Shared::g_forceAccentColorization)
		{
			auto accentColor = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"AccentColor");
			auto accentColorInactive = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"AccentColorInactive", accentColor);
			Shared::g_accentColor = Utils::FromAbgr(accentColor);
			Shared::g_accentColorInactive = Utils::FromAbgr(accentColorInactive);
		}

		Shared::g_enableFullDirty = static_cast<bool>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"EnableFullDirty"));
		Shared::g_enableFullDirty ? g_hackVisual.as<IDCompositionVisualDebug>()->EnableRedrawRegions() : g_hackVisual.as<IDCompositionVisualDebug>()->DisableRedrawRegions();
		LOG_IF_FAILED(uDwm::CDesktopManager::s_pDesktopManagerInstance->GetDCompDevice()->Commit());
	}

	auto lock = wil::EnterCriticalSection(uDwm::CDesktopManager::s_csDwmInstance);
	if (!Shared::IsBackdropAllowed())
	{
		VisualManager::LegacyVisualOverrider::Shutdown();
	}
	else
	{
		ULONG_PTR desktopID{ 0 };
		Utils::GetDesktopID(1, &desktopID);
		auto windowList = uDwm::CDesktopManager::s_pDesktopManagerInstance->GetWindowList()->GetWindowListForDesktop(desktopID);
		for (auto i = windowList->Blink; i != windowList; i = i->Blink)
		{
			auto data = reinterpret_cast<uDwm::CWindowData*>(i);
			auto hwnd = data->GetHwnd();
			if (!hwnd || !IsWindow(hwnd)) { continue; }
			auto window = data->GetWindow();
			if (!window) { continue; }

			VisualManager::RedrawTopLevelWindow(window);
			LOG_IF_FAILED(window->ValidateVisual());
		}
	}
}

HRESULT GlassFramework::Startup()
{
	g_dwOverlayTestMode = *dwmcore::CCommonRegistryData::m_dwOverlayTestMode;
	*dwmcore::CCommonRegistryData::m_dwOverlayTestMode = 0x5;

	RETURN_IF_FAILED(uDwm::CDesktopManager::s_pDesktopManagerInstance->GetDCompDevice()->CreateVisual(g_hackVisual.put()));

	uDwm::GetAddressFromSymbolMap("CDrawGeometryInstruction::Create", g_CDrawGeometryInstruction_Create_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::UpdateNCAreaBackground", g_CTopLevelWindow_UpdateNCAreaBackground_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::UpdateClientBlur", g_CTopLevelWindow_UpdateClientBlur_Org);
	uDwm::GetAddressFromSymbolMap("CAccent::UpdateAccentPolicy", g_CAccent_UpdateAccentPolicy_Org);
	uDwm::GetAddressFromSymbolMap("CAccent::_UpdateSolidFill", g_CAccent__UpdateSolidFill_Org);
	uDwm::GetAddressFromSymbolMap("CRenderDataVisual::AddInstruction", g_CRenderDataVisual_AddInstruction_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::CalculateBackgroundType", g_CTopLevelWindow_CalculateBackgroundType_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::UpdateSystemBackdropVisual", g_CTopLevelWindow_UpdateSystemBackdropVisual_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::~CTopLevelWindow", g_CTopLevelWindow_Destructor_Org);
	uDwm::GetAddressFromSymbolMap("CAnimatedGlassSheet::OnRectUpdated", g_CAnimatedGlassSheet_OnRectUpdated_Org);
	uDwm::GetAddressFromSymbolMap("CAnimatedGlassSheet::~CAnimatedGlassSheet", g_CAnimatedGlassSheet_Destructor_Org);
	uDwm::GetAddressFromSymbolMap("CLivePreview::_UpdateGlassVisual", g_CLivePreview__UpdateGlassVisual_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::CloneVisualTreeForLivePreview", g_CTopLevelWindow_CloneVisualTreeForLivePreview_Org);

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
		HookHelper::Detours::Attach(&g_CAnimatedGlassSheet_OnRectUpdated_Org, MyCAnimatedGlassSheet_OnRectUpdated);
		HookHelper::Detours::Attach(&g_CAnimatedGlassSheet_Destructor_Org, MyCAnimatedGlassSheet_Destructor);

		//HookHelper::Detours::Attach(&g_CLivePreview__UpdateGlassVisual_Org, MyCLivePreview__UpdateGlassVisual);
		HookHelper::Detours::Attach(&g_CTopLevelWindow_CloneVisualTreeForLivePreview_Org, MyCTopLevelWindow_CloneVisualTreeForLivePreview);
	});
}

void GlassFramework::Shutdown()
{
	*dwmcore::CCommonRegistryData::m_dwOverlayTestMode = g_dwOverlayTestMode;

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
		HookHelper::Detours::Detach(&g_CAnimatedGlassSheet_OnRectUpdated_Org, MyCAnimatedGlassSheet_OnRectUpdated);
		HookHelper::Detours::Detach(&g_CAnimatedGlassSheet_Destructor_Org, MyCAnimatedGlassSheet_Destructor);

		//HookHelper::Detours::Detach(&g_CLivePreview__UpdateGlassVisual_Org, MyCLivePreview__UpdateGlassVisual);
		HookHelper::Detours::Detach(&g_CTopLevelWindow_CloneVisualTreeForLivePreview_Org, MyCTopLevelWindow_CloneVisualTreeForLivePreview);
	});

	if (g_CreateRoundRectRgn_Org)
	{
		HookHelper::WriteIAT(uDwm::g_moduleHandle, "gdi32.dll", "CreateRoundRectRgn", g_CreateRoundRectRgn_Org);
	}

	g_capturedWindow = nullptr;
	VisualManager::LegacyVisualOverrider::Shutdown();
	VisualManager::AnimatedGlassSheetOverrider::Shutdown();
	ULONG_PTR desktopID{ 0 };
	Utils::GetDesktopID(1, &desktopID);
	auto windowList = uDwm::CDesktopManager::s_pDesktopManagerInstance->GetWindowList()->GetWindowListForDesktop(desktopID);
	for (auto i = windowList->Blink; i != windowList; i = i->Blink)
	{
		auto data = reinterpret_cast<uDwm::CWindowData*>(i);
		auto hwnd = data->GetHwnd();
		if (!hwnd || !IsWindow(hwnd)) { continue; }
		auto window = data->GetWindow();
		if (!window) { continue; }

		VisualManager::RedrawTopLevelWindow(window);
		LOG_IF_FAILED(window->ValidateVisual());
	}

	g_hackVisual = nullptr;
}