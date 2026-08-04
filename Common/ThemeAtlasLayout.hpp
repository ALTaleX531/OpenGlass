#pragma once

#include <Windows.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace OpenGlass::ThemeAtlasLayout
{
	inline constexpr std::size_t MaximumFileSize = 1024 * 1024;
	inline constexpr std::size_t MaximumLineCount = 512;
	inline constexpr std::size_t MaximumLineLength = 4096;

	struct Mapping
	{
		std::int32_t part{};
		std::int32_t state{};
		std::int32_t property{};
		std::array<std::int32_t, 4> value{};
	};

	struct Property
	{
		std::string name;
		std::int32_t value{};
	};

	using Record = std::variant<Mapping, Property>;

	struct Document
	{
		std::vector<Record> records;
	};

	[[nodiscard]] HRESULT Parse(
		std::span<const std::byte> bytes,
		Document& document,
		std::size_t* errorLine = nullptr
	) noexcept;
}
