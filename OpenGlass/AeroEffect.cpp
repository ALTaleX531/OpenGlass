#include "pch.h"
#include "AeroEffect.hpp"

using namespace OpenGlass;

HRESULT CAeroEffect::Initialize(ID2D1DeviceContext* context)
{
	m_customBlurEffect = winrt::make<CCustomBlurEffect>();

	if (!s_factory)
	{
		winrt::com_ptr<ID2D1Factory> factory{};
		context->GetFactory(factory.put());
		RETURN_IF_FAILED(factory->QueryInterface(s_factory.put()));
		RETURN_IF_FAILED(CAeroColorizationEffect::Register(s_factory.get()));
	}
	RETURN_IF_FAILED(
		context->CreateEffect(
			CLSID_AeroColorizationEffect,
			m_colorizationEffect.put()
		)
	);

	m_initialized = true;

	return S_OK;
}

HRESULT CAeroEffect::Build(
	ID2D1DeviceContext* context,
	ID2D1Image* inputImage,
	const D2D1_RECT_F& imageRectangle,
	const void* additionalParams
)
{
	if (!m_initialized)
	{
		RETURN_IF_FAILED(Initialize(context));
	}
	const auto params = static_cast<const CAeroParams*>(additionalParams);

	RETURN_IF_FAILED(
		m_customBlurEffect->Build(
			context,
			inputImage,
			imageRectangle,
			additionalParams
		)
	);
	RETURN_IF_FAILED(
		m_colorizationEffect->SetValue(
			AEROCOLORIZATION_PROP_AFTERGLOW,
			D2D1::Vector4F(
				params->afterglow.r,
				params->afterglow.g,
				params->afterglow.b,
				params->afterglow.a
			)
		)
	);
	RETURN_IF_FAILED(
		m_colorizationEffect->SetValue(
			AEROCOLORIZATION_PROP_BLURBALANCE,
			D2D1::Vector4F(
				params->blurBalance
			)
		)
	);
	RETURN_IF_FAILED(
		m_colorizationEffect->SetValue(
			AEROCOLORIZATION_PROP_COLOR,
			D2D1::Vector4F(
				params->color.r,
				params->color.g,
				params->color.b,
				params->color.a
			)
		)
	);
	RETURN_IF_FAILED(
		m_colorizationEffect->SetValue(
			AEROCOLORIZATION_PROP_FALLBACK,
			D2D1::Vector4F(
				params->fallback.r,
				params->fallback.g,
				params->fallback.b,
				params->fallback.a
			)
		)
	);

	winrt::com_ptr<ID2D1Image> image{};
	m_customBlurEffect->GetOutput(image.put());
	m_colorizationEffect->SetInput(0, image.get());
	m_outputEffect = m_colorizationEffect;

	return S_OK;
}

D2D1_MATRIX_3X2_F CAeroEffect::GetOutputMatrix() const
{
	return m_customBlurEffect->GetOutputMatrix();
}

void CAeroEffect::GetOutput(ID2D1Image** output) const
{
	if (m_outputEffect)
	{
		m_outputEffect->GetOutput(output);
	}
	else
	{
		m_customBlurEffect->GetOutput(output);
	}
}

void CAeroEffect::Reset()
{
	if (m_customBlurEffect)
	{
		m_customBlurEffect->Reset();
	}
	if (m_colorizationEffect)
	{
		m_colorizationEffect->SetInput(0, nullptr);
	}
}
