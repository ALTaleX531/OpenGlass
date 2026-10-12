#pragma once

#include "DwmSettings.hpp"

#include <string>
#include <string_view>

namespace OpenGlass
{
	struct TransparencyDiagnostics
	{
		bool windowsTransparencyEnabled{ false };
		DWORD colorizationOpaqueBlend{ 0 };
		DwmSettings::ValueSource colorizationOpaqueBlendSource{ DwmSettings::ValueSource::Default };
		bool disableGlassOnBattery{ true };
		DwmSettings::ValueSource disableGlassOnBatterySource{ DwmSettings::ValueSource::Default };
		bool powerSaverActive{ false };
	};

	struct DwmCrashDumpConfiguration
	{
		bool enabled{ false };
		DWORD dumpType{ 1 };
		DWORD dumpCount{ 10 };
		std::wstring dumpFolder;
	};

	[[nodiscard]] std::wstring GetDefaultDwmCrashDumpFolder();
	[[nodiscard]] HRESULT QueryTransparencyDiagnostics(std::wstring_view userSid, TransparencyDiagnostics& diagnostics) noexcept;
	[[nodiscard]] HRESULT QueryDwmCrashDumpConfiguration(DwmCrashDumpConfiguration& configuration) noexcept;
	[[nodiscard]] HRESULT EnableDwmCrashDumps(std::wstring_view requestedFolder, std::wstring& configuredFolder) noexcept;
	[[nodiscard]] HRESULT DisableDwmCrashDumps() noexcept;
}
