#pragma once

#include "SettingsCatalog.hpp"

#include <variant>

namespace OpenGlass::ConfigurationMigrationPolicy
{
	template <typename Value>
	struct HiveValues
	{
		Value user;
		Value machine;
	};

	template <typename Value>
	[[nodiscard]] HiveValues<Value> Canonicalize(Settings::Scope canonicalScope, HiveValues<Value> values)
	{
		const auto missing = [](const Value& value) { return std::holds_alternative<std::monostate>(value); };
		if (canonicalScope == Settings::Scope::User)
		{
			if (missing(values.user) && !missing(values.machine)) values.user = values.machine;
			values.machine = std::monostate{};
		}
		else
		{
			if (!missing(values.user)) values.machine = values.user;
			values.user = std::monostate{};
		}
		return values;
	}
}
