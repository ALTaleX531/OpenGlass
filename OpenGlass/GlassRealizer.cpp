#include "pch.h"
#include "Util.hpp"
#include "D2DPrivates.hpp"
#include "GlassRealizer.hpp"
#include "BackdropCopy.hpp"
#include "AeroEffect.hpp"
#include "BlurEffect.hpp"
#include "DxgiPrivates.hpp"

using namespace OpenGlass;

struct CGlassRealizer::RenderTargetInfo
{
	winrt::com_ptr<ID2D1Bitmap1> m_bitmap{};
	winrt::com_ptr<ID3D11Texture2D> m_texture{};
	winrt::com_ptr<ID3D11Device> m_device{};
	D3D11_TEXTURE2D_DESC m_desc{};
	BOOL m_hwProtectionEnabled{ FALSE };
};

bool CGlassRealizer::EnsureGlassEffect(Shared::GlassType type)
{
	if (m_glassType != type)
	{
		m_batch.ResetGraph();
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
	return bool(m_glassEffect);
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

	RenderTargetInfo targetInfo{};
	RETURN_IF_FAILED(Util::GetTargetBitmapFromD2DContext(context, targetInfo.m_bitmap));
	const auto targetSize = targetInfo.m_bitmap->GetSize();

	D2D1_RECT_F geometryBounds{};
	RETURN_IF_FAILED(geometry->GetBounds(nullptr, &geometryBounds));

	const auto expansion = GetBlurExpansion(params.blurAmount);
	D2D1_RECT_F samplingWorldBounds
	{
		std::max(drawingWorldBounds.left - expansion, 0.f),
		std::max(drawingWorldBounds.top - expansion, 0.f),
		std::min(drawingWorldBounds.right + expansion, targetSize.width),
		std::min(drawingWorldBounds.bottom + expansion, targetSize.height)
	};
	RectF::IntersectUnsafe(samplingWorldBounds, geometryBounds);

	auto bandBounds = drawingWorldBounds;
	RectF::IntersectUnsafe(bandBounds, D2D1::RectF(0.f, 0.f, targetSize.width, targetSize.height));
	const bool plannedBands = rectangles.size() > 1 && m_batch.PlanBands(bandBounds, rectangles);

	winrt::com_ptr<ID2D1Bitmap1> sharedAtlasBitmap{};
	winrt::com_ptr<ID2D1PrivateCompositorDeviceContext> compositorContext;
	if (
		const auto bitmapProperties = D2D1::BitmapProperties1(
			targetInfo.m_bitmap->GetOptions(),
			targetInfo.m_bitmap->GetPixelFormat()
		);
		(rectangles.size() == 1 || plannedBands) &&
		!(bitmapProperties.bitmapOptions & D2D1_BITMAP_OPTIONS_CANNOT_DRAW) &&
		SUCCEEDED(context->QueryInterface(compositorContext.put()))
	)
	{
		RETURN_IF_FAILED(
			compositorContext->CreateSharedAtlasBitmap(
				targetInfo.m_bitmap.get(),
				&bitmapProperties,
				sharedAtlasBitmap.put()
			)
		);
	}
	if (!sharedAtlasBitmap || plannedBands)
	{
		RETURN_IF_FAILED(
			Util::GetTextureFromD2DBitmap(
				targetInfo.m_bitmap.get(),
				targetInfo.m_texture
			)
		);
		RETURN_IF_FAILED(
			Util::GetDescAndHwProtectionStateFromTexture(
				targetInfo.m_texture.get(),
				targetInfo.m_desc,
				targetInfo.m_hwProtectionEnabled
			)
		);

		targetInfo.m_texture->GetDevice(targetInfo.m_device.put());
	}
	if (!sharedAtlasBitmap)
	{
		RETURN_IF_FAILED(EnsureBuffer(context, targetInfo));
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
	auto outputMatrix = m_glassEffect->GetOutputMatrix();
	if (sharedAtlasBitmap && plannedBands && outputMatrix._11 == 1.f && outputMatrix._22 == 1.f)
	{
		// Unit-scale coverage keeps native draws. Capture before those draws can
		// feed back into the source; the unchanged graph owns scale selection.
		// Rebuilding can change scale because it also depends on input image bounds.
		sharedAtlasBitmap = nullptr;
		RETURN_IF_FAILED(EnsureBuffer(context, targetInfo));
		RETURN_IF_FAILED(
			m_glassEffect->Build(
				context,
				m_buffer.m_bitmap.get(),
				samplingWorldBounds,
				static_cast<const void*>(&params)
			)
		);
		outputMatrix = m_glassEffect->GetOutputMatrix();
	}
	const bool banded = plannedBands && (outputMatrix._11 != 1.f || outputMatrix._22 != 1.f);
	if (!sharedAtlasBitmap)
	{
		CopyBackdropForEffect(targetInfo, drawingWorldBounds, samplingWorldBounds, rectangles, expansion);
	}

	if (banded)
	{
		return m_batch.Render(
			context,
			targetInfo.m_bitmap.get(),
			targetInfo.m_device.get(),
			targetInfo.m_desc,
			targetInfo.m_hwProtectionEnabled != FALSE,
			m_glassEffect.get(),
			params,
			type,
			bandBounds,
			rectangles
		);
	}

	return RenderEffect(context, targetInfo.m_bitmap.get(), drawingWorldBounds, rectangles, outputMatrix);
}

HRESULT CGlassRealizer::EnsureBuffer(ID2D1DeviceContext* context, const RenderTargetInfo& targetInfo)
{
	RETURN_IF_FAILED(
		m_buffer.Ensure(
			targetInfo.m_device.get(),
			targetInfo.m_desc.Width,
			targetInfo.m_desc.Height,
			targetInfo.m_desc,
			D3D11_BIND_SHADER_RESOURCE,
			targetInfo.m_hwProtectionEnabled
		)
	);
	RETURN_IF_FAILED(
		m_buffer.EnsureD2D(
			context,
			D2D1::BitmapProperties1(
				D2D1_BITMAP_OPTIONS_NONE,
				targetInfo.m_bitmap->GetPixelFormat()
			)
		)
	);

	return S_OK;
}

void CGlassRealizer::CopyBackdropForEffect(
	const RenderTargetInfo& targetInfo,
	const D2D1_RECT_F& bounds,
	const D2D1_RECT_F& samplingBounds,
	std::span<const D2D1_RECT_F> rectangles,
	float expansion
)
{
	m_copyRectangles.clear();
	for (auto rectangle : rectangles)
	{
		if (RectF::IntersectUnsafe(rectangle, bounds))
		{
			// Crop defines the content bounds; Border mirrors that content at the edges.
			rectangle =
			{
				rectangle.left - expansion, rectangle.top - expansion,
				rectangle.right + expansion, rectangle.bottom + expansion
			};
			if (RectF::IntersectUnsafe(rectangle, samplingBounds))
			{
				m_copyRectangles.push_back(RectF::ToRectU(rectangle));
			}
		}
	}
	CoalesceCopyRectangles(m_copyRectangles);
	winrt::com_ptr<ID3D11DeviceContext> d3dContext;
	targetInfo.m_device->GetImmediateContext(d3dContext.put());
	for (const auto copyRegionRect : m_copyRectangles)
	{
		Util::CopyTextureRegion(
			d3dContext.get(),
			targetInfo.m_texture.get(),
			m_buffer.m_texture.get(),
			copyRegionRect,
			copyRegionRect.left,
			copyRegionRect.top
		);
	}
}

HRESULT CGlassRealizer::RenderEffect(
	ID2D1DeviceContext* context,
	ID2D1Bitmap1* target,
	const D2D1_RECT_F& bounds,
	std::span<const D2D1_RECT_F> rectangles,
	const D2D1_MATRIX_3X2_F& outputMatrix
)
{
	winrt::com_ptr<ID2D1Image> outputImage{};
	m_glassEffect->GetOutput(outputImage.put());

	RETURN_HR_IF_NULL(D2DERR_UNSUPPORTED_PIXEL_FORMAT, outputImage);

	D2D1_MATRIX_3X2_F originalMatrix{};
	auto inverseMatrix = outputMatrix;
	D2D1InvertMatrix(&inverseMatrix);
	context->GetTransform(&originalMatrix);
	const auto restoreMatrix = wil::scope_exit([&]
	{
		context->SetTransform(originalMatrix);
	});
	D2D1_RENDERING_CONTROLS controls;
	context->GetRenderingControls(&controls);
	const auto restoreControls = wil::scope_exit([&]
	{
		context->SetRenderingControls(controls);
	});
	const auto size = target->GetPixelSize();
	if (controls.tileSize.width < size.width || controls.tileSize.height < size.height)
	{
		auto enlarged = controls;
		enlarged.tileSize = size;
		context->SetRenderingControls(enlarged);
	}

	const auto offset = m_glassEffect->GetOutputOffset();
	context->SetTransform(outputMatrix);
	for (auto rectangle : rectangles)
	{
		if (RectF::IntersectUnsafe(rectangle, bounds))
		{
			const auto transformed = RectF::TransformRect(rectangle, inverseMatrix);
			context->DrawImage(
				outputImage.get(),
				D2D1::Point2F(transformed.left, transformed.top),
				D2D1::RectF(
					transformed.left + offset.x, transformed.top + offset.y,
					transformed.right + offset.x, transformed.bottom + offset.y
				),
				D2D1_INTERPOLATION_MODE_LINEAR,
				D2D1_COMPOSITE_MODE_BOUNDED_SOURCE_COPY
			);
		}
	}
	return S_OK;
}

float CGlassRealizer::GetBlurExpansion(float blurAmount)
{
	return std::ceil(blurAmount * 3.f);
}
