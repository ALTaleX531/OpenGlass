#include "pch.h"
#include "TestHarness.hpp"
#include "DwmSettings.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestDwmSettings()
	{
		GUID id{};
		THROW_IF_FAILED(CoCreateGuid(&id));
		WCHAR guidString[40]{};
		THROW_HR_IF(E_UNEXPECTED, !StringFromGUID2(id, guidString, ARRAYSIZE(guidString)));
		const auto path = std::wstring(L"Software\\OpenGlassTests\\DwmSettings\\") + guidString;
		const auto cleanup = wil::scope_exit([&] { RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()); });
		const auto user = wil::reg::create_unique_key(HKEY_CURRENT_USER, path.c_str(), wil::reg::key_access::readwrite);
		const auto machine = wil::reg::create_unique_key(user.get(), L"Machine", wil::reg::key_access::readwrite);

		// Explicit zero is a user value; an invalid type falls through to HKLM.
		DwmSettings::ValueSource source{};
		wil::reg::set_value_dword(machine.get(), L"Value", 1);
		wil::reg::set_value_dword(user.get(), L"Value", 0);
		Check(DwmSettings::GetDword(user.get(), machine.get(), L"Value", 1, &source) == 0);
		Check(source == DwmSettings::ValueSource::User);
		wil::reg::set_value_string(user.get(), L"Value", L"invalid DWORD");
		Check(DwmSettings::GetDword(user.get(), machine.get(), L"Value", 0, &source) == 1);
		Check(source == DwmSettings::ValueSource::Machine);
		Check(DwmSettings::GetDword(user.get(), nullptr, L"Value", 7, &source) == 7);
		Check(source == DwmSettings::ValueSource::Default);
		Check(DwmSettings::GetDword(nullptr, machine.get(), L"Value") == 1);
		wil::reg::set_value_dword(user.get(), L"Value", 2);
		wil::reg::set_value_dword(machine.get(), L"Override", 3);
		Check(DwmSettings::GetOverridableDword(user.get(), machine.get(), L"Value", L"Override") == 3);
		wil::reg::set_value_dword(user.get(), L"Override", 4);
		Check(DwmSettings::GetOverridableDword(user.get(), machine.get(), L"Value", L"Override") == 4);

		WCHAR value[16]{};
		wil::reg::set_value_string(machine.get(), L"Path", L"machine.png");
		DwmSettings::GetString(user.get(), machine.get(), L"Path", value);
		Check(std::wstring_view(value) == L"machine.png");
		// An empty string masks HKLM, and REG_EXPAND_SZ remains unexpanded.
		wil::reg::set_value_string(user.get(), L"Path", L"");
		DwmSettings::GetString(user.get(), machine.get(), L"Path", value);
		Check(value[0] == L'\0');
		wil::reg::set_value_expanded_string(user.get(), L"Path", L"%TEMP%\\x.png");
		DwmSettings::GetString(user.get(), machine.get(), L"Path", value);
		Check(std::wstring_view(value) == L"%TEMP%\\x.png");
		wil::reg::set_value_dword(user.get(), L"Path", 0);
		DwmSettings::GetString(user.get(), machine.get(), L"Path", value);
		Check(std::wstring_view(value) == L"machine.png");
		// A too-small buffer falls through; failure in both layers clears it.
		wil::reg::set_value_string(user.get(), L"Path", L"a path longer than the buffer");
		DwmSettings::GetString(user.get(), machine.get(), L"Path", value);
		Check(std::wstring_view(value) == L"machine.png");
		DwmSettings::GetString(user.get(), nullptr, L"Path", value);
		Check(value[0] == L'\0');
		DwmSettings::GetString(nullptr, machine.get(), L"Path", value);
		Check(std::wstring_view(value) == L"machine.png");
		DwmSettings::GetString(nullptr, nullptr, L"Path", value);
		Check(value[0] == L'\0');
	}
}
