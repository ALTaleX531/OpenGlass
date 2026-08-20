#pragma once
#include <algorithm>
#include <cstdint>

namespace OpenGlass::BlurSettings
{
	// Glass8 introduced BlurDeviation before its D2D chain gained 0.5x internal downsampling.
	// OpenGlass preserves that registry contract and decodes it to compositor BlurAmount at 3x.
	inline constexpr std::uint32_t DefaultEncodedDeviation = 30;
	inline constexpr float MaximumBlurAmount = 250.f;
	inline constexpr int GuiMinimumBlurAmount = 0;
	inline constexpr int GuiMaximumBlurAmount = 30;
	inline constexpr float Direct3DStandardDeviation = 3.f;

	[[nodiscard]] constexpr float DecodeBlurAmount(std::uint32_t encodedDeviation) noexcept
	{
		return std::clamp(
			static_cast<float>(encodedDeviation) / 10.f * 3.f,
			0.f,
			MaximumBlurAmount
		);
	}

	[[nodiscard]] constexpr int DecodeGuiBlurAmount(std::uint32_t encodedDeviation) noexcept
	{
		const auto roundedBlurAmount =
			(static_cast<std::uint64_t>(encodedDeviation) * 3u + 5u) / 10u;
		return static_cast<int>(std::min<std::uint64_t>(roundedBlurAmount, GuiMaximumBlurAmount));
	}

	[[nodiscard]] constexpr std::uint32_t EncodeGuiBlurAmount(int blurAmount) noexcept
	{
		const auto clampedBlurAmount = std::clamp(
			blurAmount,
			GuiMinimumBlurAmount,
			GuiMaximumBlurAmount
		);
		return static_cast<std::uint32_t>((clampedBlurAmount * 10 + 2) / 3);
	}
}
