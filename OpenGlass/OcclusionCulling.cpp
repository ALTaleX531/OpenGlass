#include "pch.h"
#include "OcclusionCulling.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "BackdropManager.hpp"

using namespace OpenGlass;
namespace OpenGlass::OcclusionCulling
{
	dwmcore::CWindowBackgroundTreatment* MyCVisual_GetWindowBackgroundTreatmentInternal(dwmcore::CVisual* This);
	HRESULT STDMETHODCALLTYPE MyCArrayBasedCoverageSet_AddAntiOccluderRect(
		dwmcore::CArrayBasedCoverageSet* This,
		const D2D1_RECT_F& lprc,
		int depth,
		const MilMatrix3x2D* matrix
	);
	bool STDMETHODCALLTYPE MyCArrayBasedCoverageSet_IsCovered(
		dwmcore::CArrayBasedCoverageSet* This,
		const D2D1_RECT_F& lprc,
		int depth,
		bool deprecated
	);
	HRESULT STDMETHODCALLTYPE MyCOcclusionContext_PostSubgraph(
		dwmcore::COcclusionContext* This, 
		dwmcore::CVisualTree* visualTree, 
		bool* unknown
	);
#ifdef _DEBUG
	HRESULT WINAPI MyCDrawingContext_GetBackdropImageFromRenderTarget(
		dwmcore::CDrawingContext* This,
		const D2D1_RECT_F& lprc,
		bool requestValidBackdrop,
		dwmcore::EffectInput ** effectInput
	);
	HRESULT WINAPI MyCBrush_GenerateDrawList(
		dwmcore::CBrush* This,
		dwmcore::CDrawingContext* context,
		const D2D1_SIZE_F& size,
		dwmcore::CDrawListCache* cache
	);
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
	decltype(&MyCVisual_GetWindowBackgroundTreatmentInternal) g_CVisual_GetWindowBackgroundTreatmentInternal_Org{ nullptr };
	decltype(&MyCArrayBasedCoverageSet_AddAntiOccluderRect) g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org{ nullptr };
	decltype(&MyCArrayBasedCoverageSet_IsCovered) g_CArrayBasedCoverageSet_IsCovered_Org{ nullptr };
	decltype(&MyCOcclusionContext_PostSubgraph) g_COcclusionContext_PostSubgraph_Org{ nullptr };
#ifdef _DEBUG
	decltype(&MyCDrawingContext_GetBackdropImageFromRenderTarget) g_CDrawingContext_GetBackdropImageFromRenderTarget_Org{ nullptr };
	decltype(&MyCBrushRenderingGraph_RenderSubgraphs) g_CBrushRenderingGraph_RenderSubgraphs_Org{ nullptr };
	decltype(&MyCBrush_GenerateDrawList) g_CBrush_GenerateDrawList_Org{ nullptr };
	decltype(&MyCWindowNode_RenderImage) g_CWindowNode_RenderImage_Org{ nullptr };
#endif

	BOOL g_enableOcclusionCulling{ TRUE };

	ULONGLONG g_frameId{ 0ull };
	bool g_hasWindowBackgroundTreatment{ false };
	dwmcore::CVisual* g_visual{ nullptr };
	dwmcore::MyDynArrayImpl<dwmcore::CZOrderedRect> g_validAntiOccluderList{};
}

dwmcore::CWindowBackgroundTreatment* OcclusionCulling::MyCVisual_GetWindowBackgroundTreatmentInternal(dwmcore::CVisual* This)
{
	auto result{ g_CVisual_GetWindowBackgroundTreatmentInternal_Org(This) };
	if (g_visual)
	{
		g_hasWindowBackgroundTreatment = true;
	}
	return result;
}
HRESULT STDMETHODCALLTYPE OcclusionCulling::MyCArrayBasedCoverageSet_AddAntiOccluderRect(
	dwmcore::CArrayBasedCoverageSet* This,
	const D2D1_RECT_F& lprc,
	int depth,
	const MilMatrix3x2D* matrix
)
{
	if (g_enableOcclusionCulling) [[likely]]
	{
		auto currentFrameId{ dwmcore::GetCurrentFrameId() };
		if (g_frameId != currentFrameId)
		{
#ifdef _DEBUG
			OutputDebugStringW(
				L"new frame detected, executing GC...\n"
			);
#endif
			g_frameId = currentFrameId;
			g_validAntiOccluderList.Clear();
		}

		if (
			g_visual->GetOwningProcessId() != GetCurrentProcessId() /* actually this is useless, GetOwningProcessId only returns the process id of dwm, idk why */ || 
			g_hasWindowBackgroundTreatment
		)
		{
			dwmcore::CZOrderedRect zorderedRect{ lprc, depth, lprc };
			zorderedRect.UpdateDeviceRect(matrix);
			g_validAntiOccluderList.Add(zorderedRect);
#ifdef _DEBUG
			OutputDebugStringW(
				std::format(
					L"{} - depth: {}, owning process id: {}\n",
					__FUNCTIONW__,
					depth,
					g_visual->GetOwningProcessId()
				).c_str()
			);
#endif
		}
	}

	return g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org(This, lprc, depth, matrix);
}
bool STDMETHODCALLTYPE OcclusionCulling::MyCArrayBasedCoverageSet_IsCovered(
	dwmcore::CArrayBasedCoverageSet* This,
	const D2D1_RECT_F& lprc,
	int depth,
	bool deprecated
)
{
	if (g_enableOcclusionCulling) [[likely]]
	{
		auto array{ This->GetAntiOccluderArray() };
		auto arrayBackup{ *array };
		*array = g_validAntiOccluderList;

		bool result{ g_CArrayBasedCoverageSet_IsCovered_Org(This, lprc, depth, deprecated) };

		*array = arrayBackup;
		return result;
	}

	return g_CArrayBasedCoverageSet_IsCovered_Org(This, lprc, depth, deprecated);
}
HRESULT STDMETHODCALLTYPE OcclusionCulling::MyCOcclusionContext_PostSubgraph(
	dwmcore::COcclusionContext* This,
	dwmcore::CVisualTree* visualTree,
	bool* unknown
)
{
	g_visual = This->GetVisual();
	g_hasWindowBackgroundTreatment = false;
	HRESULT hr{ g_COcclusionContext_PostSubgraph_Org(This, visualTree, unknown) };
	g_visual = nullptr;
	g_hasWindowBackgroundTreatment = false;

	return hr;
}
#ifdef _DEBUG
HRESULT WINAPI OcclusionCulling::MyCDrawingContext_GetBackdropImageFromRenderTarget(
	dwmcore::CDrawingContext* This,
	const D2D1_RECT_F& lprc,
	bool requestValidBackdrop,
	dwmcore::EffectInput** effectInput
)
{
	OutputDebugStringW(__FUNCTIONW__);
	return g_CDrawingContext_GetBackdropImageFromRenderTarget_Org(This, lprc, requestValidBackdrop, effectInput);
}
HRESULT WINAPI OcclusionCulling::MyCBrush_GenerateDrawList(
	dwmcore::CBrush* This,
	dwmcore::CDrawingContext* context,
	const D2D1_SIZE_F& size,
	dwmcore::CDrawListCache* cache
)
{
	OutputDebugStringW(__FUNCTIONW__);
	return g_CBrush_GenerateDrawList_Org(This, context, size, cache);
}
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

void OcclusionCulling::UpdateConfiguration(ConfigurationFramework::UpdateType type)
{
	if (type & ConfigurationFramework::UpdateType::Framework)
	{
		g_enableOcclusionCulling = TRUE;
		LOG_IF_FAILED(
			wil::reg::get_value_dword_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"EnableOcclusionCulling",
				reinterpret_cast<DWORD*>(&g_enableOcclusionCulling)
			)
		);
	}
}

HRESULT OcclusionCulling::Startup()
{
	dwmcore::GetAddressFromSymbolMap("CVisual::GetWindowBackgroundTreatmentInternal", g_CVisual_GetWindowBackgroundTreatmentInternal_Org);
	dwmcore::GetAddressFromSymbolMap("CArrayBasedCoverageSet::AddAntiOccluderRect", g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org);
	dwmcore::GetAddressFromSymbolMap("CArrayBasedCoverageSet::IsCovered", g_CArrayBasedCoverageSet_IsCovered_Org);
	dwmcore::GetAddressFromSymbolMap("COcclusionContext::PostSubgraph", g_COcclusionContext_PostSubgraph_Org);
#ifdef _DEBUG
	dwmcore::GetAddressFromSymbolMap("CDrawingContext::GetBackdropImageFromRenderTarget", g_CDrawingContext_GetBackdropImageFromRenderTarget_Org);
	dwmcore::GetAddressFromSymbolMap("CBrush::GenerateDrawList", g_CBrush_GenerateDrawList_Org);
	dwmcore::GetAddressFromSymbolMap("CBrushRenderingGraph::RenderSubgraphs", g_CBrushRenderingGraph_RenderSubgraphs_Org);
	dwmcore::GetAddressFromSymbolMap("CWindowNode::RenderImage", g_CWindowNode_RenderImage_Org);
#endif
	return HookHelper::Detours::Write([]()
	{
		if (os::buildNumber < os::build_w11_21h2)
		{
			HookHelper::Detours::Attach(&g_CVisual_GetWindowBackgroundTreatmentInternal_Org, MyCVisual_GetWindowBackgroundTreatmentInternal);
			HookHelper::Detours::Attach(&g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org, MyCArrayBasedCoverageSet_AddAntiOccluderRect);
			HookHelper::Detours::Attach(&g_CArrayBasedCoverageSet_IsCovered_Org, MyCArrayBasedCoverageSet_IsCovered);
			HookHelper::Detours::Attach(&g_COcclusionContext_PostSubgraph_Org, MyCOcclusionContext_PostSubgraph);
#ifdef _DEBUG
			HookHelper::Detours::Attach(&g_CDrawingContext_GetBackdropImageFromRenderTarget_Org, MyCDrawingContext_GetBackdropImageFromRenderTarget);
			HookHelper::Detours::Attach(&g_CBrush_GenerateDrawList_Org, MyCBrush_GenerateDrawList);
			HookHelper::Detours::Attach(&g_CBrushRenderingGraph_RenderSubgraphs_Org, MyCBrushRenderingGraph_RenderSubgraphs);
#endif
		}
#ifdef _DEBUG
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
			HookHelper::Detours::Detach(&g_CVisual_GetWindowBackgroundTreatmentInternal_Org, MyCVisual_GetWindowBackgroundTreatmentInternal);
			HookHelper::Detours::Detach(&g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org, MyCArrayBasedCoverageSet_AddAntiOccluderRect);
			HookHelper::Detours::Detach(&g_CArrayBasedCoverageSet_IsCovered_Org, MyCArrayBasedCoverageSet_IsCovered);
			HookHelper::Detours::Detach(&g_COcclusionContext_PostSubgraph_Org, MyCOcclusionContext_PostSubgraph);
#ifdef _DEBUG
			HookHelper::Detours::Detach(&g_CDrawingContext_GetBackdropImageFromRenderTarget_Org, MyCDrawingContext_GetBackdropImageFromRenderTarget);
			HookHelper::Detours::Detach(&g_CBrush_GenerateDrawList_Org, MyCBrush_GenerateDrawList);
			HookHelper::Detours::Detach(&g_CBrushRenderingGraph_RenderSubgraphs_Org, MyCBrushRenderingGraph_RenderSubgraphs);
#endif
		}
#ifdef _DEBUG
		HookHelper::Detours::Detach(&g_CWindowNode_RenderImage_Org, MyCWindowNode_RenderImage);
#endif
	});
}