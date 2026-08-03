#pragma once

#include <KnownFolders.h>
#include <ShlObj.h>
#include <wil/resource.h>
#include <wil/result.h>

#include <filesystem>
#include <string_view>

#pragma comment(lib, "shell32.lib")

namespace OpenGlass::ApplicationPaths
{
	inline std::filesystem::path GetProgramDataRoot()
	{
		wil::unique_cotaskmem_string programData;
		THROW_IF_FAILED(SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT, nullptr, programData.put()));
		return std::filesystem::path{ programData.get() } / L"OpenGlass";
	}

	inline std::filesystem::path GetProgramDataSubdirectory(std::wstring_view name)
	{
		return GetProgramDataRoot() / name;
	}
}
