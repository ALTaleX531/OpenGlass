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
	const auto bindings = std::span{m_bindings, m_bindingCount};
	for (size_t bindingIndex = 0; bindingIndex < bindings.size(); bindingIndex++)
	{
		const auto& binding = bindings[bindingIndex];
		if (std::find_if(bindings.begin(), bindings.begin() + bindingIndex, [&](const BindingSpec& prior)
		{
			return prior.storage == binding.storage;
		}) != bindings.begin() + bindingIndex)
		{
			continue;
		}

		PVOID value{binding.fallback};
		for (const auto& candidateBinding : bindings)
		{
			if (candidateBinding.storage != binding.storage || candidateBinding.symbolIndex >= m_symbolCount)
			{
				continue;
			}
			const auto symbolIndex = candidateBinding.symbolIndex;
			if (m_resolutionStates[symbolIndex] == ResolutionState::Unique)
			{
				value = m_candidates[symbolIndex];
				break;
			}
		}
		memcpy(binding.storage, &value, sizeof(value));
	}
}

bool Projection::ModuleRegistry::Freeze(Version version) noexcept
{
	m_version = version;
	m_descriptorError = false;

	if (!SupportsVersion(version))
	{
		std::fill_n(m_layoutSupported, m_layoutCount, false);
		ResetSymbols();
		return false;
	}

	for (const auto& spec : std::span{m_symbolSpecs, m_symbolSpecCount})
	{
		if (spec.symbolIndex >= m_symbolCount || !spec.nameCount ||
			static_cast<size_t>(spec.firstNameIndex) + spec.nameCount > m_symbolNameCount ||
			spec.minVersionIndex >= m_versionCount || spec.maxVersionIndex >= m_versionCount)
		{
			m_descriptorError = true;
		}
	}

	for (size_t symbolIndex = 0; symbolIndex < m_symbolCount; symbolIndex++)
	{
		bool activeBinding{};
		for (const auto& spec : std::span{m_symbolSpecs, m_symbolSpecCount})
		{
			if (spec.symbolIndex != symbolIndex || !IsEnabled(spec) ||
				!IsVersionInRange(version, RangeOf(spec)))
			{
				continue;
			}
			if (activeBinding)
			{
				m_descriptorError = true;
				break;
			}
			activeBinding = true;
		}
	}

	for (size_t index = 0; index < m_layoutCount; index++)
	{
		const auto& layout = m_layoutSpecs[index];
		m_layoutSupported[index] = false;
		if (static_cast<size_t>(layout.firstCase) + layout.caseCount > m_layoutCaseCount)
		{
			m_descriptorError = true;
			continue;
		}
		for (const auto& item : std::span{m_layoutCases + layout.firstCase, layout.caseCount})
		{
			if (item.boundaryVersionIndex >= m_versionCount)
			{
				m_descriptorError = true;
				break;
			}
			const auto boundary = VersionAt(item.boundaryVersionIndex);
			if (!boundary.build || VersionBefore(version, boundary))
			{
				m_selectedOffsets[index] = item.offset;
				m_layoutSupported[index] = true;
				break;
			}
		}
	}
	ResetSymbols();
	return !m_descriptorError;
}

void Projection::ModuleRegistry::ResetSymbols() noexcept
{
	std::fill_n(m_candidates, m_symbolCount, nullptr);
	std::fill_n(m_resolved, m_symbolCount, nullptr);
	std::fill_n(m_resolutionStates, m_symbolCount, ResolutionState::Missing);
	m_undecorationFailureCount = 0;
	PublishBindings();
}

bool Projection::ModuleRegistry::CollectAddress(size_t symbolIndex, PVOID address) noexcept
{
	if (symbolIndex >= m_symbolCount || !address)
	{
		return false;
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
	return true;
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

bool Projection::ModuleRegistry::CollectResolvedAddress(size_t symbolIndex, PVOID address) noexcept
{
	return CollectAddress(symbolIndex, address);
}
bool Projection::ModuleRegistry::SymbolIsData(size_t symbolIndex, bool& isData) const noexcept
{
	if (symbolIndex >= m_symbolCount)
	{
		return false;
	}
	for (size_t specIndex = 0; specIndex < m_symbolSpecCount; specIndex++)
	{
		const auto& spec = m_symbolSpecs[specIndex];
		if (spec.symbolIndex == symbolIndex && IsEnabled(spec) && IsVersionInRange(m_version, RangeOf(spec)))
		{
			isData = HasFlag(spec.flags, SymbolFlags::Data);
			return true;
		}
	}
	return false;
}


void Projection::ModuleRegistry::RecordUndecorationFailure() noexcept
{
	if (m_undecorationFailureCount != ULONG_MAX)
	{
		m_undecorationFailureCount++;
	}
}

bool Projection::ModuleRegistry::ValidateSymbols() const noexcept
{
	if (m_descriptorError)
	{
		return false;
	}
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

[[noreturn]] void Projection::ProjectedFailFastCommon() noexcept
{
	FAIL_FAST_HR(E_UNEXPECTED);
	__assume(0);
}

__declspec(noinline) PVOID* Projection::PrepareDetourStorage(PVOID* storage, ModuleRegistry& registry, size_t index)
{
	if (!*storage)
	{
		*storage = registry.SymbolAddress(index, true);
	}
	return storage;
}
