#pragma once

#include <Windows.h>
#include <powersetting.h>

#include <string>
#include <string_view>

namespace OpenGlass
{
	enum class DiagnosticRegistrySource
	{
		Default,
		User,
		Machine
	};

	struct TransparencyDiagnostics
	{
		bool windowsTransparencyEnabled{ false };
		DWORD colorizationOpaqueBlend{ 0 };
		DiagnosticRegistrySource colorizationOpaqueBlendSource{ DiagnosticRegistrySource::Default };
		bool disableGlassOnBattery{ true };
		DiagnosticRegistrySource disableGlassOnBatterySource{ DiagnosticRegistrySource::Default };
		EFFECTIVE_POWER_MODE effectivePowerMode{ EffectivePowerModeBalanced };
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
