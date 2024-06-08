#include "pch.h"
#include "GlassOptimizer.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "BackdropManager.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassOptimizer
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
	HRESULT STDMETHODCALLTYPE MyCCustomBlur_BuildEffect(
		dwmcore::CCustomBlur* This,
		ID2D1Image* image,
		D2D1_RECT_F& sourceRect,
		const D2D1_SIZE_F& blurAmount,
		D2D1_GAUSSIANBLUR_OPTIMIZATION optimization,
		const D2D1_VECTOR_2F& outputScale,
		D2D1_VECTOR_2F* scaleAmount
	);
#endif
	float STDMETHODCALLTYPE MyCCustomBlur_DetermineOutputScale(
		float size, 
		float blurAmount, 
		D2D1_GAUSSIANBLUR_OPTIMIZATION optimization
	);
	void STDMETHODCALLTYPE MyCBlurRenderingGraph_DeterminePreScale(
		const dwmcore::EffectInput& input1,
		const dwmcore::EffectInput& input2,
		D2D1_GAUSSIANBLUR_OPTIMIZATION optimization,
		const D2D1_VECTOR_2F& blurAmount,
		D2D1_VECTOR_2F* scaleAmount
	);
	HRESULT WINAPI MyCDrawingContext_GetBackdropImageFromRenderTarget(
		dwmcore::CDrawingContext* This,
		D2D1_RECT_F& lprc,
		bool notUsingFullTargetAsBackdropInput,
		dwmcore::EffectInput** effectInput
	);

	decltype(&MyCVisual_GetWindowBackgroundTreatmentInternal) g_CVisual_GetWindowBackgroundTreatmentInternal_Org{ nullptr };
	decltype(&MyCArrayBasedCoverageSet_AddAntiOccluderRect) g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org{ nullptr };
	decltype(&MyCArrayBasedCoverageSet_IsCovered) g_CArrayBasedCoverageSet_IsCovered_Org{ nullptr };
	decltype(&MyCOcclusionContext_PostSubgraph) g_COcclusionContext_PostSubgraph_Org{ nullptr };
#ifdef _DEBUG
	decltype(&MyCCustomBlur_BuildEffect) g_CCustomBlur_BuildEffect_Org{ nullptr };
#endif
	decltype(&MyCCustomBlur_DetermineOutputScale) g_CCustomBlur_DetermineOutputScale_Org{ nullptr };
	decltype(&MyCBlurRenderingGraph_DeterminePreScale) g_CBlurRenderingGraph_DeterminePreScale_Org{ nullptr };
	decltype(&MyCDrawingContext_GetBackdropImageFromRenderTarget) g_CDrawingContext_GetBackdropImageFromRenderTarget_Org{ nullptr };

	BOOL g_enableOcclusionCulling{ TRUE };
	BOOL g_enableEffectInputOptimization{ TRUE };
	float g_additionalPreScaleAmount{ 0.5f };

	ULONGLONG g_frameId{ 0ull };
	bool g_hasWindowBackgroundTreatment{ false };
	dwmcore::CVisual* g_visual{ nullptr };
	dwmcore::MyDynArrayImpl<dwmcore::CZOrderedRect> g_validAntiOccluderList{};
}

dwmcore::CWindowBackgroundTreatment* STDMETHODCALLTYPE GlassOptimizer::MyCVisual_GetWindowBackgroundTreatmentInternal(dwmcore::CVisual* This)
{
	auto result{ g_CVisual_GetWindowBackgroundTreatmentInternal_Org(This) };
	if (g_visual)
	{
		g_hasWindowBackgroundTreatment = true;
	}
	return result;
}
HRESULT STDMETHODCALLTYPE GlassOptimizer::MyCArrayBasedCoverageSet_AddAntiOccluderRect(
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
bool STDMETHODCALLTYPE GlassOptimizer::MyCArrayBasedCoverageSet_IsCovered(
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
HRESULT STDMETHODCALLTYPE GlassOptimizer::MyCOcclusionContext_PostSubgraph(
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
HRESULT STDMETHODCALLTYPE GlassOptimizer::MyCCustomBlur_BuildEffect(
	dwmcore::CCustomBlur* This,
	ID2D1Image* image,
	D2D1_RECT_F& sourceRect,
	const D2D1_SIZE_F& blurAmount,
	D2D1_GAUSSIANBLUR_OPTIMIZATION optimization,
	const D2D1_VECTOR_2F& outputScale,
	D2D1_VECTOR_2F* scaleAmount
)
{
	HRESULT hr{ g_CCustomBlur_BuildEffect_Org(This, image, sourceRect, blurAmount, optimization, outputScale, scaleAmount) };
	
	D2D1_RECT_F cropRect{};
	This->GetCropEffect()->GetValue(D2D1_CROP_PROP_RECT, &cropRect);
	OutputDebugStringW(
		std::format(
			L"cropRect: [{},{},{},{}], sourceRect: [{},{},{},{}], blurAmount: [{},{}], outputScale:[{},{}], scaleAmount:[{},{}]\n",
			cropRect.left,
			cropRect.top,
			cropRect.right,
			cropRect.bottom,
			sourceRect.left,
			sourceRect.top,
			sourceRect.right,
			sourceRect.bottom,
			blurAmount.width,
			blurAmount.height,
			outputScale.x,
			outputScale.y,
			scaleAmount->x,
			scaleAmount->y
		).c_str()
	);

	return hr;
}
#endif

float STDMETHODCALLTYPE GlassOptimizer::MyCCustomBlur_DetermineOutputScale(
	float size,
	float blurAmount,
	D2D1_GAUSSIANBLUR_OPTIMIZATION optimization
)
{
	auto result{ g_CCustomBlur_DetermineOutputScale_Org(size, blurAmount, optimization) };
#ifdef _DEBUG
//	OutputDebugStringW(std::format(L"size: {}, blurAmount: {}\n", size, blurAmount).c_str());
#endif
	return result * g_additionalPreScaleAmount;
}
void STDMETHODCALLTYPE GlassOptimizer::MyCBlurRenderingGraph_DeterminePreScale(
	const dwmcore::EffectInput& input1,
	const dwmcore::EffectInput& input2,
	D2D1_GAUSSIANBLUR_OPTIMIZATION optimization,
	const D2D1_VECTOR_2F& blurAmount,
	D2D1_VECTOR_2F* scaleAmount
)
{
	g_CBlurRenderingGraph_DeterminePreScale_Org(input1, input2, optimization, blurAmount, scaleAmount);
	if (scaleAmount)
	{
		scaleAmount->x *= g_additionalPreScaleAmount;
		scaleAmount->y *= g_additionalPreScaleAmount;
	}
}
HRESULT WINAPI GlassOptimizer::MyCDrawingContext_GetBackdropImageFromRenderTarget(
	dwmcore::CDrawingContext* This,
	D2D1_RECT_F& lprc,
	bool notUsingFullTargetAsBackdropInput,
	dwmcore::EffectInput** effectInput
)
{
#ifdef _DEBUG
	OutputDebugStringW(
		std::format(
			L"MyCDrawingContext_GetBackdropImageFromRenderTarget: [{},{},{},{}], notUsingFullTargetAsBackdropInput: [{}]\n", 
			lprc.left, 
			lprc.top, 
			lprc.right, 
			lprc.bottom,
			notUsingFullTargetAsBackdropInput
		).c_str()
	);
#endif
	return g_CDrawingContext_GetBackdropImageFromRenderTarget_Org(This, lprc, static_cast<bool>(g_enableEffectInputOptimization), effectInput);
}

void GlassOptimizer::UpdateConfiguration(ConfigurationFramework::UpdateType type)
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
		g_enableEffectInputOptimization = TRUE;
		LOG_IF_FAILED(
			wil::reg::get_value_dword_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"EnableEffectInputOptimization",
				reinterpret_cast<DWORD*>(&g_enableEffectInputOptimization)
			)
		);

		DWORD value{ 0 };
		if (os::buildNumber < os::build_w11_21h2)
		{
			value = 50;
		}
		else
		{
			value = 75;
		}
		LOG_IF_FAILED(
			wil::reg::get_value_dword_nothrow(
				ConfigurationFramework::GetDwmKey(),
				L"GlassAdditionalPreScaleAmount",
				&value
			)
		);
		g_additionalPreScaleAmount = std::clamp(static_cast<float>(value) / 100.f, 0.001f, 1.f);
	}
}

HRESULT GlassOptimizer::Startup()
{
	dwmcore::GetAddressFromSymbolMap("CVisual::GetWindowBackgroundTreatmentInternal", g_CVisual_GetWindowBackgroundTreatmentInternal_Org);
	dwmcore::GetAddressFromSymbolMap("CArrayBasedCoverageSet::AddAntiOccluderRect", g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org);
	dwmcore::GetAddressFromSymbolMap("CArrayBasedCoverageSet::IsCovered", g_CArrayBasedCoverageSet_IsCovered_Org);
	dwmcore::GetAddressFromSymbolMap("COcclusionContext::PostSubgraph", g_COcclusionContext_PostSubgraph_Org);
#ifdef _DEBUG
	dwmcore::GetAddressFromSymbolMap("CCustomBlur::BuildEffect", g_CCustomBlur_BuildEffect_Org);
#endif
	dwmcore::GetAddressFromSymbolMap("CCustomBlur::DetermineOutputScale", g_CCustomBlur_DetermineOutputScale_Org);
	dwmcore::GetAddressFromSymbolMap("CBlurRenderingGraph::DeterminePreScale", g_CBlurRenderingGraph_DeterminePreScale_Org);
	dwmcore::GetAddressFromSymbolMap("CDrawingContext::GetBackdropImageFromRenderTarget", g_CDrawingContext_GetBackdropImageFromRenderTarget_Org);
	
	return HookHelper::Detours::Write([]()
	{
		if (os::buildNumber < os::build_w11_21h2)
		{
			HookHelper::Detours::Attach(&g_CVisual_GetWindowBackgroundTreatmentInternal_Org, MyCVisual_GetWindowBackgroundTreatmentInternal);
			HookHelper::Detours::Attach(&g_CArrayBasedCoverageSet_AddAntiOccluderRect_Org, MyCArrayBasedCoverageSet_AddAntiOccluderRect);
			HookHelper::Detours::Attach(&g_CArrayBasedCoverageSet_IsCovered_Org, MyCArrayBasedCoverageSet_IsCovered);
			HookHelper::Detours::Attach(&g_COcclusionContext_PostSubgraph_Org, MyCOcclusionContext_PostSubgraph);
#ifdef _DEBUG
			HookHelper::Detours::Attach(&g_CCustomBlur_BuildEffect_Org, MyCCustomBlur_BuildEffect);
#endif
			HookHelper::Detours::Attach(&g_CCustomBlur_DetermineOutputScale_Org, MyCCustomBlur_DetermineOutputScale);
			HookHelper::Detours::Attach(&g_CDrawingContext_GetBackdropImageFromRenderTarget_Org, MyCDrawingContext_GetBackdropImageFromRenderTarget);
		}
		else
		{
			HookHelper::Detours::Attach(&g_CBlurRenderingGraph_DeterminePreScale_Org, MyCBlurRenderingGraph_DeterminePreScale);
		}
	});
}
void GlassOptimizer::Shutdown()
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
			HookHelper::Detours::Detach(&g_CCustomBlur_BuildEffect_Org, MyCCustomBlur_BuildEffect);
#endif
			HookHelper::Detours::Detach(&g_CCustomBlur_DetermineOutputScale_Org, MyCCustomBlur_DetermineOutputScale);
			HookHelper::Detours::Detach(&g_CDrawingContext_GetBackdropImageFromRenderTarget_Org, MyCDrawingContext_GetBackdropImageFromRenderTarget);
		}
		else
		{
			HookHelper::Detours::Detach(&g_CBlurRenderingGraph_DeterminePreScale_Org, MyCBlurRenderingGraph_DeterminePreScale);
		}
	});
}