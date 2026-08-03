#include "pch.h"
#include "RegistryConfig.hpp"

namespace OpenGlass
{
	namespace
	{
		constexpr auto DwmSubKey = L"SOFTWARE\\Microsoft\\Windows\\DWM";
	}

	RegistryConfig::RegistryConfig(Mode mode, std::wstring userSid)
		: m_mode(mode)
		, m_userSid(std::move(userSid))
	{
	}

	Settings::Scope RegistryConfig::ScopeFor(const std::wstring& valueName) const noexcept
	{
		if (m_mode == Mode::User)
		{
			return Settings::Scope::User;
		}
		if (m_mode == Mode::Machine)
		{
			return Settings::Scope::Machine;
		}
		if (const auto spec = Settings::Find(valueName))
		{
			return spec->scope;
		}
		return Settings::Scope::Machine;
	}

	std::pair<HKEY, std::wstring> RegistryConfig::GetLocation(const std::wstring& valueName) const
	{
		if (ScopeFor(valueName) == Settings::Scope::Machine)
		{
			return { HKEY_LOCAL_MACHINE, DwmSubKey };
		}
		if (m_userSid.empty())
		{
			return { HKEY_CURRENT_USER, DwmSubKey };
		}
		return { HKEY_USERS, m_userSid + L"\\" + DwmSubKey };
	}

	wil::unique_hkey RegistryConfig::OpenKey(const std::wstring& valueName, bool readOnly) const
	{
		const auto [root, subKey] = GetLocation(valueName);
		wil::unique_hkey key;
		
		wil::reg::key_access access = wil::reg::key_access::read;
		if (!readOnly)
		{
			access = wil::reg::key_access::readwrite;
		}

		// Try to open existing key
		if (FAILED(wil::reg::open_unique_key_nothrow(root, subKey.c_str(), key, access)))
		{
			// If writing, try to create it
			if (!readOnly)
			{
				if (FAILED(wil::reg::create_unique_key_nothrow(root, subKey.c_str(), key, access)))
				{
					// Fallback
				}
			}
		}

		return key;
	}

	DWORD RegistryConfig::GetDword(const std::wstring& valueName, DWORD defaultValue) const
	{
		auto key = OpenKey(valueName, true);
		if (!key) return defaultValue;

		DWORD value = 0;
		if (SUCCEEDED(wil::reg::get_value_dword_nothrow(key.get(), valueName.c_str(), &value)))
		{
			return value;
		}
		return defaultValue;
	}

	HRESULT RegistryConfig::SetDword(const std::wstring& valueName, DWORD value)
	{
		auto key = OpenKey(valueName, false);
		RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), !key);

		return wil::reg::set_value_dword_nothrow(key.get(), valueName.c_str(), value);
	}

	bool RegistryConfig::TryGetDword(const std::wstring& valueName, DWORD& value) const
	{
		auto key = OpenKey(valueName, true);
		if (!key) return false;

		return SUCCEEDED(wil::reg::get_value_dword_nothrow(key.get(), valueName.c_str(), &value));
	}

	std::wstring RegistryConfig::GetString(const std::wstring& valueName, const std::wstring& defaultValue) const
	{
		auto key = OpenKey(valueName, true);
		if (!key) return defaultValue;

		wil::unique_cotaskmem_string result;
		if (SUCCEEDED(wil::reg::get_value_string_nothrow(key.get(), valueName.c_str(), result)))
		{
			return std::wstring(result.get());
		}
		return defaultValue;
	}

	HRESULT RegistryConfig::SetString(const std::wstring& valueName, const std::wstring& value)
	{
		auto key = OpenKey(valueName, false);
		RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), !key);

		return wil::reg::set_value_string_nothrow(key.get(), valueName.c_str(), value.c_str());
	}

	bool RegistryConfig::TryGetString(const std::wstring& valueName, std::wstring& value) const
	{
		auto key = OpenKey(valueName, true);
		if (!key) return false;

		wil::unique_cotaskmem_string result;
		if (SUCCEEDED(wil::reg::get_value_string_nothrow(key.get(), valueName.c_str(), result)))
		{
			value = result.get();
			return true;
		}
		return false;
	}

	HRESULT RegistryConfig::DeleteValue(const std::wstring& valueName)
	{
		const auto [root, subKey] = GetLocation(valueName);
		wil::unique_hkey key;
		const auto openResult = wil::reg::open_unique_key_nothrow(root, subKey.c_str(), key, wil::reg::key_access::readwrite);
		if (openResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) return S_OK;
		RETURN_IF_FAILED(openResult);
		const auto error = RegDeleteValueW(key.get(), valueName.c_str());
		return error == ERROR_FILE_NOT_FOUND ? S_OK : HRESULT_FROM_WIN32(error);
	}

	bool RegistryConfig::HasValue(const std::wstring& valueName) const
	{
		auto key = OpenKey(valueName, true);
		if (!key) return false;
		return RegQueryValueExW(key.get(), valueName.c_str(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
	}

	bool RegistryConfig::HasKey() const
	{
		auto key = OpenKey(L"", true);
		return key != nullptr;
	}
}
