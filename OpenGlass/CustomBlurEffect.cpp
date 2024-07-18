#include "pch.h"
#include "CustomBlurEffect.hpp"

using namespace OpenGlass;
const float CCustomBlurEffect::k_optimizations[16]
{
	8.f, 6.f, 1.5f, 2.5f, static_cast<float>(-42.28171817154095), 8.f, 6.f, 1.5f,
	2.5f, static_cast<float>(-34.12687268616382), 12.f, 6.f, 2.f, 3.f, static_cast<float>(-34.12687268616382), 0.f
};

CCustomBlurEffect::CCustomBlurEffect(ID2D1DeviceContext* deviceContext)
{
	winrt::copy_from_abi(m_deviceContext, deviceContext);
}

// CropInput -> ScaleDown (optional) -> Border -> DirectionalBlurX -> DirectionalBlurY -> ScaleUp (optional)
HRESULT CCustomBlurEffect::Initialize()
{
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Crop,
			m_cropInputEffect.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Scale,
			m_scaleDownEffect.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Border,
			m_borderEffect.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1DirectionalBlurKernel,
			m_directionalBlurXEffect.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1DirectionalBlurKernel,
			m_directionalBlurYEffect.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Scale,
			m_scaleUpEffect.put()
		)
	);

	SetParams();

	m_initialized = true;

	return S_OK;
}

// CropInput -> ScaleDown (optional) -> Border -> DirectionalBlurX -> DirectionalBlurY -> ScaleUp (optional)
HRESULT CCustomBlurEffect::InitializeAero()
{
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Crop,
			m_cropInputEffect.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Scale,
			m_scaleDownEffect.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Border,
			m_borderEffect.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1DirectionalBlurKernel,
			m_directionalBlurXEffect.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1DirectionalBlurKernel,
			m_directionalBlurYEffect.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Scale,
			m_scaleUpEffect.put()
		)
	);

	// imma try n put whats relevant here
	// ts is jus creating the effects

	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Composite,
			m_compositeEffect.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Composite,
			m_compositeEffect_pass2.put()
	)
	);

	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Tint,
			m_tintEffect.put()
		)
	);

	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Saturation,
			m_saturationEffect.put()
		)
	);

	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1ColorMatrix,
			m_ColorizationAfterglowBalance.put()
		)
	);
	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1ColorMatrix,
			m_ColorizationBlurBalance.put()
		)
	);

	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1ColorMatrix,
			m_ColorizationColorBalance.put()
		)
	);

	RETURN_IF_FAILED(
		m_deviceContext->CreateEffect(
			CLSID_D2D1Flood,
			m_ColorizationColor.put()
		)
	);

	/*
	RETURN_IF_FAILED(
		m_cropInputEffect->SetValue(D2D1_PROPERTY_CACHED, TRUE)
	);*/

	SetParamsAero();
	

	m_initialized = true;

	return S_OK;
}

HRESULT CCustomBlurEffect::SetParams()
{
	/*
	RETURN_IF_FAILED(
	m_cropInputEffect->SetValue(D2D1_PROPERTY_CACHED, TRUE)
	);*/
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
	RETURN_IF_FAILED(
		m_scaleDownEffect->SetValue(
			D2D1_SCALE_PROP_INTERPOLATION_MODE,
			D2D1_SCALE_INTERPOLATION_MODE_LINEAR
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

	return S_OK;
}

HRESULT CCustomBlurEffect::SetParamsAero()
{
	// now here we start setting values, or atleast the default values
	// imma have to investigate later in the file whats being done but otherwise



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
	RETURN_IF_FAILED(
		m_scaleDownEffect->SetValue(
			D2D1_SCALE_PROP_INTERPOLATION_MODE,
			D2D1_SCALE_INTERPOLATION_MODE_ANISOTROPIC
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


	// CREDITS: @kfh83, @wiktorwiktor12, @TorutheRedFox and @WackyIdeas. special shoutouts to @aubymori and @kawapure for testing/help
	
	// okay my conclusion is that i shouldnt tamper with whatever was already set up (no point in removing the scales or adding gaussian blur)
	// so since all of the blur work is done here, starting my part of it:
	
	// hardcoded values whatever (EDIT: Not Anymore!)
	//m_colorizationAfterglowBalanceVal = 0.43f;
	//m_colorizationBlurBalanceVal = 0.796f;
	//m_colorizationColorBalanceVal = 0.08f;

	// afterglow
	m_saturationEffect->SetInputEffect(0, m_directionalBlurYEffect.get());
	RETURN_IF_FAILED(
		m_saturationEffect->SetValue(
			D2D1_SATURATION_PROP_SATURATION, 
			0.0f
	)
	);
	
	m_tintEffect->SetInputEffect(0, m_saturationEffect.get());
	RETURN_IF_FAILED(
		m_tintEffect->SetValue(
			D2D1_TINT_PROP_COLOR, 
			D2D1::Vector4F(m_Color.r, m_Color.g, m_Color.b, 1.0f)
	)
	);

	
	D2D1_MATRIX_5X4_F AfterglowBalanceM = D2D1::Matrix5x4F(m_colorizationAfterglowBalanceVal, 0.f, 0.f, 0.f,   
														   0.f, m_colorizationAfterglowBalanceVal, 0.f, 0.f,   
														   0.f, 0.f, m_colorizationAfterglowBalanceVal, 0.f,
														   0.f, 0.f, 0.f, 1.f,   
														   0.f, 0.f, 0.f, 0.f);

	m_ColorizationAfterglowBalance->SetInputEffect(0, m_tintEffect.get());
	RETURN_IF_FAILED(
		m_ColorizationAfterglowBalance->SetValue(
			D2D1_COLORMATRIX_PROP_COLOR_MATRIX, 
			AfterglowBalanceM
	)
	);

	// afterglow done
	
	// heres blur balance
	D2D1_MATRIX_5X4_F BlurBalanceM = D2D1::Matrix5x4F(m_colorizationBlurBalanceVal, 0.f, 0.f, 0.f,
													  0.f, m_colorizationBlurBalanceVal, 0.f, 0.f,
													  0.f, 0.f, m_colorizationBlurBalanceVal, 0.f,
													  0.f, 0.f, 0.f, 1.f,   
													  0.f, 0.f, 0.f, 0.f);

	m_ColorizationBlurBalance->SetInputEffect(0, m_directionalBlurYEffect.get());
	RETURN_IF_FAILED(
		m_ColorizationBlurBalance->SetValue(
			D2D1_COLORMATRIX_PROP_COLOR_MATRIX, 
			BlurBalanceM
	)
	);

	// and finally ColorizationColor

	RETURN_IF_FAILED(
		m_ColorizationColor->SetValue(
			D2D1_FLOOD_PROP_COLOR, 
			D2D1::Vector4F(m_Color.r, m_Color.g, m_Color.b, 1.0f)
	)
	);

	D2D1_MATRIX_5X4_F ColorBalanceM = D2D1::Matrix5x4F(m_colorizationColorBalanceVal, 0.f, 0.f, 0.f,
													   0.f, m_colorizationColorBalanceVal, 0.f, 0.f,
													   0.f, 0.f, m_colorizationColorBalanceVal, 0.f,
													   0.f, 0.f, 0.f, 1.f,   
													   0.f, 0.f, 0.f, 0.f);

	m_ColorizationColorBalance->SetInputEffect(0, m_ColorizationColor.get());
	RETURN_IF_FAILED(
		m_ColorizationColorBalance->SetValue(
			D2D1_COLORMATRIX_PROP_COLOR_MATRIX, 
			ColorBalanceM
	)
	);


	// imma blend it together jus to see
	m_compositeEffect->SetInputEffect(0, m_ColorizationBlurBalance.get());
	m_compositeEffect->SetInputEffect(1, m_ColorizationAfterglowBalance.get());
	RETURN_IF_FAILED(
		m_compositeEffect->SetValue(
			D2D1_COMPOSITE_PROP_MODE, 
			D2D1_COMPOSITE_MODE_PLUS
	)
	);

	m_compositeEffect_pass2->SetInputEffect(0, m_compositeEffect.get());
	m_compositeEffect_pass2->SetInputEffect(1, m_ColorizationColorBalance.get());
	RETURN_IF_FAILED(
		m_compositeEffect_pass2->SetValue(
			D2D1_COMPOSITE_PROP_MODE, 
			D2D1_COMPOSITE_MODE_PLUS
	)
	);
	// okay

	m_scaleUpEffect->SetInputEffect(0, m_compositeEffect_pass2.get());
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
			D2D1_SCALE_INTERPOLATION_MODE_ANISOTROPIC
	)
	);

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

HRESULT STDMETHODCALLTYPE CCustomBlurEffect::Invalidate(
	ID2D1Image* inputImage,
	const D2D1_RECT_F& imageRectangle,
	const D2D1_RECT_F& imageBounds,
	float blurAmount,
	float colorizationAfterglowBalanceVal,
	float colorizationBlurBalanceVal,
	float colorizationColorBalanceVal,
	D2D1_COLOR_F color,
	Type type
)
{
	//colorizationAfterglowBalanceVal = 0.49f;
	//colorizationBlurBalanceVal = 0.796f;
	//colorizationColorBalanceVal = 0.08f;
	if (!inputImage) return S_FALSE;

	bool recalculateParams{ false };
	if (m_blurAmount != blurAmount || m_colorizationAfterglowBalanceVal != colorizationAfterglowBalanceVal || 
		m_colorizationBlurBalanceVal != colorizationBlurBalanceVal ||
		m_colorizationColorBalanceVal != colorizationColorBalanceVal || (m_Color.r != color.r) || (m_Color.g != color.g) || (m_Color.b != color.b) || (m_Color.a != color.a) )
	{
		m_blurAmount = blurAmount;
		m_colorizationAfterglowBalanceVal = colorizationAfterglowBalanceVal;
		m_colorizationBlurBalanceVal = colorizationBlurBalanceVal;
		m_colorizationColorBalanceVal = colorizationColorBalanceVal;
		m_Color = color;
		recalculateParams = true;
	}
	
	//todo: fix handling type changes at runtime
	if (!m_initialized || m_type != type)
	{
		m_type = type;
		if (m_type == Type::Blur)
			RETURN_IF_FAILED(Initialize());
		else if (m_type == Type::Aero)
			RETURN_IF_FAILED(InitializeAero());
	}
	if (m_effectInput != inputImage)
	{
		m_effectInput = inputImage;
		m_cropInputEffect->SetInput(0, inputImage);
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

	D2D1_VECTOR_2F prescaleAmount
	{
		DetermineOutputScale(actualImageRect.right - actualImageRect.left, blurAmount),
		DetermineOutputScale(actualImageRect.bottom - actualImageRect.top, blurAmount)
	};
	D2D1_VECTOR_2F finalBlurAmount{ blurAmount, blurAmount };
	D2D1_VECTOR_2F outputOffset{ 0.f, 0.f };
	auto finalPrescaleAmount{ prescaleAmount };

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

	if (m_type == Type::Blur)
		RETURN_IF_FAILED(SetParams());
	else if (m_type == Type::Aero)
		RETURN_IF_FAILED(SetParamsAero());

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

HRESULT STDMETHODCALLTYPE CCustomBlurEffect::Draw(
	CONST D2D1_RECT_F& bounds,
	D2D1_INTERPOLATION_MODE interpolationMode,
	D2D1_COMPOSITE_MODE compositeMode
)
{
	m_deviceContext->DrawImage(
		m_effectOutput.get(),
		D2D1::Point2F(
			bounds.left + m_imageRectangle.left,
			bounds.top + m_imageRectangle.top
		),
		D2D1::RectF(
			m_imageRectangle.left,
			m_imageRectangle.top,
			m_imageRectangle.left + min(m_imageRectangle.right - m_imageRectangle.left, bounds.right - bounds.left - m_imageRectangle.left),
			m_imageRectangle.top + min(m_imageRectangle.bottom - m_imageRectangle.top, bounds.bottom - bounds.top - m_imageRectangle.top)
		),
		interpolationMode,
		compositeMode
	);
	/*{
		winrt::com_ptr<ID2D1SolidColorBrush> brush{};
		m_deviceContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), brush.put());
		m_deviceContext->DrawRectangle(D2D1::RectF(
			bounds.left + m_imageRectangle.left + 0.5f,
			bounds.left + m_imageRectangle.top + 0.5f,
			min(bounds.left + m_imageRectangle.right - 0.5f, bounds.right),
			min(bounds.top + m_imageRectangle.bottom - 0.5f, bounds.bottom)
		), brush.get(), 1.f);
	}*/

	return S_OK;
}
