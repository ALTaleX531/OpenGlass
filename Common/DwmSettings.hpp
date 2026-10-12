#pragma once

#include <Windows.h>
#include <wil/result.h>
#include <wil/registry.h>
#include "RegistryValueResolver.hpp"

namespace OpenGlass::DwmSettings
{
	inline constexpr PCWSTR g_keyPath = L"SOFTWARE\\Microsoft\\Windows\\DWM";

	enum class ValueSource
	{
		Default,
		User,
		Machine
	};

	// Callers supply the target user's DWM key; either handle may be null.
	inline DWORD GetDword(
		HKEY userKey,
		HKEY machineKey,
		PCWSTR name,
		DWORD defaultValue = 0,
		ValueSource* source = nullptr
	) noexcept
	{
		DWORD value{};
		if (userKey && SUCCEEDED(wil::reg::get_value_dword_nothrow(userKey, name, &value)))
		{
			if (source) *source = ValueSource::User;
			return value;
		}
		if (machineKey && SUCCEEDED(wil::reg::get_value_dword_nothrow(machineKey, name, &value)))
		{
			if (source) *source = ValueSource::Machine;
			return value;
		}
		if (source) *source = ValueSource::Default;
		return defaultValue;
	}

	inline DWORD GetOverridableDword(
		HKEY userKey,
		HKEY machineKey,
		PCWSTR name,
		PCWSTR overrideName,
		DWORD defaultValue = 0
	) noexcept
	{
		const auto Read = [](HKEY key, PCWSTR valueName) -> std::optional<DWORD>
		{
			DWORD value{};
			if (key && SUCCEEDED(wil::reg::get_value_dword_nothrow(key, valueName, &value))) return value;
			return std::nullopt;
		};
		// HKCU Override -> HKLM Override -> HKCU base -> HKLM base -> default.
		return ResolveOverridableRegistryValue(
			Read(userKey, overrideName),
			Read(userKey, name),
			Read(machineKey, overrideName),
			Read(machineKey, name),
			defaultValue
		);
	}

	template <size_t Length>
	inline void GetString(HKEY userKey, HKEY machineKey, PCWSTR name, WCHAR(&value)[Length]) noexcept
	{
		if (userKey && SUCCEEDED(wil::reg::get_value_string_nothrow(userKey, name, value))) return;
		if (machineKey && SUCCEEDED(wil::reg::get_value_string_nothrow(machineKey, name, value))) return;
		value[0] = L'\0';
	}

	constexpr bool IsTransparencyDisabled(
		bool opaqueBlend,
		bool transparencyEnabled,
		bool energySaverActive,
		bool disableOnBattery
	) noexcept
	{
		return opaqueBlend || (energySaverActive && disableOnBattery) || !transparencyEnabled;
	}
}
