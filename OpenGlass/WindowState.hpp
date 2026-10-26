#pragma once
#include <Windows.h>

namespace OpenGlass::WindowState
{
	inline constexpr PCWSTR ActivationOverrideProperty = L"OpenGlass.ActivationOverride";
	inline constexpr PCWSTR MaximizationOverrideProperty = L"OpenGlass.MaximizationOverride";

	enum Override : ULONG_PTR
	{
		ForceFalse = 1,
		ForceTrue = 2
	};

	// Distinct nonzero values keep the hot path to a direct GetPropW lookup.
	// Property changes alone do not notify DWM.
	inline bool Resolve(HWND window, PCWSTR property, bool nativeValue)
	{
		switch (reinterpret_cast<ULONG_PTR>(GetPropW(window, property)))
		{
		case ForceFalse:
			return false;
		case ForceTrue:
			return true;
		default:
			return nativeValue;
		}
	}
}
