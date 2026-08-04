#include "PngAssetValidation.hpp"

#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace OpenGlass::PngAssetValidation
{
	namespace
	{
		constexpr std::array<std::byte, 8> Signature
		{
			std::byte{ 0x89 }, std::byte{ 0x50 }, std::byte{ 0x4E }, std::byte{ 0x47 },
			std::byte{ 0x0D }, std::byte{ 0x0A }, std::byte{ 0x1A }, std::byte{ 0x0A }
		};

		constexpr HRESULT BadImage = WINCODEC_ERR_BADIMAGE;

		[[nodiscard]] std::uint32_t ReadBigEndian(std::span<const std::byte, 4> bytes) noexcept
		{
			return
				(std::to_integer<std::uint32_t>(bytes[0]) << 24)
				| (std::to_integer<std::uint32_t>(bytes[1]) << 16)
				| (std::to_integer<std::uint32_t>(bytes[2]) << 8)
				| std::to_integer<std::uint32_t>(bytes[3]);
		}

		[[nodiscard]] std::uint32_t UpdateCrc(std::uint32_t crc, std::span<const std::byte> bytes) noexcept
		{
			for (const std::byte value : bytes)
			{
				crc ^= std::to_integer<std::uint8_t>(value);
				for (unsigned bit = 0; bit < 8; ++bit)
				{
					crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
				}
			}
			return crc;
		}

		[[nodiscard]] bool IsChunkType(std::span<const std::byte, 4> type, const char (&name)[5]) noexcept
		{
			return std::memcmp(type.data(), name, type.size()) == 0;
		}

		[[nodiscard]] bool ValidDimensions(const ImageInfo& info) noexcept
		{
			return info.width != 0
				&& info.height != 0
				&& info.width <= MaximumDimension
				&& info.height <= MaximumDimension
				&& static_cast<std::uint64_t>(info.width) * info.height <= MaximumPixelCount;
		}
	}

	HRESULT ValidateStructure(std::span<const std::byte> bytes, ImageInfo& info) noexcept
	{
		info = {};
		if (bytes.size() > MaximumFileSize)
		{
			return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
		}
		if (bytes.size() < Signature.size() || !std::ranges::equal(bytes.first(Signature.size()), Signature))
		{
			return BadImage;
		}

		std::size_t offset = Signature.size();
		std::size_t chunkCount{};
		bool sawHeader{};
		bool sawImageData{};
		bool sawEnd{};
		while (offset < bytes.size())
		{
			if (++chunkCount > MaximumChunkCount || bytes.size() - offset < 12)
			{
				return BadImage;
			}

			const auto lengthBytes = std::span<const std::byte, 4>{ bytes.subspan(offset, 4) };
			const std::size_t dataLength = ReadBigEndian(lengthBytes);
			if (dataLength > bytes.size() - offset - 12)
			{
				return BadImage;
			}

			const auto type = std::span<const std::byte, 4>{ bytes.subspan(offset + 4, 4) };
			const auto data = bytes.subspan(offset + 8, dataLength);
			const auto storedCrc = ReadBigEndian(std::span<const std::byte, 4>{ bytes.subspan(offset + 8 + dataLength, 4) });
			std::uint32_t calculatedCrc = UpdateCrc(0xFFFFFFFFu, type);
			calculatedCrc = UpdateCrc(calculatedCrc, data) ^ 0xFFFFFFFFu;
			if (calculatedCrc != storedCrc)
			{
				return BadImage;
			}

			if (!sawHeader)
			{
				if (!IsChunkType(type, "IHDR") || dataLength != 13)
				{
					return BadImage;
				}
				info.width = ReadBigEndian(std::span<const std::byte, 4>{ data.first<4>() });
				info.height = ReadBigEndian(std::span<const std::byte, 4>{ data.subspan<4, 4>() });
				if (!ValidDimensions(info))
				{
					return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
				}
				sawHeader = true;
			}
			else if (IsChunkType(type, "IHDR"))
			{
				return BadImage;
			}

			if (IsChunkType(type, "IDAT"))
			{
				sawImageData = true;
			}
			else if (IsChunkType(type, "IEND"))
			{
				if (dataLength != 0 || sawEnd || !sawImageData)
				{
					return BadImage;
				}
				sawEnd = true;
			}

			offset += 12 + dataLength;
			if (sawEnd && offset != bytes.size())
			{
				return BadImage;
			}
		}

		return sawHeader && sawImageData && sawEnd ? S_OK : BadImage;
	}

	HRESULT CreateValidatedWicSource(
		IWICImagingFactory* factory,
		IStream* stream,
		const ImageInfo* expectedInfo,
		IWICFormatConverter** converter,
		ImageInfo* decodedInfo
	) noexcept
	{
		if (!factory || !stream || !converter)
		{
			return E_INVALIDARG;
		}
		*converter = nullptr;
		if (decodedInfo)
		{
			*decodedInfo = {};
		}

		LARGE_INTEGER origin{};
		HRESULT result = stream->Seek(origin, STREAM_SEEK_SET, nullptr);
		if (FAILED(result)) return result;

		Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
		result = factory->CreateDecoderFromStream(
			stream,
			&GUID_VendorMicrosoft,
			WICDecodeMetadataCacheOnLoad,
			&decoder
		);
		if (FAILED(result)) return result;

		GUID container{};
		result = decoder->GetContainerFormat(&container);
		if (FAILED(result)) return result;
		if (container != GUID_ContainerFormatPng) return BadImage;

		UINT frameCount{};
		result = decoder->GetFrameCount(&frameCount);
		if (FAILED(result)) return result;
		if (frameCount == 0) return BadImage;

		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
		result = decoder->GetFrame(0, &frame);
		if (FAILED(result)) return result;

		ImageInfo actual{};
		result = frame->GetSize(&actual.width, &actual.height);
		if (FAILED(result)) return result;
		if (!ValidDimensions(actual)) return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
		if (expectedInfo && (expectedInfo->width != actual.width || expectedInfo->height != actual.height))
		{
			return BadImage;
		}

		WICPixelFormatGUID sourceFormat{};
		result = frame->GetPixelFormat(&sourceFormat);
		if (FAILED(result)) return result;

		Microsoft::WRL::ComPtr<IWICFormatConverter> resultConverter;
		result = factory->CreateFormatConverter(&resultConverter);
		if (FAILED(result)) return result;
		BOOL canConvert{};
		result = resultConverter->CanConvert(sourceFormat, GUID_WICPixelFormat32bppPBGRA, &canConvert);
		if (FAILED(result)) return result;
		if (!canConvert) return WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
		result = resultConverter->Initialize(
			frame.Get(),
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			nullptr,
			0,
			WICBitmapPaletteTypeCustom
		);
		if (FAILED(result)) return result;

		if (decodedInfo)
		{
			*decodedInfo = actual;
		}
		*converter = resultConverter.Detach();
		return S_OK;
	}
}
