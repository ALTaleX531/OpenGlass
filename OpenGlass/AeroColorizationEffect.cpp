#include "pch.h"
#include "resource.h"
#include "Util.hpp"
#include "AeroColorizationEffect.hpp"

using namespace OpenGlass;

STDMETHODIMP CAeroColorizationEffect::Register(ID2D1Factory1* factory)
{
	const D2D1_PROPERTY_BINDING bindings[] =
	{
		D2D1_VALUE_TYPE_BINDING(L"vecAfterglow", &SetAfterglow, &GetAfterglow),
		D2D1_VALUE_TYPE_BINDING(L"vecBlurBalance", &SetBlurBalance, &GetBlurBalance),
		D2D1_VALUE_TYPE_BINDING(L"vecColor", &SetColor, &GetColor),
		D2D1_VALUE_TYPE_BINDING(L"vecFallback", &SetFallback, &GetFallback),
	};

	return factory->RegisterEffectFromString(
		CLSID_AeroColorizationEffect,
		L""
		"<?xml version='1.0'?>"
		"<Effect>"
			"<!-- System Properties -->"
			"<Property name='DisplayName' type='string' value='OpenGlass Aero Glass Colorization Pixel Shader Effect'/>"
			"<Property name='Author' type='string' value='ALTaleX'/>"
			"<Property name='Category' type='string'/>"
			"<Property name='Description' type='string' value='A pixel shader for colorization of aero glass.'/>"
			"<Inputs>"
				"<Input name='Source'/>"
			"</Inputs>"
			"<!-- Custom Properties go here -->"
			"<Property name='vecAfterglow' type='vector4'>"
				"<Property name='DisplayName' type='string' value='Afterglow Color'/>"
				"<Property name='Default' type='vector4' value='(0.0, 0.0, 0.0, 0.0)' />"
			"</Property>"
				"<Property name='vecBlurBalance' type='vector4'>"
				"<Property name='DisplayName' type='string' value='Blur Balance'/>"
				"<Property name='Default' type='vector4' value='(0.0, 0.0, 0.0, 0.0)' />"
			"</Property>"
			"<Property name='vecColor' type='vector4'>"
				"<Property name='DisplayName' type='string' value='Color'/>"
				"<Property name='Default' type='vector4' value='(0.0, 0.0, 0.0, 0.0)' />"
			"</Property>"
			"<Property name='vecFallback' type='vector4'>"
				"<Property name='DisplayName' type='string' value='Fallback Color'/>"
				"<Property name='Default' type='vector4' value='(0.0, 0.0, 0.0, 0.0)' />"
			"</Property>"
		"</Effect>",
		bindings,
		std::size(bindings),
		CAeroColorizationEffect::Create
	);
}

STDMETHODIMP CAeroColorizationEffect::UnRegister(ID2D1Factory1* factory)
{
	return factory->UnregisterEffect(CLSID_AeroColorizationEffect);
}

STDMETHODIMP CAeroColorizationEffect::Create(IUnknown** effect) noexcept
{
	*effect = static_cast<ID2D1EffectImpl*>(new (std::nothrow) CAeroColorizationEffect());
	RETURN_HR_IF_NULL(E_OUTOFMEMORY, *effect);
	return S_OK;
}

HRESULT CAeroColorizationEffect::SetAfterglow(D2D1_VECTOR_4F afterglow)
{
	m_constants.afterglow = afterglow;
	return S_OK;
}

D2D1_VECTOR_4F CAeroColorizationEffect::GetAfterglow() const
{
	return m_constants.afterglow;
}

HRESULT CAeroColorizationEffect::SetBlurBalance(D2D1_VECTOR_4F blurBalance)
{
	m_constants.blurBalance = blurBalance;
	return S_OK;
}

D2D1_VECTOR_4F CAeroColorizationEffect::GetBlurBalance() const
{
	return m_constants.blurBalance;
}

HRESULT CAeroColorizationEffect::SetColor(D2D1_VECTOR_4F color)
{
	m_constants.color = color;
	return S_OK;
}
D2D1_VECTOR_4F CAeroColorizationEffect::GetColor() const
{
	return m_constants.color;
}

HRESULT CAeroColorizationEffect::SetFallback(D2D1_VECTOR_4F fallback)
{
	m_constants.fallback = fallback;
	return S_OK;
}

D2D1_VECTOR_4F CAeroColorizationEffect::GetFallback() const
{
	return m_constants.fallback;
}

HRESULT CAeroColorizationEffect::UpdateConstants()
{
	return m_drawInfo->SetPixelShaderConstantBuffer(reinterpret_cast<BYTE*>(&m_constants), sizeof(m_constants));
}

IFACEMETHODIMP CAeroColorizationEffect::Initialize(
	ID2D1EffectContext* effectContext,
	ID2D1TransformGraph* transformGraph
)
{
	if (!effectContext->IsShaderLoaded(CLSID_AeroColorizationEffectPixelShader))
	{
		std::span<const UCHAR> shaderBytes{};
		RETURN_IF_FAILED(Util::GetResDataView(shaderBytes, IDR_RCDATA_AEROCOLORIZATION_PS));

		RETURN_IF_FAILED(
			effectContext->LoadPixelShader(
				CLSID_AeroColorizationEffectPixelShader,
				shaderBytes.data(),
				static_cast<UINT32>(shaderBytes.size_bytes())
			)
		);
	}
	RETURN_IF_FAILED(transformGraph->SetSingleTransformNode(static_cast<ID2D1DrawTransform*>(this)));
	m_effectContext.copy_from(effectContext);

	return S_OK;
}

IFACEMETHODIMP CAeroColorizationEffect::PrepareForRender([[maybe_unused]] D2D1_CHANGE_TYPE changeType)
{
	return UpdateConstants();
}

IFACEMETHODIMP CAeroColorizationEffect::SetGraph([[maybe_unused]] ID2D1TransformGraph* graph)
{
	return E_NOTIMPL;
}

IFACEMETHODIMP CAeroColorizationEffect::CAeroColorizationEffect::SetDrawInfo(ID2D1DrawInfo* drawInfo)
{
	m_drawInfo.copy_from(drawInfo);
	return m_drawInfo->SetPixelShader(CLSID_AeroColorizationEffectPixelShader);
}

IFACEMETHODIMP CAeroColorizationEffect::MapOutputRectToInputRects(
	const D2D1_RECT_L* outputRect,
	D2D1_RECT_L* inputRects,
	UINT32 inputRectCount
) const
{
	if (inputRectCount != 1)
	{
		return E_INVALIDARG;
	}

	inputRects[0].left = outputRect->left;
	inputRects[0].top = outputRect->top;
	inputRects[0].right = outputRect->right;
	inputRects[0].bottom = outputRect->bottom;

	return S_OK;
}

IFACEMETHODIMP CAeroColorizationEffect::MapInputRectsToOutputRect(
	CONST D2D1_RECT_L* inputRects,
	[[maybe_unused]] CONST D2D1_RECT_L* inputOpaqueSubRects,
	UINT32 inputRectCount,
	D2D1_RECT_L* outputRect,
	D2D1_RECT_L* outputOpaqueSubRect
)
{
	if (inputRectCount != 1)
	{
		return E_INVALIDARG;
	}

	*outputRect = inputRects[0];
	*outputOpaqueSubRect = {};

	return S_OK;
}

IFACEMETHODIMP CAeroColorizationEffect::MapInvalidRect(
	[[maybe_unused]] UINT32 inputIndex,
	D2D1_RECT_L invalidInputRect,
	D2D1_RECT_L* invalidOutputRect
) const
{
	*invalidOutputRect = invalidInputRect;

	return S_OK;
}

IFACEMETHODIMP_(UINT32) CAeroColorizationEffect::GetInputCount() const
{
	return 1;
}

// D2D ensures that that effects are only referenced from one thread at a time.
// To improve performance, we simply increment/decrement our reference count
// rather than use atomic InterlockedIncrement()/InterlockedDecrement() functions.
IFACEMETHODIMP_(ULONG) CAeroColorizationEffect::AddRef()
{
	return (++m_refCount);
}

IFACEMETHODIMP_(ULONG) CAeroColorizationEffect::Release()
{
	if ((--m_refCount) == 0)
	{
		delete this;
		return 0;
	}

	return m_refCount;
}

// This enables the stack of parent interfaces to be queried. In the instance
// of the CAeroColorizationEffect interface, this method simply enables the developer
// to cast a CAeroColorizationEffect instance to an ID2D1EffectImpl or IUnknown instance.
IFACEMETHODIMP CAeroColorizationEffect::QueryInterface(
	REFIID riid,
	void** output
)
{
	*output = nullptr;

	if (riid == __uuidof(ID2D1EffectImpl))
	{
		*output = static_cast<ID2D1EffectImpl*>(this);
	}
	else if (riid == __uuidof(ID2D1DrawTransform))
	{
		*output = static_cast<ID2D1DrawTransform*>(this);
	}
	else if (riid == __uuidof(ID2D1Transform))
	{
		*output = static_cast<ID2D1Transform*>(this);
	}
	else if (riid == __uuidof(ID2D1TransformNode))
	{
		*output = static_cast<ID2D1TransformNode*>(this);
	}
	else if (riid == __uuidof(IUnknown))
	{
		*output = this;
	}
	else
	{
		return E_NOINTERFACE;
	}

	if (*output != nullptr)
	{
		AddRef();
	}

	return S_OK;
}
