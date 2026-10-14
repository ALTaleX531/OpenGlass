#include "pch.h"
#include "RegistryConfig.hpp"
#include "DwmSettings.hpp"

namespace OpenGlass
{
	RegistryConfig::RegistryConfig(Mode mode, std::wstring userSid)
		: m_mode(mode)
		, m_userSid(std::move(userSid))
	{
	}

	RegistryConfig::RawValue RegistryConfig::ReadRaw(const std::wstring& name) const
	{
		const auto [root, path] = GetLocation(name.c_str());
		wil::unique_hkey key;
		auto error = RegOpenKeyExW(root, path.c_str(), 0, KEY_QUERY_VALUE, key.put());
		if (error == ERROR_FILE_NOT_FOUND) return {};
		THROW_IF_WIN32_ERROR(error);
		RawValue value;
		DWORD size{};
		error = RegQueryValueExW(key.get(), name.c_str(), nullptr, &value.type, nullptr, &size);
		if (error == ERROR_FILE_NOT_FOUND) return {};
		THROW_IF_WIN32_ERROR(error);
		do
		{
			// A null buffer would turn a zero-length value's concurrent growth into
			// another successful size query, leaving synthetic bytes in the backup.
			value.bytes.resize(std::max<DWORD>(size, 1));
			error = RegQueryValueExW(key.get(), name.c_str(), nullptr, &value.type, value.bytes.data(), &size);
		} while (error == ERROR_MORE_DATA);
		if (error == ERROR_FILE_NOT_FOUND) return {};
		THROW_IF_WIN32_ERROR(error);
		value.bytes.resize(size);
		value.present = true;
		return value;
	}

	HRESULT RegistryConfig::WriteRaw(const std::wstring& name, const RawValue& value)
	{
		if (!value.present) return DeleteValue(name);
		auto key = OpenKey(false, name.c_str());
		RETURN_HR_IF(E_ACCESSDENIED, !key);
		return HRESULT_FROM_WIN32(RegSetValueExW(key.get(), name.c_str(), 0, value.type,
			value.bytes.data(), static_cast<DWORD>(value.bytes.size())));
	}

	HRESULT RegistryConfig::CheckDeleteAccess() const
	{
		const auto [root, path] = GetLocation();
		wil::unique_hkey key;
		const auto status = RegOpenKeyExW(root, path.c_str(), 0, KEY_QUERY_VALUE | KEY_SET_VALUE, key.put());
		return status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND ? S_OK : HRESULT_FROM_WIN32(status);
	}

	std::pair<HKEY, std::wstring> RegistryConfig::GetLocation(PCWSTR valueName) const
	{
		const bool transparency = valueName && !_wcsicmp(valueName, L"EnableTransparency");
		const auto subKey = transparency
			? L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"
			: DwmSettings::g_keyPath;
		if (m_mode == Mode::Machine && !transparency)
		{
			return { HKEY_LOCAL_MACHINE, subKey };
		}
		if (m_userSid.empty())
		{
			return { HKEY_CURRENT_USER, subKey };
		}
		return { HKEY_USERS, m_userSid + L"\\" + subKey };
	}

	wil::unique_hkey RegistryConfig::OpenKey(bool readOnly, PCWSTR valueName) const
	{
		const auto [root, subKey] = GetLocation(valueName);
		wil::unique_hkey key;
		if (readOnly)
		{
			(void)wil::reg::open_unique_key_nothrow(root, subKey.c_str(), key, wil::reg::key_access::read);
		}
		else
		{
			(void)wil::reg::create_unique_key_nothrow(root, subKey.c_str(), key, wil::reg::key_access::readwrite);
		}
		return key;
	}

	DWORD RegistryConfig::GetDword(const std::wstring& name, DWORD defaultValue) const
	{
		DWORD value{};
		if (TryGetDword(name, value)) return value;
		return defaultValue;
	}

	HRESULT RegistryConfig::SetDword(const std::wstring& valueName, DWORD value)
	{
		auto key = OpenKey(false, valueName.c_str());
		RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), !key);

		return wil::reg::set_value_dword_nothrow(key.get(), valueName.c_str(), value);
	}

	bool RegistryConfig::TryGetDword(const std::wstring& valueName, DWORD& value) const
	{
		auto key = OpenKey(true, valueName.c_str());
		if (!key) return false;

		return SUCCEEDED(wil::reg::get_value_dword_nothrow(key.get(), valueName.c_str(), &value));
	}

	std::wstring RegistryConfig::GetString(const std::wstring& name, const std::wstring& defaultValue) const
	{
		std::wstring value;
		if (TryGetString(name, value)) return value;
		return defaultValue;
	}

	HRESULT RegistryConfig::SetString(const std::wstring& valueName, const std::wstring& value)
	{
		auto key = OpenKey(false, valueName.c_str());
		RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED), !key);

		return wil::reg::set_value_string_nothrow(key.get(), valueName.c_str(), value.c_str());
	}

	bool RegistryConfig::TryGetString(const std::wstring& valueName, std::wstring& value) const
	{
		auto key = OpenKey(true, valueName.c_str());
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
		// Recovery of an absent Windows preference keeps its disabled effect without deleting it.
		if (!_wcsicmp(valueName.c_str(), L"EnableTransparency")) return SetDword(valueName, 0);
		const auto [root, subKey] = GetLocation(valueName.c_str());
		wil::unique_hkey key;
		const auto openResult = wil::reg::open_unique_key_nothrow(root, subKey.c_str(), key, wil::reg::key_access::readwrite);
		if (openResult == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) return S_OK;
		RETURN_IF_FAILED(openResult);
		const auto error = RegDeleteValueW(key.get(), valueName.c_str());
		return error == ERROR_FILE_NOT_FOUND ? S_OK : HRESULT_FROM_WIN32(error);
	}
}
