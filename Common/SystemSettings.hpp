#pragma once

#pragma warning(push)
#pragma warning(disable : 4201 4324 4471)
#include <KNSoft/NDK/NDK.h>
#pragma warning(pop)
#include <wil/result.h>
#include <wil/registry.h>

namespace OpenGlass::Util
{
	inline constexpr PCWSTR g_personalizeKeyPath = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

	inline bool IsEnergySaverEnabled() noexcept
	{
		// Both saver status GUIDs publish the same DWORD for AC and DC.
		struct
		{
			ULONG m_totalLength;
			struct
			{
				ULONG m_changeStamp;
				ULONG m_valueLength;
				DWORD m_value;
			} m_entries[PoConditionMaximum];
		} powerSetting{};
		GET_POWER_SETTING_VALUE_INPUT powerInput{ GUID_POWER_SAVING_STATUS };
		if (
			!NT_SUCCESS_LOG(
				NtPowerInformation(
					GetPowerSettingValue,
					&powerInput,
					sizeof(powerInput),
					&powerSetting,
					sizeof(powerSetting)
				)
			)
		)
		{
			return false;
		}
		if (powerSetting.m_entries[0].m_value != ENERGY_SAVER_OFF)
		{
			return true;
		}

		powerInput.SettingGuid = GUID_ENERGY_SAVER_STATUS;
		const auto status = NtPowerInformation(
			GetPowerSettingValue,
			&powerInput,
			sizeof(powerInput),
			&powerSetting,
			sizeof(powerSetting)
		);
		if (status == STATUS_INVALID_PARAMETER || status == STATUS_NOT_SUPPORTED)
		{
			return false;
		}
		return NT_SUCCESS_LOG(status) && powerSetting.m_entries[0].m_value != ENERGY_SAVER_OFF;
	}

	FORCEINLINE bool WINAPI IsTransparencyEnabled()
	{
		DWORD value{ 0 };
		return SUCCEEDED(wil::reg::get_value_dword_nothrow(HKEY_CURRENT_USER, g_personalizeKeyPath, L"EnableTransparency", &value)) && value != 0;
	}
	FORCEINLINE bool WINAPI IsTransparencyEnabled(HKEY key)
	{
		DWORD value{ 0 };
		return SUCCEEDED(wil::reg::get_value_dword_nothrow(key, L"EnableTransparency", &value)) && value != 0;
	}
}
