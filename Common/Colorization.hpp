#pragma once

#include <cstdint>
#include <algorithm>

namespace OpenGlass::Colorization
{
	[[nodiscard]] constexpr std::uint32_t CalculateVistaOpacity(std::uint32_t argb) noexcept
	{
		const std::uint32_t alpha = argb >> 24;
		return (alpha * 100 + 127) / 255;
	}

	[[nodiscard]] constexpr std::uint32_t CalculateIntensityAlpha(std::uint32_t intensity) noexcept
	{
		return (intensity * 255 + 50) / 100;
	}

	struct Windows7Parameters
	{
		std::uint32_t color;
		std::uint32_t afterglow;
		std::uint32_t colorBalance;
		std::uint32_t afterglowBalance;
		std::uint32_t blurBalance;

		constexpr bool operator==(const Windows7Parameters&) const noexcept = default;
	};

	// Exact rational form of the Windows 7 themecpl.dll intensity conversion
	// documented by https://github.com/ALTaleX531/dwm_colorization_calculator.
	[[nodiscard]] constexpr Windows7Parameters CalculateWindows7Parameters(
		std::uint32_t argb,
		bool opaque
	) noexcept
	{
		const int alpha = static_cast<int>(argb >> 24);
		// Saturate the curve before converting signed weights to DWORDs. Themes
		// and Automatic can supply alpha outside the stock swatches' range.
		const int balance = std::clamp((240 * alpha - 1530) / 459, opaque ? 10 : 5, opaque ? 100 : 125);
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

}
