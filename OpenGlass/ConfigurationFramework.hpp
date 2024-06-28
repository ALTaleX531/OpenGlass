#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "uDwmProjection.hpp"

namespace OpenGlass::ConfigurationFramework
{
	enum UpdateType : UCHAR
	{
		None = 0,
		Framework = 1 << 0,
		Backdrop = 1 << 1,
		Theme = 1 << 2,
		All = Backdrop | Framework
	};

	HKEY GetDwmKey();
	HKEY GetPersonalizeKey();

	FORCEINLINE std::optional<DWORD> DwmTryDwordFromHKCUAndHKLM(std::wstring_view keyName) try
	{
		std::optional<DWORD> result{ wil::reg::try_get_value_dword(GetDwmKey(), keyName.data()) };
		if (!result.has_value())
		{
			result = wil::reg::try_get_value_dword(
				HKEY_LOCAL_MACHINE,
				L"Software\\Microsoft\\Windows\\DWM",
				keyName.data()
			);
		}

		return result;
	}
	catch (...)
	{
		LOG_CAUGHT_EXCEPTION();
		return std::nullopt;
	}

	FORCEINLINE DWORD DwmGetDwordFromHKCUAndHKLM(std::wstring_view keyName, DWORD defaultValue = 0)
	{
		HRESULT hr{ S_OK };
		DWORD value{ defaultValue };
		hr = wil::reg::get_value_dword_nothrow(
			GetDwmKey(),
			keyName.data(),
			&value
		);
		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			hr = wil::reg::get_value_dword_nothrow(
				HKEY_LOCAL_MACHINE,
				L"Software\\Microsoft\\Windows\\DWM",
				keyName.data(),
				&value
			);
			LOG_IF_FAILED_WITH_EXPECTED(hr, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
		}
		else
		{
			LOG_IF_FAILED(hr);
		}

		return value;
	}
	template <size_t Length>
	FORCEINLINE void DwmGetStringFromHKCUAndHKLM(std::wstring_view keyName, WCHAR(&returnValue)[Length])
	{
		HRESULT hr{ S_OK };
		hr = wil::reg::get_value_string_nothrow(
			GetDwmKey(),
			keyName.data(),
			returnValue
		);
		if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			hr = wil::reg::get_value_string_nothrow(
				HKEY_LOCAL_MACHINE,
				L"Software\\Microsoft\\Windows\\DWM",
				keyName.data(),
				returnValue
			);
			LOG_IF_FAILED_WITH_EXPECTED(hr, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
		}
		else
		{
			LOG_IF_FAILED(hr);
		}
	}

	void Load(bool updateNow = true);
	void Unload();
	void Update(UpdateType type);
}