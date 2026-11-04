#include "pch.h"
#include "GlassEffectBatch.hpp"
#include "BackdropCopy.hpp"
#include "RectF.hpp"

using namespace OpenGlass;

void CGlassEffectBatch::ResetGraph()
{
	m_horizontalBlurEffect = nullptr;
	m_verticalBlurEffect = nullptr;
	m_preparedInputImage = nullptr;
	m_cachedBlurAmount = -1.f;
}

HRESULT CGlassEffectBatch::Bind(ID2D1Image* effectOutputImage, Shared::GlassType glassType, float tintAlpha)
{
	if (m_verticalBlurEffect)
	{
		return S_OK;
	}
	winrt::com_ptr<ID2D1Effect> outputEffect;
	RETURN_IF_FAILED(effectOutputImage->QueryInterface(outputEffect.put()));
	// CAeroEffect: colorization -> Y -> X -> prepared input.
	// CBlurEffect: optional tint composite -> Y -> X -> prepared input.
	// These views belong to the current effect; replacement calls ResetGraph.
	if (glassType == Shared::GlassType::Aero || tintAlpha != 0.f)
	{
		winrt::com_ptr<ID2D1Image> blurOutputImage;
		outputEffect->GetInput(0, blurOutputImage.put());
		RETURN_IF_FAILED(blurOutputImage->QueryInterface(outputEffect.put()));
	}
	winrt::com_ptr<ID2D1Image> horizontalBlurOutputImage, preparedInputImage;
	winrt::com_ptr<ID2D1Effect> horizontalBlurEffect;
	outputEffect->GetInput(0, horizontalBlurOutputImage.put());
	RETURN_IF_FAILED(horizontalBlurOutputImage->QueryInterface(horizontalBlurEffect.put()));
	horizontalBlurEffect->GetInput(0, preparedInputImage.put());
	m_verticalBlurEffect = std::move(outputEffect);
	m_horizontalBlurEffect = std::move(horizontalBlurEffect);
	m_preparedInputImage = std::move(preparedInputImage);
	return S_OK;
}

HRESULT CGlassEffectBatch::KernelInputMapping::Update(ID2D1DeviceContext* stageContext, ID2D1Effect* blurKernel)
{
	const auto unitOutputBounds = D2D1::RectF(0.f, 0.f, 1.f, 1.f);
	const auto shiftedOutputBounds = D2D1::RectF(1.f, 1.f, 2.f, 2.f);
	const D2D1_EFFECT_INPUT_DESCRIPTION inputDescription{ blurKernel, 0, D2D1::RectF(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX) };
	D2D1_RECT_F shiftedInputBounds;
	RETURN_IF_FAILED(stageContext->GetEffectRequiredInputRectangles(blurKernel, &unitOutputBounds, &inputDescription, &m_unitInputBounds, 1));
	RETURN_IF_FAILED(stageContext->GetEffectRequiredInputRectangles(blurKernel, &shiftedOutputBounds, &inputDescription, &shiftedInputBounds, 1));
	m_inputScale = { shiftedInputBounds.left - m_unitInputBounds.left, shiftedInputBounds.top - m_unitInputBounds.top };
	return S_OK;
}

D2D1_RECT_F CGlassEffectBatch::KernelInputMapping::Map(const D2D1_RECT_F& outputBounds) const
{
	return
	{
		std::floor(m_unitInputBounds.left + outputBounds.left * m_inputScale.x),
		std::floor(m_unitInputBounds.top + outputBounds.top * m_inputScale.y),
		std::ceil(m_unitInputBounds.right + (outputBounds.right - 1.f) * m_inputScale.x),
		std::ceil(m_unitInputBounds.bottom + (outputBounds.bottom - 1.f) * m_inputScale.y)
	};
}

D2D1_RECT_F CGlassEffectBatch::ToKernelOutput(const D2D1_RECT_F& effectBounds, D2D1_POINT_2F effectOutputOffset)
{
	// Output pixels touched by a scaled rectangle, plus one texel on each side for interpolation.
	return
	{
		std::floor(effectBounds.left + effectOutputOffset.x) - 1.f, std::floor(effectBounds.top + effectOutputOffset.y) - 1.f,
		std::ceil(effectBounds.right + effectOutputOffset.x) + 1.f, std::ceil(effectBounds.bottom + effectOutputOffset.y) + 1.f
	};
}

UINT CGlassEffectBatch::ScratchBuffer::GrowCapacity(UINT currentCapacity, UINT requiredSize, UINT targetSize)
{
	return requiredSize <= currentCapacity ? currentCapacity :
		std::min(std::max(requiredSize, targetSize), std::max(requiredSize, currentCapacity * 2));
}

HRESULT CGlassEffectBatch::ScratchBuffer::Ensure(
	ID2D1DeviceContext* stageContext, ID3D11Device* device,
	const D3D11_TEXTURE2D_DESC& targetDesc, bool hwProtectionEnabled,
	D2D1_PIXEL_FORMAT pixelFormat, D2D1_SIZE_U requiredSize
)
{
	// Bound spare capacity by the target, while retaining the kernel's full reach.
	const auto currentSize = m_buffer.m_bitmap ? m_buffer.m_bitmap->GetPixelSize() : D2D1_SIZE_U{};
	RETURN_IF_FAILED(
		m_buffer.Ensure(
			device,
			GrowCapacity(currentSize.width, requiredSize.width, targetDesc.Width),
			GrowCapacity(currentSize.height, requiredSize.height, targetDesc.Height),
			targetDesc,
			D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE,
			hwProtectionEnabled
		)
	);
	if (m_buffer.m_bitmap)
	{
		const auto currentFormat = m_buffer.m_bitmap->GetPixelFormat();
		if (currentFormat.format == pixelFormat.format && currentFormat.alphaMode == pixelFormat.alphaMode)
		{
			return S_OK;
		}
	}
	return m_buffer.EnsureD2D(stageContext, D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET, pixelFormat, 96.f, 96.f));
}

HRESULT CGlassEffectBatch::ScratchBuffer::GetView(ID2D1DeviceContext* stageContext, winrt::com_ptr<ID2D1Image>& imageView)
{
	if (m_imageOrigin.x == 0.f && m_imageOrigin.y == 0.f)
	{
		imageView.copy_from(m_buffer.m_bitmap.get());
		return S_OK;
	}
	// Integer translation retains the absolute sample grid for negative footprints.
	if (!m_translationEffect)
	{
		RETURN_IF_FAILED(stageContext->CreateEffect(CLSID_D2D12DAffineTransform, m_translationEffect.put()));
		RETURN_IF_FAILED(m_translationEffect->SetValue(D2D1_2DAFFINETRANSFORM_PROP_INTERPOLATION_MODE, D2D1_2DAFFINETRANSFORM_INTERPOLATION_MODE_NEAREST_NEIGHBOR));
		RETURN_IF_FAILED(m_translationEffect->SetValue(D2D1_2DAFFINETRANSFORM_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD));
	}
	RETURN_IF_FAILED(m_translationEffect->SetValue(D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM_MATRIX, D2D1::Matrix3x2F::Translation(m_imageOrigin.x, m_imageOrigin.y)));
	m_translationEffect->SetInput(0, m_buffer.m_bitmap.get());
	m_translationEffect->GetOutput(imageView.put());
	return S_OK;
}

void CGlassEffectBatch::Plan(const D2D1_MATRIX_3X2_F& worldToEffectTransform, D2D1_POINT_2F effectOutputOffset)
{
	m_effectOutputRanges.clear();
	m_horizontalBlurRanges.clear();
	m_preparedInputRanges.clear();
	// Keep non-negative footprints in native coordinates. Only negative reach
	// needs a translated bitmap view; positive-origin views add effect overhead.
	D2D1_RECT_F preparedInputBounds{}, horizontalBlurBounds{};
	float compositionRight{}, compositionBottom{};
	for (const auto worldBand : m_worldBands)
	{
		const auto effectOutputRange = ToKernelOutput(RectF::TransformRect(worldBand, worldToEffectTransform), effectOutputOffset);
		// Native kernel queries include the interpolation neighborhood in the input reach.
		const auto horizontalBlurRange = m_verticalInputMapping.Map(D2D1::RectF(
			effectOutputRange.left - 1.f, effectOutputRange.top - 1.f,
			effectOutputRange.right + 1.f, effectOutputRange.bottom + 1.f));
		const auto preparedInputRange = m_horizontalInputMapping.Map(horizontalBlurRange);
		m_effectOutputRanges.push_back(effectOutputRange);
		m_horizontalBlurRanges.push_back(horizontalBlurRange);
		m_preparedInputRanges.push_back(preparedInputRange);
		preparedInputBounds =
		{
			std::min(preparedInputBounds.left, preparedInputRange.left),
			std::min(preparedInputBounds.top, preparedInputRange.top),
			std::max(preparedInputBounds.right, preparedInputRange.right),
			std::max(preparedInputBounds.bottom, preparedInputRange.bottom)
		};
		horizontalBlurBounds =
		{
			std::min(horizontalBlurBounds.left, horizontalBlurRange.left),
			std::min(horizontalBlurBounds.top, horizontalBlurRange.top),
			std::max(horizontalBlurBounds.right, horizontalBlurRange.right),
			std::max(horizontalBlurBounds.bottom, horizontalBlurRange.bottom)
		};
		compositionRight = std::max(compositionRight, effectOutputRange.right + 2.f);
		compositionBottom = std::max(compositionBottom, effectOutputRange.bottom + 2.f);
	}
	m_compositionBuffer.m_imageOrigin = { preparedInputBounds.left, preparedInputBounds.top };
	m_horizontalBlurBuffer.m_imageOrigin = { horizontalBlurBounds.left, horizontalBlurBounds.top };
	m_compositionRequiredSize =
	{
		UINT(std::max(preparedInputBounds.right - preparedInputBounds.left, compositionRight)),
		UINT(std::max(preparedInputBounds.bottom - preparedInputBounds.top, compositionBottom))
	};
	m_horizontalBlurRequiredSize = { UINT(horizontalBlurBounds.right - horizontalBlurBounds.left), UINT(horizontalBlurBounds.bottom - horizontalBlurBounds.top) };
}

HRESULT CGlassEffectBatch::Render(
	ID2D1DeviceContext* destinationContext,
	ID2D1Bitmap1* destinationBitmap,
	ID3D11Device* device,
	const D3D11_TEXTURE2D_DESC& targetDesc,
	bool hwProtectionEnabled,
	IRenderingEffect* glassEffect,
	const CAeroParams& aeroParams,
	Shared::GlassType glassType,
	const D2D1_RECT_F& drawingWorldBounds,
	std::span<const D2D1_RECT_F> drawingRectangles
)
{
	winrt::com_ptr<ID2D1Image> effectOutputImage;
	glassEffect->GetOutput(effectOutputImage.put());
	RETURN_IF_FAILED(Bind(effectOutputImage.get(), glassType, aeroParams.color.a));
	if (!m_stageContext)
	{
		winrt::com_ptr<ID2D1Device> d2dDevice;
		destinationContext->GetDevice(d2dDevice.put());
		RETURN_IF_FAILED(d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, m_stageContext.put()));
		m_stageContext->SetUnitMode(D2D1_UNIT_MODE_PIXELS);
		m_stageContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		m_stageContext->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
	}
	const auto outputTransform = glassEffect->GetOutputMatrix();
	const auto outputOffset = glassEffect->GetOutputOffset();
	const D2D1_SIZE_F outputScale{ outputTransform._11, outputTransform._22 };
	if (
		m_cachedBlurAmount != aeroParams.blurAmount || m_cachedBlurOptimization != aeroParams.optimization ||
		m_cachedOutputScale.width != outputScale.width || m_cachedOutputScale.height != outputScale.height
	)
	{
		RETURN_IF_FAILED(m_horizontalInputMapping.Update(m_stageContext.get(), m_horizontalBlurEffect.get()));
		RETURN_IF_FAILED(m_verticalInputMapping.Update(m_stageContext.get(), m_verticalBlurEffect.get()));
		m_cachedBlurAmount = aeroParams.blurAmount;
		m_cachedBlurOptimization = aeroParams.optimization;
		m_cachedOutputScale = outputScale;
	}
	auto worldToEffectTransform = outputTransform;
	D2D1InvertMatrix(&worldToEffectTransform);
	Plan(worldToEffectTransform, outputOffset);
	const auto targetFormat = destinationBitmap->GetPixelFormat();
	RETURN_IF_FAILED(m_compositionBuffer.Ensure(m_stageContext.get(), device, targetDesc, hwProtectionEnabled, targetFormat, m_compositionRequiredSize));
	RETURN_IF_FAILED(m_horizontalBlurBuffer.Ensure(m_stageContext.get(), device, targetDesc, hwProtectionEnabled, targetFormat, m_horizontalBlurRequiredSize));
	const auto drawResult = DrawStages(destinationContext, destinationBitmap, effectOutputImage.get());
	RestoreGraph();
	RETURN_IF_FAILED(drawResult);
	if (!m_compositionBrush)
	{
		RETURN_IF_FAILED(
			destinationContext->CreateBitmapBrush(
				m_compositionBuffer.m_buffer.m_bitmap.get(),
				D2D1::BitmapBrushProperties1(
					D2D1_EXTEND_MODE_CLAMP,
					D2D1_EXTEND_MODE_CLAMP,
					D2D1_INTERPOLATION_MODE_LINEAR
				),
				m_compositionBrush.put()
			)
		);
	}
	m_compositionBrush->SetBitmap(m_compositionBuffer.m_buffer.m_bitmap.get());
	m_compositionBrush->SetTransform(D2D1::Matrix3x2F::Translation(-outputOffset.x - 2.f, -outputOffset.y - 2.f) * outputTransform);
	D2D1_MATRIX_3X2_F originalTransform;
	destinationContext->GetTransform(&originalTransform);
	const auto originalBlend = destinationContext->GetPrimitiveBlend();
	const auto originalAntialiasMode = destinationContext->GetAntialiasMode();
	destinationContext->SetTransform(D2D1::IdentityMatrix());
	destinationContext->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
	destinationContext->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
	destinationContext->PushAxisAlignedClip(drawingWorldBounds, D2D1_ANTIALIAS_MODE_ALIASED);
	// Compose through the caller's existing geometry/curve mask.
	for (auto drawingRectangle : drawingRectangles)
	{
		if (RectF::IntersectUnsafe(drawingRectangle, drawingWorldBounds))
		{
			destinationContext->FillRectangle(drawingRectangle, m_compositionBrush.get());
		}
	}
	destinationContext->PopAxisAlignedClip();
	destinationContext->SetTransform(originalTransform);
	destinationContext->SetPrimitiveBlend(originalBlend);
	destinationContext->SetAntialiasMode(originalAntialiasMode);
	return S_OK;
}

HRESULT CGlassEffectBatch::DrawStages(ID2D1DeviceContext* destinationContext, ID2D1Bitmap1* destinationBitmap, ID2D1Image* effectOutputImage)
{
	winrt::com_ptr<ID2D1Image> preparedInputView, horizontalBlurView;
	RETURN_IF_FAILED(m_compositionBuffer.GetView(m_stageContext.get(), preparedInputView));
	RETURN_IF_FAILED(m_horizontalBlurBuffer.GetView(m_stageContext.get(), horizontalBlurView));
	D2D1_RENDERING_CONTROLS renderingControls;
	destinationContext->GetRenderingControls(&renderingControls);
	renderingControls.tileSize = destinationBitmap->GetPixelSize();
	m_stageContext->SetRenderingControls(renderingControls);
	// The parent context keeps DWM's private clip, target and depth throughout.
	m_stageContext->SetTarget(m_compositionBuffer.m_buffer.m_bitmap.get());
	m_stageContext->SetTransform(D2D1::Matrix3x2F::Translation(-m_compositionBuffer.m_imageOrigin.x, -m_compositionBuffer.m_imageOrigin.y));
	m_stageContext->BeginDraw();
	DrawRanges(m_preparedInputImage.get(), m_preparedInputRanges, {});
	m_horizontalBlurEffect->SetInput(0, preparedInputView.get());
	m_stageContext->SetTarget(m_horizontalBlurBuffer.m_buffer.m_bitmap.get());
	m_stageContext->SetTransform(D2D1::Matrix3x2F::Translation(-m_horizontalBlurBuffer.m_imageOrigin.x, -m_horizontalBlurBuffer.m_imageOrigin.y));
	winrt::com_ptr<ID2D1Image> horizontalBlurOutputImage;
	m_horizontalBlurEffect->GetOutput(horizontalBlurOutputImage.put());
	DrawRanges(horizontalBlurOutputImage.get(), m_horizontalBlurRanges, {});
	// Horizontal blur has consumed the prepared input. The vertical/colorization stage
	// reads the horizontal-blur buffer and can overwrite the composition buffer.
	m_verticalBlurEffect->SetInput(0, horizontalBlurView.get());
	m_stageContext->SetTarget(m_compositionBuffer.m_buffer.m_bitmap.get());
	m_stageContext->SetTransform(D2D1::IdentityMatrix());
	DrawRanges(effectOutputImage, m_effectOutputRanges, D2D1::Point2F(2.f, 2.f));
	const auto drawResult = m_stageContext->EndDraw();
	// Detach before the parent samples the composition buffer.
	m_stageContext->SetTarget(nullptr);
	return drawResult;
}

void CGlassEffectBatch::DrawRanges(ID2D1Image* sourceImage, std::span<const D2D1_RECT_F> sourceRanges, D2D1_POINT_2F destinationOffset)
{
	for (const auto sourceRange : sourceRanges)
	{
		m_stageContext->DrawImage(
			sourceImage,
			D2D1::Point2F(sourceRange.left + destinationOffset.x, sourceRange.top + destinationOffset.y), sourceRange,
			D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR, D2D1_COMPOSITE_MODE_BOUNDED_SOURCE_COPY
		);
	}
}

void CGlassEffectBatch::RestoreGraph()
{
	m_horizontalBlurEffect->SetInput(0, m_preparedInputImage.get());
	m_verticalBlurEffect->SetInputEffect(0, m_horizontalBlurEffect.get());
	if (m_compositionBuffer.m_translationEffect)
	{
		m_compositionBuffer.m_translationEffect->SetInput(0, nullptr);
	}
	if (m_horizontalBlurBuffer.m_translationEffect)
	{
		m_horizontalBlurBuffer.m_translationEffect->SetInput(0, nullptr);
	}
}

bool CGlassEffectBatch::PlanBands(const D2D1_RECT_F& drawingWorldBounds, std::span<const D2D1_RECT_F> drawingRectangles)
{
	m_worldBands.clear();
	m_worldBands.reserve(drawingRectangles.size());
	double originalArea{};
	for (auto band : drawingRectangles)
	{
		if (RectF::IntersectUnsafe(band, drawingWorldBounds))
		{
			m_worldBands.push_back(band);
			originalArea += RectF::Area(band);
		}
	}
	// Full-coverage tiles already batch natively; an extra full-area composition
	// pass regressed GPU cost. Disconnected dirty regions also keep their draws.
	if (originalArea >= RectF::Area(drawingWorldBounds))
	{
		return false;
	}
	const auto originalBandCount = m_worldBands.size();
	const auto commandAreaCost = static_cast<double>(CopyCallTexelCost);
	size_t retainedBandCount{};
	for (auto band : m_worldBands)
	{
		double bestCostSaving{};
		size_t mergeIndex = retainedBandCount;
		D2D1_RECT_F mergedBounds{};
		for (size_t bandIndex = 0; bandIndex < retainedBandCount; ++bandIndex)
		{
			const auto existingBand = m_worldBands[bandIndex];
			if (
				existingBand.left > band.right || existingBand.top > band.bottom ||
				existingBand.right < band.left || existingBand.bottom < band.top
			)
			{
				continue;
			}
			const D2D1_RECT_F combinedBounds
			{
				std::min(band.left, existingBand.left), std::min(band.top, existingBand.top),
				std::max(band.right, existingBand.right), std::max(band.bottom, existingBand.bottom)
			};
			const auto costSaving = RectF::Area(band) + RectF::Area(existingBand) + commandAreaCost - RectF::Area(combinedBounds);
			if (costSaving > bestCostSaving)
			{
				bestCostSaving = costSaving;
				mergeIndex = bandIndex;
				mergedBounds = combinedBounds;
			}
		}
		if (mergeIndex == retainedBandCount)
		{
			m_worldBands[retainedBandCount++] = band;
		}
		else
		{
			m_worldBands[mergeIndex] = mergedBounds;
		}
	}
	m_worldBands.resize(retainedBandCount);
	return retainedBandCount > 1 && retainedBandCount < originalBandCount;
}
