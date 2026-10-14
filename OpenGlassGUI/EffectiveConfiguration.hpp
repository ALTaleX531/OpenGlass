#pragma once
#include "RegistryConfig.hpp"
#include <map>
#include <variant>
#include <cstring>

namespace OpenGlass::EffectiveConfiguration
{
	// monostate removes ordinary customization; system transparency instead writes its enabled default.
	using Value = std::variant<std::monostate, DWORD, std::wstring>;
	using Model = std::map<Settings::Id, Value>;
	struct Layer
	{
		std::map<Settings::Id, RegistryConfig::RawValue> values;
	};
	inline Layer Read(const RegistryConfig& config, bool systemTransparency = true)
	{
		Layer layer;
		for (const auto& spec : Settings::Catalog)
			if (spec.id != Settings::Id::EnableTransparency || (systemTransparency && config.GetMode() == RegistryConfig::Mode::User))
				layer.values.emplace(spec.id, config.ReadRaw(std::wstring(spec.name)));
		return layer;
	}
	inline Value Decode(const RegistryConfig::RawValue& raw, const Settings::Spec& spec)
	{
		if (!raw.present) return {};
		if (spec.type == Settings::ValueType::Dword && raw.type == REG_DWORD && raw.bytes.size() == sizeof(DWORD))
		{
			DWORD value; std::memcpy(&value, raw.bytes.data(), sizeof(value)); return value;
		}
		if (spec.type == Settings::ValueType::String && (raw.type == REG_SZ || raw.type == REG_EXPAND_SZ)
			&& raw.bytes.size() % sizeof(wchar_t) == 0)
		{
			std::wstring value(raw.bytes.size() / sizeof(wchar_t), L'\0');
			if (!raw.bytes.empty()) std::memcpy(value.data(), raw.bytes.data(), raw.bytes.size());
			// WIL reads either string type without expansion. RegGetValue supplies
			// a missing terminator; runtime consumers stop at the first NUL.
			if (const auto end = value.find(L'\0'); end != std::wstring::npos) value.resize(end);
			return value;
		}
		return {};
	}
	inline RegistryConfig::RawValue Encode(const Value& value)
	{
		RegistryConfig::RawValue raw;
		if (const auto number = std::get_if<DWORD>(&value))
		{
			raw = { true, REG_DWORD, std::vector<BYTE>(sizeof(DWORD)) };
			std::memcpy(raw.bytes.data(), number, sizeof(DWORD));
		}
		else if (const auto text = std::get_if<std::wstring>(&value))
		{
			raw = { true, REG_SZ, std::vector<BYTE>((text->size() + 1) * sizeof(wchar_t)) };
			std::memcpy(raw.bytes.data(), text->c_str(), raw.bytes.size());
		}
		return raw;
	}
	inline RegistryConfig::RawValue Raw(const Layer& layer, Settings::Id id)
	{
		const auto found = layer.values.find(id);
		return found == layer.values.end() ? RegistryConfig::RawValue{} : found->second;
	}
	inline Value Resolve(const Layer& user, const Layer& machine, Settings::Id id)
	{
		const auto& spec = Settings::Get(id);
		auto value = Decode(Raw(user, id), spec);
		if (!std::holds_alternative<std::monostate>(value)) return value;
		return Decode(Raw(machine, id), spec);
	}
	inline Model Capture(const Layer& user, const Layer& machine, unsigned version = Settings::CatalogVersion, bool systemTransparency = true)
	{
		Model result;
		for (const auto& spec : Settings::Catalog)
		{
			// Windows base colors/balances are not preset customization. In particular,
			// do not freeze a Windows-generated balance into an OpenGlass Override.
			if (!Settings::IsPresetPackSetting(spec, version) || Settings::IsWindowsColorBase(spec.id)) continue;
			const bool transparency = systemTransparency && spec.id == Settings::Id::ColorizationOpaqueBlend;
			auto value = transparency
				? Decode(Raw(user, Settings::Id::EnableTransparency), Settings::Get(Settings::Id::EnableTransparency))
				: Resolve(user, machine, spec.id);
			if (transparency)
			{
				const auto number = std::get_if<DWORD>(&value);
				// Match the runtime's disabled fallback for an absent/invalid Windows value.
				value = DWORD{ !number || !*number };
			}
			result.emplace(spec.id, std::move(value));
		}
		return result;
	}
	struct Change
	{
		Settings::Scope scope;
		Settings::Id id;
		RegistryConfig::RawValue before, after;
		std::wstring Name() const { return std::wstring(Settings::Get(id).name); }
	};
	// Accent color is original-user Windows state regardless of the editing layer.
	inline std::vector<Change> PlanColorCleanup(const Layer& user, const Layer& machine)
	{
		std::vector<Change> changes;
		for (const auto scope : { Settings::Scope::User, Settings::Scope::Machine })
			for (const auto& spec : Settings::Catalog)
			{
				if (!Settings::IsColorOverride(spec.id)) continue;
				const auto before = Raw(scope == Settings::Scope::User ? user : machine, spec.id);
				if (before.present) changes.push_back({ scope, spec.id, before, {} });
			}
		return changes;
	}
	inline std::vector<Change> PlanReset(const Layer& layer, Settings::Scope target)
	{
		std::vector<Change> changes;
		for (const auto& spec : Settings::Catalog)
		{
			// Reset all known OpenGlass settings, including those outside the preset format.
			if (Settings::IsWindowsColorBase(spec.id) || spec.id == Settings::Id::EnableTransparency) continue;
			const auto before = Raw(layer, spec.id);
			if (before.present) changes.push_back({ target, spec.id, before, {} });
		}
		return changes;
	}
	inline std::vector<Change> Plan(const Layer& user, const Layer& machine, const Model& model, Settings::Scope target, unsigned version = Settings::CatalogVersion, bool systemTransparency = true)
	{
		auto changes = PlanColorCleanup(user, machine);
		for (const auto& [id, value] : model)
		{
			const auto& spec = Settings::Get(id);
			if (!Settings::IsPresetPackSetting(spec, version) || Settings::IsWindowsColorBase(id)) continue;
			// Resolve the physical destination before planning writes and recovery.
			const bool transparency = systemTransparency && id == Settings::Id::ColorizationOpaqueBlend;
			const auto writeId = transparency ? Settings::Id::EnableTransparency : id;
			const auto writeScope = transparency ? Settings::Scope::User : target;
			auto mapped = value;
			if (transparency)
			{
				const auto number = std::get_if<DWORD>(&value);
				// Preset Default enables transparency explicitly; never delete the Windows preference.
				mapped = DWORD{ !number || !*number };
			}
			const auto before = Raw(writeScope == Settings::Scope::User ? user : machine, writeId), after = Encode(mapped);
			if (before != after) changes.push_back({ writeScope, writeId, before, after });
			// Default deliberately leaves other scopes alone, including on HKLM apply.
			if (writeScope != Settings::Scope::Machine || std::holds_alternative<std::monostate>(value)) continue;
			const auto userRaw = Raw(user, id);
			const auto userValue = Decode(userRaw, spec);
			// Invalid values and harmless same-value overrides do not block the intent.
			if (!std::holds_alternative<std::monostate>(userValue) && userValue != value)
				changes.push_back({ Settings::Scope::User, id, userRaw, {} });
		}
		return changes;
	}
}
