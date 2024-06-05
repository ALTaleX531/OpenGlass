#include "pch.h"
#include "GlassFramework.hpp"
#include "uDwmProjection.hpp"
#include "GeometryRecorder.hpp"
#include "BackdropManager.hpp"
#include "GlassReflection.hpp"
#include "dwmcoreProjection.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassFramework
{
	BackdropManager::CompositedBackdropKind GetActualBackdropKind(uDwm::CTopLevelWindow* This);

	HRESULT STDMETHODCALLTYPE MyCDrawGeometryInstruction_Create(uDwm::CBaseLegacyMilBrushProxy* brush, uDwm::CBaseGeometryProxy* geometry, uDwm::CDrawGeometryInstruction** instruction);
	HRESULT STDMETHODCALLTYPE MyCTopLevelWindow_UpdateNCAreaBackground(uDwm::CTopLevelWindow* This);
	HRESULT STDMETHODCALLTYPE MyCTopLevelWindow_UpdateClientBlur(uDwm::CTopLevelWindow* This);
	HRESULT STDMETHODCALLTYPE MyCTopLevelWindow_ValidateVisual(uDwm::CTopLevelWindow* This);
	HRESULT STDMETHODCALLTYPE MyCTopLevelWindow_UpdateAccent(uDwm::CTopLevelWindow* This, bool visibleAndUncloaked);
	DWORD STDMETHODCALLTYPE MyCTopLevelWindow_CalculateBackgroundType(uDwm::CTopLevelWindow* This);
	HRESULT STDMETHODCALLTYPE MyCTopLevelWindow_UpdateSystemBackdropVisual(uDwm::CTopLevelWindow* This);
	void STDMETHODCALLTYPE MyCTopLevelWindow_Destructor(uDwm::CTopLevelWindow* This);
	HRESULT STDMETHODCALLTYPE MyCTopLevelWindow_InitializeVisualTreeClone(uDwm::CTopLevelWindow* This, uDwm::CTopLevelWindow* window, UINT cloneOptions);
	HRESULT STDMETHODCALLTYPE MyCTopLevelWindow_OnClipUpdated(uDwm::CTopLevelWindow* This);
	struct MILCMD_DWM_REDIRECTION_ACCENTBLURRECTUPDATE
	{
		HWND GetHwnd() const
		{
			return *reinterpret_cast<HWND*>(reinterpret_cast<ULONG_PTR>(this) + 4);
		}
		LPCRECT GetRect() const
		{
			return reinterpret_cast<LPCRECT>(reinterpret_cast<ULONG_PTR>(this) + 12);
		}
	};
	HRESULT STDMETHODCALLTYPE MyCWindowList_UpdateAccentBlurRect(uDwm::CWindowList* This, const MILCMD_DWM_REDIRECTION_ACCENTBLURRECTUPDATE* milCmd);

	decltype(&MyCDrawGeometryInstruction_Create) g_CDrawGeometryInstruction_Create_Org{ nullptr };
	decltype(&MyCTopLevelWindow_UpdateNCAreaBackground) g_CTopLevelWindow_UpdateNCAreaBackground_Org{ nullptr };
	decltype(&MyCTopLevelWindow_UpdateClientBlur) g_CTopLevelWindow_UpdateClientBlur_Org{ nullptr };
	decltype(&MyCTopLevelWindow_ValidateVisual) g_CTopLevelWindow_ValidateVisual_Org{ nullptr };
	decltype(&MyCTopLevelWindow_UpdateAccent) g_CTopLevelWindow_UpdateAccent_Org{ nullptr };
	decltype(&MyCTopLevelWindow_CalculateBackgroundType) g_CTopLevelWindow_CalculateBackgroundType_Org{ nullptr };
	decltype(&MyCTopLevelWindow_UpdateSystemBackdropVisual) g_CTopLevelWindow_UpdateSystemBackdropVisual_Org{ nullptr };
	decltype(&MyCTopLevelWindow_Destructor) g_CTopLevelWindow_Destructor_Org{ nullptr };
	decltype(&MyCTopLevelWindow_InitializeVisualTreeClone) g_CTopLevelWindow_InitializeVisualTreeClone_Org{ nullptr };
	decltype(&MyCTopLevelWindow_OnClipUpdated) g_CTopLevelWindow_OnClipUpdated_Org{ nullptr };
	decltype(&MyCWindowList_UpdateAccentBlurRect) g_CWindowList_UpdateAccentBlurRect_Org{ nullptr };

	size_t g_captureRef{ 0 };
	std::vector<winrt::com_ptr<uDwm::CBaseGeometryProxy>> g_geometryBuffer{};

	ULONGLONG g_oldBackdropBlurCachingThrottleQPCTimeDelta{ 0ull };
	BOOL g_disableOnBattery{ TRUE };
	BOOL g_overrideAccent{ FALSE };
	bool g_batteryMode{ false };
	bool g_transparencyEnabled{ true };
	FORCEINLINE bool IsBackdropAllowed()
	{
		if (g_batteryMode && g_disableOnBattery)
		{
			return false;
		}
		if (!g_transparencyEnabled)
		{
			return false;
		}

		return true;
	}
}

BackdropManager::CompositedBackdropKind GlassFramework::GetActualBackdropKind(uDwm::CTopLevelWindow* This)
{
	auto backgroundType{ BackdropManager::CompositedBackdropKind::Legacy };
	auto windowData{ This->GetData() };

	if (os::buildNumber < os::build_w11_21h2)
	{
		backgroundType = windowData ?
			(windowData->GetAccentPolicy()->IsActive() ? BackdropManager::CompositedBackdropKind::Accent : BackdropManager::CompositedBackdropKind::Legacy) :
			BackdropManager::CompositedBackdropKind::Legacy;
	}
	else if (os::buildNumber < os::build_w11_22h2)
	{
		backgroundType = windowData ?
			(
				*reinterpret_cast<DWORD*>(reinterpret_cast<ULONG_PTR>(windowData) + 204) ?
				BackdropManager::CompositedBackdropKind::SystemBackdrop :
				(
					windowData->GetAccentPolicy()->IsActive() ?
					BackdropManager::CompositedBackdropKind::Accent :
					BackdropManager::CompositedBackdropKind::Legacy
					)
				) :
			BackdropManager::CompositedBackdropKind::Legacy;
	}
	else
	{
		auto calculatedBackdropType{ g_CTopLevelWindow_CalculateBackgroundType_Org(This) };
		if (calculatedBackdropType == 2 || calculatedBackdropType == 3)
		{
			backgroundType = BackdropManager::CompositedBackdropKind::SystemBackdrop;
		}
		else if (calculatedBackdropType == 4 || calculatedBackdropType == 0)
		{
			backgroundType = BackdropManager::CompositedBackdropKind::Legacy;
		}
		else
		{
			backgroundType = BackdropManager::CompositedBackdropKind::Accent;
		}
	}

	if (!g_overrideAccent && backgroundType == BackdropManager::CompositedBackdropKind::Accent)
	{
		backgroundType = BackdropManager::CompositedBackdropKind::Legacy;
	}
	return backgroundType;
}

// record the geometry that has been used in the creation of draw-geometry-instruction
HRESULT STDMETHODCALLTYPE GlassFramework::MyCDrawGeometryInstruction_Create(uDwm::CBaseLegacyMilBrushProxy* brush, uDwm::CBaseGeometryProxy* geometry, uDwm::CDrawGeometryInstruction** instruction)
{
	if (g_captureRef)
	{
		winrt::com_ptr<uDwm::CBaseGeometryProxy> referencedGeometry{ nullptr };
		winrt::copy_from_abi(referencedGeometry, geometry);
		g_geometryBuffer.push_back(referencedGeometry);
	}

	return g_CDrawGeometryInstruction_Create_Org(brush, geometry, instruction);
}
// record the titlebar and border region
// and make the legacy titlebar transparent
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_UpdateNCAreaBackground(uDwm::CTopLevelWindow* This)
{
	if (!IsBackdropAllowed())
	{
		return g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);
	}
	auto data{ This->GetData() };
	if (!data)
	{
		return g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);
	}

	HRESULT hr{ S_OK };

	winrt::com_ptr<BackdropManager::ICompositedBackdropVisual> backdrop{ BackdropManager::GetOrCreateBackdropVisual(This) };

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
			backdrop = BackdropManager::GetOrCreateBackdropVisual(This, true);
			// the titlebar region has been updated
			// let's update our backdrop region
			if (GeometryRecorder::GetGeometryCount() && backdrop)
			{
				auto borderGeometry{ This->GetBorderGeometry() };
				auto captionGeometry{ This->GetCaptionGeometry() };
				
				HRGN borderRegion{ GeometryRecorder::GetRegionFromGeometry(borderGeometry) };
				HRGN captionRegion{ GeometryRecorder::GetRegionFromGeometry(captionGeometry) };
				wil::unique_hrgn realBorderRegion{ CreateRectRgn(0, 0, 0, 0) };
				wil::unique_hrgn emptyRegion{ CreateRectRgn(0, 0, 0, 0) };

				if (!BackdropManager::Configuration::g_overrideBorder && !data->IsFrameExtendedIntoClientAreaLRB())
				{
					RECT captionBox{};
					RECT borderBox{};
					GetRgnBox(captionRegion, &captionBox);
					if (GetRgnBox(borderRegion, &borderBox) != NULLREGION)
					{
						LONG borderWidth{ captionBox.top - borderBox.top };
						wil::unique_hrgn nonBorderRegion{ CreateRectRgn(borderBox.left + borderWidth, borderBox.top + borderWidth, borderBox.right - borderWidth, borderBox.bottom - borderWidth) };
						CombineRgn(realBorderRegion.get(), borderRegion, nonBorderRegion.get(), RGN_DIFF);
					}
				}

				backdrop->SetBorderRegion(borderRegion);
				backdrop->SetCaptionRegion(captionRegion);

				if (borderGeometry)
				{
					uDwm::ResourceHelper::CreateGeometryFromHRGN(realBorderRegion.get(), &borderGeometry);
				}
				if (captionGeometry)
				{
					uDwm::ResourceHelper::CreateGeometryFromHRGN(emptyRegion.get(), &captionGeometry);
				}
				// This->GetLegacyVisual()->ClearInstructions();

			}
		}

		GeometryRecorder::EndCapture();
	}
	else
	{
		backdrop = BackdropManager::GetOrCreateBackdropVisual(This, GetActualBackdropKind(This) == BackdropManager::CompositedBackdropKind::Accent);
		hr = g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);

		// let's update our backdrop region
		if (SUCCEEDED(hr) && backdrop)
		{
			wil::unique_hrgn emptyRegion{ CreateRectRgn(0, 0, 0, 0) };
			backdrop->SetBorderRegion(emptyRegion.get());
			backdrop->SetCaptionRegion(emptyRegion.get());
		}
	}

	if (backdrop)
	{
		backdrop->UpdateNCBackground();
		backdrop->ValidateVisual();
	}

	return hr;
}

// make the visual of DwmEnableBlurBehind invisible
// and record the blur region
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_UpdateClientBlur(uDwm::CTopLevelWindow* This)
{
	if (!IsBackdropAllowed())
	{
		return g_CTopLevelWindow_UpdateClientBlur_Org(This);
	}
	auto data{ This->GetData() };
	if (!data)
	{
		return g_CTopLevelWindow_UpdateClientBlur_Org(This);
	}

	GeometryRecorder::BeginCapture();
	g_captureRef += 1;

	HRESULT hr{ g_CTopLevelWindow_UpdateClientBlur_Org(This) };
	if (SUCCEEDED(hr))
	{
		// the dwmblurregion is updated
		// UpdateClientBlur will only create at most one geometry draw instrution
		if (!g_geometryBuffer.empty())
		{
			HRGN clientBlurRegion{ GeometryRecorder::GetRegionFromGeometry(g_geometryBuffer[0].get()) };
			RECT clientBlurBox{};
			// let's update our backdrop region
			auto backdrop{ BackdropManager::GetOrCreateBackdropVisual(This, GetRgnBox(clientBlurRegion, &clientBlurBox) != NULLREGION && !IsRectEmpty(&clientBlurBox), true) };
			if (backdrop)
			{
				backdrop->SetClientBlurRegion(clientBlurRegion);
				backdrop->ValidateVisual();
			}
		}
	}

	g_captureRef -= 1;
	g_geometryBuffer.clear();
	GeometryRecorder::EndCapture();

	return hr;
}

// update the glass reflection and other stuff
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_ValidateVisual(uDwm::CTopLevelWindow* This)
{
	if (!IsBackdropAllowed())
	{
		return g_CTopLevelWindow_ValidateVisual_Org(This);
	}
	auto data{ This->GetData() };
	if (!data)
	{
		return g_CTopLevelWindow_ValidateVisual_Org(This);
	}

	auto kind{ static_cast<BackdropManager::CompositedBackdropKind>(GetActualBackdropKind(This)) };
	auto accentPolicy{ data->GetAccentPolicy() };
	auto oldAccentState{ accentPolicy->AccentState };

	if (kind == BackdropManager::CompositedBackdropKind::Accent) { accentPolicy->AccentState = 0; }
	HRESULT hr{ g_CTopLevelWindow_ValidateVisual_Org(This) };
	if (kind == BackdropManager::CompositedBackdropKind::Accent) { accentPolicy->AccentState = oldAccentState; }

	if (SUCCEEDED(hr))
	{
		auto backdrop{ BackdropManager::GetOrCreateBackdropVisual(This) };
		if (backdrop)
		{
			// update existing backdrop
			backdrop->ValidateVisual();
		}
	}

	return hr;
}

// trick dwm into thinking the accent is gone
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_UpdateAccent(uDwm::CTopLevelWindow* This, bool visibleAndUncloaked)
{
	if (!IsBackdropAllowed())
	{
		return g_CTopLevelWindow_UpdateAccent_Org(This, visibleAndUncloaked);
	}
	auto data{ This->GetData() };
	if (!data)
	{
		return g_CTopLevelWindow_UpdateAccent_Org(This, visibleAndUncloaked);
	}

	auto kind{ static_cast<BackdropManager::CompositedBackdropKind>(GetActualBackdropKind(This)) };
	HRESULT hr{ S_OK };
	if (kind == BackdropManager::CompositedBackdropKind::Accent)
	{
		auto accentPolicy{ data->GetAccentPolicy() };
		auto oldAccentState{ accentPolicy->AccentState };

		accentPolicy->AccentState = 0;
		hr = g_CTopLevelWindow_UpdateAccent_Org(This, visibleAndUncloaked);
		accentPolicy->AccentState = oldAccentState;

		winrt::com_ptr<BackdropManager::ICompositedBackdropVisual> backdrop{ BackdropManager::GetOrCreateBackdropVisual(This, kind == BackdropManager::CompositedBackdropKind::Accent) };
		if (backdrop)
		{
			backdrop->ValidateVisual();
		}
	}
	else
	{
		hr = g_CTopLevelWindow_UpdateAccent_Org(This, visibleAndUncloaked);
	}

	return hr;
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
	if (!IsBackdropAllowed())
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
	if (!IsBackdropAllowed())
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
	BackdropManager::RemoveBackdrop(This, true);
	g_CTopLevelWindow_Destructor_Org(This);
}

// thumbnail/aero peek
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_InitializeVisualTreeClone(uDwm::CTopLevelWindow* This, uDwm::CTopLevelWindow* window, UINT cloneOptions)
{
	HRESULT hr{ g_CTopLevelWindow_InitializeVisualTreeClone_Org(This, window, cloneOptions) };
	
	if (SUCCEEDED(hr))
	{
		BackdropManager::TryCloneBackdropVisualForWindow(This, window);
	}
	return hr;
}

// some apps may call SetWindowRgn to restrict accent blur region
HRESULT STDMETHODCALLTYPE GlassFramework::MyCTopLevelWindow_OnClipUpdated(uDwm::CTopLevelWindow* This)
{
	if (!IsBackdropAllowed())
	{
		return g_CTopLevelWindow_OnClipUpdated_Org(This);
	}
	auto data{ This->GetData() };
	if (!data)
	{
		return g_CTopLevelWindow_OnClipUpdated_Org(This);
	}

	HRESULT hr{ g_CTopLevelWindow_OnClipUpdated_Org(This) };

	if (SUCCEEDED(hr))
	{
		auto kind{ static_cast<BackdropManager::CompositedBackdropKind>(GetActualBackdropKind(This)) };
		winrt::com_ptr<BackdropManager::ICompositedBackdropVisual> backdrop{ BackdropManager::GetOrCreateBackdropVisual(This, kind == BackdropManager::CompositedBackdropKind::Accent) };
		if (backdrop)
		{
			wil::unique_hrgn clipRgn{ CreateRectRgn(0, 0, 0, 0) };
			if (GetWindowRgn(data->GetHwnd(), clipRgn.get()) != ERROR)
			{
				backdrop->SetGdiWindowRegion(clipRgn.get());
			}
			else
			{
				backdrop->SetGdiWindowRegion(nullptr);
			}

			backdrop->ValidateVisual();
		}
	}
	return hr;
}

// some apps may call DwmpUpdateAccentBlurRect to restrict accent blur region
HRESULT STDMETHODCALLTYPE GlassFramework::MyCWindowList_UpdateAccentBlurRect(uDwm::CWindowList* This, const MILCMD_DWM_REDIRECTION_ACCENTBLURRECTUPDATE* milCmd)
{
	if (!IsBackdropAllowed())
	{
		return g_CWindowList_UpdateAccentBlurRect_Org(This, milCmd);
	}

	HRESULT hr{ g_CWindowList_UpdateAccentBlurRect_Org(This, milCmd) };

	uDwm::CWindowData* data{ nullptr };
	uDwm::CTopLevelWindow* window{ nullptr };
	auto lock{ wil::EnterCriticalSection(uDwm::CDesktopManager::s_csDwmInstance) };
	if (SUCCEEDED(hr) && SUCCEEDED(This->GetSyncedWindowDataByHwnd(milCmd->GetHwnd(), &data)) && data && (window = data->GetWindow()))
	{
		auto kind{ static_cast<BackdropManager::CompositedBackdropKind>(GetActualBackdropKind(window)) };
		winrt::com_ptr<BackdropManager::ICompositedBackdropVisual> backdrop{ BackdropManager::GetOrCreateBackdropVisual(window, kind == BackdropManager::CompositedBackdropKind::Accent) };
		if (backdrop)
		{
			if (data->GetAccentPolicy()->IsClipEnabled())
			{
				auto lprc{ milCmd->GetRect() };
				if (
					lprc->right <= lprc->left ||
					lprc->bottom <= lprc->top
					)
				{
					lprc = nullptr;
				}
				backdrop->SetAccentRect(lprc);
			}

			backdrop->ValidateVisual();
		}
	}
	return hr;
}

void GlassFramework::UpdateConfiguration(ConfigurationFramework::UpdateType type)
{
	if (type & ConfigurationFramework::UpdateType::Framework)
	{
		g_disableOnBattery = TRUE;
		LOG_IF_FAILED(
			wil::reg::get_value_dword_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"DisableGlassOnBattery",
				reinterpret_cast<DWORD*>(&g_disableOnBattery)
			)
		);
		g_batteryMode = Utils::IsBatterySaverEnabled();
	}
	if (type & ConfigurationFramework::UpdateType::Backdrop)
	{
		g_transparencyEnabled = Utils::IsTransparencyEnabled(ConfigurationFramework::GetPersonalizeKey());

		DWORD value{ 0 };
		LOG_IF_FAILED(
			wil::reg::get_value_dword_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"RoundRectRadius",
				&value
			)
		);
		BackdropManager::Configuration::g_roundRectRadius = static_cast<float>(value);

		value = FALSE;
		LOG_IF_FAILED(
			wil::reg::get_value_dword_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"GlassOverrideBorder",
				&value
			)
		);
		BackdropManager::Configuration::g_overrideBorder = static_cast<bool>(value);

		value = 87;
		LOG_IF_FAILED(
			wil::reg::get_value_dword_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"GlassCrossFadeTime",
				&value
			)
		);
		BackdropManager::Configuration::g_crossfadeTime = std::chrono::milliseconds{ uDwm::CDesktopManager::s_pDesktopManagerInstance->IsWindowAnimationEnabled() ? value : 0 };

		value = FALSE;
		LOG_IF_FAILED(
			wil::reg::get_value_dword_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"GlassOverrideAccent",
				&value
			)
		);
		g_overrideAccent = value;

		value = 0;
		LOG_IF_FAILED(
			wil::reg::get_value_dword_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"ColorizationGlassReflectionIntensity",
				&value
			)
		);
		CGlassReflectionVisual::UpdateIntensity(std::clamp(static_cast<float>(value) / 100.f, 0.f, 1.f));

		value = 10;
		LOG_IF_FAILED(
			wil::reg::get_value_dword_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"ColorizationGlassReflectionParallaxIntensity",
				&value
			)
		);
		CGlassReflectionVisual::UpdateParallaxIntensity(std::clamp(static_cast<float>(value) / 100.f, 0.f, 1.f));

		WCHAR reflectionTexturePath[MAX_PATH + 1]{};
		LOG_IF_FAILED(
			wil::reg::get_value_string_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"CustomThemeReflection",
				reflectionTexturePath
			)
		);
		CGlassReflectionVisual::UpdateReflectionSurface(reflectionTexturePath);
	}

	auto lock{ wil::EnterCriticalSection(uDwm::CDesktopManager::s_csDwmInstance) };
	if (!IsBackdropAllowed())
	{
		BackdropManager::Shutdown();
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

			auto backdrop{ BackdropManager::GetOrCreateBackdropVisual(window, window->HasNonClientBackground() || GetActualBackdropKind(window) == BackdropManager::CompositedBackdropKind::Accent) };
			if (backdrop)
			{
				backdrop->ValidateVisual();
			}
		}
	}
}

HRESULT GlassFramework::Startup()
{
	// Remove blurred backdrop low framerate refresh limitation!!!
	g_oldBackdropBlurCachingThrottleQPCTimeDelta = *dwmcore::CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta;
	*dwmcore::CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta = 0;
#ifdef _DEBUG
	OutputDebugStringW(
		std::format(
			L"CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta: {}\n",
			g_oldBackdropBlurCachingThrottleQPCTimeDelta
		).c_str()
	);
#endif
	uDwm::GetAddressFromSymbolMap("CDrawGeometryInstruction::Create", g_CDrawGeometryInstruction_Create_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::UpdateNCAreaBackground", g_CTopLevelWindow_UpdateNCAreaBackground_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::UpdateClientBlur", g_CTopLevelWindow_UpdateClientBlur_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::ValidateVisual", g_CTopLevelWindow_ValidateVisual_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::UpdateAccent", g_CTopLevelWindow_UpdateAccent_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::CalculateBackgroundType", g_CTopLevelWindow_CalculateBackgroundType_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::UpdateSystemBackdropVisual", g_CTopLevelWindow_UpdateSystemBackdropVisual_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::~CTopLevelWindow", g_CTopLevelWindow_Destructor_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::InitializeVisualTreeClone", g_CTopLevelWindow_InitializeVisualTreeClone_Org);
	uDwm::GetAddressFromSymbolMap("CTopLevelWindow::OnClipUpdated", g_CTopLevelWindow_OnClipUpdated_Org);
	uDwm::GetAddressFromSymbolMap("CWindowList::UpdateAccentBlurRect", g_CWindowList_UpdateAccentBlurRect_Org);
	
	return HookHelper::Detours::Write([]()
	{
		HookHelper::Detours::Attach(&g_CDrawGeometryInstruction_Create_Org, MyCDrawGeometryInstruction_Create);
		HookHelper::Detours::Attach(&g_CTopLevelWindow_UpdateNCAreaBackground_Org, MyCTopLevelWindow_UpdateNCAreaBackground);
		HookHelper::Detours::Attach(&g_CTopLevelWindow_UpdateClientBlur_Org, MyCTopLevelWindow_UpdateClientBlur);
		HookHelper::Detours::Attach(&g_CTopLevelWindow_ValidateVisual_Org, MyCTopLevelWindow_ValidateVisual);
		HookHelper::Detours::Attach(&g_CTopLevelWindow_UpdateAccent_Org, MyCTopLevelWindow_UpdateAccent);
		HookHelper::Detours::Attach(&g_CTopLevelWindow_Destructor_Org, MyCTopLevelWindow_Destructor);
		HookHelper::Detours::Attach(&g_CTopLevelWindow_OnClipUpdated_Org, MyCTopLevelWindow_OnClipUpdated);
		HookHelper::Detours::Attach(&g_CWindowList_UpdateAccentBlurRect_Org, MyCWindowList_UpdateAccentBlurRect);
		if (os::buildNumber >= os::build_w10_2004)
		{
			HookHelper::Detours::Attach(&g_CTopLevelWindow_InitializeVisualTreeClone_Org, MyCTopLevelWindow_InitializeVisualTreeClone);
		}
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
		HookHelper::Detours::Detach(&g_CTopLevelWindow_ValidateVisual_Org, MyCTopLevelWindow_ValidateVisual);
		HookHelper::Detours::Detach(&g_CTopLevelWindow_UpdateAccent_Org, MyCTopLevelWindow_UpdateAccent);
		HookHelper::Detours::Detach(&g_CTopLevelWindow_Destructor_Org, MyCTopLevelWindow_Destructor);
		HookHelper::Detours::Detach(&g_CTopLevelWindow_OnClipUpdated_Org, MyCTopLevelWindow_OnClipUpdated);
		HookHelper::Detours::Detach(&g_CWindowList_UpdateAccentBlurRect_Org, MyCWindowList_UpdateAccentBlurRect);
		if (os::buildNumber >= os::build_w10_2004)
		{
			HookHelper::Detours::Detach(&g_CTopLevelWindow_InitializeVisualTreeClone_Org, MyCTopLevelWindow_InitializeVisualTreeClone);
		}
		if (os::buildNumber == os::build_w11_21h2)
		{
			HookHelper::Detours::Detach(&g_CTopLevelWindow_UpdateSystemBackdropVisual_Org, MyCTopLevelWindow_UpdateSystemBackdropVisual);
		}
		if (os::buildNumber >= os::build_w11_22h2)
		{
			HookHelper::Detours::Detach(&g_CTopLevelWindow_CalculateBackgroundType_Org, MyCTopLevelWindow_CalculateBackgroundType);
		}
	});
	g_captureRef = 0;
	g_geometryBuffer.clear();
	BackdropManager::Shutdown();

	*dwmcore::CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta = g_oldBackdropBlurCachingThrottleQPCTimeDelta;
	CGlassReflectionVisual::Shutdown();
}