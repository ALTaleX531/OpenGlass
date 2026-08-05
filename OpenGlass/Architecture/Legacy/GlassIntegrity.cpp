#include "pch.h"
#include "HookHelper.hpp"
#include "uDWMProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "Shared.hpp"
#include "GlassKernel.hpp"
#include "GlassIntegrity.hpp"
#include "GlassSafetyZoneLayer.hpp"
#include "GlassCoverageSet.hpp"
#include "GlassRenderer.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassIntegrity
{
	HRESULT MyCOcclusionContext_CheckAndRecordOverlayCandidate(
		dwmcore::COcclusionContext* This,
		dwmcore::CVisual* visual,
		dwmcore::ISwapChainContent* surface,
		const dwmcore::CMILMatrix* matrix,
		const dwmcore::CShape* shape,
		int flags
	);
	HRESULT MyCOcclusionContext_CheckAndRecordOverlayCandidate(
		dwmcore::COcclusionContext* This,
		dwmcore::CVisual* visual,
		dwmcore::CCompositionSurfaceInfo* surface,
		const dwmcore::CMILMatrix* matrix,
		const dwmcore::CShape* shape,
		int flags
	);
	HRESULT MyCOcclusionContext_CheckAndRecordOverlayCandidate(
		dwmcore::COcclusionContext* This,
		dwmcore::CVisual* visual,
		dwmcore::CCompositionSurfaceInfo* surface,
		const dwmcore::CMILMatrix& matrix,
		const dwmcore::CShape* shape,
		int flags
	);
	HRESULT MyCOcclusionContext_CollectRectangleForOcclusion(
		dwmcore::COcclusionContext* This,
		const D2D1_RECT_F* rectangle,
		bool unknown,
		D2D1_RECT_F* outRect
	);
	void MyCOcclusionContext_CollectRectangleForOcclusion(
		dwmcore::COcclusionContext* This,
		const D2D1_RECT_F* rectangle,
		bool unknown1,
		bool unknown2,
		D2D1_RECT_F* outRect
	);
	void MyCOcclusionContext_CollectRectangleForOcclusion(
		dwmcore::COcclusionContext* This,
		const D2D1_RECT_F& rectangle,
		D2D1_RECT_F* outRect
	);
	void MyCOcclusionContext_CollectRectangleForOcclusion(
		dwmcore::COcclusionContext* This,
		const D2D1_RECT_F& rectangle,
		bool unknown,
		D2D1_RECT_F* outRect
	);
	HRESULT MyCOcclusionContext_Compute_Pre_W10_2004(
		dwmcore::COcclusionContext* This,
		const dwmcore::CVisualTree* visualTree,
		UINT count,
		const D2D1_RECT_F* rectangles,
		float unknown1,
		bool unknown2,
		const dwmcore::CMILMatrix* matrix,
		const DWM::span<dwmcore::COverlayContext*>& overlays
	);
	HRESULT MyCOcclusionContext_Compute(
		dwmcore::COcclusionContext* This,
		const dwmcore::CVisualTree* visualTree,
		const DWM::span<const D2D1_RECT_F>& rectangles,
		float unknown,
		const DWM::span<dwmcore::COverlayContext*>& overlays
	);
	HRESULT MyCOcclusionContext_DrawGeometry(
		dwmcore::IDrawingContext* This,
		dwmcore::CLegacyMilBrush* brush,
		dwmcore::CGeometry* geometry
	);
	HRESULT MyCOcclusionContext_SetDeviceTransform(
		dwmcore::COcclusionContext* This,
		const dwmcore::CMILMatrix* matrix
	);
	void MyCOcclusionContext_Destructor(dwmcore::COcclusionContext* This);

	template <typename Callback>
	bool MyCArrayBasedCoverageSet_IsCovered(
		dwmcore::CArrayBasedCoverageSet* This,
		const D2D1_RECT_F& coverage,
		int depth,
		Callback&& invokeOriginal
	);
	bool MyCArrayBasedCoverageSet_IsCovered_Pre_26100(
		dwmcore::CArrayBasedCoverageSet* This,
		const D2D1_RECT_F& coverage,
		int depth,
		bool unknown
	);
	bool MyCArrayBasedCoverageSet_IsCovered_26100(
		dwmcore::CArrayBasedCoverageSet* This,
		const D2D1_RECT_F& coverage,
		int depth,
		bool unknown1,
		bool unknown2
	);
	template <typename Callback>
	bool MyCOcclusionContext_IsOccluded(
		dwmcore::COcclusionContext* This,
		const D2D1_RECT_F& coverage,
		int depth,
		bool ignoreDeviceTransform,
		Callback&& invokeOriginal
	);
	bool MyCOcclusionContext_IsOccluded_Pre_22000(
		dwmcore::COcclusionContext* This,
		const D2D1_RECT_F& coverage,
		bool ignoreDeviceTransform,
		int depth
	);
	bool MyCOcclusionContext_IsOccluded_20348(
		dwmcore::COcclusionContext* This,
		const D2D1_RECT_F& coverage,
		int depth,
		bool ignoreDeviceTransform
	);
	bool MyCOcclusionContext_PageInPixelsRectToDeviceRect(
		dwmcore::COcclusionContext* This,
		const D2D1_RECT_F& src,
		D2D1_RECT_F* dst
	);
	HRESULT MyCHwndRenderTarget_RenderDirtyRegion(
		dwmcore::CHwndRenderTarget* This,
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CComposeTop* composeTop
	);
	D2D1_RECT_F* MyCDirtyRegion_GetUnOccludedDirtyRegion(
		dwmcore::CDirtyRegion* This,
		D2D1_RECT_F* dirtyRect,
		dwmcore::COcclusionContext* occlusionContext,
		const dwmcore::CVisualTree* tree,
		bool inflate,
		unsigned int i,
		const D2D1_RECT_F& bounds
	);
	D2D1_RECT_F* MyCDirtyRegion_GetUnOccludedDirtyRect(
		dwmcore::CDirtyRegion* This,
		D2D1_RECT_F* dirtyRect,
		UINT i,
		const D2D1_RECT_F& bounds,
		bool useSuperSample,
		const DWM::span<const dwmcore::CVisual*>& visuals,
		const dwmcore::COcclusionContext* occlusionContext
	);
	D2D1_RECT_F* MyCDirtyRegion_GetOptimizedRect_WS2022(
		dwmcore::CDirtyRegion* This,
		D2D1_RECT_F* dirtyRect,
		UINT i,
		const D2D1_RECT_F& bounds,
		const D2D1_SIZE_U& size,
		bool transform,
		const dwmcore::CMILMatrix* matrix,
		const DWM::span<const dwmcore::CVisual*>& visuals,
		dwmcore::CRegion* region,
		const dwmcore::COcclusionContext* occlusionContext
	);
	D2D1_RECT_F* MyCDirtyRegion_GetOptimizedRect(
		dwmcore::CDirtyRegion* This,
		D2D1_RECT_F* dirtyRect,
		UINT i,
		const D2D1_RECT_F& bounds,
		dwmcore::CRegion* region,
		const dwmcore::CMILMatrix* matrix,
		bool useSuperSample,
		const DWM::span<const dwmcore::CVisual*>& visuals,
		const dwmcore::COcclusionContext* occlusionContext
	);
	D2D1_RECT_F* MyCTreeDirty_GetOptimizedRect(
		dwmcore::CTreeDirty* This,
		D2D1_RECT_F* dirtyRect,
		UINT i,
		const D2D1_RECT_F& bounds,
		const dwmcore::COcclusionContext& occlusionContext,
		dwmcore::CRegion* region,
		const dwmcore::CMILMatrix* matrix,
		bool useSuperSample,
		const DWM::span<const dwmcore::CVisual*>& visuals
	);

	template <typename T>
	HRESULT MyCDrawingContext_DrawVisualTree(
		dwmcore::CDrawingContext* This,
		const D2D1_RECT_F& rectangle,
		const dwmcore::COcclusionContext* occlusionContext,
		T&& callback
	);
	HRESULT MyCDrawingContext_DrawVisualTree_Win10_1809(
		dwmcore::CDrawingContext* This,
		const dwmcore::CVisualTree* tree,
		const D2D1_RECT_F& rectangle,
		dwmcore::COverlayContext* overlayContext,
		int unknown1,
		bool unknown2,
		bool unknown3,
		bool useOcclusionContext,
		bool unknown4,
		bool unknown5,
		bool unknown6,
		bool unknown7
	);
	HRESULT MyCDrawingContext_DrawVisualTree_Win10_1903(
		dwmcore::CDrawingContext* This,
		const dwmcore::CVisualTree* tree,
		const D2D1_RECT_F& rectangle,
		dwmcore::COverlayContext* overlayContext,
		int unknown1,
		bool unknown2,
		bool unknown3,
		bool useOcclusionContext,
		const D2D1_RECT_F* unknown4,
		bool unknown5,
		bool unknown6,
		bool unknown7
	);
	HRESULT MyCDrawingContext_DrawVisualTree_Win10(
		dwmcore::CDrawingContext* This,
		const dwmcore::CVisualTree* tree,
		const D2D1_RECT_F& rectangle,
		const dwmcore::COcclusionContext* occlusionContext,
		int clearMode,
		bool useSuperSample
	);
	HRESULT MyCDrawingContext_DrawVisualTree_Win11(
		dwmcore::CDrawingContext* This,
		const dwmcore::CVisualTree* tree,
		const D2D1_RECT_F& rectangle,
		const dwmcore::COcclusionContext* occlusionContext,
		int clearMode,
		bool useSuperSample,
		dwmcore::CVisual* visualOverride
	);

	Projection::Detour<dwmcore::Symbol_COcclusionContext_CheckAndRecordOverlayCandidate_Pre_19041, dwmcore::COcclusionContext_CheckAndRecordOverlayCandidate_Pre_19041_t> g_COcclusionContext_CheckAndRecordOverlayCandidate_Pre_19041_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext_CheckAndRecordOverlayCandidate_19041, dwmcore::COcclusionContext_CheckAndRecordOverlayCandidate_19041_t> g_COcclusionContext_CheckAndRecordOverlayCandidate_19041_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext_CheckAndRecordOverlayCandidate_26100, dwmcore::COcclusionContext_CheckAndRecordOverlayCandidate_26100_t> g_COcclusionContext_CheckAndRecordOverlayCandidate_26100_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext_CollectRectangleForOcclusion_Pre_22000, dwmcore::COcclusionContext_CollectRectangleForOcclusion_Pre_22000_t> g_COcclusionContext_CollectRectangleForOcclusion_Pre_22000_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext_CollectRectangleForOcclusion_22000, dwmcore::COcclusionContext_CollectRectangleForOcclusion_22000_t> g_COcclusionContext_CollectRectangleForOcclusion_22000_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext_CollectRectangleForOcclusion_26100_Pre_7840, dwmcore::COcclusionContext_CollectRectangleForOcclusion_26100_Pre_7840_t> g_COcclusionContext_CollectRectangleForOcclusion_26100_Pre_7840_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext_CollectRectangleForOcclusion_26100_7840, dwmcore::COcclusionContext_CollectRectangleForOcclusion_26100_7840_t> g_COcclusionContext_CollectRectangleForOcclusion_26100_7840_Org{};

	Projection::Detour<dwmcore::Symbol_COcclusionContext_Compute_Pre_19041, decltype(&MyCOcclusionContext_Compute_Pre_W10_2004)> g_COcclusionContext_Compute_Pre_W10_2004_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext_Compute_19041, decltype(&MyCOcclusionContext_Compute)> g_COcclusionContext_Compute_Org{};
	decltype(&MyCOcclusionContext_DrawGeometry) g_COcclusionContext_DrawGeometry_Org{ nullptr };
	decltype(&MyCOcclusionContext_DrawGeometry)* g_COcclusionContext_DrawGeometry_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCOcclusionContext_DrawGeometry> g_COcclusionContext_DrawGeometry_Hook;
	Projection::Detour<dwmcore::Symbol_COcclusionContext_SetDeviceTransform, decltype(&MyCOcclusionContext_SetDeviceTransform)> g_COcclusionContext_SetDeviceTransform_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext__COcclusionContext, decltype(&MyCOcclusionContext_Destructor)> g_COcclusionContext_Destructor_Org{};

	Projection::Detour<dwmcore::Symbol_CArrayBasedCoverageSet_IsCovered_Pre_26100, decltype(&MyCArrayBasedCoverageSet_IsCovered_Pre_26100)> g_CArrayBasedCoverageSet_IsCovered_Pre_26100_Org{};
	Projection::Detour<dwmcore::Symbol_CArrayBasedCoverageSet_IsCovered_26100, decltype(&MyCArrayBasedCoverageSet_IsCovered_26100)> g_CArrayBasedCoverageSet_IsCovered_26100_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext_IsOccluded_Pre_22000, decltype(&MyCOcclusionContext_IsOccluded_Pre_22000)> g_COcclusionContext_IsOccluded_Pre_22000_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext_IsOccluded, decltype(&MyCOcclusionContext_IsOccluded_20348)> g_COcclusionContext_IsOccluded_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext_PageInPixelsRectToDeviceRect, decltype(&MyCOcclusionContext_PageInPixelsRectToDeviceRect)> g_COcclusionContext_PageInPixelsRectToDeviceRect_Org{};
	Projection::Detour<dwmcore::Symbol_CHwndRenderTarget_RenderDirtyRegion, decltype(&MyCHwndRenderTarget_RenderDirtyRegion)> g_CHwndRenderTarget_RenderDirtyRegion_Org{};

	Projection::Detour<dwmcore::Symbol_CDirtyRegion_GetUnOccludedDirtyRegion, decltype(&MyCDirtyRegion_GetUnOccludedDirtyRegion)> g_CDirtyRegion_GetUnOccludedDirtyRegion_Org{};
	Projection::Detour<dwmcore::Symbol_CDirtyRegion_GetUnOccludedDirtyRect, decltype(&MyCDirtyRegion_GetUnOccludedDirtyRect)> g_CDirtyRegion_GetUnOccludedDirtyRect_Org{};
	Projection::Detour<dwmcore::Symbol_CDirtyRegion_GetOptimizedRect_Server_2022, decltype(&MyCDirtyRegion_GetOptimizedRect_WS2022)> g_CDirtyRegion_GetOptimizedRect_WS2022_Org{};
	Projection::Detour<dwmcore::Symbol_CDirtyRegion_GetOptimizedRect, decltype(&MyCDirtyRegion_GetOptimizedRect)> g_CDirtyRegion_GetOptimizedRect_Org{};
	Projection::Detour<dwmcore::Symbol_CTreeDirty_GetOptimizedRect, decltype(&MyCTreeDirty_GetOptimizedRect)> g_CTreeDirty_GetOptimizedRect_Org{};

	Projection::Detour<dwmcore::Symbol_CDrawingContext_DrawVisualTree_17763, decltype(&MyCDrawingContext_DrawVisualTree_Win10_1809)> g_CDrawingContext_DrawVisualTree_Win10_1809_Org{};
	Projection::Detour<dwmcore::Symbol_CDrawingContext_DrawVisualTree_18362, decltype(&MyCDrawingContext_DrawVisualTree_Win10_1903)> g_CDrawingContext_DrawVisualTree_Win10_1903_Org{};
	Projection::Detour<dwmcore::Symbol_CDrawingContext_DrawVisualTree_19041, decltype(&MyCDrawingContext_DrawVisualTree_Win10)> g_CDrawingContext_DrawVisualTree_Win10_Org{};
	Projection::Detour<dwmcore::Symbol_CDrawingContext_DrawVisualTree_20348, decltype(&MyCDrawingContext_DrawVisualTree_Win11)> g_CDrawingContext_DrawVisualTree_Win11_Org{};

	bool HasCArrayBasedCoverageSetIsCovered() noexcept
	{
		return
			static_cast<bool>(g_CArrayBasedCoverageSet_IsCovered_Pre_26100_Org) ||
			static_cast<bool>(g_CArrayBasedCoverageSet_IsCovered_26100_Org);
	}

	Util::ObjectPool<dwmcore::CD2DContext*, CGlassSafetyZoneLayer> g_safetyZonePool{};

	std::unordered_map<dwmcore::COcclusionContext*, ULONGLONG> g_shrunkCoverageSetMap{};
	void ShrinkOccludersAboveGlass(dwmcore::COcclusionContext* occlusionContext);

	struct CCollectedOcclusionRectangle
	{
		dwmcore::COcclusionContext* context{};
		D2D1_RECT_F rectangle{};
		int depth{ -1 };
	};
	CCollectedOcclusionRectangle g_lastCollectedOcclusionRectangle{};

	enum class GlassSafetyZoneMode : UCHAR
	{
		Disabled,
		Visible,
		Always
	};
	GlassSafetyZoneMode g_glassSafetyZoneMode{ GlassSafetyZoneMode::Visible };

	struct UnoccludedDirtyRegionCalculationContext
	{
		dwmcore::COcclusionContext* occlusionContext;
		D2D1_RECT_F* dirtyRect;
		dwmcore::CMILMatrix deviceTransform;
		UINT deviceTransformFlag;

		void Enter(dwmcore::COcclusionContext* context)
		{
			if (g_glassSafetyZoneMode != GlassSafetyZoneMode::Disabled && context)
			{
				occlusionContext = context;
				if (HasCArrayBasedCoverageSetIsCovered())
				{
					dirtyRect = nullptr;
					deviceTransformFlag = *occlusionContext->GetDeviceTransformFlag();
					if (!(deviceTransformFlag & 0x1))
					{
						deviceTransform = *occlusionContext->GetDeviceTransform();
						*occlusionContext->GetDeviceTransformFlag() |= 0x1;
						*const_cast<dwmcore::CMILMatrix*>(occlusionContext->GetDeviceTransform()) = *dwmcore::CMILMatrix::Identity;
					}
				}
			}
		}
		void Leave()
		{
			if (occlusionContext)
			{
				if (HasCArrayBasedCoverageSetIsCovered())
				{
					if (!(deviceTransformFlag & 0x1))
					{
						*occlusionContext->GetDeviceTransformFlag() = deviceTransformFlag;
						*const_cast<dwmcore::CMILMatrix*>(occlusionContext->GetDeviceTransform()) = deviceTransform;
					}
					dirtyRect = nullptr;
					deviceTransformFlag = 0;
				}
				occlusionContext = nullptr;
			}
		}
		bool IsActive()
		{
			return occlusionContext != nullptr;
		}
	};
	using udrcc_leave_scope_exit = wil::unique_any<UnoccludedDirtyRegionCalculationContext*, decltype(&UnoccludedDirtyRegionCalculationContext::Leave), &UnoccludedDirtyRegionCalculationContext::Leave, wil::details::pointer_access_none>;
	[[nodiscard]] inline udrcc_leave_scope_exit EnterUnoccludedDirtyRegionCalculationContext(UnoccludedDirtyRegionCalculationContext* pudrcc, dwmcore::COcclusionContext* context) noexcept
	{
		pudrcc->Enter(context);
		return udrcc_leave_scope_exit{ pudrcc };
	}
	UnoccludedDirtyRegionCalculationContext g_calculationContext{};

	uint16_t g_COcclusionContext_IsDeviceTransformAssigned_Instructions[] =
	{
		0x41, 0x80, 0xBF, HookHelper::c_patwc, 0x03, 0x00, 0x00, 0x00,	// cmp     byte ptr[r15 + 3xxh], 0
		0x74, 0x23,														// jz      short xx
	};
	uint8_t* g_COcclusionContext_IsDeviceTransformAssigned_PatchLocation = nullptr;
	HookHelper::InstructionPatch g_COcclusionContext_IsDeviceTransformAssigned_Patch;

	struct CoverageSetCheckpoint
	{
		dwmcore::COcclusionContext* context;
		ULONGLONG id;
		std::unordered_map<dwmcore::CZOrderedRectBase*, D2D1_RECT_F> saves;
	};
	std::unordered_map<dwmcore::CArrayBasedCoverageSet*, CoverageSetCheckpoint> g_coverageSetCheckpointMap;

	void CaptureCollectedOcclusionRectangle(
		dwmcore::COcclusionContext* occlusionContext,
		const D2D1_RECT_F* rectangle
	)
	{
		g_lastCollectedOcclusionRectangle = {};
		if (rectangle)
		{
			g_lastCollectedOcclusionRectangle = {
				occlusionContext,
				*rectangle,
				static_cast<int>(occlusionContext->GetCurrentZ())
			};
		}
	}

	bool IsOverlayCandidateCoveredByGlass(
		dwmcore::COcclusionContext* occlusionContext,
		const dwmcore::CShape* shape
	)
	{
		const auto collected = std::exchange(g_lastCollectedOcclusionRectangle, {});
		if (!GlassKernel::GetBlurRadius())
		{
			return false;
		}

		const auto depth = static_cast<int>(occlusionContext->GetCurrentZ());
		D2D1_RECT_F deviceBounds{};
		if (shape)
		{
			D2D1_RECT_F bounds{};
			if (SUCCEEDED(shape->GetTightBounds(&bounds, occlusionContext->GetWorldTransform())))
			{
				deviceBounds = occlusionContext->PageInPixelsRectToDeviceRect(bounds);
			}
		}
		else if (
			collected.context == occlusionContext &&
			collected.depth == depth
		)
		{
			const auto worldBounds = RectF::TransformRect(
				collected.rectangle,
				occlusionContext->GetWorldTransform()->GetD2DMatrix()
			);
			deviceBounds = occlusionContext->PageInPixelsRectToDeviceRect(worldBounds);
		}

		if (wil::rect_is_empty(deviceBounds))
		{
			return false;
		}

		const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(occlusionContext->GetArrayBasedCoverageSet());
		return glassCoverageSet && glassCoverageSet->IsPartiallyCovered(deviceBounds, depth);
	}
}

void GlassIntegrity::ShrinkOccludersAboveGlass(dwmcore::COcclusionContext* occlusionContext)
{
	const auto frameId = dwmcore::g_versionInfo.build < os::build_w10_2004 ? dwmcore::GetCurrentFrameId() : occlusionContext->GetFrameId();
	const auto expansion = GlassKernel::GetBlurRadius();

	if (!expansion)
	{
		return;
	}
	const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(occlusionContext->GetArrayBasedCoverageSet());
	if (!glassCoverageSet || glassCoverageSet->IsEmpty())
	{
		return;
	}
	if (g_shrunkCoverageSetMap[occlusionContext] == frameId)
	{
		return;
	}
	else
	{
		g_shrunkCoverageSetMap[occlusionContext] = frameId;
	}

	enum ShrinkSide
	{
		ShrinkSide_Left,
		ShrinkSide_Top,
		ShrinkSide_Right,
		ShrinkSide_Bottom
	};
	std::unordered_set<const dwmcore::CZOrderedRect*> visibleGlassSet{};

	const auto coverageSet = occlusionContext->GetArrayBasedCoverageSet();
	const auto matrix = occlusionContext->GetDeviceTransform();
	for (const auto& glassRegion : glassCoverageSet->GetViews())
	{
		if (
			!coverageSet->IsCovered(
				glassRegion.m_transformedRect,
				glassRegion.m_depth
			)
		)
		{
			visibleGlassSet.insert(&glassRegion);
		}
	}

	const auto collectAndTryShrinkOccluders = [visibleGlassSet, expansion, &matrix, occlusionContext, coverageSet, frameId](auto&& views)
	{
		using CZOrderedRectT = std::remove_reference_t<decltype(views)>::value_type;
		std::unordered_map<CZOrderedRectT*, std::bitset<4>> targetOccluderSet{};

		for (const auto& glassRegion : visibleGlassSet)
		{
			for (auto& occluder : views)
			{
				if (occluder.m_depth >= glassRegion->m_depth)
				{
					break;
				}

				const D2D1_RECT_F& glassRect = glassRegion->m_transformedRect;
				if (
					!wil::rect_is_empty(occluder.m_transformedRect) &&
					std::abs(wil::rect_height(occluder.m_transformedRect) * wil::rect_width(occluder.m_transformedRect)) > 1.f &&

					RectF::DoesIntersectUnsafe(occluder.m_transformedRect, glassRect)
				)
				{
					std::bitset<4> sides{};
					if (occluder.m_transformedRect.left > glassRect.left)
					{
						sides.set(ShrinkSide_Left, true);
					}
					if (occluder.m_transformedRect.top > glassRect.top)
					{
						sides.set(ShrinkSide_Top, true);
					}
					if (occluder.m_transformedRect.right < glassRect.right)
					{
						sides.set(ShrinkSide_Right, true);
					}
					if (occluder.m_transformedRect.bottom < glassRect.bottom)
					{
						sides.set(ShrinkSide_Bottom, true);
					}
					targetOccluderSet.try_emplace(&occluder, sides).first->second |= sides;
				}
			}
		}

		auto& checkpoint = g_coverageSetCheckpointMap[coverageSet];
		checkpoint.context = occlusionContext;
		checkpoint.id = frameId;
		checkpoint.saves.clear();
		checkpoint.saves.reserve(targetOccluderSet.size());
		for (auto& [occluder, sides] : targetOccluderSet)
		{
			auto& originalRect = occluder->m_originalRect;

			checkpoint.saves.emplace(const_cast<dwmcore::CZOrderedRectBase*>(reinterpret_cast<dwmcore::CZOrderedRectBase const*>(occluder)), originalRect);

			if (sides.test(ShrinkSide_Left))
			{
				originalRect.left += expansion;
			}
			if (sides.test(ShrinkSide_Top))
			{
				originalRect.top += expansion;
			}
			if (sides.test(ShrinkSide_Right))
			{
				originalRect.right -= expansion;
			}
			if (sides.test(ShrinkSide_Bottom))
			{
				originalRect.bottom -= expansion;
			}

			if (wil::rect_is_empty(originalRect))
			{
				originalRect = {};
			}
			occluder->UpdateDeviceRect(matrix);
		}
	};

	if (Util::VersionBefore<os::build_w11_24h2, os::revision_24h2_with_25h2_code_staged>(dwmcore::g_versionInfo.build, dwmcore::g_versionInfo.revision))
	{
		collectAndTryShrinkOccluders(coverageSet->GetOccluderArray()->views());
	}
	else
	{
		collectAndTryShrinkOccluders(coverageSet->GetOccluderArray2()->views());
	}
}

void GlassIntegrity::FlipOccludersCheckpoint(dwmcore::CArrayBasedCoverageSet* coverageSet)
{
	if (
		const auto it = g_coverageSetCheckpointMap.find(coverageSet);
		it != g_coverageSetCheckpointMap.end()
	)
	{
		auto& checkpoint = it->second;
		if (dwmcore::GetCurrentFrameId() != checkpoint.id)
		{
			return;
		}

		const auto matrix = checkpoint.context->GetDeviceTransform();
		for (auto& [occluder, backup] : checkpoint.saves)
		{
			std::swap(occluder->GetOriginalRect(), backup);
			occluder->UpdateDeviceRect(matrix);
		}
	}
}

HRESULT GlassIntegrity::MyCOcclusionContext_CollectRectangleForOcclusion(
	dwmcore::COcclusionContext* This,
	const D2D1_RECT_F* rectangle,
	bool unknown,
	D2D1_RECT_F* outRect
)
{
	CaptureCollectedOcclusionRectangle(This, rectangle);
	const auto hr = g_COcclusionContext_CollectRectangleForOcclusion_Pre_22000_Org(This, rectangle, unknown, outRect);
	return hr;
}

void GlassIntegrity::MyCOcclusionContext_CollectRectangleForOcclusion(
	dwmcore::COcclusionContext* This,
	const D2D1_RECT_F* rectangle,
	bool unknown1,
	bool unknown2,
	D2D1_RECT_F* outRect
)
{
	CaptureCollectedOcclusionRectangle(This, rectangle);
	g_COcclusionContext_CollectRectangleForOcclusion_22000_Org(This, rectangle, unknown1, unknown2, outRect);
}

void GlassIntegrity::MyCOcclusionContext_CollectRectangleForOcclusion(
	dwmcore::COcclusionContext* This,
	const D2D1_RECT_F& rectangle,
	D2D1_RECT_F* outRect
)
{
	CaptureCollectedOcclusionRectangle(This, &rectangle);
	g_COcclusionContext_CollectRectangleForOcclusion_26100_Pre_7840_Org(This, rectangle, outRect);
}

void GlassIntegrity::MyCOcclusionContext_CollectRectangleForOcclusion(
	dwmcore::COcclusionContext* This,
	const D2D1_RECT_F& rectangle,
	bool unknown,
	D2D1_RECT_F* outRect
)
{
	CaptureCollectedOcclusionRectangle(This, &rectangle);
	g_COcclusionContext_CollectRectangleForOcclusion_26100_7840_Org(This, rectangle, unknown, outRect);
}

HRESULT GlassIntegrity::MyCOcclusionContext_CheckAndRecordOverlayCandidate(
	dwmcore::COcclusionContext* This,
	dwmcore::CVisual* visual,
	dwmcore::ISwapChainContent* surface,
	const dwmcore::CMILMatrix* matrix,
	const dwmcore::CShape* shape,
	int flags
)
{
	if (IsOverlayCandidateCoveredByGlass(This, shape))
	{
		return S_OK;
	}
	return g_COcclusionContext_CheckAndRecordOverlayCandidate_Pre_19041_Org(This, visual, surface, matrix, shape, flags);
}

HRESULT GlassIntegrity::MyCOcclusionContext_CheckAndRecordOverlayCandidate(
	dwmcore::COcclusionContext* This,
	dwmcore::CVisual* visual,
	dwmcore::CCompositionSurfaceInfo* surface,
	const dwmcore::CMILMatrix* matrix,
	const dwmcore::CShape* shape,
	int flags
)
{
	if (IsOverlayCandidateCoveredByGlass(This, shape))
	{
		return S_OK;
	}
	return g_COcclusionContext_CheckAndRecordOverlayCandidate_19041_Org(This, visual, surface, matrix, shape, flags);
}

HRESULT GlassIntegrity::MyCOcclusionContext_CheckAndRecordOverlayCandidate(
	dwmcore::COcclusionContext* This,
	dwmcore::CVisual* visual,
	dwmcore::CCompositionSurfaceInfo* surface,
	const dwmcore::CMILMatrix& matrix,
	const dwmcore::CShape* shape,
	int flags
)
{
	if (IsOverlayCandidateCoveredByGlass(This, shape))
	{
		return S_OK;
	}
	return g_COcclusionContext_CheckAndRecordOverlayCandidate_26100_Org(This, visual, surface, matrix, shape, flags);
}

HRESULT GlassIntegrity::MyCOcclusionContext_Compute_Pre_W10_2004(
	dwmcore::COcclusionContext* This,
	const dwmcore::CVisualTree* visualTree,
	UINT count,
	const D2D1_RECT_F* rectangles,
	float unknown1,
	bool unknown2,
	const dwmcore::CMILMatrix* matrix,
	const DWM::span<dwmcore::COverlayContext*>& overlays
)
{
	HRESULT hr{ S_OK };
	if (!g_COcclusionContext_DrawGeometry_Org)
	{
		g_COcclusionContext_DrawGeometry_Org_Address = dwmcore::IDrawingContext_DrawGeometry_VtableSlot.address(
			HookHelper::get_vftable_from(This)
		);
		g_COcclusionContext_DrawGeometry_Hook.AttachOnce(g_COcclusionContext_DrawGeometry_Org_Address, &g_COcclusionContext_DrawGeometry_Org);
	}
	if (const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(This->GetArrayBasedCoverageSet()); glassCoverageSet)
	{
		glassCoverageSet->Clear();
	}
	const auto expansion = GlassKernel::GetBlurRadius();
	if (
		expansion &&
		count
	)
	{
		const auto extendedRectangles = std::make_unique_for_overwrite<D2D1_RECT_F[]>(count);
		memcpy_s(
			extendedRectangles.get(),
			count * sizeof(D2D1_RECT_F),
			rectangles,
			count * sizeof(D2D1_RECT_F)
		);
		for (auto& rectangle : std::span{ extendedRectangles.get(), count })
		{
			rectangle.left -= expansion * 2.f;
			rectangle.top -= expansion * 2.f;
			rectangle.right += expansion * 2.f;
			rectangle.bottom += expansion * 2.f;
		}

		hr = g_COcclusionContext_Compute_Pre_W10_2004_Org(
			This,
			visualTree,
			count,
			extendedRectangles.get(),
			unknown1,
			unknown2,
			matrix,
			overlays
		);
	}
	else
	{
		hr = g_COcclusionContext_Compute_Pre_W10_2004_Org(
			This,
			visualTree,
			count,
			rectangles,
			unknown1,
			unknown2,
			matrix,
			overlays
		);
	}

	return hr;
}
HRESULT GlassIntegrity::MyCOcclusionContext_Compute(
	dwmcore::COcclusionContext* This,
	const dwmcore::CVisualTree* visualTree,
	const DWM::span<const D2D1_RECT_F>& rectangles,
	float unknown,
	const DWM::span<dwmcore::COverlayContext*>& overlays
)
{
	HRESULT hr{ S_OK };
	if (!g_COcclusionContext_DrawGeometry_Org)
	{
		g_COcclusionContext_DrawGeometry_Org_Address = dwmcore::IDrawingContext_DrawGeometry_VtableSlot.address(
			HookHelper::get_vftable_from(This)
		);
		g_COcclusionContext_DrawGeometry_Hook.AttachOnce(g_COcclusionContext_DrawGeometry_Org_Address, &g_COcclusionContext_DrawGeometry_Org);
	}
	if (const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(This->GetArrayBasedCoverageSet()); glassCoverageSet)
	{
		glassCoverageSet->Clear();
	}
	const auto expansion = GlassKernel::GetBlurRadius();
	if (
		expansion &&
		rectangles.length
	)
	{
		const auto extendedRectangles = std::make_unique_for_overwrite<D2D1_RECT_F[]>(rectangles.length);
		memcpy_s(
			extendedRectangles.get(),
			rectangles.length * sizeof(D2D1_RECT_F),
			rectangles.data,
			rectangles.length * sizeof(D2D1_RECT_F)
		);
		for (auto& rectangle : std::span{ extendedRectangles.get(), rectangles.length })
		{
			rectangle.left -= expansion * 2.f;
			rectangle.top -= expansion * 2.f;
			rectangle.right += expansion * 2.f;
			rectangle.bottom += expansion * 2.f;
		}

		hr = g_COcclusionContext_Compute_Org(
			This,
			visualTree,
			DWM::span<const D2D1_RECT_F>{ rectangles.length, extendedRectangles.get() },
			unknown,
			overlays
		);
	}
	else
	{
		hr = g_COcclusionContext_Compute_Org(
			This,
			visualTree,
			rectangles,
			unknown,
			overlays
		);
	}

	return hr;
}
HRESULT GlassIntegrity::MyCOcclusionContext_DrawGeometry(
	dwmcore::IDrawingContext* This,
	dwmcore::CLegacyMilBrush* brush,
	dwmcore::CGeometry* geometry
)
{
	const auto hr = g_COcclusionContext_DrawGeometry_Org(
		This,
		brush,
		geometry
	);

	if (
		FAILED(hr) ||
		HookHelper::get_vftable_from(brush) != dwmcore::CSolidColorLegacyMilBrush::vftable
	)
	{
		return hr;
	}

	dwmcore::CShapePtr geometryShape{};
	if (
		FAILED(geometry->GetShapeData(nullptr, &geometryShape)) ||
		!geometryShape ||
		geometryShape->IsEmpty()
	)
	{
		return hr;
	}

	const auto solidColorBrush = static_cast<dwmcore::CSolidColorLegacyMilBrush*>(brush);
	auto color = solidColorBrush->GetRealizedColor();
	const auto expansion = GlassKernel::GetBlurRadius();
	const auto reinterpreter = GlassKernel::AlphaChannelReinterpreter(color.a);
	const auto valid = reinterpreter.GetIsValid();
	const auto active = reinterpreter.GetIsActive();
	const auto maximized = reinterpreter.GetIsMaximized();

	if (
		GlassKernel::CRealizedGlassColorizationParameters realizedGlassColorizationParameters;
		!valid ||
		(
			valid &&
			(
				color.a = 1.f,
				realizedGlassColorizationParameters = GlassKernel::RealizeWindowColorization(
					GlassKernel::GetBaseColor(Shared::IsTransparencyDisabled(), maximized),
					GlassKernel::GetSourceColor(active),
					GlassKernel::GetColorizationOpacity(active, maximized),
					Shared::IsTransparencyDisabled(),
					false
				),
				Shared::IsTransparencyDisabled() ||
				Shared::IsGlassFullyOpaque(
					realizedGlassColorizationParameters.color.a,
					realizedGlassColorizationParameters.blurBalance,
					realizedGlassColorizationParameters.afterglowBalance
				)
			)
		)
	)
	{
		UINT count{};
		if (!geometryShape->IsRectangles(&count))
		{
			return hr;
		}
		const auto rectangles = std::make_unique_for_overwrite<D2D1_RECT_F[]>(count);
		if (!geometryShape->GetRectangles(rectangles.get(), count))
		{
			return hr;
		}

		for (const auto& rect : std::span{ rectangles.get(), count })
		{
			RETURN_IF_FAILED(
				This->DrawSolidRectangle(
					rect,
					color
				)
			);
		}
	}
	// here are the glass regions
	else if (
		valid &&
		expansion
	)
	{
		const auto occlusionContext = This->GetOcclusionContext();
		if (const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(occlusionContext->GetArrayBasedCoverageSet(), true); glassCoverageSet)
		{
			D2D1_RECT_F bounds{};
			RETURN_IF_FAILED(geometryShape->GetTightBounds(&bounds, occlusionContext->GetWorldTransform()));
			if (
				!wil::rect_is_empty(bounds) &&
				std::abs(wil::rect_height(bounds) * wil::rect_width(bounds)) > 1.f
			)
			{
				glassCoverageSet->Add(
					bounds,
					occlusionContext->GetCurrentZ(),
					occlusionContext->GetDeviceTransform()
				);
			}
		}
	}

	return hr;
}

HRESULT GlassIntegrity::MyCOcclusionContext_SetDeviceTransform(
	dwmcore::COcclusionContext* This,
	const dwmcore::CMILMatrix* matrix
)
{
	if (
		const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(This->GetArrayBasedCoverageSet());
		glassCoverageSet
	)
	{
		glassCoverageSet->SetDeviceTransform(matrix);
	}

	return g_COcclusionContext_SetDeviceTransform_Org(This, matrix);
}

void GlassIntegrity::MyCOcclusionContext_Destructor(dwmcore::COcclusionContext* This)
{
	if (g_lastCollectedOcclusionRectangle.context == This)
	{
		g_lastCollectedOcclusionRectangle = {};
	}
	CArrayBasedGlassCoverageSet::Remove(This->GetArrayBasedCoverageSet());
	g_coverageSetCheckpointMap.erase(This->GetArrayBasedCoverageSet());
	g_shrunkCoverageSetMap.erase(This);
	return g_COcclusionContext_Destructor_Org(This);
}


template <typename Callback>
bool GlassIntegrity::MyCArrayBasedCoverageSet_IsCovered(
	dwmcore::CArrayBasedCoverageSet* This,
	const D2D1_RECT_F& coverage,
	int depth,
	Callback&& invokeOriginal
)
{
	FlipOccludersCheckpointScoped(This);

	const auto expansion = GlassKernel::GetBlurRadius();
	if (!expansion || !g_calculationContext.IsActive())
	{
		return invokeOriginal(coverage, depth);
	}

	const auto dirtyRectScope = wil::scope_exit([] static { g_calculationContext.dirtyRect = nullptr; });
	const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(This);
	if (Util::VersionBefore<os::build_w11_24h2, os::revision_24h2_with_25h2_code_staged>(dwmcore::g_versionInfo.build, dwmcore::g_versionInfo.revision))
	{
		auto covered = invokeOriginal(coverage, depth);
		if (
			glassCoverageSet &&
			!glassCoverageSet->IsEmpty() &&
			!covered &&
			glassCoverageSet->IsPartiallyCovered(coverage, depth)
		)
		{
			const D2D1_RECT_F extendedCoverage
			{
				coverage.left - expansion,
				coverage.top - expansion,
				coverage.right + expansion,
				coverage.bottom + expansion
			};

			covered = invokeOriginal(extendedCoverage, depth);

			if (!covered)
			{
				// unpaged dirty rect
				if (g_calculationContext.dirtyRect)
				{
					g_calculationContext.dirtyRect->left -= expansion;
					g_calculationContext.dirtyRect->top -= expansion;
					g_calculationContext.dirtyRect->right += expansion;
					g_calculationContext.dirtyRect->bottom += expansion;
				}
				// paged dirty rect
				const_cast<D2D1_RECT_F&>(coverage) = extendedCoverage;
			}
		}

		return covered;
	}
	else
	{
		const D2D1_RECT_F shrunkCoverage =
		{
			coverage.left + expansion,
			coverage.top + expansion,
			coverage.right - expansion,
			coverage.bottom - expansion
		};
		auto covered = invokeOriginal(shrunkCoverage, depth);
		bool shrink = true;
		if (
			glassCoverageSet &&
			!glassCoverageSet->IsEmpty() &&
			glassCoverageSet->IsPartiallyCovered(shrunkCoverage, depth) &&
			!covered
		)
		{
			covered = invokeOriginal(coverage, depth);

			if (!covered)
			{
				shrink = false;
			}
		}

		if (shrink)
		{
			// unpaged dirty rect
			if (g_calculationContext.dirtyRect)
			{
				g_calculationContext.dirtyRect->left += expansion;
				g_calculationContext.dirtyRect->top += expansion;
				g_calculationContext.dirtyRect->right -= expansion;
				g_calculationContext.dirtyRect->bottom -= expansion;
			}
			// paged dirty rect
			const_cast<D2D1_RECT_F&>(coverage) = shrunkCoverage;
		}

		return covered;
	}
}

bool GlassIntegrity::MyCArrayBasedCoverageSet_IsCovered_Pre_26100(
	dwmcore::CArrayBasedCoverageSet* This,
	const D2D1_RECT_F& coverage,
	int depth,
	bool unknown
)
{
	return MyCArrayBasedCoverageSet_IsCovered(
		This,
		coverage,
		depth,
		[This, unknown](const D2D1_RECT_F& candidate, int candidateDepth)
		{
			return g_CArrayBasedCoverageSet_IsCovered_Pre_26100_Org(
				This,
				candidate,
				candidateDepth,
				unknown
			);
		}
	);
}

bool GlassIntegrity::MyCArrayBasedCoverageSet_IsCovered_26100(
	dwmcore::CArrayBasedCoverageSet* This,
	const D2D1_RECT_F& coverage,
	int depth,
	bool unknown1,
	bool unknown2
)
{
	return MyCArrayBasedCoverageSet_IsCovered(
		This,
		coverage,
		depth,
		[This, unknown1, unknown2](const D2D1_RECT_F& candidate, int candidateDepth)
		{
			return g_CArrayBasedCoverageSet_IsCovered_26100_Org(
				This,
				candidate,
				candidateDepth,
				unknown1,
				unknown2
			);
		}
	);
}

template <typename Callback>
bool GlassIntegrity::MyCOcclusionContext_IsOccluded(
	dwmcore::COcclusionContext* This,
	const D2D1_RECT_F& coverage,
	int depth,
	bool ignoreDeviceTransform,
	Callback&& invokeOriginal
)
{
	FlipOccludersCheckpointScoped(This->GetArrayBasedCoverageSet());

	auto occluded = invokeOriginal(
		coverage,
		depth,
		ignoreDeviceTransform
	);

	const auto expansion = GlassKernel::GetBlurRadius();
	if (!expansion || !g_calculationContext.IsActive())
	{
		return occluded;
	}

	const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(This->GetArrayBasedCoverageSet());

	if (
		glassCoverageSet &&
		!glassCoverageSet->IsEmpty() &&
		glassCoverageSet->IsPartiallyCovered(This->PageInPixelsRectToDeviceRect(coverage), depth)
	)
	{
		const D2D1_RECT_F extendedCoverage
		{
			coverage.left - expansion,
			coverage.top - expansion,
			coverage.right + expansion,
			coverage.bottom + expansion
		};

		occluded = invokeOriginal(
			extendedCoverage,
			depth,
			ignoreDeviceTransform
		);

		if (!occluded)
		{
			// coverage is actually paged dirty rect
			const_cast<D2D1_RECT_F&>(coverage) = extendedCoverage;
		}
	}

	return occluded;
}

bool GlassIntegrity::MyCOcclusionContext_IsOccluded_Pre_22000(
	dwmcore::COcclusionContext* This,
	const D2D1_RECT_F& coverage,
	bool ignoreDeviceTransform,
	int depth
)
{
	return MyCOcclusionContext_IsOccluded(
		This,
		coverage,
		depth,
		ignoreDeviceTransform,
		[This](const D2D1_RECT_F& candidate, int candidateDepth, bool candidateIgnoreDeviceTransform)
		{
			return g_COcclusionContext_IsOccluded_Pre_22000_Org(
				This,
				candidate,
				candidateIgnoreDeviceTransform,
				candidateDepth
			);
		}
	);
}

bool GlassIntegrity::MyCOcclusionContext_IsOccluded_20348(
	dwmcore::COcclusionContext* This,
	const D2D1_RECT_F& coverage,
	int depth,
	bool ignoreDeviceTransform
)
{
	return MyCOcclusionContext_IsOccluded(
		This,
		coverage,
		depth,
		ignoreDeviceTransform,
		[This](const D2D1_RECT_F& candidate, int candidateDepth, bool candidateIgnoreDeviceTransform)
		{
			return g_COcclusionContext_IsOccluded_Org(
				This,
				candidate,
				candidateDepth,
				candidateIgnoreDeviceTransform
			);
		}
	);
}
bool GlassIntegrity::MyCOcclusionContext_PageInPixelsRectToDeviceRect(
	dwmcore::COcclusionContext* This,
	const D2D1_RECT_F& src,
	D2D1_RECT_F* dst
)
{
	if (g_calculationContext.IsActive())
	{
		g_calculationContext.dirtyRect = const_cast<D2D1_RECT_F*>(&src);
		if (
			const auto expansion = GlassKernel::GetBlurRadius();
			expansion &&
			!Util::VersionBefore<os::build_w11_24h2, os::revision_24h2_with_25h2_code_staged>(dwmcore::g_versionInfo.build, dwmcore::g_versionInfo.revision)
		)
		{
			g_calculationContext.dirtyRect->left -= expansion;
			g_calculationContext.dirtyRect->top -= expansion;
			g_calculationContext.dirtyRect->right += expansion;
			g_calculationContext.dirtyRect->bottom += expansion;
		}
	}

	return g_COcclusionContext_PageInPixelsRectToDeviceRect_Org(
		This,
		src,
		dst
	);
}

HRESULT GlassIntegrity::MyCHwndRenderTarget_RenderDirtyRegion(
	dwmcore::CHwndRenderTarget* This,
	dwmcore::CDrawingContext* drawingContext,
	dwmcore::CComposeTop* composeTop
)
{
	const auto calculationScope = EnterUnoccludedDirtyRegionCalculationContext(
		&g_calculationContext,
		drawingContext->GetOcclusionContext()
	);
	return g_CHwndRenderTarget_RenderDirtyRegion_Org(
		This,
		drawingContext,
		composeTop
	);
}
D2D1_RECT_F* GlassIntegrity::MyCDirtyRegion_GetUnOccludedDirtyRegion(
	dwmcore::CDirtyRegion* This,
	D2D1_RECT_F* dirtyRect,
	dwmcore::COcclusionContext* occlusionContext,
	const dwmcore::CVisualTree* tree,
	bool inflate,
	unsigned int i,
	const D2D1_RECT_F& bounds
)
{
	const auto calculationScope = EnterUnoccludedDirtyRegionCalculationContext(
		&g_calculationContext,
		occlusionContext
	);
	return g_CDirtyRegion_GetUnOccludedDirtyRegion_Org(
		This,
		dirtyRect,
		occlusionContext,
		tree,
		inflate,
		i,
		bounds
	);
}
D2D1_RECT_F* GlassIntegrity::MyCDirtyRegion_GetUnOccludedDirtyRect(
	dwmcore::CDirtyRegion* This,
	D2D1_RECT_F* dirtyRect,
	UINT i,
	const D2D1_RECT_F& bounds,
	bool useSuperSample,
	const DWM::span<const dwmcore::CVisual*>& visuals,
	const dwmcore::COcclusionContext* occlusionContext
)
{
	const auto context = occlusionContext ? occlusionContext : This->GetOcclusionContext();
	const auto calculationScope = EnterUnoccludedDirtyRegionCalculationContext(
		&g_calculationContext,
		context->GetFrameId() == dwmcore::GetCurrentFrameId()
			? const_cast<dwmcore::COcclusionContext*>(context)
			: nullptr
	);
	return g_CDirtyRegion_GetUnOccludedDirtyRect_Org(
		This,
		dirtyRect,
		i,
		bounds,
		useSuperSample,
		visuals,
		occlusionContext
	);
}
D2D1_RECT_F* GlassIntegrity::MyCDirtyRegion_GetOptimizedRect_WS2022(
	dwmcore::CDirtyRegion* This,
	D2D1_RECT_F* dirtyRect,
	UINT i,
	const D2D1_RECT_F& bounds,
	const D2D1_SIZE_U& size,
	bool transform,
	const dwmcore::CMILMatrix* matrix,
	const DWM::span<const dwmcore::CVisual*>& visuals,
	dwmcore::CRegion* region,
	const dwmcore::COcclusionContext* occlusionContext
)
{
	const auto context = occlusionContext ? occlusionContext : This->GetOcclusionContext();
	const auto calculationScope = EnterUnoccludedDirtyRegionCalculationContext(
		&g_calculationContext,
		context->GetFrameId() == dwmcore::GetCurrentFrameId()
			? const_cast<dwmcore::COcclusionContext*>(context)
			: nullptr
	);
	return g_CDirtyRegion_GetOptimizedRect_WS2022_Org(
		This,
		dirtyRect,
		i,
		bounds,
		size,
		transform,
		matrix,
		visuals,
		region,
		occlusionContext
	);
}
D2D1_RECT_F* GlassIntegrity::MyCDirtyRegion_GetOptimizedRect(
	dwmcore::CDirtyRegion* This,
	D2D1_RECT_F* dirtyRect,
	UINT i,
	const D2D1_RECT_F& bounds,
	dwmcore::CRegion* region,
	const dwmcore::CMILMatrix* matrix,
	bool useSuperSample,
	const DWM::span<const dwmcore::CVisual*>& visuals,
	const dwmcore::COcclusionContext* occlusionContext
)
{
	const auto context = occlusionContext ? occlusionContext : This->GetOcclusionContext();
	const auto calculationScope = EnterUnoccludedDirtyRegionCalculationContext(
		&g_calculationContext,
		context->GetFrameId() == dwmcore::GetCurrentFrameId()
			? const_cast<dwmcore::COcclusionContext*>(context)
			: nullptr
	);
	return g_CDirtyRegion_GetOptimizedRect_Org(
		This,
		dirtyRect,
		i,
		bounds,
		region,
		matrix,
		useSuperSample,
		visuals,
		occlusionContext
	);
}
D2D1_RECT_F* GlassIntegrity::MyCTreeDirty_GetOptimizedRect(
	dwmcore::CTreeDirty* This,
	D2D1_RECT_F* dirtyRect,
	UINT i,
	const D2D1_RECT_F& bounds,
	const dwmcore::COcclusionContext& occlusionContext,
	dwmcore::CRegion* region,
	const dwmcore::CMILMatrix* matrix,
	bool useSuperSample,
	const DWM::span<const dwmcore::CVisual*>& visuals
)
{
	const auto calculationScope = EnterUnoccludedDirtyRegionCalculationContext(
		&g_calculationContext,
		occlusionContext.GetFrameId() == dwmcore::GetCurrentFrameId()
			? const_cast<dwmcore::COcclusionContext*>(&occlusionContext)
			: nullptr
	);
	return g_CTreeDirty_GetOptimizedRect_Org(
		This,
		dirtyRect,
		i,
		bounds,
		occlusionContext,
		region,
		matrix,
		useSuperSample,
		visuals
	);
}

template <typename T>
HRESULT GlassIntegrity::MyCDrawingContext_DrawVisualTree(
	dwmcore::CDrawingContext* This,
	const D2D1_RECT_F& rectangle,
	const dwmcore::COcclusionContext* occlusionContext,
	T&& callback
)
{
	HRESULT hr{ S_OK };

	const auto safetyZonePoolCleanup = wil::scope_exit([]
	{
		g_safetyZonePool.Cleanup(std::chrono::seconds{ 30 });
	});

	do
	{
		if (g_glassSafetyZoneMode == GlassSafetyZoneMode::Disabled)
		{
			break;
		}

		if (
			!occlusionContext ||
			(
				dwmcore::g_versionInfo.build >= os::build_w10_2004 &&
				occlusionContext->GetFrameId() != dwmcore::GetCurrentFrameId()
			)
		)
		{
			break;
		}

		const auto expansion = GlassKernel::GetBlurRadius();
		if (!expansion)
		{
			break;
		}

		auto mutableOcclusionContext = const_cast<dwmcore::COcclusionContext*>(occlusionContext);
		mutableOcclusionContext->SetDeviceTransform(This->GetDeviceTransform());
		const auto coverageSet = occlusionContext->GetArrayBasedCoverageSet();
		const auto transformedRect = occlusionContext->PageInPixelsRectToDeviceRect(rectangle);
		const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(coverageSet);

		if (
			g_glassSafetyZoneMode != GlassSafetyZoneMode::Always &&
			(
				!glassCoverageSet ||
				glassCoverageSet->IsEmpty() ||
				!glassCoverageSet->IsVisible(transformedRect, coverageSet)
			)
		)
		{
			break;
		}

		ShrinkOccludersAboveGlass(mutableOcclusionContext);
		if (GlassKernel::IsCurrentCVIFullyTransparent())
		{
			break;
		}

		const auto d2dContext = This->GetD3DDevice()->GetD2DContext();
		const auto context = d2dContext->GetDeviceContext();

		if (dwmcore::g_versionInfo.build < os::build_w10_2004)
		{
			if (!This->GetRenderTarget())
			{
				return S_OK;
			}
		}
		else
		{
			if (!This->GetDeviceTarget())
			{
				return S_OK;
			}
		}
		if (!context)
		{
			break;
		}

		LOG_IF_FAILED(This->ApplyRenderStateInternal(false)); // apply clip and other states
		LOG_IF_FAILED(This->FlushD2D()); // flush previous draw calls
		d2dContext->EnsureBeginDraw(); // refresh d2d selected target

		auto safetyZoneLayer = g_safetyZonePool.Acquire(d2dContext);
		const auto safetyZoneLayerScope = wil::scope_exit([&]
		{
			g_safetyZonePool.Release(d2dContext, std::move(safetyZoneLayer));
		});

		D2D1_RECT_F extendedPixelRectangle{};
		if (
			hr = safetyZoneLayer->Push(
				context,
				This->GetDeviceTransform()->GetD2DMatrix(),
				rectangle,
				expansion,
				extendedPixelRectangle
			);
			FAILED(hr)
		)
		{
			LOG_HR(hr);
			break;
		}

		hr = callback(extendedPixelRectangle);

		LOG_IF_FAILED(This->ApplyRenderStateInternal(false)); // apply clip and other states
		LOG_IF_FAILED(This->FlushD2D()); // flush previous draw calls

		safetyZoneLayer->Pop();

#ifdef _DEBUG
		if (GetAsyncKeyState(VK_SHIFT))
		{
			winrt::com_ptr<ID2D1SolidColorBrush> brush{};
			context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Green), brush.put());
			context->DrawRectangle(rectangle, brush.get());
		}
#endif

		return hr;
	}
	while (false);

	hr = callback(rectangle);

	return hr;
}


HRESULT GlassIntegrity::MyCDrawingContext_DrawVisualTree_Win10_1809(
	dwmcore::CDrawingContext* This,
	const dwmcore::CVisualTree* tree,
	const D2D1_RECT_F& rectangle,
	dwmcore::COverlayContext* overlayContext,
	int unknown1,
	bool unknown2,
	bool unknown3,
	bool useOcclusionContext,
	bool unknown4,
	bool unknown5,
	bool unknown6,
	bool unknown7
)
{
	return MyCDrawingContext_DrawVisualTree(
		This,
		rectangle,
		useOcclusionContext ? This->GetOcclusionContext() : nullptr,
		[=](const D2D1_RECT_F& replacedRectangle)
		{
			return g_CDrawingContext_DrawVisualTree_Win10_1809_Org(
				This,
				tree,
				replacedRectangle,
				overlayContext,
				unknown1,
				unknown2,
				unknown3,
				useOcclusionContext,
				unknown4,
				unknown5,
				unknown6,
				unknown7
			);
		}
	);
}
HRESULT GlassIntegrity::MyCDrawingContext_DrawVisualTree_Win10_1903(
	dwmcore::CDrawingContext* This,
	const dwmcore::CVisualTree* tree,
	const D2D1_RECT_F& rectangle,
	dwmcore::COverlayContext* overlayContext,
	int unknown1,
	bool unknown2,
	bool unknown3,
	bool useOcclusionContext,
	const D2D1_RECT_F* unknown4,
	bool unknown5,
	bool unknown6,
	bool unknown7
)
{
	return MyCDrawingContext_DrawVisualTree(
		This,
		rectangle,
		useOcclusionContext ? This->GetOcclusionContext() : nullptr,
		[=](const D2D1_RECT_F& replacedRectangle)
		{
			return g_CDrawingContext_DrawVisualTree_Win10_1903_Org(
				This,
				tree,
				replacedRectangle,
				overlayContext,
				unknown1,
				unknown2,
				unknown3,
				useOcclusionContext,
				unknown4,
				unknown5,
				unknown6,
				unknown7
			);
		}
	);
}
HRESULT GlassIntegrity::MyCDrawingContext_DrawVisualTree_Win10(
	dwmcore::CDrawingContext* This,
	const dwmcore::CVisualTree* tree,
	const D2D1_RECT_F& rectangle,
	const dwmcore::COcclusionContext* occlusionContext,
	int clearMode,
	bool useSuperSample
)
{
	return MyCDrawingContext_DrawVisualTree(
		This,
		rectangle,
		occlusionContext,
		[=](const D2D1_RECT_F& replacedRectangle)
		{
			return g_CDrawingContext_DrawVisualTree_Win10_Org(
				This,
				tree,
				replacedRectangle,
				occlusionContext,
				clearMode,
				useSuperSample
			);
		}
	);
}
HRESULT GlassIntegrity::MyCDrawingContext_DrawVisualTree_Win11(
	dwmcore::CDrawingContext* This,
	const dwmcore::CVisualTree* tree,
	const D2D1_RECT_F& rectangle,
	const dwmcore::COcclusionContext* occlusionContext,
	int clearMode,
	bool useSuperSample,
	dwmcore::CVisual* visualOverride
)
{
	return MyCDrawingContext_DrawVisualTree(
		This,
		rectangle,
		occlusionContext,
		[=](const D2D1_RECT_F& replacedRectangle)
		{
			return g_CDrawingContext_DrawVisualTree_Win11_Org(
				This,
				tree,
				replacedRectangle,
				occlusionContext,
				clearMode,
				useSuperSample,
				visualOverride
			);
		}
	);
}

void GlassIntegrity::DestroyDeviceResources(dwmcore::CD2DContext* d2dContext)
{
	g_safetyZonePool.Cleanup(std::chrono::seconds{ 0 }, d2dContext);
}

void GlassIntegrity::Update([[maybe_unused]] GlassEngine::UpdateType type)
{
	if (type & GlassEngine::UpdateType::Backdrop)
	{
		g_glassSafetyZoneMode = static_cast<GlassSafetyZoneMode>(std::clamp(GlassEngine::GetDwordFromRegistry(L"GlassSafetyZoneMode", 1), 0ul, 2ul));
	}
}

void GlassIntegrity::Startup()
{
	const auto build_before_w11_24h2 = dwmcore::g_versionInfo.build < os::build_w11_24h2;
	const auto build_before_server_2022 = dwmcore::g_versionInfo.build < os::build_server_2022;
	const auto build_before_w11_21h2 = dwmcore::g_versionInfo.build < os::build_w11_21h2;
	const auto build_before_w10_2004 = dwmcore::g_versionInfo.build < os::build_w10_2004;
	const auto build_before_w10_1903 = dwmcore::g_versionInfo.build < os::build_w10_1903;
	const auto hasArrayBasedCoverageSetIsCovered = HasCArrayBasedCoverageSetIsCovered();
	const auto hasCollectRectangleForOcclusion26100_Pre_7840 = static_cast<bool>(dwmcore::Symbol_COcclusionContext_CollectRectangleForOcclusion_26100_Pre_7840);
	const auto hasCollectRectangleForOcclusion26100_7840 = static_cast<bool>(dwmcore::Symbol_COcclusionContext_CollectRectangleForOcclusion_26100_7840);

	if (build_before_w10_2004 && !build_before_w10_1903)
	{
		const auto renderDirtyRegion = reinterpret_cast<const uint8_t*>(
			dwmcore::Symbol_CHwndRenderTarget_RenderDirtyRegion.get()
		);
		const std::span searchRange{ renderDirtyRegion + 1500, 3000 };
		const auto match = HookHelper::FindPattern(
			searchRange,
			g_COcclusionContext_IsDeviceTransformAssigned_Instructions
		);
		FAIL_FAST_IF_FAILED_MSG(match ? S_OK : E_NOINTERFACE, "The 18362 device-transform branch was not found");
		const auto matchOffset = static_cast<size_t>(match - searchRange.data());
		const auto remaining = searchRange.subspan(matchOffset + 1);
		FAIL_FAST_IF_FAILED_MSG(
			!HookHelper::FindPattern(remaining, g_COcclusionContext_IsDeviceTransformAssigned_Instructions) ? S_OK : E_UNEXPECTED,
			"The 18362 device-transform branch is ambiguous"
		);
		g_COcclusionContext_IsDeviceTransformAssigned_PatchLocation = const_cast<uint8_t*>(match + 8);
		g_COcclusionContext_IsDeviceTransformAssigned_Patch.Prepare(
			g_COcclusionContext_IsDeviceTransformAssigned_PatchLocation,
			std::array<uint8_t, 2>{ 0x74, 0x23 },
			std::array<uint8_t, 2>{ 0x90, 0x90 }
		);
		HookHelper::GetCurrentHookTransaction().Apply(g_COcclusionContext_IsDeviceTransformAssigned_Patch);
	}

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_COcclusionContext_CheckAndRecordOverlayCandidate_Pre_19041_Org, static_cast<dwmcore::COcclusionContext_CheckAndRecordOverlayCandidate_Pre_19041_t>(&MyCOcclusionContext_CheckAndRecordOverlayCandidate), build_before_w10_2004 },
			{ &g_COcclusionContext_CheckAndRecordOverlayCandidate_19041_Org, static_cast<dwmcore::COcclusionContext_CheckAndRecordOverlayCandidate_19041_t>(&MyCOcclusionContext_CheckAndRecordOverlayCandidate), !build_before_w10_2004 && build_before_w11_24h2 },
			{ &g_COcclusionContext_CheckAndRecordOverlayCandidate_26100_Org, static_cast<dwmcore::COcclusionContext_CheckAndRecordOverlayCandidate_26100_t>(&MyCOcclusionContext_CheckAndRecordOverlayCandidate), !build_before_w11_24h2 },
			{ &g_COcclusionContext_CollectRectangleForOcclusion_Pre_22000_Org, static_cast<dwmcore::COcclusionContext_CollectRectangleForOcclusion_Pre_22000_t>(&MyCOcclusionContext_CollectRectangleForOcclusion), build_before_w11_21h2 },
			{ &g_COcclusionContext_CollectRectangleForOcclusion_22000_Org, static_cast<dwmcore::COcclusionContext_CollectRectangleForOcclusion_22000_t>(&MyCOcclusionContext_CollectRectangleForOcclusion), !build_before_w11_21h2 && build_before_w11_24h2 },
			{ &g_COcclusionContext_CollectRectangleForOcclusion_26100_Pre_7840_Org, static_cast<dwmcore::COcclusionContext_CollectRectangleForOcclusion_26100_Pre_7840_t>(&MyCOcclusionContext_CollectRectangleForOcclusion), hasCollectRectangleForOcclusion26100_Pre_7840 },
			{ &g_COcclusionContext_CollectRectangleForOcclusion_26100_7840_Org, static_cast<dwmcore::COcclusionContext_CollectRectangleForOcclusion_26100_7840_t>(&MyCOcclusionContext_CollectRectangleForOcclusion), hasCollectRectangleForOcclusion26100_7840 },

			{ &g_COcclusionContext_Compute_Pre_W10_2004_Org, &MyCOcclusionContext_Compute_Pre_W10_2004, build_before_w10_2004 },
			{ &g_COcclusionContext_Compute_Org, &MyCOcclusionContext_Compute, !build_before_w10_2004 },
			{ &g_COcclusionContext_SetDeviceTransform_Org, &MyCOcclusionContext_SetDeviceTransform },
			{ &g_COcclusionContext_Destructor_Org, &MyCOcclusionContext_Destructor },

			{ &g_CHwndRenderTarget_RenderDirtyRegion_Org, &MyCHwndRenderTarget_RenderDirtyRegion, build_before_w10_2004 && !build_before_w10_1903 },
			{ &g_CDirtyRegion_GetUnOccludedDirtyRegion_Org, &MyCDirtyRegion_GetUnOccludedDirtyRegion, build_before_w10_2004 },
			{ &g_CDirtyRegion_GetUnOccludedDirtyRect_Org, &MyCDirtyRegion_GetUnOccludedDirtyRect, build_before_server_2022 && !build_before_w10_2004 },
			{ &g_CDirtyRegion_GetOptimizedRect_WS2022_Org, &MyCDirtyRegion_GetOptimizedRect_WS2022, build_before_w11_21h2 && !build_before_server_2022 },
			{ &g_CDirtyRegion_GetOptimizedRect_Org, &MyCDirtyRegion_GetOptimizedRect, build_before_w11_24h2 && !build_before_w11_21h2 },
			{ &g_CTreeDirty_GetOptimizedRect_Org, &MyCTreeDirty_GetOptimizedRect, !build_before_w11_24h2 },

			{ &g_COcclusionContext_IsOccluded_Pre_22000_Org, &MyCOcclusionContext_IsOccluded_Pre_22000, !hasArrayBasedCoverageSetIsCovered && build_before_server_2022 },
			{ &g_COcclusionContext_IsOccluded_Org, &MyCOcclusionContext_IsOccluded_20348, !hasArrayBasedCoverageSetIsCovered && !build_before_server_2022 },
			{ &g_CArrayBasedCoverageSet_IsCovered_Pre_26100_Org, &MyCArrayBasedCoverageSet_IsCovered_Pre_26100, hasArrayBasedCoverageSetIsCovered && build_before_w11_24h2 },
			{ &g_CArrayBasedCoverageSet_IsCovered_26100_Org, &MyCArrayBasedCoverageSet_IsCovered_26100, hasArrayBasedCoverageSetIsCovered && !build_before_w11_24h2 },
			{ &g_COcclusionContext_PageInPixelsRectToDeviceRect_Org, &MyCOcclusionContext_PageInPixelsRectToDeviceRect, hasArrayBasedCoverageSetIsCovered },

			{ &g_CDrawingContext_DrawVisualTree_Win10_1809_Org, &MyCDrawingContext_DrawVisualTree_Win10_1809, build_before_w10_1903 },
			{ &g_CDrawingContext_DrawVisualTree_Win10_1903_Org, &MyCDrawingContext_DrawVisualTree_Win10_1903, !build_before_w10_1903 && build_before_w10_2004 },
			{ &g_CDrawingContext_DrawVisualTree_Win10_Org, &MyCDrawingContext_DrawVisualTree_Win10, !build_before_w10_2004 && build_before_server_2022 },
			{ &g_CDrawingContext_DrawVisualTree_Win11_Org, &MyCDrawingContext_DrawVisualTree_Win11, !build_before_server_2022 }
		},
		true
	);
}

void GlassIntegrity::Shutdown()
{
	if (g_COcclusionContext_IsDeviceTransformAssigned_PatchLocation)
	{
		HookHelper::GetCurrentHookTransaction().Apply(g_COcclusionContext_IsDeviceTransformAssigned_Patch);
	}

	const auto build_before_w11_24h2 = dwmcore::g_versionInfo.build < os::build_w11_24h2;
	const auto build_before_server_2022 = dwmcore::g_versionInfo.build < os::build_server_2022;
	const auto build_before_w11_21h2 = dwmcore::g_versionInfo.build < os::build_w11_21h2;
	const auto build_before_w10_2004 = dwmcore::g_versionInfo.build < os::build_w10_2004;
	const auto build_before_w10_1903 = dwmcore::g_versionInfo.build < os::build_w10_1903;
	const auto hasArrayBasedCoverageSetIsCovered = HasCArrayBasedCoverageSetIsCovered();
	const auto hasCollectRectangleForOcclusion26100_Pre_7840 = static_cast<bool>(dwmcore::Symbol_COcclusionContext_CollectRectangleForOcclusion_26100_Pre_7840);
	const auto hasCollectRectangleForOcclusion26100_7840 = static_cast<bool>(dwmcore::Symbol_COcclusionContext_CollectRectangleForOcclusion_26100_7840);
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_COcclusionContext_CheckAndRecordOverlayCandidate_Pre_19041_Org, static_cast<dwmcore::COcclusionContext_CheckAndRecordOverlayCandidate_Pre_19041_t>(&MyCOcclusionContext_CheckAndRecordOverlayCandidate), build_before_w10_2004 },
			{ &g_COcclusionContext_CheckAndRecordOverlayCandidate_19041_Org, static_cast<dwmcore::COcclusionContext_CheckAndRecordOverlayCandidate_19041_t>(&MyCOcclusionContext_CheckAndRecordOverlayCandidate), !build_before_w10_2004 && build_before_w11_24h2 },
			{ &g_COcclusionContext_CheckAndRecordOverlayCandidate_26100_Org, static_cast<dwmcore::COcclusionContext_CheckAndRecordOverlayCandidate_26100_t>(&MyCOcclusionContext_CheckAndRecordOverlayCandidate), !build_before_w11_24h2 },
			{ &g_COcclusionContext_CollectRectangleForOcclusion_Pre_22000_Org, static_cast<dwmcore::COcclusionContext_CollectRectangleForOcclusion_Pre_22000_t>(&MyCOcclusionContext_CollectRectangleForOcclusion), build_before_w11_21h2 },
			{ &g_COcclusionContext_CollectRectangleForOcclusion_22000_Org, static_cast<dwmcore::COcclusionContext_CollectRectangleForOcclusion_22000_t>(&MyCOcclusionContext_CollectRectangleForOcclusion), !build_before_w11_21h2 && build_before_w11_24h2 },
			{ &g_COcclusionContext_CollectRectangleForOcclusion_26100_Pre_7840_Org, static_cast<dwmcore::COcclusionContext_CollectRectangleForOcclusion_26100_Pre_7840_t>(&MyCOcclusionContext_CollectRectangleForOcclusion), hasCollectRectangleForOcclusion26100_Pre_7840 },
			{ &g_COcclusionContext_CollectRectangleForOcclusion_26100_7840_Org, static_cast<dwmcore::COcclusionContext_CollectRectangleForOcclusion_26100_7840_t>(&MyCOcclusionContext_CollectRectangleForOcclusion), hasCollectRectangleForOcclusion26100_7840 },

			{ &g_COcclusionContext_Compute_Pre_W10_2004_Org, &MyCOcclusionContext_Compute_Pre_W10_2004, build_before_w10_2004 },
			{ &g_COcclusionContext_Compute_Org, &MyCOcclusionContext_Compute, !build_before_w10_2004 },
			{ &g_COcclusionContext_SetDeviceTransform_Org, &MyCOcclusionContext_SetDeviceTransform },
			{ &g_COcclusionContext_Destructor_Org, &MyCOcclusionContext_Destructor },

			{ &g_CHwndRenderTarget_RenderDirtyRegion_Org, &MyCHwndRenderTarget_RenderDirtyRegion, build_before_w10_2004 && !build_before_w10_1903 },
			{ &g_CDirtyRegion_GetUnOccludedDirtyRegion_Org, &MyCDirtyRegion_GetUnOccludedDirtyRegion, build_before_w10_2004 },
			{ &g_CDirtyRegion_GetUnOccludedDirtyRect_Org, &MyCDirtyRegion_GetUnOccludedDirtyRect, build_before_server_2022 && !build_before_w10_2004 },
			{ &g_CDirtyRegion_GetOptimizedRect_WS2022_Org, &MyCDirtyRegion_GetOptimizedRect_WS2022, build_before_w11_21h2 && !build_before_server_2022 },
			{ &g_CDirtyRegion_GetOptimizedRect_Org, &MyCDirtyRegion_GetOptimizedRect, build_before_w11_24h2 && !build_before_w11_21h2 },
			{ &g_CTreeDirty_GetOptimizedRect_Org, &MyCTreeDirty_GetOptimizedRect, !build_before_w11_24h2 },

			{ &g_COcclusionContext_IsOccluded_Pre_22000_Org, &MyCOcclusionContext_IsOccluded_Pre_22000, !hasArrayBasedCoverageSetIsCovered && build_before_server_2022 },
			{ &g_COcclusionContext_IsOccluded_Org, &MyCOcclusionContext_IsOccluded_20348, !hasArrayBasedCoverageSetIsCovered && !build_before_server_2022 },
			{ &g_CArrayBasedCoverageSet_IsCovered_Pre_26100_Org, &MyCArrayBasedCoverageSet_IsCovered_Pre_26100, hasArrayBasedCoverageSetIsCovered && build_before_w11_24h2 },
			{ &g_CArrayBasedCoverageSet_IsCovered_26100_Org, &MyCArrayBasedCoverageSet_IsCovered_26100, hasArrayBasedCoverageSetIsCovered && !build_before_w11_24h2 },
			{ &g_COcclusionContext_PageInPixelsRectToDeviceRect_Org, &MyCOcclusionContext_PageInPixelsRectToDeviceRect, hasArrayBasedCoverageSetIsCovered },

			{ &g_CDrawingContext_DrawVisualTree_Win10_1809_Org, &MyCDrawingContext_DrawVisualTree_Win10_1809, build_before_w10_1903 },
			{ &g_CDrawingContext_DrawVisualTree_Win10_1903_Org, &MyCDrawingContext_DrawVisualTree_Win10_1903, !build_before_w10_1903 && build_before_w10_2004 },
			{ &g_CDrawingContext_DrawVisualTree_Win10_Org, &MyCDrawingContext_DrawVisualTree_Win10, !build_before_w10_2004 && build_before_server_2022 },
			{ &g_CDrawingContext_DrawVisualTree_Win11_Org, &MyCDrawingContext_DrawVisualTree_Win11, !build_before_server_2022 }
		},
		false
	);

	if (g_COcclusionContext_DrawGeometry_Org)
	{
		HookHelper::GetCurrentHookTransaction().Apply(g_COcclusionContext_DrawGeometry_Hook);
	}

}

void GlassIntegrity::Cleanup()
{
	g_lastCollectedOcclusionRectangle = {};
	CArrayBasedGlassCoverageSet::RemoveAll();
	g_shrunkCoverageSetMap.clear();
	g_safetyZonePool.Cleanup(std::chrono::seconds{ 0 });
	g_coverageSetCheckpointMap.clear();
}
