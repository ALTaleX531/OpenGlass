#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include "Colorization.hpp"

namespace OpenGlass::ColorizationPresets
{
	inline constexpr int ClassicIntensityMinimum = 10;
	inline constexpr int ClassicIntensityMaximum = 85;

	enum class Family
	{
		Vista,
		Windows7
	};

	struct Preset
	{
		std::wstring_view name;
		std::uint32_t argb;
		Family family;
	};

	using Colorization::Windows7Parameters;
	using Colorization::CalculateWindows7Parameters;
	using Colorization::CalculateVistaOpacity;
	using Colorization::CalculateIntensityAlpha;

	// Extracted from the stock Vista and Windows 7 themecpl.dll swatch inventories.
	inline constexpr std::array Vista
	{
		Preset{ L"Default", 0x45409EFE, Family::Vista },
		Preset{ L"Graphite", 0xA3000000, Family::Vista },
		Preset{ L"Blue", 0xA8004ADE, Family::Vista },
		Preset{ L"Teal", 0x82008CA5, Family::Vista },
		Preset{ L"Red", 0x9CCE0C0F, Family::Vista },
		Preset{ L"Orange", 0xA6FF7700, Family::Vista },
		Preset{ L"Pink", 0x49F93EE7, Family::Vista },
		Preset{ L"Frost", 0xCCEFF7F7, Family::Vista }
	};

	inline constexpr std::array Windows7
	{
		Preset{ L"Sky", 0x6B74B8FC, Family::Windows7 },
		Preset{ L"Twilight", 0xA80046AD, Family::Windows7 },
		Preset{ L"Sea", 0x8032CDCD, Family::Windows7 },
		Preset{ L"Leaf", 0x6614A600, Family::Windows7 },
		Preset{ L"Lime", 0x6697D937, Family::Windows7 },
		Preset{ L"Sun", 0x54FADC0E, Family::Windows7 },
		Preset{ L"Pumpkin", 0x80FF9C00, Family::Windows7 },
		Preset{ L"Ruby", 0xA8CE0F0F, Family::Windows7 },
		Preset{ L"Fuchsia", 0x66FF0099, Family::Windows7 },
		Preset{ L"Blush", 0x70FCC7F8, Family::Windows7 },
		Preset{ L"Violet", 0x856E3BA1, Family::Windows7 },
		Preset{ L"Lavender", 0x528D5A94, Family::Windows7 },
		Preset{ L"Taupe", 0x6698844C, Family::Windows7 },
		Preset{ L"Chocolate", 0xA84F1B1B, Family::Windows7 },
		Preset{ L"Slate", 0x80555555, Family::Windows7 },
		Preset{ L"Frost", 0x54FCFCFC, Family::Windows7 }
	};

	[[nodiscard]] constexpr std::span<const Preset> Get(Family family) noexcept
	{
		return family == Family::Vista
			? std::span<const Preset>{ Vista }
			: std::span<const Preset>{ Windows7 };
	}

	// Accent color synchronization can change Alpha independently of GUI intensity.
	[[nodiscard]] constexpr bool MatchesWindows7Preset(
		const Preset& preset,
		const Windows7Parameters& actual,
		std::uint32_t intensity,
		bool opaque
	) noexcept
	{
		const auto expected = CalculateWindows7Parameters(preset.argb, opaque);
		return intensity == CalculateVistaOpacity(preset.argb)
			&& (actual.color & 0x00FFFFFF) == (expected.color & 0x00FFFFFF)
			&& (actual.afterglow & 0x00FFFFFF) == (expected.afterglow & 0x00FFFFFF)
			&& actual.colorBalance == expected.colorBalance
			&& actual.afterglowBalance == expected.afterglowBalance
			&& actual.blurBalance == expected.blurBalance;
	}
}
