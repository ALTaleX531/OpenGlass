#include "pch.h"
#include "HookHelper.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "GlassRenderer.hpp"
#include "GlassEffectManager.hpp"
#include "GlassSharedData.hpp"
#include "ReflectionRenderer.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassRenderer
{
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
	HRESULT STDMETHODCALLTYPE MyCRenderData_DrawSolidColorRectangle(
		dwmcore::CResource* This,
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CResource* drawListEntryBuilder,
		bool unknwon,
		const D2D1_RECT_F& lprc,
		const D2D1_COLOR_F& color
	);
	HRESULT STDMETHODCALLTYPE MyCDrawingContext_DrawSolidRectangle(
		dwmcore::IDrawingContext* This,
		const D2D1_RECT_F& rectangle,
		const D2D1_COLOR_F& color
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
	void STDMETHODCALLTYPE MyCGeometry_Destructor(dwmcore::CGeometry* This);
	HRESULT STDMETHODCALLTYPE MyCDirtyRegion__Add(
		dwmcore::CDirtyRegion* This,
		dwmcore::CVisual* visual,
		bool unknown,
		const D2D1_RECT_F& lprc
	);

	decltype(&MyCRenderData_TryDrawCommandAsDrawList) g_CRenderData_TryDrawCommandAsDrawList_Org{ nullptr };
	decltype(&MyCRenderData_DrawSolidColorRectangle) g_CRenderData_DrawSolidColorRectangle_Org{ nullptr };
	decltype(&MyCDrawingContext_DrawSolidRectangle) g_CDrawingContext_DrawSolidRectangle_Org{ nullptr };
	decltype(&MyCDrawingContext_DrawSolidRectangle)* g_CDrawingContext_DrawSolidRectangle_Org_Address{ nullptr };
	decltype(&MyCDrawingContext_DrawGeometry) g_CDrawingContext_DrawGeometry_Org{ nullptr };
	decltype(&MyCDrawingContext_DrawGeometry)* g_CDrawingContext_DrawGeometry_Org_Address{ nullptr };
	decltype(&MyID2D1DeviceContext_FillGeometry) g_ID2D1DeviceContext_FillGeometry_Org{ nullptr };
	decltype(&MyID2D1DeviceContext_FillGeometry)* g_ID2D1DeviceContext_FillGeometry_Org_Address{ nullptr };
	decltype(&MyCGeometry_Destructor) g_CGeometry_Destructor_Org{ nullptr };
	decltype(&MyCDirtyRegion__Add) g_CDirtyRegion__Add_Org{ nullptr };
	PVOID* g_CSolidColorLegacyMilBrush_vftable{ nullptr };

	std::optional<D2D1_COLOR_F> g_drawColor{};
	GlassEffectManager::IGlassEffect* g_glassEffectNoRef{ nullptr };
	dwmcore::IDrawingContext* g_drawingContextNoRef{ nullptr };
	ID2D1Device* g_deviceNoRef{ nullptr };

	Type g_type{ Type::Blur };
	float g_blurAmount{ 9.f };
	float g_glassOpacity{ 0.63f };
	// exclusively used by aero backdrop
	float g_colorBalance{ 0.f };
	float g_afterglowBalance{ 0.43f };
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
	g_drawColor = std::nullopt;
	HRESULT hr{ g_CRenderData_TryDrawCommandAsDrawList_Org(This, drawingContext, drawListCache, drawListEntryBuilder, unknwon, commandType, resources, succeeded) };
	if (SUCCEEDED(hr) && g_drawColor)
	{
		*succeeded = false;
	}
	return hr;
}
HRESULT STDMETHODCALLTYPE GlassRenderer::MyCRenderData_DrawSolidColorRectangle(
	dwmcore::CResource* This,
	dwmcore::CDrawingContext* drawingContext,
	dwmcore::CResource* drawListEntryBuilder,
	bool unknwon,
	const D2D1_RECT_F& lprc,
	const D2D1_COLOR_F& color
)
{
	if (
		color.a != 1.f &&
		GlassSharedData::IsBackdropAllowed()
	)
	{
		g_drawColor = color;

		if (!g_CDrawingContext_DrawSolidRectangle_Org)
		{
			g_CDrawingContext_DrawSolidRectangle_Org_Address = reinterpret_cast<decltype(g_CDrawingContext_DrawSolidRectangle_Org_Address)>(&(HookHelper::vtbl_of(drawingContext->GetInterface())[2]));
			g_CDrawingContext_DrawSolidRectangle_Org = HookHelper::WritePointer(g_CDrawingContext_DrawSolidRectangle_Org_Address, MyCDrawingContext_DrawSolidRectangle);
		}
		if (!g_CDrawingContext_DrawGeometry_Org)
		{
			g_CDrawingContext_DrawGeometry_Org_Address = reinterpret_cast<decltype(g_CDrawingContext_DrawGeometry_Org_Address)>(&(HookHelper::vtbl_of(drawingContext->GetInterface())[4]));
			g_CDrawingContext_DrawGeometry_Org = HookHelper::WritePointer(g_CDrawingContext_DrawGeometry_Org_Address, MyCDrawingContext_DrawGeometry);
		}

		return S_OK;
	}

	return g_CRenderData_DrawSolidColorRectangle_Org(
		This,
		drawingContext,
		drawListEntryBuilder,
		unknwon,
		lprc,
		color
	);
}


HRESULT STDMETHODCALLTYPE GlassRenderer::MyCDrawingContext_DrawSolidRectangle(
	dwmcore::IDrawingContext* This,
	const D2D1_RECT_F& rectangle,
	const D2D1_COLOR_F& color
)
{
	if (!g_drawColor.has_value())
	{
		return g_CDrawingContext_DrawSolidRectangle_Org(This, rectangle, color);
	}

	D2D1_COLOR_F convertedColor{ dwmcore::Convert_D2D1_COLOR_F_scRGB_To_D2D1_COLOR_F_sRGB(g_drawColor.value()) };
	g_drawColor = std::nullopt;
	return g_CDrawingContext_DrawSolidRectangle_Org(This, rectangle, convertedColor);
}

bool active = false;

HRESULT STDMETHODCALLTYPE GlassRenderer::MyCDrawingContext_DrawGeometry(
	dwmcore::IDrawingContext* This,
	dwmcore::CLegacyMilBrush* brush,
	dwmcore::CGeometry* geometry
)
{
	HRESULT hr{ g_CDrawingContext_DrawGeometry_Org(This, brush, geometry) };
	if (
		!brush ||
		!geometry ||
		HookHelper::vtbl_of(brush) != g_CSolidColorLegacyMilBrush_vftable ||
		!g_drawColor.has_value()
	)
	{
		return hr;
	}

	auto cleanUp{ wil::scope_exit([]{ g_drawColor = std::nullopt; })};
	D2D1_COLOR_F color{ dwmcore::Convert_D2D1_COLOR_F_scRGB_To_D2D1_COLOR_F_sRGB(g_drawColor.value()) };
	dwmcore::CShapePtr geometryShape{};
	if (
		FAILED(geometry->GetShapeData(nullptr, &geometryShape)) ||
		!geometryShape ||
		geometryShape->IsEmpty()
	)
	{
		return hr;
	}

	bool normalDesktopRender{ This->GetDrawingContext()->IsNormalDesktopRender() };
	D2D1_RECT_F boundRect{};
	RETURN_IF_FAILED(geometryShape->GetTightBounds(&boundRect, nullptr));
	D2D1_RECT_F worldSpaceClippedBounds{};
	This->GetDrawingContext()->CalcWorldSpaceClippedBounds(boundRect, &worldSpaceClippedBounds);
	D2D1_RECT_F localSpaceClippedBounds{};
	This->GetDrawingContext()->CalcLocalSpaceClippedBounds(boundRect, &localSpaceClippedBounds);

	if (
		g_drawColor.value().a == 1.f ||
		(
			normalDesktopRender &&
			(
				worldSpaceClippedBounds.right <= worldSpaceClippedBounds.left ||
				worldSpaceClippedBounds.bottom <= worldSpaceClippedBounds.top ||
				localSpaceClippedBounds.right <= localSpaceClippedBounds.left ||
				localSpaceClippedBounds.bottom <= localSpaceClippedBounds.top
			)
		)
	)
	{
		return This->GetDrawingContext()->FillShapeWithColor(geometryShape.ptr, &color);
	}

	auto deviceContext{ This->GetDrawingContext()->GetD2DContext()->GetDeviceContext() };
	winrt::com_ptr<ID2D1Device> device{ nullptr };
	deviceContext->GetDevice(device.put());
	// device lost
	if (g_deviceNoRef != device.get())
	{
		g_deviceNoRef = device.get();
		ReflectionRenderer::g_reflectionBitmap = nullptr;
		GlassEffectManager::Shutdown();
	}
	// allocate glass effect
	winrt::com_ptr<GlassEffectManager::IGlassEffect> glassEffect{ GlassEffectManager::GetOrCreate(geometry, deviceContext, true) };
	if (!glassEffect)
	{
		return This->GetDrawingContext()->FillShapeWithColor(geometryShape.ptr, &color);
	}
	// prepare ID2D1DeviceContext::FillGeometry
	g_glassEffectNoRef = glassEffect.get();
	g_drawingContextNoRef = This;
	if (!g_ID2D1DeviceContext_FillGeometry_Org)
	{
		g_ID2D1DeviceContext_FillGeometry_Org_Address = reinterpret_cast<decltype(g_ID2D1DeviceContext_FillGeometry_Org_Address)>(&HookHelper::vtbl_of(This->GetDrawingContext()->GetD2DContext()->GetDeviceContext())[0x17]);
		g_ID2D1DeviceContext_FillGeometry_Org = HookHelper::WritePointer(g_ID2D1DeviceContext_FillGeometry_Org_Address, MyID2D1DeviceContext_FillGeometry);
	}

	// hahaha, i can't belive i can actually convert ID2D1Image into ID2D1Bitmap1!
	winrt::com_ptr<ID2D1Image> backdropImage{};
	This->GetDrawingContext()->GetD2DContext()->GetDeviceContext()->GetTarget(backdropImage.put());
	winrt::com_ptr<ID2D1Bitmap1> backdropBitmap{ backdropImage.as<ID2D1Bitmap1>() };
	RETURN_IF_FAILED(This->GetDrawingContext()->FlushD2D());
	//TODO: ADD PROPER TYPE CHECKED CAST 
	bool bactive = false;
	if (GlassSharedData::g_LastTopLevelWindow)
		bactive = (reinterpret_cast<uDwm::CTopLevelWindow*>(GlassSharedData::g_LastTopLevelWindow)->TreatAsActiveWindow());

	//bool bactive = true;

	dwmcore::CMILMatrix matrix{};
	D2D1_RECT_F shapeWorldBounds{};
	RETURN_IF_FAILED(This->GetDrawingContext()->GetWorldTransform(&matrix));
	RETURN_IF_FAILED(geometryShape->GetTightBounds(&shapeWorldBounds, &matrix));
	glassEffect->SetGlassRenderingParameters(
		color,
		g_glassOpacity,
		g_blurAmount,
		GlassSharedData::g_ColorizationAfterglowBalance,
		bactive ? GlassSharedData::g_ColorizationBlurBalance : 0.4f * GlassSharedData::g_ColorizationBlurBalance + 0.6f,
		GlassSharedData::g_ColorizationColorBalance,
		g_type
	);
	glassEffect->SetSize(
		D2D1::SizeF(
			boundRect.right - boundRect.left,
			boundRect.bottom - boundRect.top
		)
	);
	RETURN_IF_FAILED(
		glassEffect->Invalidate(
			backdropBitmap.get(), 
			D2D1::Point2F(shapeWorldBounds.left, shapeWorldBounds.top),
			worldSpaceClippedBounds,
			normalDesktopRender
		)
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
		g_glassEffectNoRef && 
		g_drawingContextNoRef
	)
	{
		auto glassEffect{ g_glassEffectNoRef };
		g_drawingContextNoRef = nullptr;
		g_glassEffectNoRef = nullptr;

		LOG_IF_FAILED(glassEffect->Render(geometry, solidColorBrush.get()));
		return;
	}
	return g_ID2D1DeviceContext_FillGeometry_Org(This, geometry, brush, opacityBrush);
}

void STDMETHODCALLTYPE GlassRenderer::MyCGeometry_Destructor(dwmcore::CGeometry* This)
{
	GlassEffectManager::Remove(This);
	return g_CGeometry_Destructor_Org(This);
}

// this is not the best way to mitigate artifacts, it needs to be reworked in the future!
HRESULT STDMETHODCALLTYPE GlassRenderer::MyCDirtyRegion__Add(
	dwmcore::CDirtyRegion* This,
	dwmcore::CVisual* visual,
	bool unknown,
	const D2D1_RECT_F& lprc
)
{
	float extendAmount{ g_blurAmount * 3.f + 0.5f };
	D2D1_RECT_F extendedDirtyRectangle
	{
		lprc.left - extendAmount,
		lprc.top - extendAmount,
		lprc.right + extendAmount,
		lprc.bottom + extendAmount
	};
	active = unknown;
	return g_CDirtyRegion__Add_Org(
		This,
		visual,
		unknown,
		extendedDirtyRectangle
	);
}

void GlassRenderer::UpdateConfiguration(ConfigurationFramework::UpdateType type)
{
	if (type & ConfigurationFramework::UpdateType::Backdrop)
	{
		g_blurAmount = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"BlurDeviation", 30)) / 10.f * 3.f, 0.f, 250.f);
		g_glassOpacity = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"GlassOpacity", 63)) / 100.f, 0.f, 1.f);
		WCHAR reflectionTexturePath[MAX_PATH + 1]{};
		ConfigurationFramework::DwmGetStringFromHKCUAndHKLM(L"CustomThemeReflection", reflectionTexturePath);
		ReflectionRenderer::g_reflectionTexturePath = reflectionTexturePath;
		ReflectionRenderer::g_reflectionIntensity = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationGlassReflectionIntensity")) / 100.f, 0.f, 1.f);
		ReflectionRenderer::g_reflectionParallaxIntensity = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationGlassReflectionParallaxIntensity", 10)) / 100.f, 0.f, 1.f);
		ReflectionRenderer::g_reflectionBitmap = nullptr;

		auto colorBalance{ ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"ColorizationColorBalanceOverride") };
		if (!colorBalance.has_value())
		{
			colorBalance = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationColorBalance", 0);
		}
		g_colorBalance = std::clamp(static_cast<float>(colorBalance.value()) / 100.f, 0.f, 1.f);
		auto afterglowBalance{ ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"ColorizationAfterglowBalanceOverride") };
		if (!afterglowBalance.has_value())
		{
			afterglowBalance = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationAfterglowBalance", 43);
		}
		g_afterglowBalance = std::clamp(static_cast<float>(afterglowBalance.value()) / 100.f, 0.f, 1.f);

		g_type = static_cast<Type>(std::clamp(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"GlassType", 0), 0ul, 4ul));
	}
}

HRESULT GlassRenderer::Startup()
{
	dwmcore::GetAddressFromSymbolMap("CRenderData::TryDrawCommandAsDrawList", g_CRenderData_TryDrawCommandAsDrawList_Org);
	dwmcore::GetAddressFromSymbolMap("CRenderData::DrawSolidColorRectangle", g_CRenderData_DrawSolidColorRectangle_Org);
	dwmcore::GetAddressFromSymbolMap("CGeometry::~CGeometry", g_CGeometry_Destructor_Org);
	dwmcore::GetAddressFromSymbolMap("CSolidColorLegacyMilBrush::`vftable'", g_CSolidColorLegacyMilBrush_vftable);
	dwmcore::GetAddressFromSymbolMap("CDirtyRegion::_Add", g_CDirtyRegion__Add_Org);

	return HookHelper::Detours::Write([]()
	{
		HookHelper::Detours::Attach(&g_CRenderData_TryDrawCommandAsDrawList_Org, MyCRenderData_TryDrawCommandAsDrawList);
		HookHelper::Detours::Attach(&g_CRenderData_DrawSolidColorRectangle_Org, MyCRenderData_DrawSolidColorRectangle);
		HookHelper::Detours::Attach(&g_CGeometry_Destructor_Org, MyCGeometry_Destructor);
		HookHelper::Detours::Attach(&g_CDirtyRegion__Add_Org, MyCDirtyRegion__Add);
	});
}

void GlassRenderer::Shutdown()
{
	HookHelper::Detours::Write([]()
	{
		HookHelper::Detours::Detach(&g_CRenderData_TryDrawCommandAsDrawList_Org, MyCRenderData_TryDrawCommandAsDrawList);
		HookHelper::Detours::Detach(&g_CRenderData_DrawSolidColorRectangle_Org, MyCRenderData_DrawSolidColorRectangle);
		HookHelper::Detours::Detach(&g_CGeometry_Destructor_Org, MyCGeometry_Destructor);
		HookHelper::Detours::Detach(&g_CDirtyRegion__Add_Org, MyCDirtyRegion__Add);
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
	if (g_CDrawingContext_DrawSolidRectangle_Org)
	{
		HookHelper::WritePointer(g_CDrawingContext_DrawSolidRectangle_Org_Address, g_CDrawingContext_DrawSolidRectangle_Org);
		g_CDrawingContext_DrawSolidRectangle_Org_Address = nullptr;
		g_CDrawingContext_DrawSolidRectangle_Org = nullptr;
	}

	GlassEffectManager::Shutdown();
	ReflectionRenderer::g_reflectionBitmap = nullptr;
}