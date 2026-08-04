#pragma once

#include <Windows.h>
#include <objidl.h>
#include <wincodec.h>

#include <cstddef>
#include <cstdint>
#include <span>

namespace OpenGlass::PngAssetValidation
{
	inline constexpr std::size_t MaximumFileSize = 64ull * 1024 * 1024;
	inline constexpr std::size_t MaximumChunkCount = 4096;
	inline constexpr UINT MaximumDimension = 16'384;
	inline constexpr std::uint64_t MaximumPixelCount = 33'554'432;

	struct ImageInfo
	{
		UINT width{};
		UINT height{};
	};

	[[nodiscard]] HRESULT ValidateStructure(
		std::span<const std::byte> bytes,
		ImageInfo& info
	) noexcept;

	[[nodiscard]] HRESULT CreateValidatedWicSource(
		IWICImagingFactory* factory,
		IStream* stream,
		const ImageInfo* expectedInfo,
		IWICFormatConverter** converter,
		ImageInfo* decodedInfo = nullptr
	) noexcept;
}
