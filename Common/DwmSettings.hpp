#pragma once

#include <Windows.h>
#include <wil/result.h>
#include <wil/registry.h>
#include "RegistryValueResolver.hpp"

namespace OpenGlass::DwmSettings
{
	inline constexpr PCWSTR g_keyPath = L"SOFTWARE\\Microsoft\\Windows\\DWM";

	enum UpdateType : UCHAR
	{
		None = 0,
		Framework = 1 << 0,
		Backdrop = 1 << 1,
		Theme = 1 << 2,
		RestartRequired = 1 << 3,
		// Registry reload; theme resources are refreshed separately.
		All = Backdrop | Framework
	};

	constexpr UpdateType operator|(UpdateType left, UpdateType right) noexcept
	{
		return static_cast<UpdateType>(static_cast<unsigned>(left) | static_cast<unsigned>(right));
	}

	inline void NotifyChange(HWND window, UpdateType type) noexcept
	{
		// Notification delivery is best-effort, including without OpenGlass loaded.
		if (!window) return;
		if (type & UpdateType::Backdrop)
		{
			// WM_SETTINGCHANGE carries a string, so SendNotifyMessageW rejects it
			// across processes. Use a bounded synchronous send.
			SetLastError(ERROR_SUCCESS);
			LOG_IF_WIN32_BOOL_FALSE(static_cast<BOOL>(SendMessageTimeoutW(
				window, WM_SETTINGCHANGE, 0,
				reinterpret_cast<LPARAM>(L"ImmersiveColorSet"),
				SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 1000, nullptr
			) != 0));
		}
		if (type & UpdateType::Theme)
		{
			LOG_IF_WIN32_BOOL_FALSE(SendNotifyMessageW(window, WM_THEMECHANGED, 0, 0));
		}
	}

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
