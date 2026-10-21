#pragma once
#include <Windows.h>
#include <uxtheme.h>
#include <cwchar>

namespace OpenGlass
{
	inline bool IsDwmWindowTheme(HTHEME theme) noexcept
	{
		using GetThemeClassFunction = HRESULT(WINAPI*)(HTHEME, PWSTR, int);
		static const auto GetThemeClass = reinterpret_cast<GetThemeClassFunction>(
			GetProcAddress(GetModuleHandleW(L"uxtheme.dll"), MAKEINTRESOURCEA(74))
		);
		WCHAR className[10]{};
		// Separate opens of DWMWindow can own different UxTheme render objects.
		return theme && GetThemeClass
			&& SUCCEEDED(GetThemeClass(theme, className, ARRAYSIZE(className)))
			&& std::wcscmp(className, L"DWMWindow") == 0;
	}

	enum class DWM_WINDOW_THEME_PART : int
	{
		COMMON = 0,
		TEXTGLOW = 45,
		TOPFRAME = 46,
		SQUEEGEREFLECTIONMAP = 62
	};
}
