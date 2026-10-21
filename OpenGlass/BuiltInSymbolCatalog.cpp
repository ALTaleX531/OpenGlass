#include "pch.h"
#include "SymbolCatalog.hpp"
#include "SymbolCatalog.generated.hpp"

bool OpenGlass::Projection::CollectBuiltInSymbols(
	HMODULE module,
	ModuleId moduleId,
	ModuleRegistry& registry
) noexcept
{
	return CollectSymbolsFromCatalog(module, moduleId, registry, g_builtInSymbolCatalog);
}
