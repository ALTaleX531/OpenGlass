#include "pch.h"
#include "GeometryRecorder.hpp"

using namespace OpenGlass;
namespace OpenGlass::GeometryRecorder
{
	HRESULT STDMETHODCALLTYPE MyResourceHelper_CreateGeometryFromHRGN(
		HRGN hrgn,
		uDwm::CRgnGeometryProxy** geometry
	);
	HRESULT STDMETHODCALLTYPE MyResourceHelper_CreateRectangleGeometry(
		LPCRECT lprc,
		uDwm::CRgnGeometryProxy** geometry
	);
	HRESULT STDMETHODCALLTYPE MyResourceHelper_CreateCombinedGeometry(
		uDwm::CBaseGeometryProxy* geometry1,
		uDwm::CBaseGeometryProxy* geometry2,
		UINT combineMode,
		uDwm::CCombinedGeometryProxy** combinedGeometry
	);
	HRESULT STDMETHODCALLTYPE MyCRgnGeometryProxy_Update(
		uDwm::CRgnGeometryProxy* This,
		LPCRECT lprc,
		UINT count
	);
	decltype(&MyResourceHelper_CreateGeometryFromHRGN) g_ResourceHelper_CreateGeometryFromHRGN_Org{ nullptr };
	decltype(&MyResourceHelper_CreateRectangleGeometry) g_ResourceHelper_CreateRectangleGeometry_Org{ nullptr };
	decltype(&MyResourceHelper_CreateCombinedGeometry) g_ResourceHelper_CreateCombinedGeometry_Org{ nullptr };
	decltype(&MyCRgnGeometryProxy_Update) g_CRgnGeometryProxy_Update_Org{ nullptr };

	size_t g_captureRef{ 0 };
	std::unordered_map<uDwm::CBaseGeometryProxy*, wil::unique_hrgn> g_geometryMap{};

	void RecordGeometry(uDwm::CBaseGeometryProxy* geometry, HRGN region)
	{
		g_geometryMap.insert_or_assign(geometry, std::move(wil::unique_hrgn{ region }));
	}
}

// CTopLevelWindow::UpdateNCGeometry
HRESULT STDMETHODCALLTYPE GeometryRecorder::MyResourceHelper_CreateGeometryFromHRGN(
	HRGN hrgn,
	uDwm::CRgnGeometryProxy** geometry
)
{
	HRESULT hr{ g_ResourceHelper_CreateGeometryFromHRGN_Org(hrgn, geometry) };

	if (SUCCEEDED(hr) && geometry && *geometry && g_captureRef)
	{
		HRGN region{ CreateRectRgn(0, 0, 0, 0) };
		CopyRgn(region, hrgn);
		RecordGeometry(*geometry, region);
	}

	return hr;
}

// CTopLevelWindow::UpdateClientBlur
HRESULT STDMETHODCALLTYPE GeometryRecorder::MyResourceHelper_CreateRectangleGeometry(
	LPCRECT lprc,
	uDwm::CRgnGeometryProxy** geometry
)
{
	HRESULT hr{ g_ResourceHelper_CreateRectangleGeometry_Org(lprc, geometry) };

	if (SUCCEEDED(hr) && geometry && *geometry && g_captureRef)
	{
		HRGN region{ CreateRectRgnIndirect(lprc) };
		RecordGeometry(*geometry, region);
	}

	return hr;
}

// CTopLevelWindow::UpdateClientBlur
HRESULT STDMETHODCALLTYPE GeometryRecorder::MyResourceHelper_CreateCombinedGeometry(
	uDwm::CBaseGeometryProxy* geometry1,
	uDwm::CBaseGeometryProxy* geometry2,
	UINT combineMode,
	uDwm::CCombinedGeometryProxy** combinedGeometry
)
{
	HRESULT hr{ g_ResourceHelper_CreateCombinedGeometry_Org(geometry1, geometry2, combineMode, combinedGeometry) };

	if (SUCCEEDED(hr) && combinedGeometry && *combinedGeometry && g_captureRef)
	{
		HRGN region{ CreateRectRgn(0, 0, 0, 0) };
		CombineRgn(
			region,
			GetRegionFromGeometry(geometry1),
			GetRegionFromGeometry(geometry2),
			RGN_AND
		);
		RecordGeometry(*combinedGeometry, region);
	}

	return hr;
}

// Windows 11 21H2 CTopLevelWindow::UpdateNCGeometry / Windows 11 22H2 CLegacyNonClientBackground::SetBorderRegion
HRESULT STDMETHODCALLTYPE GeometryRecorder::MyCRgnGeometryProxy_Update(
	uDwm::CRgnGeometryProxy* This,
	LPCRECT lprc,
	UINT count
)
{
	HRESULT hr{ g_CRgnGeometryProxy_Update_Org(This, lprc, count) };

	if (SUCCEEDED(hr))
	{
		if (lprc && count)
		{
			HRGN region{ CreateRectRgnIndirect(lprc) };
			RecordGeometry(This, region);
		}
		else
		{
			HRGN region{ CreateRectRgn(0, 0, 0, 0) };
			RecordGeometry(This, region);
		}
	}

	return hr;
}

void GeometryRecorder::BeginCapture()
{
	g_captureRef += 1;
}
HRGN GeometryRecorder::GetRegionFromGeometry(uDwm::CBaseGeometryProxy* geometry)
{
	auto it = g_geometryMap.find(geometry);
	if (it == g_geometryMap.end())
	{
		return nullptr;
	}

	return it->second.get();
}
size_t GeometryRecorder::GetGeometryCount()
{
	return g_geometryMap.size();
}

void GeometryRecorder::EndCapture()
{
	g_captureRef -= 1;
	if (g_captureRef == 0)
	{
		g_geometryMap.clear();
	}
}

HRESULT GeometryRecorder::Startup()
{
	uDwm::GetAddressFromSymbolMap("ResourceHelper::CreateRectangleGeometry", g_ResourceHelper_CreateRectangleGeometry_Org);
	uDwm::GetAddressFromSymbolMap("ResourceHelper::CreateGeometryFromHRGN", g_ResourceHelper_CreateGeometryFromHRGN_Org);
	uDwm::GetAddressFromSymbolMap("ResourceHelper::CreateCombinedGeometry", g_ResourceHelper_CreateCombinedGeometry_Org);
	uDwm::GetAddressFromSymbolMap("CRgnGeometryProxy::Update", g_CRgnGeometryProxy_Update_Org);

	return HookHelper::Detours::Write([]()
	{
		HookHelper::Detours::Attach(&g_ResourceHelper_CreateGeometryFromHRGN_Org, MyResourceHelper_CreateGeometryFromHRGN);
		HookHelper::Detours::Attach(&g_ResourceHelper_CreateRectangleGeometry_Org, MyResourceHelper_CreateRectangleGeometry);
		HookHelper::Detours::Attach(&g_ResourceHelper_CreateCombinedGeometry_Org, MyResourceHelper_CreateCombinedGeometry);
		if (os::buildNumber >= os::build_w11_21h2)
		{
			HookHelper::Detours::Attach(&g_CRgnGeometryProxy_Update_Org, MyCRgnGeometryProxy_Update);
		}
	});
}
void GeometryRecorder::Shutdown()
{
	HookHelper::Detours::Write([]()
	{
		HookHelper::Detours::Detach(&g_ResourceHelper_CreateGeometryFromHRGN_Org, MyResourceHelper_CreateGeometryFromHRGN);
		HookHelper::Detours::Detach(&g_ResourceHelper_CreateRectangleGeometry_Org, MyResourceHelper_CreateRectangleGeometry);
		HookHelper::Detours::Detach(&g_ResourceHelper_CreateCombinedGeometry_Org, MyResourceHelper_CreateCombinedGeometry);
		if (os::buildNumber >= os::build_w11_21h2)
		{
			HookHelper::Detours::Detach(&g_CRgnGeometryProxy_Update_Org, MyCRgnGeometryProxy_Update);
		}
	});
}