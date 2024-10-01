#include "pch.h"
#include "CustomBlurEffect.hpp"

using namespace OpenGlass;
const float CCustomBlurEffect::k_optimizations[16]
{
	8.f, 6.f, 1.5f, 2.5f, 0.f, 8.f, 6.f, 1.5f,
	2.5f, 0.f, 12.f, 6.f, 2.f, 3.f, 0.f, 0.f
};

// CropInput -> ScaleDown (optional) -> Border -> DirectionalBlurX -> DirectionalBlurY -> ScaleUp (optional)
HRESULT CCustomBlurEffect::Initialize(ID2D1DeviceContext* context)
{
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Crop,
			m_cropInputEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Scale,
			m_scaleDownEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Border,
			m_borderEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1DirectionalBlurKernel,
			m_directionalBlurXEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1DirectionalBlurKernel,
			m_directionalBlurYEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Scale,
			m_scaleUpEffect.put()
		)
	);

	RETURN_IF_FAILED(
		m_cropInputEffect->SetValue(
			D2D1_CROP_PROP_BORDER_MODE,
			D2D1_BORDER_MODE_SOFT
		)
	);
	m_scaleDownEffect->SetInputEffect(0, m_cropInputEffect.get());
	RETURN_IF_FAILED(m_scaleDownEffect->SetValue(D2D1_SCALE_PROP_SHARPNESS, 1.f));
	RETURN_IF_FAILED(
		m_scaleDownEffect->SetValue(
			D2D1_SCALE_PROP_BORDER_MODE,
			D2D1_BORDER_MODE_HARD
		)
	);
	m_borderEffect->SetInputEffect(0, m_scaleDownEffect.get());
	RETURN_IF_FAILED(
		m_borderEffect->SetValue(
			D2D1_BORDER_PROP_EDGE_MODE_X, 
			D2D1_BORDER_EDGE_MODE_MIRROR
		)
	);
	RETURN_IF_FAILED(
		m_borderEffect->SetValue(
			D2D1_BORDER_PROP_EDGE_MODE_Y, 
			D2D1_BORDER_EDGE_MODE_MIRROR
		)
	);
	m_directionalBlurXEffect->SetInputEffect(0, m_borderEffect.get());
	RETURN_IF_FAILED(
		m_directionalBlurXEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_DIRECTION, 
			D2D1_DIRECTIONALBLURKERNEL_DIRECTION_X
		)
	);
	m_directionalBlurYEffect->SetInputEffect(0, m_directionalBlurXEffect.get());
	RETURN_IF_FAILED(
		m_directionalBlurYEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_DIRECTION, 
			D2D1_DIRECTIONALBLURKERNEL_DIRECTION_Y
		)
	);
	m_scaleUpEffect->SetInputEffect(0, m_directionalBlurYEffect.get());
	RETURN_IF_FAILED(m_scaleUpEffect->SetValue(D2D1_SCALE_PROP_SHARPNESS, 1.f));
	RETURN_IF_FAILED(
		m_scaleUpEffect->SetValue(
			D2D1_SCALE_PROP_BORDER_MODE,
			D2D1_BORDER_MODE_HARD
		)
	);
	RETURN_IF_FAILED(
		m_scaleUpEffect->SetValue(
			D2D1_SCALE_PROP_INTERPOLATION_MODE,
			D2D1_SCALE_INTERPOLATION_MODE_LINEAR
		)
	);
	m_initialized = true;

	return S_OK;
}

float CCustomBlurEffect::DetermineOutputScale(float size, float blurAmount)
{
	float outputScale{ 1.f };
	if (size > 1.0)
	{
		float k{ blurAmount <= k_optimizations[2] ? 1.f : 0.5f };
		outputScale = k * fmaxf(
			0.1f,
			fminf(
				1.f, 
				k_optimizations[0] / (blurAmount + k_optimizations[1])
			)
		);
		if (outputScale * size < 1.f)
		{
			return 1.f / size;
		}
	}
	return outputScale;
}

HRESULT STDMETHODCALLTYPE CCustomBlurEffect::SetInput(
	ID2D1DeviceContext* context,
	ID2D1Image* inputImage,
	const D2D1_RECT_F& imageRectangle,
	const D2D1_RECT_F& imageBounds,
	float blurAmount
)
{
	if (!m_initialized)
	{
		RETURN_IF_FAILED(Initialize(context));
	}
	if (m_effectInput != inputImage)
	{
		m_effectInput = inputImage;
		m_cropInputEffect->SetInput(0, inputImage);
	}

	bool recalculateParams{ false };
	if (m_blurAmount != blurAmount)
	{
		m_blurAmount = blurAmount;
		recalculateParams = true;
	}
	if (memcmp(&m_imageRectangle, &imageRectangle, sizeof(D2D1_RECT_F)) != 0)
	{
		m_imageRectangle = imageRectangle;
		recalculateParams = true;
	}

	if (!recalculateParams)
	{
		return S_OK;
	}

	float extendAmount{ m_blurAmount * 3.f + 0.5f };
	D2D1_RECT_F actualImageRect
	{
		max(imageRectangle.left - extendAmount, imageBounds.left),
		max(imageRectangle.top - extendAmount, imageBounds.top),
		min(imageRectangle.right + extendAmount, imageBounds.right),
		min(imageRectangle.bottom + extendAmount, imageBounds.bottom)
	};
	RETURN_IF_FAILED(
		m_cropInputEffect->SetValue(
			D2D1_CROP_PROP_RECT,
			actualImageRect
		)
	);

	RETURN_IF_FAILED(
		m_scaleDownEffect->SetValue(
			D2D1_SCALE_PROP_INTERPOLATION_MODE,
			blurAmount > 3.f ? 
			D2D1_SCALE_INTERPOLATION_MODE_ANISOTROPIC :
			D2D1_SCALE_INTERPOLATION_MODE_LINEAR
		)
	);

	D2D1_VECTOR_2F prescaleAmount
	{
		DetermineOutputScale(actualImageRect.right - actualImageRect.left, blurAmount),
		DetermineOutputScale(actualImageRect.bottom - actualImageRect.top, blurAmount)
	};
	D2D1_VECTOR_2F finalBlurAmount{ blurAmount, blurAmount };
	D2D1_VECTOR_2F outputOffset{ 0.f, 0.f };
	auto finalPrescaleAmount = prescaleAmount;

	if (prescaleAmount.x != 1.f && finalBlurAmount.x > k_optimizations[2])
	{
		if (prescaleAmount.x <= 0.5f)
		{
			outputOffset.x = 0.25f;
			finalPrescaleAmount.x *= 2.f;
		}
	}
	if (prescaleAmount.y != 1.f && finalBlurAmount.y > k_optimizations[2])
	{
		if (prescaleAmount.y <= 0.5f)
		{
			outputOffset.y = 0.25f;
			finalPrescaleAmount.y *= 2.f;
		}
	}

	if (prescaleAmount.x == 1.f && prescaleAmount.y == 1.f)
	{
		m_directionalBlurYEffect->GetOutput(m_effectOutput.put());
	}
	else
	{
		m_scaleUpEffect->GetOutput(m_effectOutput.put());
		RETURN_IF_FAILED(
			m_scaleUpEffect->SetValue(
				D2D1_SCALE_PROP_SCALE,
				D2D1::Point2F(
					1.f / prescaleAmount.x,
					1.f / prescaleAmount.y
				)
			)
		);
	}

	if (finalPrescaleAmount.x == 1.f && finalPrescaleAmount.y == 1.f)
	{
		m_borderEffect->SetInputEffect(0, m_cropInputEffect.get());
	}
	else
	{
		m_scaleDownEffect->SetInputEffect(0, m_cropInputEffect.get());
		m_borderEffect->SetInputEffect(0, m_scaleDownEffect.get());
		RETURN_IF_FAILED(
			m_scaleDownEffect->SetValue(
				D2D1_SCALE_PROP_SCALE,
				finalPrescaleAmount
			)
		);

		finalBlurAmount = D2D1::Vector2F(
			finalBlurAmount.x * finalPrescaleAmount.x,
			finalBlurAmount.y * finalPrescaleAmount.y
		);
	}

	RETURN_IF_FAILED(
		m_directionalBlurXEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_STANDARD_DEVIATION,
			finalBlurAmount.x
		)
	);
	RETURN_IF_FAILED(
		m_directionalBlurXEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_KERNEL_RANGE_FACTOR,
			k_optimizations[3]
		)
	);
	RETURN_IF_FAILED(
		m_directionalBlurXEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_OPTIMIZATION_TRANSFORM,
			(prescaleAmount.x != finalPrescaleAmount.x) ? D2D1_DIRECTIONALBLURKERNEL_OPTIMIZATION_TRANSFORM_SCALE : D2D1_DIRECTIONALBLURKERNEL_OPTIMIZATION_TRANSFORM_IDENDITY
		)
	);
	RETURN_IF_FAILED(
		m_directionalBlurYEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_STANDARD_DEVIATION,
			finalBlurAmount.y
		)
	);
	RETURN_IF_FAILED(
		m_directionalBlurYEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_KERNEL_RANGE_FACTOR,
			k_optimizations[3]
		)
	);
	RETURN_IF_FAILED(
		m_directionalBlurYEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_OPTIMIZATION_TRANSFORM,
			(prescaleAmount.y != finalPrescaleAmount.y) ? D2D1_DIRECTIONALBLURKERNEL_OPTIMIZATION_TRANSFORM_SCALE : D2D1_DIRECTIONALBLURKERNEL_OPTIMIZATION_TRANSFORM_IDENDITY
		)
	);
#ifdef _DEBUG
	OutputDebugStringW(
		std::format(
			L"dblur_x: [deviation:{},direction:{},factor:{},transform:{}], dblur_y: [deviation:{},direction:{},factor:{},transform:{}], scale: [scale:[{},{}]]",
			m_directionalBlurXEffect->GetValue<float>(D2D1_DIRECTIONALBLURKERNEL_PROP_STANDARD_DEVIATION),
			m_directionalBlurXEffect->GetValue<UINT32>(D2D1_DIRECTIONALBLURKERNEL_PROP_DIRECTION),
			m_directionalBlurXEffect->GetValue<float>(D2D1_DIRECTIONALBLURKERNEL_PROP_KERNEL_RANGE_FACTOR),
			m_directionalBlurXEffect->GetValue<UINT32>(D2D1_DIRECTIONALBLURKERNEL_PROP_OPTIMIZATION_TRANSFORM),
			m_directionalBlurYEffect->GetValue<float>(D2D1_DIRECTIONALBLURKERNEL_PROP_STANDARD_DEVIATION),
			m_directionalBlurYEffect->GetValue<UINT32>(D2D1_DIRECTIONALBLURKERNEL_PROP_DIRECTION),
			m_directionalBlurYEffect->GetValue<float>(D2D1_DIRECTIONALBLURKERNEL_PROP_KERNEL_RANGE_FACTOR),
			m_directionalBlurYEffect->GetValue<UINT32>(D2D1_DIRECTIONALBLURKERNEL_PROP_OPTIMIZATION_TRANSFORM),
			m_scaleDownEffect->GetValue<D2D1_VECTOR_2F>(D2D1_SCALE_PROP_SCALE).x,
			m_scaleDownEffect->GetValue<D2D1_VECTOR_2F>(D2D1_SCALE_PROP_SCALE).y
		).c_str()
	);
#endif

	return S_OK;
}

void STDMETHODCALLTYPE CCustomBlurEffect::Reset()
{
	if (m_cropInputEffect) 
	{ 
		m_cropInputEffect->SetInput(0, nullptr); 
	}
	m_imageRectangle = {};
	m_effectInput = nullptr;
	m_effectOutput = nullptr;
}