#pragma once
#include "SettingsCatalog.hpp"
#include <optional>
#include <span>
#include <string_view>

namespace OpenGlass::Settings
{
	[[nodiscard]] constexpr std::optional<Scope> ParseEditorScope(std::span<const std::wstring_view> arguments) noexcept
	{
		std::optional<Scope> selected;
		for (std::size_t i = 0; i < arguments.size(); ++i)
		{
			auto arg = arguments[i];
			std::wstring_view value;
			if (arg.starts_with(L"--scope=")) value = arg.substr(8);
			else if (arg == L"--scope")
			{
				if (++i == arguments.size()) return std::nullopt;
				value = arguments[i];
			}
			else continue;
			auto equal = [](std::wstring_view left, std::wstring_view right)
			{
				if (left.size() != right.size()) return false;
				for (std::size_t n = 0; n < left.size(); ++n)
				{
					const auto c = left[n] >= L'A' && left[n] <= L'Z' ? left[n] + (L'a' - L'A') : left[n];
					if (c != right[n]) return false;
				}
				return true;
			};
			const auto scope = equal(value, L"hkcu") ? Scope::User : Scope::Machine;
			if (!equal(value, L"hkcu") && !equal(value, L"hklm")) return std::nullopt;
			if (selected && *selected != scope) return std::nullopt;
			selected = scope;
		}
		return selected.value_or(Scope::Machine);
	}
}
