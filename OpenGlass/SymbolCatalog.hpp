#pragma once

#include "ProjectionHelper.hpp"

namespace OpenGlass::Projection
{


	struct SymbolCatalogRecord
	{
		ModuleId module;
		WORD machine;
		DWORD timeDateStamp;
		DWORD sizeOfImage;
		GUID pdbGuid;
		DWORD pdbAge;
		UINT32 pdbNameOffset;
		Version version;
		UINT32 firstEntry;
		UINT32 entryCount;
	};

	struct SymbolCatalog
	{
		LPCWSTR strings;
		size_t stringLength;
		std::span<const SymbolCatalogRecord> records;
		std::span<const UINT32> entries;
	};

	enum class SymbolCatalogResult : UCHAR
	{
		NotFound,
		Collected,
		Rejected
	};

	[[nodiscard]] SymbolCatalogResult CollectSymbolsFromCatalog(
		HMODULE module,
		ModuleId moduleId,
		ModuleRegistry& registry,
		const SymbolCatalog& catalog
	) noexcept;

	[[nodiscard]] SymbolCatalogResult CollectBuiltInSymbols(
		HMODULE module,
		ModuleId moduleId,
		ModuleRegistry& registry
	) noexcept;
}
