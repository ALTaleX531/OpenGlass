#pragma once

#include <Windows.h>

#include <string>

namespace OpenGlass
{
	struct PeCodeViewIdentity
	{
		WORD machine{};
		DWORD timeDateStamp{};
		DWORD sizeOfImage{};
		GUID pdbGuid{};
		DWORD pdbAge{};
		std::wstring pdbName;
	};

	[[nodiscard]] std::wstring BuildSymbolServerUrl(PCWSTR symbolServerBase, const PeCodeViewIdentity& identity);

	[[nodiscard]] HRESULT ReadLoadedPeCodeViewIdentity(
		HMODULE module,
		PeCodeViewIdentity& identity
	) noexcept;

	[[nodiscard]] HRESULT ReadFilePeCodeViewIdentity(
		PCWSTR path,
		PeCodeViewIdentity& identity
	) noexcept;
}
