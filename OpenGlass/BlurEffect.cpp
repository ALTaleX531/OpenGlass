#include "pch.h"
#include "BlurEffect.hpp"

using namespace OpenGlass;

HRESULT CBlurEffect::Initialize(ID2D1DeviceContext* context)
{
	m_customBlurEffect = winrt::make<CCustomBlurEffect>();

	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Flood,
			m_colorEffect.put()
		)
	);
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_D2D1Composite,
			m_compositeEffect.put()
		)
	);

	RETURN_IF_FAILED(
		m_compositeEffect->SetValue(
			D2D1_COMPOSITE_PROP_MODE,
			D2D1_COMPOSITE_MODE_SOURCE_OVER
		)
	);

	m_compositeEffect->SetInputEffect(1, m_colorEffect.get());
	m_initialized = true;

	return S_OK;
}

HRESULT STDMETHODCALLTYPE CBlurEffect::SetInput(
	ID2D1DeviceContext* context,
	ID2D1Image* inputImage,
	const D2D1_RECT_F& imageRectangle,
	const D2D1_RECT_F& imageBounds,
	float blurAmount,
	const D2D1_COLOR_F& color,
	float opacity
)
{
	if (!m_initialized)
	{
		RETURN_IF_FAILED(Initialize(context));
	}
	RETURN_IF_FAILED(
		m_colorEffect->SetValue(
			D2D1_FLOOD_PROP_COLOR,
			D2D1::Vector4F(
				color.r * opacity,
				color.g * opacity,
				color.b * opacity,
				opacity
			)
		)
	);
	if (opacity == 1.f)
	{
		m_colorEffect->GetOutput(m_effectOutput.put());
		return S_OK;
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
	if (opacity == 0.f)
	{
		winrt::copy_from_abi(m_effectOutput, m_customBlurEffect->GetOutput());
		return S_OK;
	}
	m_compositeEffect->SetInput(0, m_customBlurEffect->GetOutput());
	m_compositeEffect->GetOutput(m_effectOutput.put());

	return S_OK;
}

void STDMETHODCALLTYPE CBlurEffect::Reset()
{
	if (m_customBlurEffect)
	{
		m_customBlurEffect->Reset();
	}
	if (m_compositeEffect)
	{
		m_compositeEffect->SetInput(0, nullptr);
	}
	m_effectOutput = nullptr;
}