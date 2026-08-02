#include "pch.h"
#include "GlassEngine.hpp"

namespace OpenGlass::GlassEngine
{
	DWORD GetOverridableDwordFromRegistry(
		PCWSTR baseKeyName,
		PCWSTR overrideKeyName,
		DWORD defaultValue
	)
	{
		auto queryUser = [](PCWSTR keyName) -> std::optional<DWORD>
		{
			DWORD value{};
			if (SUCCEEDED(wil::reg::get_value_dword_nothrow(GetDwmKey(), keyName, &value)))
			{
				return value;
			}
			return std::nullopt;
		};
		auto queryMachine = [](PCWSTR keyName) -> std::optional<DWORD>
		{
			DWORD value{};
			if (SUCCEEDED(wil::reg::get_value_dword_nothrow(
				HKEY_LOCAL_MACHINE,
				L"Software\\Microsoft\\Windows\\DWM",
				keyName,
				&value
			)))
			{
				return value;
			}
			return std::nullopt;
		};

		const auto userOverride = queryUser(overrideKeyName);
		const auto userBase = queryUser(baseKeyName);
		const auto machineOverride = queryMachine(overrideKeyName);
		const auto machineBase = queryMachine(baseKeyName);
		return ResolveOverridableRegistryValue(
			userOverride,
			userBase,
			machineOverride,
			machineBase,
			defaultValue
		).value;
	}
}
