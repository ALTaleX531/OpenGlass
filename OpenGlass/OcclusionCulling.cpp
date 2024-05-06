#include "pch.h"
#include "OcclusionCulling.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "BackdropManager.hpp"

using namespace OpenGlass;
namespace OpenGlass::OcclusionCulling
{
	HRESULT WINAPI MyCArrayBasedCoverageSet_AddAntiOccluderRect(
		dwmcore::CArrayBasedCoverageSet* This,
		const D2D1_RECT_F& lprc,
		int unknown,
		const MilMatrix3x2D* matrix
	);
#ifdef _DEBUG
	HRESULT WINAPI MyCBrushRenderingGraph_RenderSubgraphs(
		dwmcore::CBrushRenderingGraph* This,
		dwmcore::CDrawingContext* context,
		const D2D1_SIZE_F& size,
		dwmcore::CDrawListBrush* brush,
		dwmcore::CDrawListCache* cache
	);
	HRESULT WINAPI MyCWindowNode_RenderImage(
		dwmcore::CWindowNode* This,
		dwmcore::CDrawingContext* context,
		dwmcore::CWindowOcclusionInfo* occlusionInfo,
		dwmcore::IBitmapResource* bitmap,
		dwmcore::CShape* shape,
		MARGINS* margins,
		int flag
	);
#endif
	decltype(&MyCArrayBasedCoverageSet_AddAntiOccluderRect) g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org{ nullptr };
#ifdef _DEBUG
	decltype(&MyCBrushRenderingGraph_RenderSubgraphs) g_CBrushRenderingGraph_RenderSubgraphs_Org{ nullptr };
	decltype(&MyCWindowNode_RenderImage) g_CWindowNode_RenderImage_Org{ nullptr };
#endif

	BOOL g_disableAntiOccluder{ TRUE };
}

HRESULT WINAPI OcclusionCulling::MyCArrayBasedCoverageSet_AddAntiOccluderRect(
	dwmcore::CArrayBasedCoverageSet* This,
	const D2D1_RECT_F& lprc,
	int unknown,
	const MilMatrix3x2D* matrix
)
{
	if (g_disableAntiOccluder) [[likely]]
	{
		return S_OK;
	}
#ifdef _DEBUG
	OutputDebugStringW(__FUNCTIONW__);
#endif
	return g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org(This, lprc, unknown, matrix);
}
#ifdef _DEBUG
HRESULT WINAPI OcclusionCulling::MyCBrushRenderingGraph_RenderSubgraphs(
	dwmcore::CBrushRenderingGraph* This,
	dwmcore::CDrawingContext* context,
	const D2D1_SIZE_F& size,
	dwmcore::CDrawListBrush* brush,
	dwmcore::CDrawListCache* cache
)
{
	OutputDebugStringW(__FUNCTIONW__);
	return g_CBrushRenderingGraph_RenderSubgraphs_Org(This, context, size, brush, cache);
}
HRESULT WINAPI OcclusionCulling::MyCWindowNode_RenderImage(
	dwmcore::CWindowNode* This,
	dwmcore::CDrawingContext* context,
	dwmcore::CWindowOcclusionInfo* occlusionInfo,
	dwmcore::IBitmapResource* bitmap,
	dwmcore::CShape* shape,
	MARGINS* margins,
	int flag
)
{
	WCHAR className[MAX_PATH + 1]{};
	GetClassNameW(This->GetHwnd(), className, MAX_PATH);
	OutputDebugStringW(className);

	return g_CWindowNode_RenderImage_Org(
		This,
		context,
		occlusionInfo,
		bitmap,
		shape,
		margins,
		flag
	);
}
#endif

void OcclusionCulling::InitializeFromSymbol(std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset)
{
}

void OcclusionCulling::UpdateConfiguration(ConfigurationFramework::UpdateType type)
{
	if (type & ConfigurationFramework::UpdateType::Framework)
	{
		g_disableAntiOccluder = TRUE;
		LOG_IF_FAILED(
			wil::reg::get_value_dword_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"DisableAntiOccluder",
				reinterpret_cast<DWORD*>(&g_disableAntiOccluder)
			)
		);
	}
}

HRESULT OcclusionCulling::Startup()
{
	g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org = dwmcore::CArrayBasedCoverageSet::s_AddAntiOccluderRect;
#ifdef _DEBUG
	g_CBrushRenderingGraph_RenderSubgraphs_Org = dwmcore::CBrushRenderingGraph::s_RenderSubgraphs;
	g_CWindowNode_RenderImage_Org = dwmcore::CWindowNode::s_RenderImage;
#endif
	return HookHelper::Detours::Write([]()
	{
		if (os::buildNumber < os::build_w11_21h2)
		{
			HookHelper::Detours::Attach(&g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org, MyCArrayBasedCoverageSet_AddAntiOccluderRect);
		}
#ifdef _DEBUG
		HookHelper::Detours::Attach(&g_CBrushRenderingGraph_RenderSubgraphs_Org, MyCBrushRenderingGraph_RenderSubgraphs);
		HookHelper::Detours::Attach(&g_CWindowNode_RenderImage_Org, MyCWindowNode_RenderImage);
#endif
	});
}
void OcclusionCulling::Shutdown()
{
	HookHelper::Detours::Write([]()
	{
		if (os::buildNumber < os::build_w11_21h2)
		{
			HookHelper::Detours::Detach(&g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org, MyCArrayBasedCoverageSet_AddAntiOccluderRect);
		}
#ifdef _DEBUG
		HookHelper::Detours::Detach(&g_CBrushRenderingGraph_RenderSubgraphs_Org, MyCBrushRenderingGraph_RenderSubgraphs);
		HookHelper::Detours::Detach(&g_CWindowNode_RenderImage_Org, MyCWindowNode_RenderImage);
#endif
	});
}