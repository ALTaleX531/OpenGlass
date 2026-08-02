#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "RegistryValueResolver.hpp"
#include "uDWMProjection.hpp"

namespace OpenGlass::GlassEngine
{
	enum UpdateType : UCHAR
	{
		None = 0,
		Framework = 1 << 0,
		Backdrop = 1 << 1,
		Theme = 1 << 2,
		Hook = 1 << 3,
		All = Backdrop | Framework
	};

	HKEY GetDwmKey();
	HKEY GetPersonalizeKey();

	FORCEINLINE std::optional<DWORD> TryGetDwordFromRegistry(PCWSTR keyName)
	{
		HRESULT hr{ S_OK };
		DWORD value{};
		hr = wil::reg::get_value_dword_nothrow(
			GetDwmKey(),
			keyName,
			&value
		);
		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			hr = wil::reg::get_value_dword_nothrow(
				HKEY_LOCAL_MACHINE,
				L"Software\\Microsoft\\Windows\\DWM",
				keyName,
				&value
			);
		}
		if (FAILED(hr))
		{
			return std::nullopt;
		}

		return value;
	}

	FORCEINLINE DWORD GetDwordFromRegistry(PCWSTR keyName, DWORD defaultValue = 0)
	{
		HRESULT hr{ S_OK };
		DWORD value{ defaultValue };
		hr = wil::reg::get_value_dword_nothrow(
			GetDwmKey(),
			keyName,
			&value
		);
		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			hr = wil::reg::get_value_dword_nothrow(
				HKEY_LOCAL_MACHINE,
				L"Software\\Microsoft\\Windows\\DWM",
				keyName,
				&value
			);
		}

		return value;
	}

	DWORD GetOverridableDwordFromRegistry(
		PCWSTR baseKeyName,
		PCWSTR overrideKeyName,
		DWORD defaultValue = 0
	);

	template <size_t Length>
	FORCEINLINE void GetStringFromRegistry(PCWSTR keyName, WCHAR(&returnValue)[Length])
	{
		HRESULT hr{ S_OK };
		hr = wil::reg::get_value_string_nothrow(
			GetDwmKey(),
			keyName,
			returnValue
		);
		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			hr = wil::reg::get_value_string_nothrow(
				HKEY_LOCAL_MACHINE,
				L"Software\\Microsoft\\Windows\\DWM",
				keyName,
				returnValue
			);
		}
	}

	void LoadRegistry(bool redrawNow = true);
	void UnloadRegistry();

	void Update(UpdateType type, bool redrawNow = true);
	void Startup();
	void Shutdown();
}
