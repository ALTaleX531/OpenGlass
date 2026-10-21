#pragma once

#include "ProjectionHelper.hpp"

namespace OpenGlass::Projection
{
	// Internal generated records: keys and RVA slices are validated before compilation.
	struct SymbolCatalogRecord
	{
		ModuleId module;
		GUID pdbGuid;
		DWORD pdbAge;
		std::span<const UINT32> rvas;
	};

	struct SymbolCatalog
	{
		std::span<const SymbolCatalogRecord> records;
	};

	[[nodiscard]] bool CollectSymbolsFromCatalog(
		HMODULE module,
		ModuleId moduleId,
		ModuleRegistry& registry,
		const SymbolCatalog& catalog
	) noexcept;

	[[nodiscard]] bool CollectBuiltInSymbols(
		HMODULE module,
		ModuleId moduleId,
		ModuleRegistry& registry
	) noexcept;
}
