#include "pch.h"
#include "AeroEffect.hpp"

using namespace OpenGlass;

HRESULT CAeroEffect::Initialize(ID2D1DeviceContext* context)
{
	m_customBlurEffect = winrt::make<CCustomBlurEffect>();

	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Flood,
			m_fallbackColorEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Flood,
			m_colorEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Tint,
			m_tintEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Saturation,
			m_desaturationEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Composite,
			m_innerCompositeEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1ColorMatrix,
			m_afterglowBalanceEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1ColorMatrix,
			m_blurBalanceEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Composite,
			m_compositeEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Composite,
			m_compositeEffect2.put()
		)
	);



	RETURN_IF_FAILED(
		m_desaturationEffect->SetValue(
			D2D1_SATURATION_PROP_SATURATION,
			0.f
		)
	);

	m_innerCompositeEffect->SetInputEffect(0, m_fallbackColorEffect.get());
	m_innerCompositeEffect->SetInputEffect(1, m_desaturationEffect.get());
	RETURN_IF_FAILED(
		m_innerCompositeEffect->SetValue(
			D2D1_COMPOSITE_PROP_MODE,
			D2D1_COMPOSITE_MODE_SOURCE_OVER
		)
	);

	m_tintEffect->SetInputEffect(0, m_innerCompositeEffect.get());
	m_afterglowBalanceEffect->SetInputEffect(0, m_tintEffect.get());

	m_compositeEffect->SetInputEffect(0, m_blurBalanceEffect.get());
	m_compositeEffect->SetInputEffect(1, m_afterglowBalanceEffect.get());
	RETURN_IF_FAILED(
		m_compositeEffect->SetValue(
			D2D1_COMPOSITE_PROP_MODE,
			D2D1_COMPOSITE_MODE_PLUS
		)
	);

	m_compositeEffect2->SetInputEffect(0, m_compositeEffect.get());
	m_compositeEffect2->SetInputEffect(1, m_colorEffect.get());
	RETURN_IF_FAILED(
		m_compositeEffect2->SetValue(
			D2D1_COMPOSITE_PROP_MODE,
			D2D1_COMPOSITE_MODE_PLUS
		)
	);
	m_initialized = true;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CAeroEffect::SetInput(
	ID2D1DeviceContext* context,
	ID2D1Image* inputImage,
	const D2D1_RECT_F& imageRectangle,
	const D2D1_RECT_F& imageBounds,
	float blurAmount,
	const D2D1_COLOR_F& color,
	const D2D1_COLOR_F& afterglowColor,
	float colorBalance,
	float afterglowBalance,
	float blurBalance
)
{
	if (!m_initialized)
	{
		RETURN_IF_FAILED(Initialize(context));
	}
	RETURN_IF_FAILED(
		m_customBlurEffect->SetInput(
			context,
			inputImage,
			imageRectangle,
			imageBounds,
			blurAmount
		)
	);

	// CREDITS: @kfh83, @wiktorwiktor12, @TorutheRedFox and @WackyIdeas. special shoutouts to @aubymori and @kawapure for testing/help
	// @ALTaleX modified it to adapt opening/restoring/closing animation
	auto input = m_customBlurEffect->GetOutput();

	RETURN_IF_FAILED(
		m_fallbackColorEffect->SetValue(
			D2D1_FLOOD_PROP_COLOR,
			D2D1::Vector4F(
				blurBalance / (1.f - afterglowBalance / 1.5f),
				blurBalance / (1.f - afterglowBalance / 1.5f),
				blurBalance / (1.f - afterglowBalance / 1.5f),
				blurBalance * (1.f - afterglowBalance / 1.5f)
			)
		)
	);
	m_blurBalanceEffect->SetInput(0, input);
	RETURN_IF_FAILED(
		m_blurBalanceEffect->SetValue(
			D2D1_COLORMATRIX_PROP_COLOR_MATRIX,
			D2D1::Matrix5x4F(
				blurBalance, 0.f, 0.f, 0.f,
				0.f, blurBalance, 0.f, 0.f,
				0.f, 0.f, blurBalance, 0.f,
				0.f, 0.f, 0.f, 1.f,
				0.f, 0.f, 0.f, 0.f
			)
		)
	);
	m_desaturationEffect->SetInput(0, input);
	RETURN_IF_FAILED(
		m_tintEffect->SetValue(
			D2D1_TINT_PROP_COLOR,
			D2D1::Vector4F(
				afterglowColor.r,
				afterglowColor.g,
				afterglowColor.b,
				1.f
			)
		)
	);
	RETURN_IF_FAILED(
		m_colorEffect->SetValue(
			D2D1_FLOOD_PROP_COLOR,
			D2D1::Vector4F(
				color.r,
				color.g,
				color.b,
				1.f
			)
		)
	);
	RETURN_IF_FAILED(
		m_afterglowBalanceEffect->SetValue(
			D2D1_COLORMATRIX_PROP_COLOR_MATRIX,
			D2D1::Matrix5x4F(
				afterglowBalance, 0.f, 0.f, 0.f,
				0.f, afterglowBalance, 0.f, 0.f,
				0.f, 0.f, afterglowBalance, 0.f,
				0.f, 0.f, 0.f, 1.f,
				0.f, 0.f, 0.f, 0.f
			)
		)
	);

	m_desaturationEffect->SetInput(0, input);
	RETURN_IF_FAILED(
		m_colorEffect->SetValue(
			D2D1_FLOOD_PROP_COLOR,
			D2D1::Vector4F(
				color.r * colorBalance,
				color.g * colorBalance,
				color.b * colorBalance,
				colorBalance
			)
		)
	);

	m_compositeEffect2->GetOutput(m_effectOutput.put());

	return S_OK;
}

void STDMETHODCALLTYPE CAeroEffect::Reset()
{
	if (m_customBlurEffect)
	{
		m_customBlurEffect->Reset();
	}
	if (m_blurBalanceEffect)
	{
		m_blurBalanceEffect->SetInput(0, nullptr);
	}
	if (m_desaturationEffect)
	{
		m_desaturationEffect->SetInput(0, nullptr);
	}
	m_effectOutput = nullptr;
}