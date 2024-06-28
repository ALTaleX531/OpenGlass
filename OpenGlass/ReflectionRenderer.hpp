#pragma once
#include "pch.h"
#include "resource.h"
#include "framework.hpp"
#include "cpprt.hpp"
#include "uDwmProjection.hpp"

namespace OpenGlass::ReflectionRenderer
{
	inline float g_reflectionIntensity{ 0.f };
	inline float g_reflectionParallaxIntensity{ 0.1f };
	inline std::wstring g_reflectionTexturePath{};
	inline winrt::com_ptr<ID2D1Bitmap1> g_reflectionBitmap{ nullptr };

	inline HRESULT LoadReflectionTexture(ID2D1DeviceContext* deviceContext)
	{
		winrt::com_ptr<IStream> stream{ nullptr };
		if (g_reflectionTexturePath.empty() || !PathFileExistsW(g_reflectionTexturePath.data()))
		{
			HMODULE currentModule{ wil::GetModuleInstanceHandle() };
			auto resourceHandle{ FindResourceW(currentModule, MAKEINTRESOURCE(IDB_REFLECTION), L"PNG") };
			RETURN_LAST_ERROR_IF_NULL(resourceHandle);
			auto globalHandle{ LoadResource(currentModule, resourceHandle) };
			RETURN_LAST_ERROR_IF_NULL(globalHandle);
			auto cleanUp = wil::scope_exit([&]
			{
				if (globalHandle)
				{
					UnlockResource(globalHandle);
					FreeResource(globalHandle);
				}
			});
			DWORD resourceSize{ SizeofResource(currentModule, resourceHandle) };
			RETURN_LAST_ERROR_IF(resourceSize == 0);
			auto resourceAddress{ reinterpret_cast<PBYTE>(LockResource(globalHandle)) };
			stream = { SHCreateMemStream(resourceAddress, resourceSize), winrt::take_ownership_from_abi };
		}
		else
		{
			wil::unique_hfile file{ CreateFileW(g_reflectionTexturePath.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0) };
			RETURN_LAST_ERROR_IF(!file.is_valid());

			LARGE_INTEGER fileSize{};
			RETURN_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file.get(), &fileSize));

			auto buffer{ std::make_unique<BYTE[]>(static_cast<size_t>(fileSize.QuadPart)) };
			RETURN_IF_WIN32_BOOL_FALSE(ReadFile(file.get(), buffer.get(), static_cast<DWORD>(fileSize.QuadPart), nullptr, nullptr));
			stream = { SHCreateMemStream(buffer.get(), static_cast<UINT>(fileSize.QuadPart)), winrt::take_ownership_from_abi };
		}
		RETURN_LAST_ERROR_IF_NULL(stream);

		winrt::com_ptr<IWICImagingFactory2> wicFactory{ nullptr };
		wicFactory.copy_from(uDwm::CDesktopManager::s_pDesktopManagerInstance->GetWICFactory());
		winrt::com_ptr<IWICBitmapDecoder> wicDecoder{ nullptr };
		RETURN_IF_FAILED(wicFactory->CreateDecoderFromStream(stream.get(), &GUID_VendorMicrosoft, WICDecodeMetadataCacheOnDemand, wicDecoder.put()));
		winrt::com_ptr<IWICBitmapFrameDecode> wicFrame{ nullptr };
		RETURN_IF_FAILED(wicDecoder->GetFrame(0, wicFrame.put()));
		winrt::com_ptr<IWICFormatConverter> wicConverter{ nullptr };
		RETURN_IF_FAILED(wicFactory->CreateFormatConverter(wicConverter.put()));
		winrt::com_ptr<IWICPalette> wicPalette{ nullptr };
		RETURN_IF_FAILED(
			wicConverter->Initialize(
				wicFrame.get(),
				GUID_WICPixelFormat32bppPBGRA,
				WICBitmapDitherTypeNone,
				wicPalette.get(),
				0, WICBitmapPaletteTypeCustom
			)
		);
		winrt::com_ptr<IWICBitmap> wicBitmap{ nullptr };
		RETURN_IF_FAILED(wicFactory->CreateBitmapFromSource(wicConverter.get(), WICBitmapCreateCacheOption::WICBitmapNoCache, wicBitmap.put()));

		RETURN_IF_FAILED(
			deviceContext->CreateBitmapFromWicBitmap(
				wicBitmap.get(),
				D2D1::BitmapProperties1(
					D2D1_BITMAP_OPTIONS_NONE,
					D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
				),
				g_reflectionBitmap.put()
			)
		);

		return S_OK;
	}
	inline HRESULT Draw(
		ID2D1DeviceContext* deviceContext,
		const D2D1_POINT_2F& reflectionPosition,
		const D2D1_SIZE_F& desktopSize,
		const D2D1_RECT_F& reflectionBounds
	)
	{
		if (!g_reflectionBitmap)
		{
			RETURN_IF_FAILED(LoadReflectionTexture(deviceContext));
		}
		if (g_reflectionIntensity != 0.f)
		{
			auto reflectionSize{ g_reflectionBitmap->GetSize() };
			auto bounds{ reflectionBounds };
			D2D1_POINT_2F offset
			{
				reflectionPosition.x * (1.f - g_reflectionParallaxIntensity) * (reflectionSize.width / desktopSize.width),
				reflectionPosition.y * (reflectionSize.height / desktopSize.height)
			};
			if (reflectionPosition.x < 0)
			{
				bounds.left += -reflectionPosition.x;
				offset.x += -offset.x;
			}
			if (reflectionPosition.y < 0)
			{
				bounds.top += -reflectionPosition.y;
				offset.y += -offset.y;
			}
			if (reflectionPosition.x + reflectionBounds.right - reflectionBounds.left > desktopSize.width)
			{
				bounds.right = bounds.right - (reflectionPosition.x + reflectionBounds.right - reflectionBounds.left - desktopSize.width);
			}
			if (reflectionPosition.y + reflectionBounds.bottom - reflectionBounds.top > desktopSize.height)
			{
				bounds.bottom = bounds.bottom - (reflectionPosition.y + reflectionBounds.bottom - reflectionBounds.top - desktopSize.height);
			}

			D2D1_SIZE_F size
			{
				(bounds.right - bounds.left) * (reflectionSize.width / desktopSize.width),
				(bounds.bottom - bounds.top) * (reflectionSize.height / desktopSize.height)
			};

			deviceContext->DrawBitmap(
				g_reflectionBitmap.get(),
				bounds,
				g_reflectionIntensity,
				D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
				D2D1::RectF(
					offset.x,
					offset.y,
					offset.x + size.width,
					offset.y + size.height
				)
			);
		}

		return S_OK;
	}
}