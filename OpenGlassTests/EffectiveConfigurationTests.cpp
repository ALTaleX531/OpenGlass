#include "pch.h"
#include "TestHarness.hpp"
#include "../OpenGlassGUI/EffectiveConfiguration.hpp"
#include "../OpenGlassGUI/ConfigurationMigration.hpp"
#include "../OpenGlassGUI/PreviewJournal.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestEffectiveConfiguration()
	{
		using namespace EffectiveConfiguration;
		using enum Settings::Id;
		{
			Layer user, machine;
			machine.values[EnableUxMitigation] = Encode(Value{ DWORD{0x80000001} });
			Check(Capture(user, machine).at(EnableUxMitigation) == Value{ DWORD{0x80000001} });
			Check(!Capture(user, machine, 2).contains(EnableUxMitigation));
			Check(Plan(user, machine, { { EnableUxMitigation, DWORD{0} } }, Settings::Scope::User, 2).empty());
			const auto plan = Plan(user, machine, { { EnableUxMitigation, DWORD{0} } }, Settings::Scope::User);
			Check(plan.size() == 1 && plan.front().id == EnableUxMitigation && plan.front().after == Encode(Value{ DWORD{0} }));
		}
		// The catalog is the sole classification used by cleanup; unrelated values survive.
		Layer populated;
		unsigned colorOverrides{};
		for (const auto& spec : Settings::Catalog)
		{
			populated.values[spec.id] = { true, REG_BINARY, { 1 } };
			if (Settings::IsColorOverride(spec.id))
			{
				Check(spec.id == ColorizationColorOverride || spec.id == ColorizationAfterglowOverride);
				Check(!Settings::IsPresetPackSetting(spec));
				++colorOverrides;
			}
		}
		const auto colorCleanup = PlanColorCleanup(populated, populated);
		Check(colorOverrides == 2 && colorCleanup.size() == 2 * colorOverrides);
		std::set<std::pair<Settings::Scope, Settings::Id>> removed;
		for (const auto& change : colorCleanup)
		{
			Check(Settings::IsColorOverride(change.id) && change.before == populated.values.at(change.id) && !change.after.present);
			Check(removed.emplace(change.scope, change.id).second);
		}
		for (unsigned u = 0; u < 3; ++u) for (unsigned m = 0; m < 3; ++m)
		{
			Layer user, machine;
			auto raw = [](unsigned kind, DWORD number) { return kind == 0 ? RegistryConfig::RawValue{} : kind == 1 ? Encode(Value{ number }) : RegistryConfig::RawValue{ true, REG_BINARY, { 1, 2 } }; };
			user.values[GlassOpacity] = raw(u, 17); machine.values[GlassOpacity] = raw(m, 31);
			const Value expected = u == 1 ? Value{ DWORD{17} } : m == 1 ? Value{ DWORD{31} } : Value{};
			Check(Resolve(user, machine, GlassOpacity) == expected);
			Check(Capture(user, machine).at(GlassOpacity) == expected);
			for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
			for (const auto& desired : { Value{}, Value{ DWORD{17} }, Value{ DWORD{0xFFFFFFFF} } })
			{
				auto newUser = user, newMachine = machine;
				const auto plan = Plan(user, machine, { { GlassOpacity, desired } }, target);
				for (const auto& change : plan)
				{
					auto& layer = change.scope == Settings::Scope::User ? newUser : newMachine;
					layer.values[change.id] = change.after;
				}
				if (std::holds_alternative<std::monostate>(desired))
				{
					const auto& other = target == Settings::Scope::User ? machine : user;
					Check(Resolve(newUser, newMachine, GlassOpacity) == Decode(Raw(other, GlassOpacity), Settings::Get(GlassOpacity)));
					Check(!Raw(target == Settings::Scope::User ? newUser : newMachine, GlassOpacity).present);
					if (target == Settings::Scope::Machine) Check(newUser.values == user.values);
				}
				else Check(Resolve(newUser, newMachine, GlassOpacity) == desired);
				if (target == Settings::Scope::User) Check(newMachine.values == machine.values);
			}
		}
		for (const auto [base, over] : { std::pair{ ColorizationColorBalance, ColorizationColorBalanceOverride },
			std::pair{ ColorizationAfterglowBalance, ColorizationAfterglowBalanceOverride },
			std::pair{ ColorizationBlurBalance, ColorizationBlurBalanceOverride } })
		for (unsigned u = 0; u < 3; ++u) for (unsigned m = 0; m < 3; ++m)
		{
			Layer user, machine;
			user.values[base] = Encode(Value{ DWORD{33} }); machine.values[base] = Encode(Value{ DWORD{66} });
			auto raw = [](unsigned kind, DWORD value) { return kind == 0 ? RegistryConfig::RawValue{} : kind == 1
				? Encode(Value{ value }) : RegistryConfig::RawValue{ true, REG_BINARY, { 1 } }; };
			user.values[over] = raw(u, 17); machine.values[over] = raw(m, 31);
			const Value expected = u == 1 ? Value{ DWORD{17} } : m == 1 ? Value{ DWORD{31} } : Value{};
			for (const auto version : { 1u, Settings::CatalogVersion })
			{
				const auto captured = Capture(user, machine, version);
				Check(!captured.contains(base) && captured.at(over) == expected);
				for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
					for (const auto& change : Plan(user, machine, captured, target, version)) Check(!Settings::IsWindowsColorBase(change.id));
			}
		}
		Layer user, machine;
		user.values[ColorizationColor] = Encode(Value{ DWORD{0xAA123456} });
		user.values[ColorizationAfterglow] = Encode(Value{ DWORD{0xAA654321} });
		machine.values[ColorizationColorOverride] = Encode(Value{ DWORD{0xBB112233} });
		Check(!Capture(user, machine).contains(ColorizationColorOverride));
		Check(!Capture(user, machine).contains(ColorizationAfterglowOverride));
		user.values.erase(ColorizationColor);
		Check(!Capture(user, machine).contains(ColorizationAfterglowOverride));
		user.values[ColorizationColor] = Encode(Value{ DWORD{0xAA123456} });
		user.values[ColorizationColorOverride] = { true, REG_BINARY, { 1, 2 } };
		user.values[ColorizationAfterglowOverride] = Encode(Value{ DWORD{0xBB112233} });
		machine.values[ColorizationAfterglowOverride] = Encode(Value{ DWORD{0xBB112233} });
		for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
		{
			const auto cleanup = Plan(user, machine, { { ColorizationColorOverride, DWORD{5} }, { ColorizationAfterglowOverride, DWORD{6} } }, target);
			Check(cleanup.size() == 4);
			for (const auto& change : cleanup) Check(Settings::IsColorOverride(change.id) && change.before.present && !change.after.present);
		}
		user.values[GlassOpacity] = machine.values[GlassOpacity] = Encode(Value{ DWORD{42} });
		const auto same = Plan(user, machine, { { GlassOpacity, DWORD{42} } }, Settings::Scope::Machine);
		Check(same.size() == 4);
		const auto partial = Plan(user, machine, { { ColorizationColor, DWORD{5} }, { MinMaxButtonGlowId, DWORD{6} } }, Settings::Scope::Machine);
		Check(partial.size() == 4); // Only the independent color cleanup, never base/internal settings.
		user.values[CustomThemeReflection] = { true, REG_BINARY, { 1 } };
		machine.values[CustomThemeReflection] = Encode(Value{ std::wstring(L"machine.png") });
		Check(std::get<std::wstring>(Resolve(user, machine, CustomThemeReflection)) == L"machine.png");
		user.values[GlassOpacity] = Encode(Value{ DWORD{12} });
		machine.values[GlassOpacity] = Encode(Value{ DWORD{34} });
		const Model model{ { GlassOpacity, DWORD{56} }, { CustomThemeReflection, std::monostate{} } };
		for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
		{
			const auto plan = Plan(user, machine, model, target);
			using Key = std::pair<Settings::Scope, std::wstring>;
			using RawValue = RegistryConfig::RawValue;
			std::map<Key, RawValue> initial;
			for (const auto scope : { Settings::Scope::User, Settings::Scope::Machine })
			{
				const auto& layer = scope == Settings::Scope::User ? user : machine;
				for (const auto& [id, raw] : layer.values) if (raw.present) initial[{ scope, std::wstring(Settings::Get(id).name) }] = raw;
			}
			for (std::size_t failAt = 0; failAt < plan.size(); ++failAt)
			{
				auto state = initial;
				PreviewJournal<Key, RawValue> journal;
				auto read = [&](const Key& key) { const auto found = state.find(key); return found == state.end() ? RawValue{} : found->second; };
				auto write = [&](const Key& key, const RawValue& value) { if (value.present) state[key] = value; else state.erase(key); return true; };
				journal.Begin();
				for (std::size_t index = 0; index <= failAt; ++index)
				{
					const auto& change = plan[index]; const Key key{ change.scope, change.Name() };
					journal.Touch(key, read); write(key, change.after);
				}
				journal.Reconcile(read);
				const Key outside{ Settings::Scope::User, L"FutureSetting" };
				state[outside] = Encode(Value{ DWORD{99} });
				Check(journal.RollbackAttempt(write));
				Check(state.at(outside) == Encode(Value{ DWORD{99} })); state.erase(outside);
				Check(state == initial);
			}
		}

	}

	void TestConfigurationStrings()
	{
		using namespace EffectiveConfiguration;
		constexpr auto id = Settings::Id::CustomThemeReflection;
		const std::wstring samples[]{ L"", L"user.png", std::wstring(L"user.png\0ignored", 16), L"%TEMP%\\texture.png" };
		for (const DWORD type : { REG_SZ, REG_EXPAND_SZ })
			for (const auto& text : samples)
				for (const bool terminated : { false, true })
				{
					RegistryConfig::RawValue raw{ true, type, std::vector<BYTE>((text.size() + terminated) * sizeof(wchar_t)) };
					if (!raw.bytes.empty()) std::memcpy(raw.bytes.data(), text.c_str(), raw.bytes.size());
					const Value expected{ text.substr(0, text.find(L'\0')) };
					Check(Decode(raw, Settings::Get(id)) == expected);
					Layer user, machine;
					user.values[id] = raw;
					machine.values[id] = Encode(Value{ std::wstring(L"machine.png") });
					Check(Capture(user, machine).at(id) == expected); // An explicit empty path also masks HKLM.
					for (const auto& change : Plan(user, machine, { { id, expected } }, Settings::Scope::Machine))
						Check(change.scope != Settings::Scope::User); // Same-value user strings are harmless.
					Check(ConfigurationMigration::Prepare(user.values, machine.values, Settings::Scope::User).empty());
					const auto merge = ConfigurationMigration::Prepare(user.values, machine.values, Settings::Scope::Machine);
					Check(merge.size() == 2);
					if (merge.size() == 2)
					{
						Check(merge[0].scope == Settings::Scope::Machine && merge[0].after == raw);
						Check(Decode(merge[0].after, Settings::Get(id)) == expected);
						Check(merge[1].scope == Settings::Scope::User && merge[1].before == raw && !merge[1].after.present);
					}
				}
	}

	void TestConfigurationReset()
	{
		using namespace EffectiveConfiguration;
		using RawValue = RegistryConfig::RawValue;
		using Key = std::pair<Settings::Scope, Settings::Id>;
		Layer user, machine;
		for (const auto& spec : Settings::Catalog)
		{
			user.values[spec.id] = spec.type == Settings::ValueType::String
				? Encode(Value{ std::wstring(L"user.png") }) : Encode(Value{ DWORD{17} });
			machine.values[spec.id] = spec.type == Settings::ValueType::String
				? Encode(Value{ std::wstring(L"machine.png") }) : Encode(Value{ DWORD{31} });
		}
		// Explicit reset removes malformed known values too, with exact raw rollback.
		user.values[Settings::Id::MinMaxButtonGlowId] = { true, REG_BINARY, { 9, 8, 7 } };
		const auto unknown = Settings::Id::Count;
		user.values[unknown] = machine.values[unknown] = { true, REG_BINARY, { 1, 2 } };
		for (const auto target : { Settings::Scope::User, Settings::Scope::Machine })
		{
			Check(PlanReset({}, target).empty());
			const auto plan = PlanReset(target == Settings::Scope::User ? user : machine, target);
			Check(plan.size() == Settings::Catalog.size() - 6);
			for (const auto id : { Settings::Id::MinMaxButtonGlowId, Settings::Id::CloseButtonGlowId, Settings::Id::ToolCloseButtonGlowId })
				Check(std::ranges::any_of(plan, [&](const auto& change) { return change.id == id; }));
			for (const auto& change : plan)
				Check(change.scope == target && !Settings::IsWindowsColorBase(change.id)
					&& change.id != Settings::Id::EnableTransparency && change.id != unknown && !change.after.present);
			auto users = user, machines = machine;
			auto& reset = target == Settings::Scope::User ? users : machines;
			for (const auto& change : plan) reset.values.erase(change.id);
			Check(PlanReset(reset, target).empty());
			Check((target == Settings::Scope::User ? machines.values : users.values) == (target == Settings::Scope::User ? machine.values : user.values));
			Check(reset.values.at(unknown) == user.values.at(unknown));
			Check(reset.values.at(Settings::Id::EnableTransparency) == Raw(target == Settings::Scope::User ? user : machine, Settings::Id::EnableTransparency));
			Check(!Capture(users, machines).contains(Settings::Id::EnableTransparency));
			for (const auto& spec : Settings::Catalog) if (Settings::IsWindowsColorBase(spec.id))
				Check(reset.values.at(spec.id) == Raw(target == Settings::Scope::User ? user : machine, spec.id));
			Check(Resolve(users, machines, Settings::Id::GlassOpacity) == Value{ DWORD{target == Settings::Scope::User ? 31u : 17u} });
			// Repeating a reset has no attempt changes, even while the first reset
			// still has an undo baseline. It must not require another DWM refresh.
			PreviewJournal<Key, RawValue> preview;
			reset.values[Settings::Id::GlassOpacity] = Encode(Value{ DWORD{42} });
			auto readReset = [&](const Key& key) { return Raw(reset, key.second); };
			for (const bool first : { true, false })
			{
				preview.Begin();
				for (const auto& change : PlanReset(reset, target))
				{
					preview.Touch({ change.scope, change.id }, readReset);
					reset.values.erase(change.id);
				}
				preview.Reconcile(readReset);
				Check(preview.HasAttemptChanges() == first && preview.IsDirty());
				preview.CommitAttempt();
			}
			Check(preview.Revert([&](const Key& key, const RawValue& value) { reset.values[key.second] = value; return true; }));
			Check(Raw(reset, Settings::Id::GlassOpacity) == Encode(Value{ DWORD{42} }));
			for (std::size_t failAt = 0; failAt < plan.size(); ++failAt)
			{
				std::map<Key, RawValue> state;
				for (const auto& [id, value] : user.values) state[{ Settings::Scope::User, id }] = value;
				for (const auto& [id, value] : machine.values) state[{ Settings::Scope::Machine, id }] = value;
				auto expected = state;
				PreviewJournal<Key, RawValue> journal;
				auto read = [&](const Key& key) { const auto found = state.find(key); return found == state.end() ? RawValue{} : found->second; };
				auto write = [&](const Key& key, const RawValue& value) { if (value.present) state[key] = value; else state.erase(key); return true; };
				journal.Begin();
				for (std::size_t index = 0; index <= failAt; ++index)
				{
					const auto& change = plan[index]; const Key key{ change.scope, change.id };
					journal.Touch(key, read); write(key, change.after);
				}
				const Key untouched{ target == Settings::Scope::User ? Settings::Scope::Machine : Settings::Scope::User, Settings::Id::GlassOpacity };
				state[untouched] = expected[untouched] = Encode(Value{ DWORD{77} });
				Check(journal.RollbackAttempt(write)); Check(state == expected);
			}
		}
	}
}
