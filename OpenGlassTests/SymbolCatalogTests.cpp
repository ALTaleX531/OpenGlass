#include "pch.h"
#include "TestHarness.hpp"
#include "ProjectionFixture.hpp"
#include "SymbolCatalog.hpp"
#include "PeCodeViewIdentity.hpp"
#include "Util.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestSymbolCatalogCollection()
	{
		RegistryStorage<2, 2, 0, 0> storage;
		size_t cursor{1};
		const auto requiredId = AddString(storage, cursor, "Catalog.Target");
		const auto optionalId = AddString(storage, cursor, "Catalog.Optional");
		storage.symbolNameOffsets[0] = AddString(storage, cursor, "Optional");
		storage.symbols = {{{0, requiredId, 0, 0, 0, 0, Projection::Requirement::Required, false},
			{1, optionalId, 0, 1, 0, 0, Projection::Requirement::Optional, false}}};
		PVOID published{}, optionalPublished{};
		storage.bindings[0] = {std::span{storage.bindingSymbols}.first(1), &published, Util::force_cast_from(&Replacement2)};
		storage.bindings[1] = {std::span{storage.bindingSymbols}.subspan(1), &optionalPublished, nullptr};
		const auto module = GetModuleHandleW(nullptr);
		PeCodeViewIdentity identity{};
		Check(SUCCEEDED(ReadLoadedPeCodeViewIdentity(module, identity)));
		const auto base = reinterpret_cast<const BYTE*>(module);
		const auto targetRva = static_cast<UINT32>(reinterpret_cast<const BYTE*>(Util::force_cast_from(&Target)) - base);
		const auto replacementRva = static_cast<UINT32>(reinterpret_cast<const BYTE*>(Util::force_cast_from(&Replacement)) - base);
		const std::array firstRvas{targetRva, 0u};
		const std::array secondRvas{replacementRva, 0u};
		std::array records
		{
			Projection::SymbolCatalogRecord{Projection::ModuleId::DwmCore, identity.pdbGuid, identity.pdbAge, secondRvas},
			Projection::SymbolCatalogRecord{Projection::ModuleId::uDWM, identity.pdbGuid, identity.pdbAge, firstRvas}
		};
		const Projection::SymbolCatalog catalog{records};
		Check(storage.registry.Freeze({100, 7}));
		storage.registry.Collect("Optional", Util::force_cast_from(&Replacement));
		Check(Projection::CollectSymbolsFromCatalog(module, Projection::ModuleId::uDWM, storage.registry, catalog));
		Check(storage.registry.SymbolAddress(0, false) == nullptr);
		Check(published == Util::force_cast_from(&Replacement2));
		Check(storage.registry.ValidateSymbols());
		storage.registry.CommitSymbols();
		Check(published == Util::force_cast_from(&Target));
		Check(storage.registry.SymbolAddress(1, false) == nullptr);

		records[1].pdbGuid.Data4[7] ^= 1;
		Check(!Projection::CollectSymbolsFromCatalog(module, Projection::ModuleId::uDWM, storage.registry, catalog));
		records[1].pdbGuid = identity.pdbGuid;
		++records[1].pdbAge;
		Check(!Projection::CollectSymbolsFromCatalog(module, Projection::ModuleId::uDWM, storage.registry, catalog));
		Check(published == Util::force_cast_from(&Target));
		records[1].pdbAge = identity.pdbAge;
		records[1].rvas = secondRvas;
		Check(Projection::CollectSymbolsFromCatalog(module, Projection::ModuleId::uDWM, storage.registry, catalog));
		Check(published == Util::force_cast_from(&Target));
		Check(storage.registry.ValidateSymbols());
		storage.registry.CommitSymbols();
		Check(published == Util::force_cast_from(&Replacement));

		const std::array optionalRvas{targetRva, replacementRva};
		records[1].rvas = optionalRvas;
		Check(Projection::CollectSymbolsFromCatalog(module, Projection::ModuleId::uDWM, storage.registry, catalog));
		storage.registry.CommitSymbols();
		Check(optionalPublished == Util::force_cast_from(&Replacement));
		records[1].rvas = firstRvas;
		Check(Projection::CollectSymbolsFromCatalog(module, Projection::ModuleId::uDWM, storage.registry, catalog));
		Check(optionalPublished == Util::force_cast_from(&Replacement));
		storage.registry.CommitSymbols();
		Check(storage.registry.SymbolAddress(1, false) == nullptr);
		Check(optionalPublished == nullptr);
	}
}
