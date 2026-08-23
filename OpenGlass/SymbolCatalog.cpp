#include "pch.h"
#include "PeCodeViewIdentity.hpp"
#include "SymbolCatalog.hpp"

namespace OpenGlass::Projection
{
	namespace
	{
		[[nodiscard]] bool SameVersion(Version left, Version right) noexcept
		{
			return left.build == right.build && left.revision == right.revision;
		}

		[[nodiscard]] bool SamePdbName(PCWSTR left, PCWSTR right) noexcept
		{
			return CompareStringOrdinal(left, -1, right, -1, TRUE) == CSTR_EQUAL;
		}

		[[nodiscard]] bool IsRvaInExpectedSection(
			HMODULE module,
			DWORD sizeOfImage,
			DWORD rva,
			bool isData
		) noexcept
		{
			if (!module || !rva || rva >= sizeOfImage)
			{
				return false;
			}
			const auto base = reinterpret_cast<const BYTE*>(module);
			const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 ||
				sizeOfImage < sizeof(IMAGE_NT_HEADERS) ||
				static_cast<size_t>(dos->e_lfanew) > static_cast<size_t>(sizeOfImage) - sizeof(IMAGE_NT_HEADERS))
			{
				return false;
			}
			const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
			if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.SizeOfImage != sizeOfImage)
			{
				return false;
			}
			const auto section = IMAGE_FIRST_SECTION(nt);
			const size_t sectionTableOffset = static_cast<size_t>(reinterpret_cast<const BYTE*>(section) - base);
			if (sectionTableOffset > sizeOfImage || static_cast<size_t>(nt->FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER) > sizeOfImage - sectionTableOffset)
			{
				return false;
			}
			for (WORD index = 0; index < nt->FileHeader.NumberOfSections; index++)
			{
				const auto& item = section[index];
				const DWORD sectionSize = (std::max)(item.Misc.VirtualSize, item.SizeOfRawData);
				if (rva < item.VirtualAddress || static_cast<ULONGLONG>(rva) >= static_cast<ULONGLONG>(item.VirtualAddress) + sectionSize)
				{
					continue;
				}
				return isData || (item.Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
			}
			return false;
		}

		[[nodiscard]] PCWSTR CatalogString(const SymbolCatalog& catalog, size_t offset) noexcept
		{
			if (!catalog.strings || offset >= catalog.stringLength)
			{
				return nullptr;
			}
			const auto value = catalog.strings + offset;
			return wmemchr(value, L'\0', catalog.stringLength - offset) ? value : nullptr;
		}

		[[nodiscard]] bool RecordMatches(
			const SymbolCatalog& catalog,
			const SymbolCatalogRecord& record,
			ModuleId moduleId,
			const PeCodeViewIdentity& identity
		) noexcept
		{
			const auto pdbName = CatalogString(catalog, record.pdbNameOffset);
			return
				pdbName &&
				record.module == moduleId &&
				record.machine == identity.machine &&
				record.timeDateStamp == identity.timeDateStamp &&
				record.sizeOfImage == identity.sizeOfImage &&
				InlineIsEqualGUID(record.pdbGuid, identity.pdbGuid) &&
				record.pdbAge == identity.pdbAge &&
				SamePdbName(pdbName, identity.pdbName.c_str());
		}
	}

	SymbolCatalogResult CollectSymbolsFromCatalog(
		HMODULE module,
		ModuleId moduleId,
		ModuleRegistry& registry,
		const SymbolCatalog& catalog
	) noexcept
	{
		PeCodeViewIdentity identity{};
		if (FAILED(ReadLoadedPeCodeViewIdentity(module, identity)))
		{
			return SymbolCatalogResult::NotFound;
		}
		const auto reject = [&registry]() noexcept
		{
			registry.ResetSymbols();
			return SymbolCatalogResult::Rejected;
		};


		const SymbolCatalogRecord* selected{};
		for (const auto& record : catalog.records)
		{
			if (!RecordMatches(catalog, record, moduleId, identity))
			{
				continue;
			}
			if (selected)
			{
				return reject();
			}
			selected = &record;
		}
		if (!selected)
		{
			return SymbolCatalogResult::NotFound;
		}
		if (!SameVersion(selected->version, registry.version()) ||
			selected->entryCount != registry.symbol_count() ||
			selected->firstEntry > catalog.entries.size() ||
			selected->entryCount > catalog.entries.size() - selected->firstEntry)
		{
			return reject();
		}

		const auto entries = catalog.entries.subspan(selected->firstEntry, selected->entryCount);
		for (size_t symbolIndex = 0; symbolIndex < entries.size(); symbolIndex++)
		{
			const auto rva = entries[symbolIndex];
			if (!rva)
			{
				continue;
			}
			bool isData{};
			if (!registry.SymbolIsData(symbolIndex, isData) ||
				!IsRvaInExpectedSection(module, identity.sizeOfImage, rva, isData))
			{
				return reject();
			}
		}

		registry.ResetSymbols();
		for (size_t symbolIndex = 0; symbolIndex < entries.size(); symbolIndex++)
		{
			const auto rva = entries[symbolIndex];
			if (rva && !registry.CollectResolvedAddress(symbolIndex, reinterpret_cast<BYTE*>(module) + rva))
			{
				return reject();
			}
		}
		if (!registry.ValidateSymbols())
		{
			return reject();
		}
		return SymbolCatalogResult::Collected;
	}
}
