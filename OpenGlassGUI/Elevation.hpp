#pragma once
#include "pch.h"

namespace OpenGlass::Elevation
{
	struct StartupResult
	{
		bool continueStartup{};
		std::wstring userSid;
	};

	[[nodiscard]] StartupResult PrepareElevatedStartup();
	[[nodiscard]] bool IsProcessElevated() noexcept;
}
