#pragma once

#include <optional>

namespace OpenGlass
{
	enum class RegistryValueSource
	{
		UserOverride,
		UserBase,
		MachineOverride,
		MachineBase,
		Default
	};

	template <typename T>
	struct ResolvedRegistryValue
	{
		T value;
		RegistryValueSource source;

		[[nodiscard]] constexpr bool IsOverride() const noexcept
		{
			return source == RegistryValueSource::UserOverride
				|| source == RegistryValueSource::MachineOverride;
		}
	};

	template <typename T>
	[[nodiscard]] constexpr ResolvedRegistryValue<T> ResolveOverridableRegistryValue(
		const std::optional<T>& userOverride,
		const std::optional<T>& userBase,
		const std::optional<T>& machineOverride,
		const std::optional<T>& machineBase,
		T defaultValue
	) noexcept
	{
		if (userOverride)
		{
			return { *userOverride, RegistryValueSource::UserOverride };
		}
		if (userBase)
		{
			return { *userBase, RegistryValueSource::UserBase };
		}
		if (machineOverride)
		{
			return { *machineOverride, RegistryValueSource::MachineOverride };
		}
		if (machineBase)
		{
			return { *machineBase, RegistryValueSource::MachineBase };
		}
		return { defaultValue, RegistryValueSource::Default };
	}
}
