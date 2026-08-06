#pragma once
#include "framework.hpp"
#include "wil.hpp"

namespace OpenGlass
{
	// {24CAD609-0A10-4CFF-AF35-BE2A5BE07790}
	DEFINE_GUID(CLSID_AeroColorizationEffectPixelShader, 0x24cad609, 0xa10, 0x4cff, 0xaf, 0x35, 0xbe, 0x2a, 0x5b, 0xe0, 0x77, 0x90);
	// {0A46CDF3-F10E-4B71-8BAA-FEF8B0351016}
	DEFINE_GUID(CLSID_AeroColorizationEffect, 0xa46cdf3, 0xf10e, 0x4b71, 0x8b, 0xaa, 0xfe, 0xf8, 0xb0, 0x35, 0x10, 0x16);

	enum AEROCOLORIZATION_PROP
	{
		AEROCOLORIZATION_PROP_AFTERGLOW,
		AEROCOLORIZATION_PROP_BLURBALANCE,
		AEROCOLORIZATION_PROP_COLOR,
		AEROCOLORIZATION_PROP_FALLBACK,
	};

	class CAeroColorizationEffect final : public ID2D1EffectImpl, public ID2D1DrawTransform
	{
	public:
		static STDMETHODIMP Register(ID2D1Factory1* factory);
		static STDMETHODIMP UnRegister(ID2D1Factory1* factory);
		static STDMETHODIMP Create(IUnknown** effect) noexcept;

		HRESULT SetAfterglow(D2D1_VECTOR_4F afterglow);
		D2D1_VECTOR_4F GetAfterglow() const;
		HRESULT SetBlurBalance(D2D1_VECTOR_4F blurBalance);
		D2D1_VECTOR_4F GetBlurBalance() const;
		HRESULT SetColor(D2D1_VECTOR_4F color);
		D2D1_VECTOR_4F GetColor() const;
		HRESULT SetFallback(D2D1_VECTOR_4F fallback);
		D2D1_VECTOR_4F GetFallback() const;

		IFACEMETHODIMP Initialize(
			ID2D1EffectContext* effectContext,
			ID2D1TransformGraph* transformGraph
		) override;
		IFACEMETHODIMP PrepareForRender(D2D1_CHANGE_TYPE changeType) override;
		IFACEMETHODIMP SetGraph(ID2D1TransformGraph* graph) override;
		IFACEMETHODIMP SetDrawInfo(ID2D1DrawInfo* drawInfo) override;
		IFACEMETHODIMP MapOutputRectToInputRects(
			const D2D1_RECT_L* outputRect,
			D2D1_RECT_L* inputRects,
			UINT32 inputRectCount
		) const override;
		IFACEMETHODIMP MapInputRectsToOutputRect(
			CONST D2D1_RECT_L* inputRects,
			CONST D2D1_RECT_L* inputOpaqueSubRects,
			UINT32 inputRectCount,
			D2D1_RECT_L* outputRect,
			D2D1_RECT_L* outputOpaqueSubRect
		) override;
		IFACEMETHODIMP MapInvalidRect(
			UINT32 inputIndex,
			D2D1_RECT_L invalidInputRect,
			D2D1_RECT_L* invalidOutputRect
		) const override;
		IFACEMETHODIMP_(UINT32) GetInputCount() const override;

		// Declare IUnknown implementation methods.
		IFACEMETHODIMP_(ULONG) AddRef() override;
		IFACEMETHODIMP_(ULONG) Release() override;
		IFACEMETHODIMP QueryInterface(REFIID riid, void** output) override;
	private:
		HRESULT UpdateConstants();

		struct Constants
		{
			D2D1_VECTOR_4F afterglow;
			D2D1_VECTOR_4F blurBalance;
			D2D1_VECTOR_4F color;
			D2D1_VECTOR_4F fallback;
		}; 

		ULONG m_refCount{ 1ul };
		Constants m_constants{};
		winrt::com_ptr<ID2D1DrawInfo> m_drawInfo{};
		winrt::com_ptr<ID2D1EffectContext> m_effectContext{};
	};
}
