#pragma once
#include "pch.h"
#include "SettingsCatalog.hpp"

namespace OpenGlass::Elevation
{
	struct StartupResult
	{
		bool continueStartup{};
		std::wstring userSid;
	};

	[[nodiscard]] StartupResult PrepareElevatedStartup(Settings::Scope scope, bool systemTransparency);
	[[nodiscard]] bool IsProcessElevated() noexcept;
}
