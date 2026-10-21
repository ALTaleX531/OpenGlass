#include "pch.h"
#include "TestHarness.hpp"
#include "PngAssetValidation.hpp"
#include "PngFixture.hpp"
#include <vector>
#include <wrl/client.h>

using namespace OpenGlass;

namespace
{
	std::uint32_t TestPngCrc(std::span<const unsigned char> bytes)
	{
		std::uint32_t crc = 0xFFFFFFFFu;
		for (const auto value : bytes)
		{
			crc ^= value;
			for (unsigned bit = 0; bit < 8; ++bit)
			{
				crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
			}
		}
		return crc ^ 0xFFFFFFFFu;
	}

	void AppendPngChunk(std::vector<unsigned char>& png, const char (&type)[5], std::span<const unsigned char> data = {})
	{
		const auto appendBigEndian = [&png](std::uint32_t value)
		{
			png.push_back(static_cast<unsigned char>(value >> 24));
			png.push_back(static_cast<unsigned char>(value >> 16));
			png.push_back(static_cast<unsigned char>(value >> 8));
			png.push_back(static_cast<unsigned char>(value));
		};
		appendBigEndian(static_cast<std::uint32_t>(data.size()));
		const auto crcStart = png.size();
		png.insert(png.end(), type, type + 4);
		png.insert(png.end(), data.begin(), data.end());
		appendBigEndian(TestPngCrc({ png.data() + crcStart, png.size() - crcStart }));
	}

	std::vector<unsigned char> MakeStructuralPng(UINT width = 1, UINT height = 1, std::size_t ancillaryChunks = 0)
	{
		std::vector<unsigned char> png{ 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
		std::array<unsigned char, 13> header
		{
			static_cast<unsigned char>(width >> 24), static_cast<unsigned char>(width >> 16),
			static_cast<unsigned char>(width >> 8), static_cast<unsigned char>(width),
			static_cast<unsigned char>(height >> 24), static_cast<unsigned char>(height >> 16),
			static_cast<unsigned char>(height >> 8), static_cast<unsigned char>(height),
			8, 6, 0, 0, 0
		};
		AppendPngChunk(png, "IHDR", header);
		for (std::size_t index = 0; index < ancillaryChunks; ++index)
		{
			AppendPngChunk(png, "tEXt");
		}
		const std::array<unsigned char, 1> compressed{ 0 };
		AppendPngChunk(png, "IDAT", compressed);
		AppendPngChunk(png, "IEND");
		return png;
	}

	std::span<const std::byte> AsBytes(std::span<const unsigned char> bytes)
	{
		return { reinterpret_cast<const std::byte*>(bytes.data()), bytes.size() };
	}
}

namespace OpenGlassTests
{
	void TestPngAssetValidation()
	{
		PngAssetValidation::ImageInfo info{};
		const auto& valid = ValidPng();
		Check(SUCCEEDED(PngAssetValidation::ValidateStructure(AsBytes(valid), info)));
		Check(info.width == 1 && info.height == 1);

		auto malformed = std::vector<unsigned char>{ valid.begin(), valid.end() };
		malformed[0] = 0;
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(malformed), info)));
		malformed.assign(valid.begin(), valid.end());
		malformed[29] ^= 1;
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(malformed), info)));
		malformed.assign(valid.begin(), valid.end() - 12);
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(malformed), info)));
		malformed.assign(valid.begin(), valid.end());
		malformed.push_back(0);
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(malformed), info)));

		auto structural = MakeStructuralPng();
		const std::vector<unsigned char> duplicateHeader(structural.begin() + 8, structural.begin() + 33);
		structural.insert(structural.begin() + 33, duplicateHeader.begin(), duplicateHeader.end());
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(structural), info)));
		structural = MakeStructuralPng(PngAssetValidation::MaximumDimension + 1, 1);
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(structural), info)));
		structural = MakeStructuralPng(8192, 8192);
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(structural), info)));
		structural = MakeStructuralPng(1, 1, PngAssetValidation::MaximumChunkCount);
		Check(FAILED(PngAssetValidation::ValidateStructure(AsBytes(structural), info)));

		const HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		Check(SUCCEEDED(initializeResult) || initializeResult == RPC_E_CHANGED_MODE);
		const bool uninitialize = SUCCEEDED(initializeResult);
		Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
		Check(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))));
		if (factory)
		{
			Check(SUCCEEDED(PngAssetValidation::ValidateStructure(AsBytes(valid), info)));
			Microsoft::WRL::ComPtr<IWICStream> stream;
			Check(SUCCEEDED(factory->CreateStream(&stream)));
			auto copy = std::vector<unsigned char>{ valid.begin(), valid.end() };
			Check(SUCCEEDED(stream->InitializeFromMemory(copy.data(), static_cast<DWORD>(copy.size()))));
			Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
			Check(SUCCEEDED(PngAssetValidation::CreateValidatedWicSource(factory.Get(), stream.Get(), &info, &converter)));
			converter.Reset();
			PngAssetValidation::ImageInfo wrong{ 2, 1 };
			Check(FAILED(PngAssetValidation::CreateValidatedWicSource(factory.Get(), stream.Get(), &wrong, &converter)));
		}
		factory.Reset();
		if (uninitialize) CoUninitialize();
	}
}
