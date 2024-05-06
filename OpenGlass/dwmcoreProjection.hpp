#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "Utils.hpp"
#include "HookHelper.hpp"

namespace OpenGlass::dwmcore
{
	inline HMODULE g_moduleHandle{ GetModuleHandleW(L"dwmcore.dll") };
	inline std::unordered_map<std::string, PVOID> g_offsetMap{};

	struct CChannel
	{
		HRESULT STDMETHODCALLTYPE RedirectVisualSetRedirectedVisual(UINT redirectVisualHandleIndex, UINT visualHandleIndex)
		{
			DEFINE_INVOKER(CChannel::RedirectVisualSetRedirectedVisual);
			return INVOKE_MEMBERFUNCTION(redirectVisualHandleIndex, visualHandleIndex);
		}
	};
	namespace CCommonRegistryData
	{
		inline PULONGLONG m_backdropBlurCachingThrottleQPCTimeDelta{ nullptr };
	}
	struct CArrayBasedCoverageSet : IUnknown
	{
		static inline HRESULT(WINAPI*s_AddAntiOccluderRect)(
			CArrayBasedCoverageSet* This,
			const D2D1_RECT_F& lprc,
			int unknown,
			const MilMatrix3x2D* matrix
		){ nullptr };
		HRESULT STDMETHODCALLTYPE Add(
			const D2D1_RECT_F& lprc,
			int unknown,
			const MilMatrix3x2D* matrix
		)
		{
			DEFINE_INVOKER(CArrayBasedCoverageSet::Add);
			return INVOKE_MEMBERFUNCTION(lprc, unknown, matrix);
		}
	};
	struct CVisualTree : IUnknown {};
	struct CVisual : IUnknown
	{
		HRESULT WINAPI GetVisualTree(CVisualTree** visualTree, bool value) const
		{
			DEFINE_INVOKER(CVisual::GetVisualTree);
			return INVOKE_MEMBERFUNCTION(visualTree, value);
		}
		const D2D1_RECT_F& GetBounds(CVisualTree* visualTree) const
		{
			DEFINE_INVOKER(CVisual::GetBounds);
			return INVOKE_MEMBERFUNCTION(visualTree);
		}
	};
	struct CDrawingContext : IUnknown
	{
		CDrawingContext* GetParentDrawingContext() const
		{
			return const_cast<CDrawingContext*>(this) + 3;
		}
		bool STDMETHODCALLTYPE IsOccluded(const D2D1_RECT_F& lprc, int flag) const
		{
			DEFINE_INVOKER(CDrawingContext::IsOccluded);
			return INVOKE_MEMBERFUNCTION(lprc, flag);
		}
		CVisual* STDMETHODCALLTYPE GetCurrentVisual() const
		{
			DEFINE_INVOKER(CDrawingContext::GetCurrentVisual);
			return INVOKE_MEMBERFUNCTION();
		}
		HRESULT STDMETHODCALLTYPE GetClipBoundsWorld(D2D1_RECT_F& lprc) const
		{
			DEFINE_INVOKER(CDrawingContext::GetClipBoundsWorld);
			return INVOKE_MEMBERFUNCTION(lprc);
		}
		HRESULT STDMETHODCALLTYPE Clear(const D2D1_COLOR_F& color)
		{
			DEFINE_INVOKER(CDrawingContext::Clear);
			return INVOKE_MEMBERFUNCTION(color);
		}
	};
	struct CDrawListCache : IUnknown {};
	struct CDrawListBrush : IUnknown {};
	struct CBrush : IUnknown
	{
		static inline HRESULT(STDMETHODCALLTYPE* s_GenerateDrawList)(
			CBrush* This,
			CDrawingContext* context,
			const D2D1_SIZE_F& size,
			CDrawListCache* cache
		) { nullptr };
	};
	struct CBrushRenderingGraph : IUnknown
	{
		static inline HRESULT(WINAPI* s_RenderSubgraphs)(
			CBrushRenderingGraph* This,
			CDrawingContext* context,
			const D2D1_SIZE_F& size,
			CDrawListBrush* brush,
			CDrawListCache* cache
		) { nullptr };
	};
	struct CShape : IUnknown {};
	struct CWindowOcclusionInfo : IUnknown {};
	struct IBitmapResource : IUnknown {};
	struct CWindowNode : IUnknown
	{
		HWND STDMETHODCALLTYPE GetHwnd() const
		{
			DEFINE_INVOKER(CWindowNode::GetHwnd);
			return INVOKE_MEMBERFUNCTION();
		}
		static inline HRESULT(WINAPI* s_RenderImage)(
			CWindowNode* This,
			CDrawingContext* context,
			CWindowOcclusionInfo* occlusionInfo,
			IBitmapResource* bitmap,
			CShape* shape,
			MARGINS* margins,
			int flag
		) { nullptr };
	};

	inline void InitializeFromSymbol(std::string_view functionName, std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset)
	{
		if (fullyUnDecoratedFunctionName == "CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta")
		{
			offset.To(g_moduleHandle, CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta);
		}
		if (fullyUnDecoratedFunctionName == "CArrayBasedCoverageSet::AddAntiOccluderRect")
		{
			offset.To(g_moduleHandle, CArrayBasedCoverageSet::s_AddAntiOccluderRect);
		}
		if (fullyUnDecoratedFunctionName == "CArrayBasedCoverageSet::AddAntiOccluderRect")
		{
			offset.To(g_moduleHandle, CArrayBasedCoverageSet::s_AddAntiOccluderRect);
		}
		if (fullyUnDecoratedFunctionName == "CChannel::RedirectVisualSetRedirectedVisual")
		{
			g_offsetMap.insert_or_assign(
				std::string{ fullyUnDecoratedFunctionName },
				offset.To(g_moduleHandle)
			);
		}
#ifdef _DEBUG
		if (fullyUnDecoratedFunctionName == "CBrushRenderingGraph::RenderSubgraphs")
		{
			offset.To(g_moduleHandle, CBrushRenderingGraph::s_RenderSubgraphs);
		}
		if (fullyUnDecoratedFunctionName == "CWindowNode::RenderImage")
		{
			offset.To(g_moduleHandle, CWindowNode::s_RenderImage);
		}
		if (
			fullyUnDecoratedFunctionName.starts_with("CArrayBasedCoverageSet::") ||
			fullyUnDecoratedFunctionName.starts_with("CVisual::") ||
			fullyUnDecoratedFunctionName.starts_with("CWindowNode::") ||
			(
				fullyUnDecoratedFunctionName.starts_with("CDrawingContext::") &&
				fullyUnDecoratedFunctionName != "CDrawingContext::IsOccluded"
			) ||
			functionName == "?IsOccluded@CDrawingContext@@QEBA_NAEBV?$TMilRect_@MUMilRectF@@UMil3DRectF@@UMilPointAndSizeF@@UNotNeeded@RectUniqueness@@@@H@Z"
		)
		{
			g_offsetMap.insert_or_assign(
				std::string{ fullyUnDecoratedFunctionName },
				offset.To(g_moduleHandle)
			);
		}
#endif
	}
}