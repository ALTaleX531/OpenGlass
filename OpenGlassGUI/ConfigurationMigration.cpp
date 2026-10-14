#include "pch.h"
#include "ConfigurationMigration.hpp"
#include "EffectiveConfiguration.hpp"

namespace OpenGlass::ConfigurationMigration
{
	std::vector<Change> Prepare(const RegistryConfig& user, const RegistryConfig& machine, Settings::Scope target)
	{
		return Prepare(EffectiveConfiguration::Read(user, false).values, EffectiveConfiguration::Read(machine, false).values, target);
	}

	std::vector<Change> Prepare(Values users, Values machines, Settings::Scope target)
	{
		using RawValue = RegistryConfig::RawValue;
		Values targets;
		std::map<Settings::Id, std::wstring> origins;
		auto valid = [](const Settings::Spec& spec, const RawValue& value)
		{
			return !std::holds_alternative<std::monostate>(EffectiveConfiguration::Decode(value, spec));
		};
		for (const auto& spec : Settings::Catalog)
		{
			if (Settings::IsWindowsColorBase(spec.id) || Settings::IsColorOverride(spec.id) || spec.id == Settings::Id::EnableTransparency) continue;
			if (valid(spec, users[spec.id])) { targets[spec.id] = users[spec.id]; origins[spec.id] = L"HKCU " + std::wstring(spec.name); }
			else if (valid(spec, machines[spec.id])) { targets[spec.id] = machines[spec.id]; origins[spec.id] = L"HKLM " + std::wstring(spec.name); }
		}
		// Materialize the effective balances as explicit overrides. Windows owns
		// the base values in HKCU; those values must survive this operation.
		for (const auto base : { Settings::Id::ColorizationColorBalance,
			Settings::Id::ColorizationAfterglowBalance, Settings::Id::ColorizationBlurBalance })
		{
			const auto overrideId = Settings::Find(std::wstring(Settings::Get(base).name) + L"Override")->id;
			if (targets.contains(overrideId)) continue;
			const auto& spec = Settings::Get(base);
			if (valid(spec, users[base])) { targets[overrideId] = users[base]; origins[overrideId] = L"HKCU " + std::wstring(spec.name); }
			else if (valid(spec, machines[base])) { targets[overrideId] = machines[base]; origins[overrideId] = L"HKLM " + std::wstring(spec.name); }
		}
		auto decode = [](const RawValue& value)
		{
			DWORD result{};
			if (value.type == REG_DWORD && value.bytes.size() == sizeof(result)) std::memcpy(&result, value.bytes.data(), sizeof(result));
			return result;
		};
		auto describe = [&](Settings::Id id, const RawValue& value)
		{
			if (!value.present) return std::wstring(L"<absent>");
			if (value.type == REG_DWORD && value.bytes.size() == sizeof(DWORD)) return std::format(L"0x{:08X}", decode(value));
			const auto decoded = EffectiveConfiguration::Decode(value, Settings::Get(id));
			if (const auto text = std::get_if<std::wstring>(&decoded)) return *text;
			return std::format(L"<type {}, {} bytes>", value.type, value.bytes.size());
		};
		std::vector<Change> changes;
		auto& destination = target == Settings::Scope::User ? users : machines;
		const auto targetName = target == Settings::Scope::User ? L"HKCU" : L"HKLM";
		for (const auto& [id, value] : targets)
		{
			if (destination[id] == value) continue;
			const auto name = std::wstring(Settings::Get(id).name);
			changes.push_back({ target, id, destination[id], value,
				origins[id] + L" -> " + targetName + L" " + name + L": " + describe(id, destination[id]) + L" -> " + describe(id, value)
					+ (destination[id].present ? L" (replace conflict)" : L" (create)") });
		}
		// A user merge leaves machine configuration intact for other users.
		if (target == Settings::Scope::User) return changes;
		// Delete only valid user values whose effective destination was prepared.
		// Malformed values are preserved for manual diagnosis, never destroyed.
		for (const auto& spec : Settings::Catalog)
		{
			if (Settings::IsWindowsColorBase(spec.id) || Settings::IsColorOverride(spec.id)
				|| spec.id == Settings::Id::EnableTransparency || !valid(spec, users[spec.id])) continue;
			changes.push_back({ Settings::Scope::User, spec.id, users[spec.id], {},
				L"HKCU -> HKLM: " + std::wstring(spec.name) + L" (delete HKCU after copying)" });
		}
		return changes;
	}
}
