#include "pch.h"
#include "OcclusionCulling.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "BackdropManager.hpp"

using namespace OpenGlass;
namespace OpenGlass::OcclusionCulling
{
	HRESULT WINAPI MyCArrayBasedCoverageSet_AddAntiOccluderRect(
		dwmcore::CArrayBasedCoverageSet* This,
		const D2D1_RECT_F& lprc,
		int unknown,
		const MilMatrix3x2D* matrix
	);
	HRESULT STDMETHODCALLTYPE MyCWindowList_StyleChange(uDwm::CWindowList* This, struct IDwmWindow* windowContext);
	HRESULT STDMETHODCALLTYPE MyCWindowList_CloakChange(uDwm::CWindowList* This, struct IDwmWindow* windowContext1, struct IDwmWindow* windowContext2, bool cloaked);
	HRESULT STDMETHODCALLTYPE MyCWindowList_CheckForMaximizedChange(uDwm::CWindowList* This, uDwm::CWindowData* data);

	decltype(&MyCArrayBasedCoverageSet_AddAntiOccluderRect) g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org{ nullptr };
	decltype(&MyCWindowList_StyleChange) g_CWindowList_StyleChange_Org{ nullptr };
	decltype(&MyCWindowList_CloakChange) g_CWindowList_CloakChange_Org{ nullptr };
	decltype(&MyCWindowList_CheckForMaximizedChange) g_CWindowList_CheckForMaximizedChange_Org{ nullptr };

	int g_occlusionCalculationLevel{ 0 };

	bool IsSubRegion(HRGN child, HRGN parent)
	{
		static wil::unique_hrgn region{ CreateRectRgn(0, 0, 0, 0) };
		auto result{ CombineRgn(region.get(), child, parent, g_occlusionCalculationLevel > 0 ? RGN_OR : RGN_AND) };

		if (g_occlusionCalculationLevel > 0)
		{
			return static_cast<bool>(EqualRgn(region.get(), parent));
		}
		return result != NULLREGION;
		
	}
	bool IsRegionIntersect(HRGN region1, HRGN region2)
	{
		static wil::unique_hrgn region{ CreateRectRgn(0, 0, 0, 0) };
		return CombineRgn(region.get(), region1, region2, RGN_AND) != NULLREGION;
	}
	void CalculateOcclusionRegionForWindow(
		wil::unique_hrgn& occlusionRegion,
		HWND hwnd,
		uDwm::CWindowData* data, 
		uDwm::CTopLevelWindow* window,
		wil::unique_hrgn& windowOcclusionRegion,
		wil::unique_hrgn& backdropRegion
	)
	{
		RECT windowRect{};
		if (
			data->IsWindowVisibleAndUncloaked() &&
			!IsMinimized(hwnd) &&
			!(GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) &&
			GetClassWord(hwnd, GCW_ATOM) != RegisterWindowMessageW(L"#32769")
		)
		{
			window->GetActualWindowRect(&windowRect, false, true, true);
			windowOcclusionRegion.reset(CreateRectRgnIndirect(&windowRect));
		}

		auto backdrop{ BackdropManager::GetOrCreateBackdropVisual(window) };
		if (backdrop)
		{
			CopyRgn(backdropRegion.get(), backdrop.as<BackdropManager::ICompositedBackdropVisualPrivate>()->GetCompositedRegion());
			OffsetRgn(backdropRegion.get(), windowRect.left, windowRect.top);
			CombineRgn(windowOcclusionRegion.get(), windowOcclusionRegion.get(), backdropRegion.get(), RGN_DIFF);
		}

		CombineRgn(occlusionRegion.get(), occlusionRegion.get(), windowOcclusionRegion.get(), RGN_OR);
	}
	void UpdateOcclusionInfo(uDwm::CWindowList* This)
	{
		ULONG_PTR desktopID{ 0 };
		Utils::GetDesktopID(1, &desktopID);

		bool fullyOccluded{ false };
		auto backdropCount{ BackdropManager::GetBackdropCount() };
		RECT fullOcclusionRect
		{
			GetSystemMetrics(SM_XVIRTUALSCREEN),
			GetSystemMetrics(SM_YVIRTUALSCREEN),
			GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
			GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN)
		};
		wil::unique_hrgn fullOcclusionRegion
		{ 
			CreateRectRgnIndirect(
				&fullOcclusionRect
			)
		};
		wil::unique_hrgn occlusionRegion{ CreateRectRgn(0, 0, 0, 0) };
		auto windowList{ This->GetWindowListForDesktop(desktopID) };
		for (auto i{ windowList->Blink }; i != windowList; i = i->Blink)
		{
			auto data{ reinterpret_cast<uDwm::CWindowData*>(i) };
			auto hwnd{ data->GetHwnd() };
			if (!hwnd || !IsWindow(hwnd)) { continue; }
			auto window{ data->GetWindow() };
			if (!window) { continue; }

			auto backdrop{ BackdropManager::GetOrCreateBackdropVisual(window) };

			if (!fullyOccluded)
			{
				static wil::unique_hrgn backdropRegion{ CreateRectRgn(0, 0, 0, 0) };

				// more accurate but more time-consuming implementation
				if (g_occlusionCalculationLevel > 0)
				{
					wil::unique_hrgn windowOcclusionRegion{ CreateRectRgn(0, 0, 0, 0) };

					CalculateOcclusionRegionForWindow(occlusionRegion, hwnd, data, window, windowOcclusionRegion, backdropRegion);

					if (EqualRgn(occlusionRegion.get(), fullOcclusionRegion.get()))
					{
						fullyOccluded = true;
					}
				}
				// much faster one...
				else
				{
					RECT windowRect{};
					window->GetActualWindowRect(&windowRect, false, true, true);
					backdropRegion.reset(CreateRectRgnIndirect(&windowRect));

					if (
						data->IsWindowVisibleAndUncloaked() &&
						!(GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED)
					)
					{
						if (
							EqualRect(&windowRect, &fullOcclusionRect) &&
							GetClassWord(hwnd, GCW_ATOM) != RegisterWindowMessageW(L"#32769")
						)
						{
							fullyOccluded = true;
						}
						if (IsMaximized(hwnd))
						{
							CombineRgn(occlusionRegion.get(), occlusionRegion.get(), backdropRegion.get(), RGN_OR);
							continue;
						}
					}
				}

				if (backdrop)
				{
					backdrop.as<BackdropManager::ICompositedBackdropVisualPrivate>()->MarkAsOccluded(IsSubRegion(backdropRegion.get(), occlusionRegion.get()) ? true : false);
					backdropCount -= 1;
				}
			}
			else if (backdrop)
			{
				backdrop.as<BackdropManager::ICompositedBackdropVisualPrivate>()->MarkAsOccluded(true);
				backdropCount -= 1;
			}

			if (!backdropCount)
			{
				break;
			}
		}
	}
}

HRESULT WINAPI OcclusionCulling::MyCArrayBasedCoverageSet_AddAntiOccluderRect(
	dwmcore::CArrayBasedCoverageSet* This,
	const D2D1_RECT_F& lprc,
	int unknown,
	const MilMatrix3x2D* matrix
)
{
	/*OutputDebugStringW(
		std::format(
			L"lprc:[{},{},{},{}] - {}\n",
			lprc.left, lprc.top, lprc.right, lprc.bottom,
			unknown
		).c_str()
	);*/
	return S_OK;
}
HRESULT STDMETHODCALLTYPE OcclusionCulling::MyCWindowList_StyleChange(uDwm::CWindowList* This, struct IDwmWindow* windowContext)
{
	HRESULT hr{ g_CWindowList_StyleChange_Org(This, windowContext) };

	if (g_occlusionCalculationLevel >= 0)
	{
		UpdateOcclusionInfo(This);
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE OcclusionCulling::MyCWindowList_CloakChange(uDwm::CWindowList* This, struct IDwmWindow* windowContext1, struct IDwmWindow* windowContext2, bool cloaked)
{
	HRESULT hr{ g_CWindowList_CloakChange_Org(This, windowContext1, windowContext2, cloaked) };

	if (g_occlusionCalculationLevel >= 0)
	{
		UpdateOcclusionInfo(This);
	}

	return hr;
}
HRESULT STDMETHODCALLTYPE OcclusionCulling::MyCWindowList_CheckForMaximizedChange(uDwm::CWindowList* This, uDwm::CWindowData* data)
{
	HRESULT hr{ g_CWindowList_CheckForMaximizedChange_Org(This, data) };

	if (g_occlusionCalculationLevel > 0 )
	{
		UpdateOcclusionInfo(This);
	}

	return hr;
}

void OcclusionCulling::InitializeFromSymbol(std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset)
{
	if (fullyUnDecoratedFunctionName == "CWindowList::StyleChange")
	{
		offset.To(uDwm::g_moduleHandle, g_CWindowList_StyleChange_Org);
	}
	if (fullyUnDecoratedFunctionName == "CWindowList::CloakChange")
	{
		offset.To(uDwm::g_moduleHandle, g_CWindowList_CloakChange_Org);
	}
	if (fullyUnDecoratedFunctionName == "CWindowList::CheckForMaximizedChange")
	{
		offset.To(uDwm::g_moduleHandle, g_CWindowList_CheckForMaximizedChange_Org);
	}
}

void OcclusionCulling::UpdateConfiguration(ConfigurationFramework::UpdateType type)
{
	if (type & ConfigurationFramework::UpdateType::Backdrop)
	{
		wil::reg::get_value_dword_nothrow(
			ConfigurationFramework::GetDwmKey(),
			L"OcclusionCalculationLevel",
			reinterpret_cast<DWORD*>(&g_occlusionCalculationLevel)
		);
	}
}

HRESULT OcclusionCulling::Startup()
{
	g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org = dwmcore::CArrayBasedCoverageSet::s_AddAntiOccluderRect;
	return HookHelper::Detours::Write([]()
	{
		if (os::buildNumber >= os::build_w10_2004 && os::buildNumber < os::build_w11_21h2)
		{
			HookHelper::Detours::Attach(&g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org, MyCArrayBasedCoverageSet_AddAntiOccluderRect);
		}
		HookHelper::Detours::Attach(&g_CWindowList_StyleChange_Org, MyCWindowList_StyleChange);
		HookHelper::Detours::Attach(&g_CWindowList_CloakChange_Org, MyCWindowList_CloakChange);
		HookHelper::Detours::Attach(&g_CWindowList_CheckForMaximizedChange_Org, MyCWindowList_CheckForMaximizedChange);
	});
}
void OcclusionCulling::Shutdown()
{
	HookHelper::Detours::Write([]()
	{
		if (os::buildNumber >= os::build_w10_2004 && os::buildNumber < os::build_w11_21h2)
		{
			HookHelper::Detours::Detach(&g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org, MyCArrayBasedCoverageSet_AddAntiOccluderRect);
		}
		HookHelper::Detours::Detach(&g_CWindowList_StyleChange_Org, MyCWindowList_StyleChange);
		HookHelper::Detours::Detach(&g_CWindowList_CloakChange_Org, MyCWindowList_CloakChange);
		HookHelper::Detours::Detach(&g_CWindowList_CheckForMaximizedChange_Org, MyCWindowList_CheckForMaximizedChange);
	});
}