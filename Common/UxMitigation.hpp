#pragma once
#include <Windows.h>

namespace OpenGlass::UxMitigation
{
	enum Flags : DWORD
	{
		RestrictAccentOverride = 1u << 0
	};
	inline constexpr DWORD DefaultFlags = RestrictAccentOverride;
	// Set a nonzero value on the application's HWND before its Accent request.
	// Property changes alone do not notify DWM; eligibility changes on the next Accent update.
	inline constexpr PCWSTR NoAccentOverrideProperty = L"OpenGlass.NoAccentOverride";

	inline bool AllowsAccentOverride(HWND window, DWORD flags)
	{
		if (GetPropW(window, NoAccentOverrideProperty))
		{
			return false;
		}
		if (!(flags & RestrictAccentOverride))
		{
			return true;
		}
		constexpr PCWSTR shellClasses[]
		{
			L"Shell_TrayWnd",
			L"Shell_SecondaryTrayWnd",
			L"SIBJumpView",
			L"TaskListThumbnailWnd",
			L"NotifyIconOverflowWindow"
		};
		const auto windowClass = GetClassWord(window, GCW_ATOM);
		for (const auto name : shellClasses)
		{
			if (RegisterWindowMessageW(name) == windowClass)
			{
				return true;
			}
		}
		return RegisterWindowMessageW(L"DV2ControlHost") == windowClass && GetPropW(window, L"StartMenuTag");
	}
}
