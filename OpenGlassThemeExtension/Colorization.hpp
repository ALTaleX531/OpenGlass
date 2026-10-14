#pragma once

#define NOMINMAX
#include <windows.h>
#include "../Common/Colorization.hpp"

namespace OpenGlass::Colorization
{
	// dwmapi ordinals 127/131, verified on 10.0.26100.9549. The sixth DWORD is
	// EnableWindowColorization on modern Windows, not Win7's opaque-blend flag.
	struct ColorizationParameters
	{
		DWORD color;
		DWORD afterglow;
		DWORD colorBalance;
		DWORD afterglowBalance;
		DWORD blurBalance;
		DWORD enableWindowColorization;
		DWORD glassAttribute;
		DWORD flags;
	};
	static_assert(sizeof(ColorizationParameters) == 32);
	ColorizationParameters CalculateColorization(const ColorizationParameters& input, bool opaque) noexcept;
	// Explicit DWM key handles also allow isolated registry tests. machineDwm may be null.
	ColorizationParameters ReadColorization(HKEY userDwm, HKEY machineDwm);
	// A null userAccent skips AccentColorMenu persistence; DWM values always update.
	void WriteColorization(HKEY userDwm, HKEY userAccent, const ColorizationParameters& parameters);
}
