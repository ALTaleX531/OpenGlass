#include "pch.h"
#include "TestHarness.hpp"
#include "UxMitigation.hpp"
#include <wil/resource.h>

namespace OpenGlassTests
{
	void TestAccentWhitelist()
	{
		using namespace OpenGlass::UxMitigation;
		Check(DefaultFlags == RestrictAccentOverride);
		struct Case { PCWSTR name; bool allowed; };
		constexpr Case cases[]
		{
			{ L"Shell_TrayWnd", true },
			{ L"Shell_SecondaryTrayWnd", true },
			{ L"SIBJumpView", true },
			{ L"TaskListThumbnailWnd", true },
			{ L"NotifyIconOverflowWindow", true },
			{ L"DV2ControlHost", false },
			{ L"OpenGlassTests.AccentPopup", false },
			{ L"Shell_TrayWnd.Other", false }
		};
		for (const auto& item : cases)
		{
			const auto name = item.name;
			WNDCLASSW windowClass{};
			windowClass.hInstance = GetModuleHandleW(nullptr);
			windowClass.lpfnWndProc = DefWindowProcW;
			windowClass.lpszClassName = name;
			const auto atom = RegisterClassW(&windowClass);
			Check(atom != 0);
			if (!atom) continue;
			const auto cleanupClass = wil::scope_exit([&] { UnregisterClassW(name, windowClass.hInstance); });
			const auto window = CreateWindowExW(0, name, L"", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, windowClass.hInstance, nullptr);
			Check(window != nullptr);
			if (!window) continue;
			const auto cleanupWindow = wil::scope_exit([&] { DestroyWindow(window); });
			Check(AllowsAccentOverride(window, 0));
			Check(AllowsAccentOverride(window, 0x80000000));
			Check(AllowsAccentOverride(window, DefaultFlags) == item.allowed);
			Check(AllowsAccentOverride(window, DefaultFlags | 0x80000000) == item.allowed);
			Check(SetPropW(window, L"StartMenuTag", reinterpret_cast<HANDLE>(1)) != FALSE);
			Check(AllowsAccentOverride(window, DefaultFlags) == (item.allowed || _wcsicmp(name, L"DV2ControlHost") == 0));
			Check(SetPropW(window, NoAccentOverrideProperty, reinterpret_cast<HANDLE>(1)) != FALSE);
			Check(!AllowsAccentOverride(window, DefaultFlags | 0x80000000));
			Check(!AllowsAccentOverride(window, 0));
			RemovePropW(window, NoAccentOverrideProperty);
			Check(AllowsAccentOverride(window, DefaultFlags) == (item.allowed || _wcsicmp(name, L"DV2ControlHost") == 0));
			RemovePropW(window, L"StartMenuTag");
			Check(AllowsAccentOverride(window, DefaultFlags) == item.allowed);
			Check(SetPropW(window, NoAccentOverrideProperty, reinterpret_cast<HANDLE>(1)) != FALSE);
			Check(!AllowsAccentOverride(window, DefaultFlags));
			Check(!AllowsAccentOverride(window, 0));
			RemovePropW(window, NoAccentOverrideProperty);
			Check(AllowsAccentOverride(window, DefaultFlags) == item.allowed);
			Check(AllowsAccentOverride(window, 0));
		}
		Check(!AllowsAccentOverride(nullptr, DefaultFlags));
	}
}
