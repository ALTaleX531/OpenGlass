#include "TestHarness.hpp"
#include "../OpenGlassThemeExtension/Colorization.hpp"
#include <objbase.h>
#include <cstdio>
#include <string>
#include <wil/resource.h>
#include <wil/result.h>
#include <wil/registry.h>

using namespace OpenGlass::Colorization;

namespace OpenGlassTests
{
	void TestThemeColorization()
	{
		// The incoming modern balances and EnableWindowColorization must not
		// decide the Win7 calculation. Keep independent colors and trailing flags.
		ColorizationParameters input{ 0x6B74B8FC, 0x80445566, 42, 10, 48, 1, 2, 2 };
		auto sky = CalculateColorization(input, false);
		Check(sky.color == input.color && sky.afterglow == input.afterglow);
		Check(sky.colorBalance == 8 && sky.afterglowBalance == 43 && sky.blurBalance == 49);
		Check(sky.enableWindowColorization == 1 && sky.glassAttribute == 2 && sky.flags == 2);
		const auto opaque = CalculateColorization(input, true);
		Check(opaque.colorBalance == 42 && opaque.afterglowBalance == 10 && opaque.blurBalance == 48);
		for (const DWORD alpha : { 0u, 25u, 101u, 102u, 189u, 190u, 217u, 255u })
		{
			for (const bool isOpaque : { false, true })
			{
				input.color = (alpha << 24) | 0x00123456;
				const auto parameters = CalculateColorization(input, isOpaque);
				Check(parameters.color == input.color && parameters.afterglow == input.afterglow);
				Check(parameters.colorBalance <= 100 && parameters.afterglowBalance <= 100 && parameters.blurBalance <= 100);
				Check(parameters.colorBalance + parameters.afterglowBalance + parameters.blurBalance == 100);
			}
		}
		GUID id{};
		THROW_IF_FAILED(CoCreateGuid(&id));
		wchar_t guidString[40]{};
		Check(StringFromGUID2(id, guidString, ARRAYSIZE(guidString)) != 0);
		const auto path = std::wstring(L"Software\\OpenGlassTests\\ThemeColorization\\") + guidString;
		const auto cleanup = wil::scope_exit([&] { RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str()); });
		const auto key = wil::reg::create_unique_key(HKEY_CURRENT_USER, path.c_str(), wil::reg::key_access::readwrite);
		const auto machine = wil::reg::create_unique_key(key.get(), L"Machine", wil::reg::key_access::readwrite);
		const auto accent = wil::reg::create_unique_key(key.get(), L"Accent", wil::reg::key_access::readwrite);
		const PCWSTR untouched[]
		{
			L"ColorizationColorBalance", L"ColorizationAfterglowBalance", L"ColorizationBlurBalance",
			L"EnableWindowColorization", L"ColorizationGlassAttribute"
		};
		for (const auto name : untouched) wil::reg::set_value_dword(key.get(), name, 73);
		wil::reg::set_value_dword(key.get(), L"ColorizationOpaqueBlend", 1);
		wil::reg::set_value_dword(key.get(), L"ColorizationColorOverride", 0x11223344);
		wil::reg::set_value_dword(key.get(), L"ColorizationColorBalanceOverride", 99);
		wil::reg::set_value_dword(accent.get(), L"AccentColorMenu", 0xFF332211);
		WriteColorization(key.get(), nullptr, sky);
		Check(wil::reg::get_value_dword(key.get(), L"ColorizationColor") == 0x6B74B8FC);
		Check(wil::reg::get_value_dword(key.get(), L"ColorizationAfterglow") == 0x80445566);
		Check(wil::reg::get_value_dword(key.get(), L"AccentColor") == 0xFFFCB874);
		Check(wil::reg::get_value_dword(accent.get(), L"AccentColorMenu") == 0xFF332211);
		WriteColorization(key.get(), accent.get(), sky);
		Check(wil::reg::get_value_dword(accent.get(), L"AccentColorMenu") == 0xFFFCB874);
		Check(wil::reg::get_value_dword(key.get(), L"ColorizationColorBalanceOverride") == 8);
		Check(wil::reg::get_value_dword(key.get(), L"ColorizationAfterglowBalanceOverride") == 43);
		Check(wil::reg::get_value_dword(key.get(), L"ColorizationBlurBalanceOverride") == 49);
		Check(wil::reg::get_value_dword(key.get(), L"GlassOpacity") == 42);
		for (const auto name : untouched) Check(wil::reg::get_value_dword(key.get(), name) == 73);
		Check(wil::reg::get_value_dword(key.get(), L"ColorizationOpaqueBlend") == 1);
		Check(wil::reg::get_value_dword(key.get(), L"ColorizationColorOverride") == 0x11223344);
		auto read = ReadColorization(key.get(), machine.get());
		Check(read.color == sky.color && read.afterglow == sky.afterglow);
		Check(read.colorBalance == 8 && read.afterglowBalance == 43 && read.blurBalance == 49);
		Check(read.enableWindowColorization == 73 && read.glassAttribute == 73 && read.flags == 0);
		// Machine Override must precede user base; user Override precedes both.
		wil::reg::set_value_dword(machine.get(), L"ColorizationColorBalanceOverride", 22);
		wil::reg::set_value_dword(machine.get(), L"ColorizationColorBalance", 33);
		Check(ReadColorization(key.get(), machine.get()).colorBalance == 8);
		wil::reg::set_value_string(key.get(), L"ColorizationColorBalanceOverride", L"invalid DWORD");
		Check(ReadColorization(key.get(), machine.get()).colorBalance == 22);
		THROW_IF_WIN32_ERROR(RegDeleteValueW(key.get(), L"ColorizationColorBalanceOverride"));
		Check(ReadColorization(key.get(), machine.get()).colorBalance == 22);
		THROW_IF_WIN32_ERROR(RegDeleteValueW(machine.get(), L"ColorizationColorBalanceOverride"));
		Check(ReadColorization(key.get(), machine.get()).colorBalance == 73);
		THROW_IF_WIN32_ERROR(RegDeleteValueW(key.get(), L"ColorizationColorBalance"));
		Check(ReadColorization(key.get(), machine.get()).colorBalance == 33);
		Check(ReadColorization(key.get(), nullptr).colorBalance == 0);
		// GUI opacity changes are independent of Windows ARGB alpha.
		wil::reg::set_value_dword(machine.get(), L"GlassOpacity", 17);
		Check(ReadColorization(key.get(), machine.get()).color == sky.color);
		THROW_IF_WIN32_ERROR(RegDeleteValueW(key.get(), L"GlassOpacity"));
		Check(ReadColorization(key.get(), machine.get()).color == 0x2B74B8FC);
		Check(ReadColorization(key.get(), nullptr).color == 0xA174B8FC);
		wil::reg::set_value_dword(key.get(), L"GlassOpacity", 85);
		Check(ReadColorization(key.get(), machine.get()).color == 0xD974B8FC);
		wil::reg::set_value_dword(key.get(), L"GlassOpacity", 150);
		read = ReadColorization(key.get(), machine.get());
		Check(read.color == 0xFF74B8FC && read.colorBalance == 130);
		WriteColorization(key.get(), nullptr, sky);
		// No preview journal: native Cancel can resubmit its previously read ARGB.
		const auto before = ReadColorization(key.get(), machine.get());
		WriteColorization(key.get(), nullptr, CalculateColorization(input, false));
		Check(wil::reg::get_value_dword(key.get(), L"ColorizationColor") == 0xFF123456);
		Check(wil::reg::get_value_dword(key.get(), L"AccentColor") == 0xFF563412);
		Check(wil::reg::get_value_dword(accent.get(), L"AccentColorMenu") == 0xFFFCB874);
		WriteColorization(key.get(), nullptr, CalculateColorization(before, false));
		Check(wil::reg::get_value_dword(key.get(), L"ColorizationColor") == 0x6B74B8FC);
		Check(wil::reg::get_value_dword(key.get(), L"AccentColor") == 0xFFFCB874);
		Check(wil::reg::get_value_dword(key.get(), L"GlassOpacity") == 42);
		Check(wil::reg::get_value_dword(key.get(), L"ColorizationColorBalanceOverride") == 8);
		for (const auto name : untouched) RegDeleteValueW(key.get(), name);
		// Include the native FF-alpha decoding, not just a raw struct round-trip.
		for (const DWORD alpha : { 0u, 25u, 101u, 102u, 189u, 190u, 217u, 255u })
		{
			input.color = (alpha << 24) | 0x00123456;
			const auto parameters = CalculateColorization(input, false);
			WriteColorization(key.get(), nullptr, parameters);
			read = ReadColorization(key.get(), nullptr);
			auto decoded = read.color;
			if ((decoded >> 24) == 255)
			{
				const auto decodedAlpha = static_cast<DWORD>(((static_cast<int>(read.colorBalance) - 10) * .75 / 100. + .1) * 255. + .5);
				decoded = (decoded & 0x00FFFFFF) | (decodedAlpha << 24);
			}
			Check(decoded == input.color && read.afterglow == input.afterglow);
			Check(wil::reg::get_value_dword(key.get(), L"ColorizationColorBalanceOverride") == parameters.colorBalance);
		}
		for (const auto name : untouched)
		{
			DWORD value{};
			Check(wil::reg::get_value_dword_nothrow(key.get(), name, &value) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
		}
		const auto readOnly = wil::reg::open_unique_key(HKEY_CURRENT_USER, path.c_str(), wil::reg::key_access::read);
		Check(ReadColorization(readOnly.get(), nullptr).color == input.color);
		bool failed{};
		try { WriteColorization(readOnly.get(), nullptr, sky); }
		catch (...) { failed = wil::ResultFromCaughtException() == E_ACCESSDENIED; }
		Check(failed);
	}
}
