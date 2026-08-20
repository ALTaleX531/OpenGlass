#include "pch.h"
#include "Util.hpp"
#include "uDWMProjection.hpp"
#include "GlassRealizer.hpp"
#include "AeroEffect.hpp"
#include "BlurEffect.hpp"
#include "DxgiPrivates.hpp"

using namespace OpenGlass;

bool CGlassRealizer::EnsureGlassEffect(Shared::GlassType type)
{
	if (m_glassType != type)
	{
		m_glassType = type;
		switch (m_glassType)
		{
		case Shared::GlassType::Blur:
		{
			m_glassEffect = winrt::make<CBlurEffect>();
			break;
		}
		case Shared::GlassType::Aero:
		{
			m_glassEffect = winrt::make<CAeroEffect>();
			break;
		}
		default:
			m_glassEffect = nullptr;
			break;
		}
	}
	if (!m_glassEffect)
	{
		return false;
	}

	return true;
}

HRESULT CGlassRealizer::Render(
	ID2D1DeviceContext* context,
	ID2D1Geometry* geometry,
	const D2D1_RECT_F& drawingWorldBounds,
	const std::span<const D2D1_RECT_F>& rectangles,
	const CAeroParams& params,
	Shared::GlassType type
)
{
	if (!EnsureGlassEffect(type))
	{
		return S_OK;
	}

	winrt::com_ptr<ID2D1Bitmap1> renderTargetBitmap{ nullptr };
	RETURN_IF_FAILED(Util::GetTargetBitmapFromD2DContext(context, renderTargetBitmap));

	D2D1_RECT_F geometryBounds{};
	RETURN_IF_FAILED(geometry->GetBounds(nullptr, &geometryBounds));

	const auto targetSize = renderTargetBitmap->GetSize();
	const auto expansion = GetBlurExpansion(params.blurAmount);
	D2D1_RECT_F samplingWorldBounds
	{
		std::max(drawingWorldBounds.left - expansion, 0.f),
		std::max(drawingWorldBounds.top - expansion, 0.f),
		std::min(drawingWorldBounds.right + expansion, targetSize.width),
		std::min(drawingWorldBounds.bottom + expansion, targetSize.height)
	};
	RectF::IntersectUnsafe(samplingWorldBounds, geometryBounds);

	winrt::com_ptr<ID2D1PrivateCompositorDeviceContext> compositorDeviceContext{};
	winrt::com_ptr<ID2D1Bitmap1> sharedAtlasBitmap{};

	if (
		const auto bitmapProperties = D2D1::BitmapProperties1(
			renderTargetBitmap->GetOptions(),
			renderTargetBitmap->GetPixelFormat()
		);
		rectangles.size() == 1 &&
		!(bitmapProperties.bitmapOptions & D2D1_BITMAP_OPTIONS_CANNOT_DRAW) &&
		SUCCEEDED(context->QueryInterface(compositorDeviceContext.put()))
	)
	{
		RETURN_IF_FAILED(context->QueryInterface(compositorDeviceContext.put()));
		RETURN_IF_FAILED(
			compositorDeviceContext->CreateSharedAtlasBitmap(
				renderTargetBitmap.get(),
				&bitmapProperties,
				sharedAtlasBitmap.put()
			)
		);
	}
	if (!sharedAtlasBitmap)
	{
		BOOL hwProtectionEnabled = FALSE;
		winrt::com_ptr<ID3D11Texture2D> renderTargetTexture{ nullptr };
		D3D11_TEXTURE2D_DESC renderTargetDesc{};

		RETURN_IF_FAILED(
			Util::GetTextureFromD2DBitmap(
				renderTargetBitmap.get(),
				renderTargetTexture
			)
		);
		RETURN_IF_FAILED(
			Util::GetDescAndHwProtectionStateFromTexture(
				renderTargetTexture.get(),
				renderTargetDesc,
				hwProtectionEnabled
			)
		);

		winrt::com_ptr<ID3D11Device> d3dDevice{};
		renderTargetTexture->GetDevice(d3dDevice.put());
		winrt::com_ptr<ID3D11DeviceContext> d3dContext{};
		d3dDevice->GetImmediateContext(d3dContext.put());

		RETURN_IF_FAILED(
			m_buffer.Ensure(
				d3dDevice.get(),
				renderTargetDesc.Width,
				renderTargetDesc.Height,
				renderTargetDesc,
				D3D11_BIND_SHADER_RESOURCE,
				hwProtectionEnabled
			)
		);
		RETURN_IF_FAILED(
			m_buffer.EnsureD2D(
				context,
				D2D1::BitmapProperties1(
					D2D1_BITMAP_OPTIONS_NONE,
					renderTargetBitmap->GetPixelFormat()
				)
			)
		);

		if (rectangles.size() <= 12)
		{
			for (auto subRectangle : rectangles)
			{
				if (RectF::IntersectUnsafe(subRectangle, drawingWorldBounds))
				{
					subRectangle = 
					{
						std::max(subRectangle.left - expansion, 0.f),
						std::max(subRectangle.top - expansion, 0.f),
						std::min(subRectangle.right + expansion, targetSize.width),
						std::min(subRectangle.bottom + expansion, targetSize.height)
					};
					RectF::IntersectUnsafe(subRectangle, geometryBounds);

					const auto copyRegionRect = RectF::ToRectU(subRectangle);
					Util::CopyTextureRegion(
						d3dContext.get(),
						renderTargetTexture.get(),
						m_buffer.m_texture.get(),
						copyRegionRect,
						copyRegionRect.left,
						copyRegionRect.top
					);
				}
			}
		}
		else
		{
			const auto copyRegionRect = RectF::ToRectU(samplingWorldBounds);
			Util::CopyTextureRegion(
				d3dContext.get(),
				renderTargetTexture.get(),
				m_buffer.m_texture.get(),
				copyRegionRect,
				copyRegionRect.left,
				copyRegionRect.top
			);
		}

		d3dContext->Flush();
	}

	const auto cleanupCustomEffect = wil::scope_exit([this]
	{
		m_glassEffect->Reset();
	});

	RETURN_IF_FAILED(
		m_glassEffect->Build(
			context,
			sharedAtlasBitmap ? sharedAtlasBitmap.get() : m_buffer.m_bitmap.get(),
			samplingWorldBounds,
			static_cast<const void*>(&params)
		)
	);
	winrt::com_ptr<ID2D1Image> outputImage{};
	m_glassEffect->GetOutput(outputImage.put());

	RETURN_HR_IF_NULL(D2DERR_UNSUPPORTED_PIXEL_FORMAT, outputImage);

	D2D1_MATRIX_3X2_F originalMatrix, outputMatrix = m_glassEffect->GetOutputMatrix();
	context->GetTransform(&originalMatrix);
	context->SetTransform(outputMatrix);
	D2D1_RENDERING_CONTROLS renderingControls{};
	context->GetRenderingControls(&renderingControls);
	if (const auto targetPixelSize = renderTargetBitmap->GetPixelSize(); renderingControls.tileSize.width < targetPixelSize.width || renderingControls.tileSize.height < targetPixelSize.height)
	{
		renderingControls.tileSize = targetPixelSize;
		context->SetRenderingControls(renderingControls);
	}

	D2D1InvertMatrix(&outputMatrix);

	const auto offset = m_glassEffect->GetOutputOffset();
	for (auto subRectangle : rectangles)
	{
		if (RectF::IntersectUnsafe(subRectangle, drawingWorldBounds))
		{
			auto transformedSubRectangle = RectF::TransformRect(subRectangle, outputMatrix);

			context->DrawImage(
				outputImage.get(),
				D2D1::Point2F(
					transformedSubRectangle.left,
					transformedSubRectangle.top
				),
				D2D1::RectF(
					transformedSubRectangle.left + offset.x,
					transformedSubRectangle.top + offset.y,
					transformedSubRectangle.right + offset.x,
					transformedSubRectangle.bottom + offset.y
				),
				D2D1_INTERPOLATION_MODE_LINEAR,
				D2D1_COMPOSITE_MODE_BOUNDED_SOURCE_COPY
			);
		}
	}

	context->SetTransform(originalMatrix);

	return S_OK;
}

float CGlassRealizer::GetBlurExpansion(float blurAmount)
{
	return std::ceil(blurAmount * 3.f);
}
