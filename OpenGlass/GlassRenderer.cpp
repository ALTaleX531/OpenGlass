#include "pch.h"
#include "HookHelper.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "Shared.hpp"
#include "GlassRenderer.hpp"
#include "GlassEffect.hpp"
#include "ReflectionEffect.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassRenderer
{
	bool STDMETHODCALLTYPE MyCSolidColorLegacyMilBrush_IsOfType(
		dwmcore::CSolidColorLegacyMilBrush* This, 
		UINT type
	);
	HRESULT STDMETHODCALLTYPE MyCRenderData_TryDrawCommandAsDrawList(
		dwmcore::CResource* This,
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CDrawListCache* drawListCache,
		dwmcore::CResource* drawListEntryBuilder,
		bool unknwon,
		int commandType,
		dwmcore::CResource** resources,
		bool* succeeded
	);
	HRESULT STDMETHODCALLTYPE MyCDrawingContext_DrawGeometry(
		dwmcore::IDrawingContext* This,
		dwmcore::CLegacyMilBrush* brush,
		dwmcore::CGeometry* geometry
	);
	void STDMETHODCALLTYPE MyID2D1DeviceContext_FillGeometry(
		ID2D1DeviceContext* This,
		ID2D1Geometry* geometry,
		ID2D1Brush* brush,
		ID2D1Brush* opacityBrush
	);
	HRESULT STDMETHODCALLTYPE MyCDirtyRegion__Add(
		dwmcore::CDirtyRegion* This,
		dwmcore::CVisual* visual,
		bool unknown,
		const D2D1_RECT_F& lprc
	);
	void STDMETHODCALLTYPE MyCGeometry_Destructor(dwmcore::CGeometry* This);

	decltype(&MyCSolidColorLegacyMilBrush_IsOfType) g_CSolidColorLegacyMilBrush_IsOfType_Org{ nullptr };
	decltype(&MyCRenderData_TryDrawCommandAsDrawList) g_CRenderData_TryDrawCommandAsDrawList_Org{ nullptr };
	decltype(&MyCDrawingContext_DrawGeometry) g_CDrawingContext_DrawGeometry_Org{ nullptr };
	decltype(&MyCDrawingContext_DrawGeometry)* g_CDrawingContext_DrawGeometry_Org_Address{ nullptr };
	decltype(&MyID2D1DeviceContext_FillGeometry) g_ID2D1DeviceContext_FillGeometry_Org{ nullptr };
	decltype(&MyID2D1DeviceContext_FillGeometry)* g_ID2D1DeviceContext_FillGeometry_Org_Address{ nullptr };
	decltype(&MyCDirtyRegion__Add) g_CDirtyRegion__Add_Org{ nullptr };
	decltype(&MyCGeometry_Destructor) g_CGeometry_Destructor_Org{ nullptr };
	PVOID* g_CSolidColorLegacyMilBrush_vftable{ nullptr };

	IGlassEffect* g_glassEffectNoRef{ nullptr };
	ID2D1Device* g_deviceNoRef{ nullptr };
	dwmcore::IDrawingContext* g_drawingContextNoRef{ nullptr };
	bool g_solidColorLegacyMilBrush{ false };
}

bool STDMETHODCALLTYPE GlassRenderer::MyCSolidColorLegacyMilBrush_IsOfType(
	dwmcore::CSolidColorLegacyMilBrush* This, 
	UINT type
)
{
	g_solidColorLegacyMilBrush = true;
	return g_CSolidColorLegacyMilBrush_IsOfType_Org(This, type);
}

HRESULT STDMETHODCALLTYPE GlassRenderer::MyCRenderData_TryDrawCommandAsDrawList(
	dwmcore::CResource* This,
	dwmcore::CDrawingContext* drawingContext,
	dwmcore::CDrawListCache* drawListCache,
	dwmcore::CResource* drawListEntryBuilder,
	bool unknwon,
	int commandType,
	dwmcore::CResource** resources,
	bool* succeeded
)
{
	g_solidColorLegacyMilBrush = false;
	auto hr = g_CRenderData_TryDrawCommandAsDrawList_Org(This, drawingContext, drawListCache, drawListEntryBuilder, unknwon, commandType, resources, succeeded);

	if (SUCCEEDED(hr) && g_solidColorLegacyMilBrush && commandType == 461)
	{
		if (!g_CDrawingContext_DrawGeometry_Org)
		{
			g_CDrawingContext_DrawGeometry_Org_Address = reinterpret_cast<decltype(g_CDrawingContext_DrawGeometry_Org_Address)>(&(HookHelper::vtbl_of(drawingContext->GetInterface())[4]));
			g_CDrawingContext_DrawGeometry_Org = HookHelper::WritePointer(g_CDrawingContext_DrawGeometry_Org_Address, MyCDrawingContext_DrawGeometry);
		}
		*succeeded = false;

		return S_OK;
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE GlassRenderer::MyCDrawingContext_DrawGeometry(
	dwmcore::IDrawingContext* This,
	dwmcore::CLegacyMilBrush* brush,
	dwmcore::CGeometry* geometry
)
{
	if (
		!brush ||
		!geometry ||
		HookHelper::vtbl_of(brush) != g_CSolidColorLegacyMilBrush_vftable
	)
	{
		return g_CDrawingContext_DrawGeometry_Org(This, brush, geometry);
	}

	auto color = dwmcore::Convert_D2D1_COLOR_F_scRGB_To_D2D1_COLOR_F_sRGB(reinterpret_cast<dwmcore::CSolidColorLegacyMilBrush*>(brush)->GetRealizedColor());

	// shape is nullptr or empty
	dwmcore::CShapePtr geometryShape{};
	if (
		FAILED(geometry->GetShapeData(nullptr, &geometryShape)) ||
		!geometryShape ||
		geometryShape->IsEmpty()
	)
	{
		return g_CDrawingContext_DrawGeometry_Org(This, brush, geometry);
	}

	// not our job
	if (color.a == 1.f)
	{
		return This->GetDrawingContext()->FillShapeWithColor(geometryShape.ptr, &color);
	}

	if (!g_ID2D1DeviceContext_FillGeometry_Org)
	{
		g_ID2D1DeviceContext_FillGeometry_Org_Address = reinterpret_cast<decltype(g_ID2D1DeviceContext_FillGeometry_Org_Address)>(&HookHelper::vtbl_of(This->GetDrawingContext()->GetD2DContext()->GetDeviceContext())[0x17]);
		g_ID2D1DeviceContext_FillGeometry_Org = HookHelper::WritePointer(g_ID2D1DeviceContext_FillGeometry_Org_Address, MyID2D1DeviceContext_FillGeometry);
	}

	if (color.a == 0.25f)
	{
		g_drawingContextNoRef = This;
		return This->GetDrawingContext()->FillShapeWithColor(geometryShape.ptr, &color);
	}

	winrt::com_ptr<ID2D1Device> device{ nullptr };
	auto context = This->GetDrawingContext()->GetD2DContext()->GetDeviceContext();
	context->GetDevice(device.put());
	// device lost
	if (g_deviceNoRef != device.get())
	{
		g_deviceNoRef = device.get();
		ReflectionEffect::Reset();
		GlassEffectFactory::Shutdown();
	}

	// allocate glass effect
	auto glassEffect = GlassEffectFactory::GetOrCreate(geometry, true);
	if (!glassEffect)
	{
		return This->GetDrawingContext()->FillShapeWithColor(geometryShape.ptr, &color);
	}

	// prepare and hook ID2D1DeviceContext::FillGeometry
	RETURN_IF_FAILED(This->GetDrawingContext()->FlushD2D());
	g_glassEffectNoRef = glassEffect.get();
	g_drawingContextNoRef = This;

	// hack! but a bit better
	auto active = color.a == 0.5f;
	// the reason why i keep it that is the hacky way is much faster and the normal way is less stable
	// normally, you can get the HWND from CDrawingContext::PreSubgraph like this
	/*
	HRESULT STDMETHODCALLTYPE GlassRenderer::MyCDrawingContext_PreSubgraph(
		dwmcore::CDrawingContext* This,
		dwmcore::CVisualTree* visualTree,
		bool* conditionalBreak
	)
	{
		HWND hwnd{ reinterpret_cast<dwmcore::CDrawingContext*>(This->GetD2DContextOwner())->GetCurrentVisual()->GetHwnd() };
		if (hwnd && g_current != hwnd) { g_current = hwnd; g_data = nullptr; }
		HRESULT hr{ g_CDrawingContext_PreSubgraph_Org(This, visualTree, conditionalBreak) };

		return hr;
	}
	*/
	glassEffect->SetGlassRenderingParameters(
		(Shared::g_type == Shared::Type::Aero) ? Shared::g_color : color,
		Shared::g_afterglow,
		Shared::g_glassOpacity,
		Shared::g_blurAmount,
		active ? Shared::g_colorBalance : (0.4f * Shared::g_colorBalance),		// y = 0.4x
		Shared::g_afterglowBalance,												// stays the same
		active ? Shared::g_blurBalance : (0.4f * Shared::g_blurBalance + 0.6f),	// y = 0.4x + 60
		Shared::g_type
	);

	return This->GetDrawingContext()->FillShapeWithColor(geometryShape.ptr, &color);
}

void STDMETHODCALLTYPE GlassRenderer::MyID2D1DeviceContext_FillGeometry(
	ID2D1DeviceContext* This,
	ID2D1Geometry* geometry,
	ID2D1Brush* brush,
	ID2D1Brush* opacityBrush
)
{
	winrt::com_ptr<ID2D1SolidColorBrush> solidColorBrush{ nullptr };
	if (
		SUCCEEDED(brush->QueryInterface(solidColorBrush.put())) && 
		g_drawingContextNoRef
	)
	{
		if (g_glassEffectNoRef)
		{
			D2D1_RECT_F clipWorldBounds{};
			D2D1_ANTIALIAS_MODE mode{};
			g_drawingContextNoRef->GetDrawingContext()->GetD2DContext()->GetClip(
				g_drawingContextNoRef->GetDrawingContext()->GetD2DContextOwner(),
				&clipWorldBounds,
				&mode
			);

			auto hr = g_glassEffectNoRef->Render(
				This, 
				geometry, 
				clipWorldBounds,
				g_drawingContextNoRef->GetDrawingContext()->IsNormalDesktopRender()
			);
			if (FAILED(hr))
			{
				LOG_HR(hr);
				g_ID2D1DeviceContext_FillGeometry_Org(This, geometry, brush, opacityBrush);
			}

			g_glassEffectNoRef = nullptr;
		}
		else
		{
			LOG_IF_FAILED(
				ReflectionEffect::Render(
					This,
					geometry,
					Shared::g_reflectionIntensity,
					0.f
				)
			);
		}

		g_drawingContextNoRef = nullptr;
		return;
	}
	return g_ID2D1DeviceContext_FillGeometry_Org(This, geometry, brush, opacityBrush);
}

// this is not the best way to mitigate artifacts, it needs to be reworked in the future!
HRESULT STDMETHODCALLTYPE GlassRenderer::MyCDirtyRegion__Add(
	dwmcore::CDirtyRegion* This,
	dwmcore::CVisual* visual,
	bool unknown,
	const D2D1_RECT_F& lprc
)
{
	if (Shared::g_enableFullDirty)
	{
		return g_CDirtyRegion__Add_Org(
			This,
			visual,
			unknown,
			lprc
		);
	}

	// at high blur radius, there is no need to extend that much,
	// it will only cause severe flickering
	float extendAmount{ min(Shared::g_blurAmount * 3.f + 0.5f, 15.5f) };
	D2D1_RECT_F extendedDirtyRectangle
	{
		lprc.left - extendAmount,
		lprc.top - extendAmount,
		lprc.right + extendAmount,
		lprc.bottom + extendAmount
	};

	return g_CDirtyRegion__Add_Org(
		This,
		visual,
		unknown,
		extendedDirtyRectangle
	);
}

void STDMETHODCALLTYPE GlassRenderer::MyCGeometry_Destructor(dwmcore::CGeometry* This)
{
	GlassEffectFactory::Remove(This);
	return g_CGeometry_Destructor_Org(This);
}

void GlassRenderer::UpdateConfiguration(ConfigurationFramework::UpdateType type)
{
	if (type & ConfigurationFramework::UpdateType::Backdrop)
	{
		Shared::g_blurAmount = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"BlurDeviation", 30)) / 10.f * 3.f, 0.f, 250.f);
		Shared::g_glassOpacity = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"GlassOpacity", 63)) / 100.f, 0.f, 1.f);
		
		WCHAR reflectionTexturePath[MAX_PATH + 1]{};
		ConfigurationFramework::DwmGetStringFromHKCUAndHKLM(L"CustomThemeReflection", reflectionTexturePath);

		ReflectionEffect::UpdateTexture(reflectionTexturePath);
		Shared::g_reflectionIntensity = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationGlassReflectionIntensity")) / 100.f, 0.f, 1.f);
		Shared::g_reflectionParallaxIntensity = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationGlassReflectionParallaxIntensity", 10)) / 100.f, 0.f, 1.f);

		std::optional<DWORD> result{ std::nullopt };

		result = ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"ColorizationColorOverride");
		if (!result.has_value())
		{
			result = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationColor", 0xff74b8fc);
		}
		Shared::g_color = Utils::FromArgb(result.value());

		result = ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"ColorizationAfterglowOverride");
		if (!result.has_value())
		{
			result = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationAfterglow", 0xff74b8fc);
		}
		Shared::g_afterglow = Utils::FromArgb(result.value());

		result = ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"ColorizationBlurBalanceOverride");
		if (!result.has_value())
		{
			result = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationBlurBalance", 49);
		}
		Shared::g_blurBalance = std::clamp(static_cast<float>(result.value()) / 100.f, 0.f, 1.f);

		result = ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"ColorizationColorBalanceOverride");
		if (!result.has_value())
		{
			result = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationColorBalance", 8);
		}
		Shared::g_colorBalance = std::clamp(static_cast<float>(result.value()) / 100.f, 0.f, 1.f);

		result = ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"ColorizationAfterglowBalanceOverride");
		if (!result.has_value())
		{
			result = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationAfterglowBalance", 43);
		}
		Shared::g_afterglowBalance = std::clamp(static_cast<float>(result.value()) / 100.f, 0.f, 1.f);

		Shared::g_type = static_cast<Shared::Type>(std::clamp(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"GlassType", 0), 0ul, 1ul));
	}
}

HRESULT GlassRenderer::Startup()
{
	dwmcore::GetAddressFromSymbolMap("CRenderData::TryDrawCommandAsDrawList", g_CRenderData_TryDrawCommandAsDrawList_Org);
	dwmcore::GetAddressFromSymbolMap("CDirtyRegion::_Add", g_CDirtyRegion__Add_Org);
	dwmcore::GetAddressFromSymbolMap("CGeometry::~CGeometry", g_CGeometry_Destructor_Org);
	dwmcore::GetAddressFromSymbolMap("CSolidColorLegacyMilBrush::`vftable'", g_CSolidColorLegacyMilBrush_vftable);
	dwmcore::GetAddressFromSymbolMap("CSolidColorLegacyMilBrush::IsOfType", g_CSolidColorLegacyMilBrush_IsOfType_Org);

	return HookHelper::Detours::Write([]()
	{
		HookHelper::Detours::Attach(&g_CSolidColorLegacyMilBrush_IsOfType_Org, MyCSolidColorLegacyMilBrush_IsOfType);
		HookHelper::Detours::Attach(&g_CRenderData_TryDrawCommandAsDrawList_Org, MyCRenderData_TryDrawCommandAsDrawList);
		HookHelper::Detours::Attach(&g_CDirtyRegion__Add_Org, MyCDirtyRegion__Add);
		HookHelper::Detours::Attach(&g_CGeometry_Destructor_Org, MyCGeometry_Destructor);
	});
}

void GlassRenderer::Shutdown()
{
	HookHelper::Detours::Write([]()
	{
		HookHelper::Detours::Detach(&g_CSolidColorLegacyMilBrush_IsOfType_Org, MyCSolidColorLegacyMilBrush_IsOfType);
		HookHelper::Detours::Detach(&g_CRenderData_TryDrawCommandAsDrawList_Org, MyCRenderData_TryDrawCommandAsDrawList);
		HookHelper::Detours::Detach(&g_CDirtyRegion__Add_Org, MyCDirtyRegion__Add);
		HookHelper::Detours::Detach(&g_CGeometry_Destructor_Org, MyCGeometry_Destructor);
	});

	if (g_ID2D1DeviceContext_FillGeometry_Org)
	{
		HookHelper::WritePointer(g_ID2D1DeviceContext_FillGeometry_Org_Address, g_ID2D1DeviceContext_FillGeometry_Org);
		g_ID2D1DeviceContext_FillGeometry_Org_Address = nullptr;
		g_ID2D1DeviceContext_FillGeometry_Org = nullptr;
	}
	if (g_CDrawingContext_DrawGeometry_Org)
	{
		HookHelper::WritePointer(g_CDrawingContext_DrawGeometry_Org_Address, g_CDrawingContext_DrawGeometry_Org);
		g_CDrawingContext_DrawGeometry_Org_Address = nullptr;
		g_CDrawingContext_DrawGeometry_Org = nullptr;
	}

	GlassEffectFactory::Shutdown();
	ReflectionEffect::Reset();
}
