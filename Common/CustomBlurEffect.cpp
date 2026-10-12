#include "CustomBlurEffect.hpp"
#include "D2DPrivates.hpp"
#include <wil/result.h>
#include <wil/win32_helpers.h>
#include <algorithm>
#include <cstring>

using namespace OpenGlass;
const float CCustomBlurEffect::k_optimizations[16]
{
	8.f, 6.f, 1.5f, 2.5f, D2D1_SCALE_INTERPOLATION_MODE_LINEAR,
	8.f, 6.f, 1.5f, 2.5f, D2D1_SCALE_INTERPOLATION_MODE_MULTI_SAMPLE_LINEAR,
	12.f, 6.f, 2.f, 3.f, D2D1_SCALE_INTERPOLATION_MODE_MULTI_SAMPLE_LINEAR,
	0.f
};

float CCustomBlurEffect::DetermineOutputScale(
	float size
)
{
	float outputScale{ 1.f };
	if (size > 1.0)
	{
		const auto k = m_blurAmount <= k_optimizations[5 * static_cast<int>(m_optimization) + 2] ? 1.f : 0.5f;
		outputScale = k * (std::max)(
			0.1f,
			(std::min)(
				1.f,
				k_optimizations[5 * static_cast<int>(m_optimization)] / (m_blurAmount + k_optimizations[5 * static_cast<int>(m_optimization) + 1])
			)
		);
		if (outputScale * size < 1.f)
		{
			return 1.f / size;
		}
	}
	return outputScale;
}

HRESULT CCustomBlurEffect::Initialize(ID2D1DeviceContext* context)
{
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Scale,
			m_scaleDownEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Crop,
			m_cropInputEffect.put()
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
		m_scaleDownEffect->SetValue(
			D2D1_SCALE_PROP_BORDER_MODE,
			D2D1_BORDER_MODE_HARD
		)
	);

	RETURN_IF_FAILED(
		m_cropInputEffect->SetValue(
			D2D1_CROP_PROP_BORDER_MODE,
			D2D1_BORDER_MODE_HARD
		)
	);

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
	m_borderEffect->SetInputEffect(0, m_cropInputEffect.get());

	RETURN_IF_FAILED(
		m_directionalBlurXEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_DIRECTION,
			D2D1_DIRECTIONALBLURKERNEL_DIRECTION_X
		)
	);
	m_directionalBlurXEffect->SetInputEffect(0, m_borderEffect.get());

	RETURN_IF_FAILED(
		m_directionalBlurYEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_DIRECTION,
			D2D1_DIRECTIONALBLURKERNEL_DIRECTION_Y
		)
	);
	m_directionalBlurYEffect->SetInputEffect(0, m_directionalBlurXEffect.get());

	m_initialized = true;

	return S_OK;
}

HRESULT CCustomBlurEffect::CalculateAndSetEffectParams(ID2D1DeviceContext* context, ID2D1Image* inputImage)
{
	D2D1_RECT_F scaledImageRectangle = m_imageRectangle;

	D2D1_RECT_F imageBounds{};
	RETURN_IF_FAILED(context->GetImageLocalBounds(inputImage, &imageBounds));

	m_prescaleAmount = 
	{
		DetermineOutputScale(wil::rect_width(imageBounds)),
		DetermineOutputScale(wil::rect_height(imageBounds))
	};

	D2D1_VECTOR_2F deviation{ m_blurAmount, m_blurAmount };
	auto scaleProduct = m_prescaleAmount;

	m_offset = {};
	if (m_prescaleAmount.x != 1.f && deviation.x > k_optimizations[5 * static_cast<int>(m_optimization) + 2])
	{
		if (m_prescaleAmount.x <= 0.5f)
		{
			scaleProduct.x *= 2.f;
			m_offset.x = 0.25f;
		}
	}
	if (m_prescaleAmount.y != 1.f && deviation.y > k_optimizations[5 * static_cast<int>(m_optimization) + 2])
	{
		if (m_prescaleAmount.y <= 0.5f)
		{
			scaleProduct.y *= 2.f;
			m_offset.y = 0.25f;
		}
	}

	if (m_offset.x == 1.f && m_offset.y == 1.f)
	{
		m_scaleDownEffect->SetInput(0, nullptr);
		m_cropInputEffect->SetInput(0, inputImage);
	}
	else
	{
		m_scaleDownEffect->SetInput(0, inputImage);
		m_cropInputEffect->SetInput(0, nullptr);
		m_cropInputEffect->SetInputEffect(0, m_scaleDownEffect.get());

		RETURN_IF_FAILED(
			m_scaleDownEffect->SetValue(
				D2D1_SCALE_PROP_INTERPOLATION_MODE,
				static_cast<D2D1_SCALE_INTERPOLATION_MODE>(k_optimizations[5 * m_optimization + 4])
			)
		);
		RETURN_IF_FAILED(
			m_scaleDownEffect->SetValue(
				D2D1_SCALE_PROP_SCALE,
				scaleProduct
			)
		);

		deviation = D2D1::Vector2F(
			deviation.x * scaleProduct.x,
			deviation.y * scaleProduct.y
		);
		scaledImageRectangle =
		{
			m_imageRectangle.left * scaleProduct.x + 1.f,
			m_imageRectangle.top * scaleProduct.y + 1.f,
			m_imageRectangle.right * scaleProduct.x - 1.f,
			m_imageRectangle.bottom * scaleProduct.y - 1.f,
		};
		if (wil::rect_width(scaledImageRectangle) < 1.f)
		{
			scaledImageRectangle.left = (m_imageRectangle.left + m_imageRectangle.right) / 2.f * scaleProduct.x - 0.5f;
			scaledImageRectangle.right = scaledImageRectangle.left + 1.f;
		}
		if (wil::rect_height(scaledImageRectangle) < 1.f)
		{
			scaledImageRectangle.top = (m_imageRectangle.top + m_imageRectangle.bottom) / 2.f * scaleProduct.y - 0.5f;
			scaledImageRectangle.bottom = scaledImageRectangle.top + 1.f;
		}
	}
	RETURN_IF_FAILED(
		m_cropInputEffect->SetValue(
			D2D1_CROP_PROP_RECT,
			scaledImageRectangle
		)
	);

	RETURN_IF_FAILED(
		m_directionalBlurXEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_STANDARD_DEVIATION,
			deviation.x
		)
	);
	RETURN_IF_FAILED(
		m_directionalBlurXEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_KERNEL_RANGE_FACTOR,
			k_optimizations[5 * m_optimization + 3]
		)
	);
	RETURN_IF_FAILED(
		m_directionalBlurXEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_OPTIMIZATION_TRANSFORM,
			(m_prescaleAmount.x != scaleProduct.x) ? D2D1_DIRECTIONALBLURKERNEL_OPTIMIZATION_TRANSFORM_SCALE : D2D1_DIRECTIONALBLURKERNEL_OPTIMIZATION_TRANSFORM_IDENDITY
		)
	);
	RETURN_IF_FAILED(
		m_directionalBlurYEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_STANDARD_DEVIATION,
			deviation.y
		)
	);
	RETURN_IF_FAILED(
		m_directionalBlurYEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_KERNEL_RANGE_FACTOR,
			k_optimizations[5 * m_optimization + 3]
		)
	);
	RETURN_IF_FAILED(
		m_directionalBlurYEffect->SetValue(
			D2D1_DIRECTIONALBLURKERNEL_PROP_OPTIMIZATION_TRANSFORM,
			(m_prescaleAmount.y != scaleProduct.y) ? D2D1_DIRECTIONALBLURKERNEL_OPTIMIZATION_TRANSFORM_SCALE : D2D1_DIRECTIONALBLURKERNEL_OPTIMIZATION_TRANSFORM_IDENDITY
		)
	);

	return S_OK;
}

HRESULT CCustomBlurEffect::Build(
	ID2D1DeviceContext* context,
	ID2D1Image* inputImage,
	const D2D1_RECT_F& imageRectangle,
	const void* additionalParams
)
{
	const auto params = static_cast<const CCustomBlurParams*>(additionalParams);

	if (!m_initialized)
	{
		RETURN_IF_FAILED(Initialize(context));
	}

	bool recalculateParams{ false };
	if (m_inputImage.get() != inputImage)
	{
		m_inputImage.copy_from(inputImage);
		recalculateParams = true;
	}
	if (m_blurAmount != params->blurAmount)
	{
		m_blurAmount = params->blurAmount;
		recalculateParams = true;
	}
	if (m_optimization != params->optimization)
	{
		m_optimization = params->optimization;
		recalculateParams = true;
	}
	if (memcmp(&m_imageRectangle, &imageRectangle, sizeof(D2D1_RECT_F)) != 0)
	{
		m_imageRectangle = imageRectangle;
		recalculateParams = true;
	}

	if (recalculateParams)
	{
		RETURN_IF_FAILED(
			CalculateAndSetEffectParams(
				context,
				inputImage
			)
		);
	}

	return S_OK;
}

D2D1_MATRIX_3X2_F CCustomBlurEffect::GetOutputMatrix() const
{
	if (m_blurAmount == 0.f)
	{
		return D2D1::IdentityMatrix();
	}

	return D2D1::Matrix3x2F::Scale(
		D2D1::SizeF(
			1.f / m_prescaleAmount.x,
			1.f / m_prescaleAmount.y
		)
	);
}

void CCustomBlurEffect::GetOutput(ID2D1Image** output) const
{
	if (m_blurAmount == 0.f)
	{
		m_inputImage.copy_to(output);
	}
	else
	{
		m_directionalBlurYEffect->GetOutput(output);
	}
}

void CCustomBlurEffect::Reset()
{
	if (m_scaleDownEffect)
	{
		m_scaleDownEffect->SetInput(0, nullptr);
	}
	if (m_cropInputEffect)
	{
		m_cropInputEffect->SetInput(0, nullptr);
	}
}
