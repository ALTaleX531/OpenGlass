#include "pch.h"
#include "PeCodeViewIdentity.hpp"
#include "SymbolCatalog.hpp"

namespace OpenGlass::Projection
{
	bool CollectSymbolsFromCatalog(
		HMODULE module,
		ModuleId moduleId,
		ModuleRegistry& registry,
		const SymbolCatalog& catalog
	) noexcept
	{
		PeCodeViewIdentity identity{};
		if (FAILED(ReadLoadedPeCodeViewIdentity(module, identity)))
		{
			return false;
		}
		for (const auto& record : catalog.records)
		{
			if (record.module == moduleId && InlineIsEqualGUID(record.pdbGuid, identity.pdbGuid) &&
				record.pdbAge == identity.pdbAge)
			{
				registry.CollectCatalog(module, record.rvas);
				return true;
			}
		}
		return false;
	}
}
