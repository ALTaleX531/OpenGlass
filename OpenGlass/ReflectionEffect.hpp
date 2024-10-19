#pragma once
#include "resource.h"
#include "framework.hpp"
#include "cpprt.hpp"
#include "uDwmProjection.hpp"

namespace OpenGlass::ReflectionEffect
{
	namespace Details
	{
		inline bool g_reflectionTexturePathChanged{ false };
		inline winrt::com_ptr<ID2D1Effect> g_reflectionBitmapSourceEffect{ nullptr };
		inline winrt::com_ptr<ID2D1Effect> g_cropEffect{ nullptr };
		inline std::wstring g_reflectionTexturePath{};
		inline UINT g_reflectionTextureWidth{}, g_reflectionTextureHeight{};
	}

	inline void UpdateTexture(
		std::wstring_view reflectionTexturePath
	)
	{
		Details::g_reflectionTexturePath = reflectionTexturePath;
		Details::g_reflectionTextureWidth = Details::g_reflectionTextureHeight = 0;
		Details::g_reflectionTexturePathChanged = true;
	}
	inline HRESULT Render(
		ID2D1DeviceContext* context,
		ID2D1Geometry* geometry,
		float reflectionIntensity,
		float parallaxIntensity,
		// reserved for the next release
		const D2D1_SIZE_F* canvasSize = nullptr,
		const D2D1_POINT_2F* glassOffset = nullptr
	)
	{
		if (reflectionIntensity == 0.f)
		{
			return S_OK;
		}
		// setup effects
		if (!Details::g_reflectionBitmapSourceEffect)
		{
			RETURN_IF_FAILED(
				context->CreateEffect(
					CLSID_D2D1BitmapSource,
					Details::g_reflectionBitmapSourceEffect.put()
				)
			);
			RETURN_IF_FAILED(
				Details::g_reflectionBitmapSourceEffect->SetValue(
					D2D1_BITMAPSOURCE_PROP_ALPHA_MODE,
					D2D1_ALPHA_MODE_PREMULTIPLIED
				)
			);
			RETURN_IF_FAILED(
				Details::g_reflectionBitmapSourceEffect->SetValue(
					D2D1_BITMAPSOURCE_PROP_INTERPOLATION_MODE,
					D2D1_INTERPOLATION_MODE_ANISOTROPIC
				)
			);
			RETURN_IF_FAILED(
				Details::g_reflectionBitmapSourceEffect->SetValue(
					D2D1_PROPERTY_CACHED,
					TRUE
				)
			);

			Details::g_reflectionTexturePathChanged = true;
		}
		if (!Details::g_cropEffect)
		{
			RETURN_IF_FAILED(
				context->CreateEffect(
					CLSID_D2D1Crop,
					Details::g_cropEffect.put()
				)
			);
			RETURN_IF_FAILED(
				Details::g_cropEffect->SetValue(
					D2D1_CROP_PROP_BORDER_MODE,
					D2D1_BORDER_MODE_SOFT
				)
			);
			Details::g_cropEffect->SetInputEffect(0, Details::g_reflectionBitmapSourceEffect.get());
		}

		if (Details::g_reflectionTexturePathChanged)
		{
			winrt::com_ptr<IStream> stream{ nullptr };
			if (Details::g_reflectionTexturePath.empty() || !PathFileExistsW(Details::g_reflectionTexturePath.data()))
			{
				HMODULE currentModule{ wil::GetModuleInstanceHandle() };
				auto resourceHandle = FindResourceW(currentModule, MAKEINTRESOURCE(IDB_REFLECTION), L"PNG");
				RETURN_LAST_ERROR_IF_NULL(resourceHandle);
				auto globalHandle = LoadResource(currentModule, resourceHandle);
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
				auto resourceAddress = reinterpret_cast<PBYTE>(LockResource(globalHandle));
				stream = { SHCreateMemStream(resourceAddress, resourceSize), winrt::take_ownership_from_abi };
			}
			else
			{
				wil::unique_hfile file{ CreateFileW(Details::g_reflectionTexturePath.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0) };
				RETURN_LAST_ERROR_IF(!file.is_valid());

				LARGE_INTEGER fileSize{};
				RETURN_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file.get(), &fileSize));

				auto buffer = std::make_unique<BYTE[]>(static_cast<size_t>(fileSize.QuadPart));
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
			RETURN_IF_FAILED(
				wicConverter->Initialize(
					wicFrame.get(),
					GUID_WICPixelFormat32bppPBGRA,
					WICBitmapDitherTypeNone,
					nullptr,
					0,
					WICBitmapPaletteTypeCustom
				)
			);

			RETURN_IF_FAILED(
				Details::g_reflectionBitmapSourceEffect->SetValue(
					D2D1_BITMAPSOURCE_PROP_WIC_BITMAP_SOURCE,
					wicConverter.get()
				)
			);
			RETURN_IF_FAILED(
				wicConverter->GetSize(
					&Details::g_reflectionTextureWidth,
					&Details::g_reflectionTextureHeight
				)
			);
			Details::g_reflectionTexturePathChanged = false;
		}
		
		// initialize effect params
		if (!canvasSize)
		{
			auto size = context->GetSize();
			RETURN_IF_FAILED(
				Details::g_reflectionBitmapSourceEffect->SetValue(
					D2D1_BITMAPSOURCE_PROP_SCALE,
					D2D1::Vector2F(
						size.width / static_cast<float>(Details::g_reflectionTextureWidth),
						size.height / static_cast<float>(Details::g_reflectionTextureHeight)
					)
				)
			);
		}
		else
		{
			RETURN_IF_FAILED(
				Details::g_reflectionBitmapSourceEffect->SetValue(
					D2D1_BITMAPSOURCE_PROP_SCALE,
					D2D1::Vector2F(
						canvasSize->width / static_cast<float>(Details::g_reflectionTextureWidth),
						canvasSize->height / static_cast<float>(Details::g_reflectionTextureHeight)
					)
				)
			);
		}

		D2D1_MATRIX_3X2_F matrix{};
		context->GetTransform(&matrix);

		D2D1_POINT_2F offset{};
		if (!glassOffset)
		{
			offset = D2D1::Point2F(matrix.dx, matrix.dy);
		}
		else
		{
			offset = *glassOffset;
		}
		matrix.dx = matrix.dy = 0.f;

		D2D1_RECT_F bounds{};
		RETURN_IF_FAILED(geometry->GetBounds(nullptr, &bounds));

		D2D1_POINT_2F point{ offset.x >= 0.f ? bounds.left : 0.f, offset.y >= 0.f ? bounds.top : 0.f };
		D2D1_RECT_F cropRect
		{
			offset.x * (1.f - parallaxIntensity),
			offset.y
		};
		cropRect.right = max(cropRect.left, 0.f) + (bounds.right - bounds.left);
		cropRect.bottom = max(cropRect.top, 0.f) + (bounds.bottom - bounds.top);

		RETURN_IF_FAILED(
			Details::g_cropEffect->SetValue(
				D2D1_CROP_PROP_RECT,
				cropRect
			)
		);

		if (memcmp(matrix.m, D2D1::IdentityMatrix().m, sizeof(matrix.m)) != 0)
		{
			// not identity matrix, stop rendering it
			// currently not support

			return S_OK;
		}
		context->PushLayer(
			D2D1::LayerParameters1(
				bounds,
				geometry,
				D2D1_ANTIALIAS_MODE_ALIASED,
				D2D1::IdentityMatrix(),
				reflectionIntensity,
				nullptr,
				D2D1_LAYER_OPTIONS1_NONE
			),
			nullptr
		);
		// do some rendering job
		context->DrawImage(
			Details::g_cropEffect.get(),
			point,
			cropRect
		);
		context->PopLayer();

		return S_OK;
	}
	inline void Reset()
	{
		Details::g_reflectionBitmapSourceEffect = nullptr;
		Details::g_cropEffect = nullptr;
		Details::g_reflectionTextureWidth = Details::g_reflectionTextureHeight = 0;
	}
}