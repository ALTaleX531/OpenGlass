#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

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
		std::wstring_view id;
		std::wstring_view name;
		std::uint32_t argb;
		Family family;
	};

	struct Windows7Parameters
	{
		std::uint32_t color;
		std::uint32_t afterglow;
		std::uint32_t colorBalance;
		std::uint32_t afterglowBalance;
		std::uint32_t blurBalance;

		constexpr bool operator==(const Windows7Parameters&) const noexcept = default;
	};

	struct Application
	{
		std::uint32_t color;
		std::optional<std::uint32_t> vistaOpacity;
		std::optional<Windows7Parameters> windows7;
	};

	// Extracted from the stock Vista and Windows 7 themecpl.dll swatch inventories.
	inline constexpr std::array Vista
	{
		Preset{ L"vista.default", L"Default", 0x45409EFE, Family::Vista },
		Preset{ L"vista.graphite", L"Graphite", 0xA3000000, Family::Vista },
		Preset{ L"vista.blue", L"Blue", 0xA8004ADE, Family::Vista },
		Preset{ L"vista.teal", L"Teal", 0x82008CA5, Family::Vista },
		Preset{ L"vista.red", L"Red", 0x9CCE0C0F, Family::Vista },
		Preset{ L"vista.orange", L"Orange", 0xA6FF7700, Family::Vista },
		Preset{ L"vista.pink", L"Pink", 0x49F93EE7, Family::Vista },
		Preset{ L"vista.frost", L"Frost", 0xCCEFF7F7, Family::Vista }
	};

	inline constexpr std::array Windows7
	{
		Preset{ L"windows7.sky", L"Sky", 0x6B74B8FC, Family::Windows7 },
		Preset{ L"windows7.twilight", L"Twilight", 0xA80046AD, Family::Windows7 },
		Preset{ L"windows7.sea", L"Sea", 0x8032CDCD, Family::Windows7 },
		Preset{ L"windows7.leaf", L"Leaf", 0x6614A600, Family::Windows7 },
		Preset{ L"windows7.lime", L"Lime", 0x6697D937, Family::Windows7 },
		Preset{ L"windows7.sun", L"Sun", 0x54FADC0E, Family::Windows7 },
		Preset{ L"windows7.pumpkin", L"Pumpkin", 0x80FF9C00, Family::Windows7 },
		Preset{ L"windows7.ruby", L"Ruby", 0xA8CE0F0F, Family::Windows7 },
		Preset{ L"windows7.fuchsia", L"Fuchsia", 0x66FF0099, Family::Windows7 },
		Preset{ L"windows7.blush", L"Blush", 0x70FCC7F8, Family::Windows7 },
		Preset{ L"windows7.violet", L"Violet", 0x856E3BA1, Family::Windows7 },
		Preset{ L"windows7.lavender", L"Lavender", 0x528D5A94, Family::Windows7 },
		Preset{ L"windows7.taupe", L"Taupe", 0x6698844C, Family::Windows7 },
		Preset{ L"windows7.chocolate", L"Chocolate", 0xA84F1B1B, Family::Windows7 },
		Preset{ L"windows7.slate", L"Slate", 0x80555555, Family::Windows7 },
		Preset{ L"windows7.frost", L"Frost", 0x54FCFCFC, Family::Windows7 }
	};

	[[nodiscard]] constexpr std::span<const Preset> Get(Family family) noexcept
	{
		return family == Family::Vista
			? std::span<const Preset>{ Vista }
			: std::span<const Preset>{ Windows7 };
	}

	[[nodiscard]] constexpr std::uint32_t CalculateVistaOpacity(std::uint32_t argb) noexcept
	{
		const std::uint32_t alpha = argb >> 24;
		return (alpha * 100 + 127) / 255;
	}

	[[nodiscard]] constexpr std::uint32_t CalculateIntensityAlpha(std::uint32_t intensity) noexcept
	{
		return (intensity * 255 + 50) / 100;
	}

	// Exact rational form of the Windows 7 themecpl.dll intensity conversion
	// documented by https://github.com/ALTaleX531/dwm_colorization_calculator.
	[[nodiscard]] constexpr Windows7Parameters CalculateWindows7Parameters(
		std::uint32_t argb,
		bool opaque
	) noexcept
	{
		const int alpha = static_cast<int>(argb >> 24);
		const int balance = (240 * alpha - 1530) / 459;
		Windows7Parameters parameters
		{
			argb,
			argb,
			0,
			0,
			0
		};

		if (opaque)
		{
			parameters.afterglowBalance = 10;
			parameters.colorBalance = static_cast<std::uint32_t>(balance - 10);
			parameters.blurBalance = static_cast<std::uint32_t>(100 - balance);
			return parameters;
		}

		if (balance < 50)
		{
			parameters.colorBalance = 5;
			parameters.blurBalance = static_cast<std::uint32_t>(100 - balance);
			parameters.afterglowBalance = 100 - parameters.colorBalance - parameters.blurBalance;
			return parameters;
		}

		if (balance >= 95)
		{
			parameters.colorBalance = static_cast<std::uint32_t>(balance - 25);
			parameters.afterglowBalance = 0;
			parameters.blurBalance = 100 - parameters.colorBalance;
			return parameters;
		}

		parameters.afterglowBalance = static_cast<std::uint32_t>(95 - balance);
		parameters.blurBalance = static_cast<std::uint32_t>(50 - ((balance - 50) >> 1));
		parameters.colorBalance = 100 - parameters.afterglowBalance - parameters.blurBalance;
		return parameters;
	}

	[[nodiscard]] constexpr Application BuildApplication(
		std::uint32_t argb,
		Family family,
		bool opaque
	) noexcept
	{
		if (family == Family::Vista)
		{
			return { argb, CalculateVistaOpacity(argb), std::nullopt };
		}
		return { argb, std::nullopt, CalculateWindows7Parameters(argb, opaque) };
	}

	[[nodiscard]] constexpr Application BuildApplication(const Preset& preset, bool opaque) noexcept
	{
		return BuildApplication(preset.argb, preset.family, opaque);
	}
}
