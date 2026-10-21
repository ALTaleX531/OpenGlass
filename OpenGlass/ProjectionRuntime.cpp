#include "pch.h"
#include "ProjectionHelper.hpp"

using namespace OpenGlass;

bool Projection::ModuleRegistry::Matches(const SymbolSpec& spec, LPCSTR completeSymbolName) const noexcept
{
	for (size_t index = 0; index < spec.nameCount; index++)
	{
		if (!strcmp(String(m_symbolNameOffsets[spec.firstNameIndex + index]), completeSymbolName))
		{
			return true;
		}
	}
	return false;
}

void Projection::ModuleRegistry::PublishBindings() noexcept
{
	for (const auto& binding : std::span{m_bindings, m_bindingCount})
	{
		PVOID value{binding.fallback};
		for (const auto symbolIndex : binding.symbolIndices)
		{
			if (m_resolved[symbolIndex])
			{
				value = m_resolved[symbolIndex];
				break;
			}
		}
		memcpy(binding.storage, &value, sizeof(value));
	}
}

bool Projection::ModuleRegistry::Freeze(Version version) noexcept
{
	m_version = version;
	if (!SupportsVersion(version))
	{
		std::fill_n(m_layoutSupported, m_layoutCount, false);
		ResetSymbols();
		return false;
	}

	for (size_t index = 0; index < m_layoutCount; index++)
	{
		const auto& layout = m_layoutSpecs[index];
		m_layoutSupported[index] = false;
		for (const auto& item : std::span{m_layoutCases + layout.firstCase, layout.caseCount})
		{
			const auto boundary = m_versions[item.boundaryVersionIndex];
			if (!boundary.build || VersionBefore(version, boundary))
			{
				m_selectedOffsets[index] = item.offset;
				m_layoutSupported[index] = true;
				break;
			}
		}
	}
	ResetSymbols();
	return true;
}

void Projection::ModuleRegistry::ResetSymbols() noexcept
{
	std::fill_n(m_candidates, m_symbolCount, nullptr);
	std::fill_n(m_resolved, m_symbolCount, nullptr);
	std::fill_n(m_resolutionStates, m_symbolCount, ResolutionState::Missing);
	m_undecorationFailureCount = 0;
	PublishBindings();
}

void Projection::ModuleRegistry::CollectAddress(size_t symbolIndex, PVOID address) noexcept
{
	if (!address)
	{
		return;
	}
	switch (m_resolutionStates[symbolIndex])
	{
	case ResolutionState::Missing:
		m_candidates[symbolIndex] = address;
		m_resolutionStates[symbolIndex] = ResolutionState::Unique;
		break;
	case ResolutionState::Unique:
		if (m_candidates[symbolIndex] != address)
		{
			m_resolutionStates[symbolIndex] = ResolutionState::Ambiguous;
		}
		break;
	case ResolutionState::Ambiguous:
		break;
	}
}

void Projection::ModuleRegistry::Collect(LPCSTR completeSymbolName, PVOID address) noexcept
{
	for (size_t specIndex = 0; specIndex < m_symbolSpecCount; specIndex++)
	{
		const auto& spec = m_symbolSpecs[specIndex];
		if (!IsEnabled(spec) || !IsVersionInRange(m_version, RangeOf(spec)) ||
			!Matches(spec, completeSymbolName))
		{
			continue;
		}
		CollectAddress(spec.symbolIndex, address);
	}
}

void Projection::ModuleRegistry::CollectCatalog(HMODULE module, std::span<const UINT32> rvas) noexcept
{
	const auto base = reinterpret_cast<BYTE*>(module);
	for (size_t index = 0; index < rvas.size(); index++)
	{
		const auto rva = rvas[index];
		m_candidates[index] = rva ? base + rva : nullptr;
		m_resolutionStates[index] = rva ? ResolutionState::Unique : ResolutionState::Missing;
	}
	m_undecorationFailureCount = 0;
}

void Projection::ModuleRegistry::RecordUndecorationFailure() noexcept
{
	m_undecorationFailureCount++;
}

bool Projection::ModuleRegistry::ValidateSymbols() const noexcept
{
	for (size_t specIndex = 0; specIndex < m_symbolSpecCount; specIndex++)
	{
		const auto& spec = m_symbolSpecs[specIndex];
		if (IsEnabled(spec) && IsRequired(spec) && IsVersionInRange(m_version, RangeOf(spec)) &&
			m_resolutionStates[spec.symbolIndex] != ResolutionState::Unique)
		{
			return false;
		}
	}
	return true;
}

void Projection::ModuleRegistry::CommitSymbols() noexcept
{
	std::fill_n(m_resolved, m_symbolCount, nullptr);
	for (size_t specIndex = 0; specIndex < m_symbolSpecCount; specIndex++)
	{
		const auto& spec = m_symbolSpecs[specIndex];
		if (IsEnabled(spec) && IsVersionInRange(m_version, RangeOf(spec)) &&
			m_resolutionStates[spec.symbolIndex] == ResolutionState::Unique)
		{
			m_resolved[spec.symbolIndex] = m_candidates[spec.symbolIndex];
		}
	}
	PublishBindings();
}

void Projection::ModuleRegistry::ReportUnresolved(std::string& output, std::string_view prefix) const
{
	bool hasUnresolvedRequiredSymbol{};
	for (size_t specIndex = 0; specIndex < m_symbolSpecCount; specIndex++)
	{
		const auto& spec = m_symbolSpecs[specIndex];
		if (!IsEnabled(spec) || !IsRequired(spec) || !IsVersionInRange(m_version, RangeOf(spec)) ||
			m_resolutionStates[spec.symbolIndex] == ResolutionState::Unique)
		{
			continue;
		}
		hasUnresolvedRequiredSymbol = true;
		output.append(prefix);
		output.append(String(spec.idOffset));
		output.append(m_resolutionStates[spec.symbolIndex] == ResolutionState::Ambiguous ? " (ambiguous)\n" : " (missing)\n");
	}
	if (hasUnresolvedRequiredSymbol && m_undecorationFailureCount)
	{
		output.append(prefix);
		output.append("complete-name undecoration failures: ");
		output.append(std::to_string(m_undecorationFailureCount));
		output.push_back('\n');
	}
}

PVOID Projection::ModuleRegistry::SymbolAddress(size_t index, bool checked) const
{
	const auto address = m_resolved[index];
	THROW_HR_IF(E_UNEXPECTED, checked && !address);
	return address;
}

Projection::VersionRange Projection::ModuleRegistry::SymbolRange(size_t index) const
{
	for (const auto& spec : std::span{m_symbolSpecs, m_symbolSpecCount})
	{
		if (spec.symbolIndex == index && IsEnabled(spec) && IsVersionInRange(m_version, RangeOf(spec)))
		{
			return RangeOf(spec);
		}
	}
	return {{1, 0}, {1, 0}};
}

LONG Projection::ModuleRegistry::LayoutOffset(size_t index) const
{
	THROW_HR_IF(E_UNEXPECTED, !m_layoutSupported[index]);
	return m_selectedOffsets[index];
}

bool Projection::CommitModules(ModuleRegistry& first, ModuleRegistry& second) noexcept
{
	if (!first.ValidateSymbols() || !second.ValidateSymbols())
	{
		return false;
	}
	first.CommitSymbols();
	second.CommitSymbols();
	return true;
}

__declspec(noinline) PVOID* Projection::PrepareDetourStorage(PVOID* storage, ModuleRegistry& registry, size_t index)
{
	if (!*storage)
	{
		*storage = registry.SymbolAddress(index, true);
	}
	return storage;
}
