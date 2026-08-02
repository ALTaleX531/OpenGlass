#include "pch.h"
#include "Shared.hpp"
#include "uDWMProjection.hpp"
#include "ReflectionRealizer.hpp"

using namespace OpenGlass;

HRESULT CReflectionRealizer::LoadTexture(ID2D1DeviceContext* context)
{
	winrt::com_ptr<IStream> stream{ nullptr };
	if (
		Shared::g_reflectionTexturePath.empty() ||
		PathIsRelativeW(Shared::g_reflectionTexturePath.data()) ||
		PathIsNetworkPathW(Shared::g_reflectionTexturePath.data()) ||
		!PathFileExistsW(Shared::g_reflectionTexturePath.data())
	)
	{
		LOG_HR_IF_MSG(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !Shared::g_reflectionTexturePath.empty(), "invalid reflection texture path");
		std::span<const UCHAR> textureBytes{};
		RETURN_IF_FAILED(Util::GetResDataView(textureBytes, IDB_REFLECTION, wil::GetModuleInstanceHandle(), L"PNG"));

		stream = { SHCreateMemStream(textureBytes.data(), static_cast<UINT>(textureBytes.size_bytes())), winrt::take_ownership_from_abi };
	}
	else
	{
		wil::unique_hfile file{ CreateFileW(Shared::g_reflectionTexturePath.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0) };
		RETURN_LAST_ERROR_IF(!file.is_valid());

		LARGE_INTEGER fileSize{};
		RETURN_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file.get(), &fileSize));

		auto buffer{ std::make_unique<BYTE[]>(static_cast<size_t>(fileSize.QuadPart)) };
		RETURN_IF_WIN32_BOOL_FALSE(ReadFile(file.get(), buffer.get(), static_cast<DWORD>(fileSize.QuadPart), nullptr, nullptr));
		stream = { SHCreateMemStream(buffer.get(), static_cast<UINT>(fileSize.QuadPart)), winrt::take_ownership_from_abi };
	}
	RETURN_HR_IF_NULL(E_OUTOFMEMORY, stream);

	winrt::com_ptr<IWICImagingFactory> wicFactory{ nullptr };
	wicFactory.copy_from(uDWM::CDesktopManager::GetInstance()->GetWICFactory());
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
		context->CreateBitmapFromWicBitmap(
			wicConverter.get(),
			D2D1::BitmapProperties1(
				D2D1_BITMAP_OPTIONS_NONE,
				D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
			),
			m_reflectionBitmap.put()
		)
	);

	return S_OK;
}

HRESULT CReflectionRealizer::Render(
	ID2D1DeviceContext* context,
	const std::span<const D2D1_RECT_F>& rectangles,
	const ReflectionContext& reflectionContext
)
{
	if (!m_reflectionBitmap)
	{
		RETURN_IF_FAILED(LoadTexture(context));
	}
	/*winrt::com_ptr<ID2D1DeviceContext3> contextForSpriteBatch{};
	RETURN_IF_FAILED(context->QueryInterface(contextForSpriteBatch.put()));
	if (!m_spriteBatch)
	{
		RETURN_IF_FAILED(contextForSpriteBatch->CreateSpriteBatch(m_spriteBatch.put()));
	}*/

	const auto worldTransform3D = reflectionContext.worldTransform->GetD3DMatrix();
	auto worldTransform2DInversed = reflectionContext.worldTransform->GetD2DMatrix();
	D2D1InvertMatrix(&worldTransform2DInversed);

	const auto reflectionBitmapSize = m_reflectionBitmap->GetSize();
	const D2D1_SIZE_F viewportSize
	{
		wil::rect_width(*reflectionContext.viewport),
		wil::rect_height(*reflectionContext.viewport)
	};
	for (auto subRectangle : rectangles)
	{
		subRectangle = RectF::TransformRect(subRectangle, worldTransform2DInversed);
		if (RectF::IntersectUnsafe(subRectangle, *reflectionContext.viewport))
		{
			D2D1_RECT_F sourceRectangle
			{
				(subRectangle.left - reflectionContext.viewport->left) / viewportSize.width * reflectionBitmapSize.width,
				(subRectangle.top - reflectionContext.viewport->top) / viewportSize.height * reflectionBitmapSize.height,
				(subRectangle.right - reflectionContext.viewport->left) / viewportSize.width * reflectionBitmapSize.width,
				(subRectangle.bottom - reflectionContext.viewport->top) / viewportSize.height * reflectionBitmapSize.height,
			};
			context->DrawBitmap(
				m_reflectionBitmap.get(),
				subRectangle,
				reflectionContext.opacity,
				D2D1_INTERPOLATION_MODE_LINEAR,
				&sourceRectangle,
				&worldTransform3D
			);
		}
	}

	/*if (rectangles.size() == 1)
	{
		auto subRectangle = RectF::TransformRect(rectangles[0], worldTransform2DInversed);
		if (RectF::IntersectUnsafe(subRectangle, viewport))
		{
			D2D1_RECT_F sourceRectangle
			{
				(subRectangle.left - reflectionContext.viewport->left) / viewportSize.width * reflectionBitmapSize.width,
				(subRectangle.top - reflectionContext.viewport->top) / viewportSize.height * reflectionBitmapSize.height,
				(subRectangle.right - reflectionContext.viewport->left) / viewportSize.width * reflectionBitmapSize.width,
				(subRectangle.bottom - reflectionContext.viewport->top) / viewportSize.height * reflectionBitmapSize.height,
			};
			context->DrawBitmap(
				m_reflectionBitmap.get(),
				subRectangle,
				opacity,
				D2D1_INTERPOLATION_MODE_LINEAR,
				&sourceRectangle,
				&worldTransform3D
			);
		}
	}
	else
	{
		bool ignoreLayer{ opacity == 1.f };
		if (!ignoreLayer)
		{
			context->PushLayer(
				D2D1::LayerParameters1(
					D2D1::InfiniteRect(),
					nullptr,
					D2D1_ANTIALIAS_MODE_ALIASED,
					D2D1::IdentityMatrix(),
					opacity,
					nullptr,
					D2D1_LAYER_OPTIONS1_NONE
				),
				nullptr
			);
		}

		struct CSpriteInfo
		{
			D2D1_RECT_F destinationRectangle;
			D2D1_RECT_U sourceRectangle;
		};
		UINT32 spriteCount{};
		auto spriteInfoArray = std::make_unique_for_overwrite<CSpriteInfo[]>(rectangles.size());
		for (size_t i = 0; i < rectangles.size(); i++)
		{
			auto subRectangle = RectF::TransformRect(rectangles[i], worldTransform2DInversed);
			if (RectF::IntersectUnsafe(subRectangle, viewport))
			{
				spriteInfoArray[spriteCount].destinationRectangle = subRectangle;
				spriteInfoArray[spriteCount].sourceRectangle =
				{
					static_cast<UINT32>(std::round((subRectangle.left - reflectionContext.viewport->left) / viewportSize.width * reflectionBitmapSize.width)),
					static_cast<UINT32>(std::round((subRectangle.top - reflectionContext.viewport->top) / viewportSize.height * reflectionBitmapSize.height)),
					static_cast<UINT32>(std::round((subRectangle.right - reflectionContext.viewport->left) / viewportSize.width * reflectionBitmapSize.width)),
					static_cast<UINT32>(std::round((subRectangle.bottom - reflectionContext.viewport->top) / viewportSize.height * reflectionBitmapSize.height)),
				};

				spriteCount += 1;
			}
		}
		const auto worldTransform2D = worldTransform->GetD2DMatrix();
		m_spriteBatch->AddSprites(
			spriteCount,
			&spriteInfoArray[0].destinationRectangle,
			&spriteInfoArray[0].sourceRectangle,
			nullptr,
			&worldTransform2D,
			sizeof(CSpriteInfo),
			sizeof(CSpriteInfo),
			0U,
			0U
		);

		const auto primitiveBlend = contextForSpriteBatch->GetPrimitiveBlend();
		const auto antialiasMode = contextForSpriteBatch->GetAntialiasMode();
		if (!ignoreLayer)
		{
			contextForSpriteBatch->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
		}
		contextForSpriteBatch->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		contextForSpriteBatch->DrawSpriteBatch(m_spriteBatch.get(), m_reflectionBitmap.get());
		contextForSpriteBatch->SetAntialiasMode(antialiasMode);
		if (!ignoreLayer)
		{
			contextForSpriteBatch->SetPrimitiveBlend(primitiveBlend);
		}
		m_spriteBatch->Clear();

		if (!ignoreLayer)
		{
			context->PopLayer();
		}
	}*/

	return S_OK;
}
