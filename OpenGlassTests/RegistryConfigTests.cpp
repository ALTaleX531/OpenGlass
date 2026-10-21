#include "pch.h"
#include "TestHarness.hpp"
#include "../OpenGlassGUI/RegistryConfig.hpp"
#include "../OpenGlassGUI/EffectiveConfiguration.hpp"
#include "../OpenGlassGUI/ConfigurationMigration.hpp"
#include "../OpenGlassGUI/PreviewJournal.hpp"
#include "SystemSettings.hpp"
#include "../OpenGlassGUI/ColorizationPresets.hpp"
#include "Util.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestTransparencyRegistry()
	{
		using namespace EffectiveConfiguration;
		using enum Settings::Id;
		const auto path = L"Software\\OpenGlassTests\\" + std::to_wstring(GetCurrentProcessId()) + L"-Transparency";
		auto root = wil::reg::create_unique_key(HKEY_CURRENT_USER, path.c_str(), wil::reg::key_access::readwrite);
		const auto cleanup = wil::scope_exit([&]
		{
			RegDeleteTreeW(root.get(), nullptr);
			root.reset();
			RegDeleteKeyW(HKEY_CURRENT_USER, path.c_str());
		});
		const auto users = wil::reg::create_unique_key(root.get(), L"Users", wil::reg::key_access::readwrite);
		const auto elevated = wil::reg::create_unique_key(root.get(), L"ElevatedUser", wil::reg::key_access::readwrite);
		const auto machine = wil::reg::create_unique_key(root.get(), L"Machine", wil::reg::key_access::readwrite);
		const auto restoreRoots = wil::scope_exit([&]
		{
			RegOverridePredefKey(HKEY_USERS, nullptr);
			RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
			RegOverridePredefKey(HKEY_LOCAL_MACHINE, nullptr);
		});
		THROW_IF_WIN32_ERROR(RegOverridePredefKey(HKEY_USERS, users.get()));
		THROW_IF_WIN32_ERROR(RegOverridePredefKey(HKEY_CURRENT_USER, elevated.get()));
		THROW_IF_WIN32_ERROR(RegOverridePredefKey(HKEY_LOCAL_MACHINE, machine.get()));

		constexpr PCWSTR sid = L"S-1-5-21-100-200-300-1001";
		constexpr PCWSTR personalize = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
		const auto personalKey = wil::reg::create_unique_key(users.get(), (std::wstring(sid) + L"\\" + personalize).c_str(), wil::reg::key_access::readwrite);
		const auto dwmKey = wil::reg::create_unique_key(users.get(), (std::wstring(sid) + L"\\" + DwmSettings::g_keyPath).c_str(), wil::reg::key_access::readwrite);
		wil::reg::set_value_dword(dwmKey.get(), L"EnableTransparency", 99);
		RegistryConfig userConfig(RegistryConfig::Mode::User, sid);
		RegistryConfig machineConfig(RegistryConfig::Mode::Machine, sid);
		THROW_IF_FAILED(machineConfig.SetDword(L"ColorizationOpaqueBlend", 1));
		THROW_IF_FAILED(userConfig.SetDword(L"ColorizationOpaqueBlend", 0));
		for (const auto& before : { RegistryConfig::RawValue{}, RegistryConfig::RawValue{ true, REG_BINARY, { 7, 8 } },
			Encode(Value{ DWORD{0} }), Encode(Value{ DWORD{1} }), Encode(Value{ DWORD{2} }) })
		{
			const auto restored = before.present ? before : Encode(Value{ DWORD{0} });
			const auto SetInitialValue = [&]
			{
				if (before.present) THROW_IF_FAILED(userConfig.WriteRaw(L"EnableTransparency", before));
				else
				{
					// Only the isolated fixture may create an absent starting value.
					const auto error = RegDeleteValueW(personalKey.get(), L"EnableTransparency");
					THROW_HR_IF(HRESULT_FROM_WIN32(error), error != ERROR_SUCCESS && error != ERROR_FILE_NOT_FOUND);
				}
			};
			SetInitialValue();
			Check(userConfig.ReadRaw(L"EnableTransparency") == before);
			Check((userConfig.GetDword(L"EnableTransparency", 0) != 0) == Util::IsTransparencyEnabled(personalKey.get()));
			PreviewJournal<Settings::Id, RegistryConfig::RawValue> preview;
			preview.Begin();
			preview.Touch(Settings::Id::EnableTransparency, [&](auto) { return userConfig.ReadRaw(L"EnableTransparency"); });
			THROW_IF_FAILED(machineConfig.SetDword(L"EnableTransparency", 0));
			Check(wil::reg::get_value_dword(personalKey.get(), L"EnableTransparency") == 0);
			Check(userConfig.GetDword(L"enabletransparency", 1) == 0);
			preview.CommitAttempt();
			Check(preview.Revert([&](auto, const auto& value) { return SUCCEEDED(userConfig.WriteRaw(L"EnableTransparency", value)); }));
			Check(userConfig.ReadRaw(L"EnableTransparency") == restored);

			SetInitialValue();
			const auto userLayer = Read(userConfig), machineLayer = Read(machineConfig);
			const auto legacyUserLayer = Read(userConfig, false);
			Check(Raw(userLayer, EnableTransparency) == before);
			Check(!machineLayer.values.contains(EnableTransparency));
			Check(!legacyUserLayer.values.contains(EnableTransparency));
			const auto captured = Capture(userLayer, machineLayer);
			const auto enabled = Decode(before, Settings::Get(EnableTransparency));
			Check(captured.at(ColorizationOpaqueBlend) == (std::holds_alternative<DWORD>(enabled)
				? Value{ DWORD{std::get<DWORD>(enabled) == 0} } : Value{ DWORD{1} }));
			Check(!captured.contains(EnableTransparency));
			Check(Capture(legacyUserLayer, machineLayer, Settings::CatalogVersion, false).at(ColorizationOpaqueBlend) == Value{ DWORD{0} });
			for (const auto scope : { Settings::Scope::User, Settings::Scope::Machine })
				for (const auto version : { 1u, Settings::CatalogVersion })
					for (const auto& opaque : { Value{}, Value{ DWORD{0} }, Value{ DWORD{1} }, Value{ DWORD{2} } })
					{
						SetInitialValue();
						const Model model{ { ColorizationOpaqueBlend, opaque } };
						const auto after = std::holds_alternative<DWORD>(opaque)
							? Encode(Value{ DWORD{std::get<DWORD>(opaque) == 0} }) : Encode(Value{ DWORD{1} });
						const auto plan = Plan(userLayer, machineLayer, model, scope, version);
						Check(plan.size() == (before != after ? 1u : 0u));
						preview.Begin();
						for (const auto& change : plan)
						{
							Check(change.scope == Settings::Scope::User && change.id == EnableTransparency && change.before == before);
							Check(change.after.present && change.after.type == REG_DWORD);
							preview.Touch(change.id, [&](auto) { return userConfig.ReadRaw(change.Name()); });
							THROW_IF_FAILED(userConfig.WriteRaw(change.Name(), change.after));
						}
						preview.CommitAttempt();
						Check(userConfig.ReadRaw(L"EnableTransparency") == after);
						Check(Plan(Read(userConfig), Read(machineConfig), model, scope, version).empty());
						Check(preview.Revert([&](auto, const auto& value) { return SUCCEEDED(userConfig.WriteRaw(L"EnableTransparency", value)); }));
						Check(userConfig.ReadRaw(L"EnableTransparency") == restored);
						Check(userConfig.GetDword(L"ColorizationOpaqueBlend", 7) == 0);
						Check(machineConfig.GetDword(L"ColorizationOpaqueBlend", 7) == 1);
						for (const auto& change : Plan(legacyUserLayer, machineLayer, model, scope, version, false))
							Check(change.id == ColorizationOpaqueBlend);
					}
		}
		// Both direct deletion and an old absent-value recovery record must retain a DWORD.
		THROW_IF_FAILED(userConfig.SetDword(L"EnableTransparency", 1));
		THROW_IF_FAILED(machineConfig.DeleteValue(L"enabletransparency"));
		Check(userConfig.ReadRaw(L"EnableTransparency") == Encode(Value{ DWORD{0} }));
		THROW_IF_FAILED(userConfig.SetDword(L"EnableTransparency", 1));
		THROW_IF_FAILED(machineConfig.WriteRaw(L"EnableTransparency", {}));
		Check(userConfig.ReadRaw(L"EnableTransparency") == Encode(Value{ DWORD{0} }));
		Check(wil::reg::get_value_dword(dwmKey.get(), L"EnableTransparency") == 99);
		wil::unique_hkey missing;
		Check(wil::reg::open_unique_key_nothrow(elevated.get(), personalize, missing) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
		Check(wil::reg::open_unique_key_nothrow(machine.get(), personalize, missing) == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
	}
}
