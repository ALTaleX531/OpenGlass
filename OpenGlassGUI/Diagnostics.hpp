#pragma once

#include <Windows.h>

#include <string>
#include <string_view>

namespace OpenGlass
{
	struct DwmCrashDumpConfiguration
	{
		bool enabled{ false };
		DWORD dumpType{ 1 };
		DWORD dumpCount{ 10 };
		std::wstring dumpFolder;
	};

	[[nodiscard]] std::wstring GetDefaultDwmCrashDumpFolder();
	[[nodiscard]] HRESULT QueryDwmCrashDumpConfiguration(DwmCrashDumpConfiguration& configuration) noexcept;
	[[nodiscard]] HRESULT EnableDwmCrashDumps(std::wstring_view requestedFolder, std::wstring& configuredFolder) noexcept;
	[[nodiscard]] HRESULT DisableDwmCrashDumps() noexcept;
}
