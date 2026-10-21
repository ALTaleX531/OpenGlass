#include "pch.h"
#include "TestHarness.hpp"
#include "../OpenGlassGUI/ConfigurationMigration.hpp"
#include "../OpenGlassGUI/EffectiveConfiguration.hpp"
#include "../OpenGlassGUI/PreviewJournal.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestConfigurationMergePolicy()
	{
		using enum Settings::Id;
		using Raw = RegistryConfig::RawValue;
		using Values = ConfigurationMigration::Values;
		auto raw = [](DWORD value) { return EffectiveConfiguration::Encode(EffectiveConfiguration::Value{ value }); };
		const Values user{ { GlassOpacity, raw(40) }, { ColorizationColorBalance, raw(33) },
			{ ColorizationAfterglowBalance, raw(12) }, { ColorizationBlurBalance, raw(55) },
			{ ColorizationColor, raw(0x8074B8FC) }, { ColorizationAfterglow, raw(0x80ABCDEF) } };
		const Values machine{ { GlassOpacity, raw(60) }, { ColorizationColorBalanceOverride, raw(70) },
			{ CustomThemeReflection, EffectiveConfiguration::Encode(EffectiveConfiguration::Value{ std::wstring(L"machine.png") }) },
			{ ColorizationColor, raw(0xFF123456) }, { ColorizationAfterglow, raw(0xFF123456) } };
		auto apply = [](Values& users, Values& machines, const auto& plan)
		{
			for (const auto& change : plan)
			{
				auto& layer = change.scope == Settings::Scope::User ? users : machines;
				if (change.after.present) layer[change.id] = change.after;
				else layer.erase(change.id);
			}
		};
		for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
		{
			auto withUserOverrides = user, withMachineOverrides = machine;
			withUserOverrides[EnableTransparency] = raw(0);
			withMachineOverrides[EnableTransparency] = raw(1);
			withUserOverrides[ColorizationColorOverride] = raw(0xAA123456);
			withMachineOverrides[ColorizationAfterglowOverride] = raw(0xBB112233);
			const auto ignored = ConfigurationMigration::Prepare(withUserOverrides, withMachineOverrides, target);
			for (const auto& change : ignored) Check(!Settings::IsColorOverride(change.id) && change.id != EnableTransparency);
			apply(withUserOverrides, withMachineOverrides, ignored);
			Check(withUserOverrides.at(ColorizationColorOverride) == raw(0xAA123456));
			Check(withMachineOverrides.at(ColorizationAfterglowOverride) == raw(0xBB112233));
			Check(!withMachineOverrides.contains(ColorizationColorOverride) && !withUserOverrides.contains(ColorizationAfterglowOverride));
			const auto plan = ConfigurationMigration::Prepare(user, machine, target);
			bool deleting{};
			for (const auto& change : plan)
			{
				Check(!Settings::IsWindowsColorBase(change.id) && !Settings::IsColorOverride(change.id));
				if (target == Settings::Scope::User) Check(change.scope == target && change.after.present);
				else
				{
					if (change.scope == Settings::Scope::User) { deleting = true; Check(!change.after.present); }
					else Check(!deleting); // Complete destination writes before deleting sources.
					Check(change.id != ColorizationColorBalanceOverride); // Machine Override already wins.
				}
			}
			auto users = user, machines = machine;
			apply(users, machines, plan);
			const auto& destination = target == Settings::Scope::User ? users : machines;
			Check(destination.at(GlassOpacity) == raw(40));
			Check(destination.at(ColorizationColorBalanceOverride) == raw(70));
			Check(destination.at(ColorizationAfterglowBalanceOverride) == raw(12));
			Check(destination.at(ColorizationBlurBalanceOverride) == raw(55));
			Check(!destination.contains(ColorizationAfterglowOverride));
			Check(destination.at(CustomThemeReflection) == machine.at(CustomThemeReflection));
			if (target == Settings::Scope::User) Check(machines == machine);
			else Check(!users.contains(GlassOpacity));
			for (const auto& spec : Settings::Catalog) if (Settings::IsWindowsColorBase(spec.id))
			{
				if (user.contains(spec.id)) Check(users.at(spec.id) == user.at(spec.id));
				else Check(!users.contains(spec.id));
				if (machine.contains(spec.id)) Check(machines.at(spec.id) == machine.at(spec.id));
				else Check(!machines.contains(spec.id));
			}
			Check(ConfigurationMigration::Prepare(users, machines, target).empty());
			// Every failed step restores both the destination and any deleted source.
			for (std::size_t failAt = 0; failAt < plan.size(); ++failAt)
			{
				using Key = std::pair<Settings::Scope, Settings::Id>;
				std::map<Key, Raw> state;
				for (const auto& [id, value] : user) state[{ Settings::Scope::User, id }] = value;
				for (const auto& [id, value] : machine) state[{ Settings::Scope::Machine, id }] = value;
				const auto before = state;
				PreviewJournal<Key, Raw> journal; journal.Begin();
				for (std::size_t n = 0; n <= failAt; ++n)
				{
					const auto& change = plan[n]; const Key key{ change.scope, change.id };
					journal.Touch(key, [&](const Key& k) { const auto it = state.find(k); return it == state.end() ? Raw{} : it->second; });
					if (change.after.present) state[key] = change.after; else state.erase(key);
				}
				Check(journal.RollbackAttempt([&](const Key& key, const Raw& value) { if (value.present) state[key] = value; else state.erase(key); return true; }));
				Check(state == before);
			}
			users = user;
			users[GlassOpacity] = { true, REG_BINARY, { 0, 1 } };
			const auto malformed = ConfigurationMigration::Prepare(users, machine, target);
			if (target == Settings::Scope::Machine)
				Check(std::ranges::none_of(malformed, [](const auto& change) { return change.id == GlassOpacity; }));
			else
			{
				const auto replacement = std::ranges::find(malformed, GlassOpacity, &ConfigurationMigration::Change::id);
				Check(replacement != malformed.end() && replacement->before == users.at(GlassOpacity) && replacement->after == raw(60));
			}
		}
		// Odd byte counts cannot represent a complete UTF-16 sequence.
		for (const auto badPath : { Raw{ true, REG_SZ, { 'x' } }, Raw{ true, REG_EXPAND_SZ, { 'x', 0, 'y' } } })
		{
			const Values malformedUser{ { CustomThemeReflection, badPath } };
			Check(std::holds_alternative<std::monostate>(EffectiveConfiguration::Decode(badPath, Settings::Get(CustomThemeReflection))));
			const auto toMachine = ConfigurationMigration::Prepare(malformedUser, machine, Settings::Scope::Machine);
			Check(std::ranges::none_of(toMachine, [](const auto& change) { return change.id == CustomThemeReflection; }));
			const auto toUser = ConfigurationMigration::Prepare(malformedUser, machine, Settings::Scope::User);
			const auto replacement = std::ranges::find(toUser, CustomThemeReflection, &ConfigurationMigration::Change::id);
			Check(replacement != toUser.end() && replacement->before == badPath && replacement->after == machine.at(CustomThemeReflection));
		}
		// Compatibility merge preserves all four lookup positions without changing base values.
		for (unsigned uo = 0; uo < 3; ++uo) for (unsigned mo = 0; mo < 3; ++mo)
		for (unsigned ub = 0; ub < 3; ++ub) for (unsigned mb = 0; mb < 3; ++mb)
		{
			auto value = [&](unsigned kind, DWORD number) { return kind == 0 ? Raw{} : kind == 1 ? raw(number) : Raw{ true, REG_BINARY, { 1, 2 } }; };
			const Values users{ { ColorizationColorBalanceOverride, value(uo, 10) }, { ColorizationColorBalance, value(ub, 30) } };
			const Values machines{ { ColorizationColorBalanceOverride, value(mo, 20) }, { ColorizationColorBalance, value(mb, 40) } };
			const auto expected = uo == 1 ? raw(10) : mo == 1 ? raw(20) : ub == 1 ? raw(30) : mb == 1 ? raw(40) : Raw{};
			for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
			{
				auto mergedUsers = users, mergedMachines = machines;
				const auto plan = ConfigurationMigration::Prepare(users, machines, target);
				apply(mergedUsers, mergedMachines, plan);
				if (expected.present) Check((target == Settings::Scope::User ? mergedUsers : mergedMachines).at(ColorizationColorBalanceOverride) == expected);
				else Check(plan.empty());
				Check(mergedUsers.at(ColorizationColorBalance) == users.at(ColorizationColorBalance));
				Check(mergedMachines.at(ColorizationColorBalance) == machines.at(ColorizationColorBalance));
				if (target == Settings::Scope::User) Check(mergedMachines == machines);
			}
		}
	}
}
