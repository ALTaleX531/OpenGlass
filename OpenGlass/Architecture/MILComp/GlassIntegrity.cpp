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
		dwmcore::CCompositionSurfaceInfo* surface,
		const dwmcore::CMILMatrix* matrix,
		dwmcore::CShape* shape,
		int flags
	);

	HRESULT MyCCombinedGeometry_ProcessUpdate(
		dwmcore::CCombinedGeometry* This,
		dwmcore::CResourceTable* resourceTable,
		const DWM::MILCMD_COMBINEDGEOMETRY* command
	);
	void MyCVisual_SetClip(
		dwmcore::CVisual* This,
		dwmcore::CGeometry* geometry
	);
	void MyCSpriteVisual_Destructor(dwmcore::CSpriteVisual* This);
	bool MyCShape_AllowsOcclusion(dwmcore::CShape* This);
	HRESULT MyCGeometry_GetShapeData(
		dwmcore::CGeometry* This,
		const D2D1_SIZE_F* size,
		dwmcore::CShapePtr* shape
	);
	void MyCGeometry_Destructor(dwmcore::CGeometry* This);

	HRESULT MyCOcclusionContext_Compute(
		dwmcore::COcclusionContext* This,
		const dwmcore::CVisualTree* visualTree,
		const DWM::span<D2D1_RECT_F>& rectangles,
		float unknown,
		const DWM::span<dwmcore::COverlayContext*>& overlays
	);
	HRESULT MyCOcclusionContext_SetDeviceTransform(
		dwmcore::COcclusionContext* This,
		const dwmcore::CMILMatrix* matrix
	);
	HRESULT MyCVisual_CollectOcclusion(
		dwmcore::CVisual* This,
		dwmcore::COcclusionContext* occlusionContext,
		dwmcore::COcclusionInfo* occlusionInfo
	);
	HRESULT MyCColorBrush_AddOcclusionInformation(
		dwmcore::CColorBrush* This,
		dwmcore::COcclusionContext* occlusionContext,
		const D2D1_SIZE_F& worldSize
	);
	void MyCOcclusionContext_Destructor(dwmcore::COcclusionContext* This);

	bool MyCOcclusionContext_IsOccluded(
		dwmcore::COcclusionContext* This,
		const D2D1_RECT_F& coverage,
		int depth,
		bool ignoreDeviceTransform
	);
	D2D1_RECT_F* MyCTreeDirty_GetOptimizedRect(
		dwmcore::CTreeDirty* This,
		D2D1_RECT_F* dirtyRect,
		UINT i,
		const D2D1_RECT_F& bounds,
		const dwmcore::COcclusionContext* occlusionContext,
		const dwmcore::CRegion* region,
		const dwmcore::CMILMatrix* matrix,
		const DWM::span<dwmcore::CVisual>& visuals
	);
	HRESULT MyCDrawingContext_DrawVisualTree(
		dwmcore::CDrawingContext* This,
		dwmcore::CVisualTree* tree,
		const D2D1_RECT_F& rectangle,
		const dwmcore::COcclusionContext* occlusionContext,
		int clearMode,
		float padding,
		dwmcore::CVisual* visualOverride
	);

	Projection::Detour<dwmcore::Symbol_COcclusionContext_CheckAndRecordOverlayCandidate, decltype(&MyCOcclusionContext_CheckAndRecordOverlayCandidate)> g_COcclusionContext_CheckAndRecordOverlayCandidate_Org{};

	Projection::Detour<dwmcore::Symbol_CCombinedGeometry_ProcessUpdate, decltype(&MyCCombinedGeometry_ProcessUpdate)> g_CCombinedGeometry_ProcessUpdate_Org{};
	Projection::Detour<dwmcore::Symbol_CVisual_SetClip, decltype(&MyCVisual_SetClip)> g_CVisual_SetClip_Org{};
	Projection::Detour<dwmcore::Symbol_CSpriteVisual_CSpriteVisual, decltype(&MyCSpriteVisual_Destructor)> g_CSpriteVisual_Destructor_Org{};
	Projection::Detour<dwmcore::Symbol_CShape_AllowsOcclusion, decltype(&MyCShape_AllowsOcclusion)> g_CShape_AllowsOcclusion_Org{};
	Projection::Detour<dwmcore::Symbol_CGeometry_GetShapeData, decltype(&MyCGeometry_GetShapeData)> g_CGeometry_GetShapeData_Org{};
	Projection::Detour<dwmcore::Symbol_CGeometry_CGeometry, decltype(&MyCGeometry_Destructor)> g_CGeometry_Destructor_Org{};

	Projection::Detour<dwmcore::Symbol_COcclusionContext_Compute, decltype(&MyCOcclusionContext_Compute)> g_COcclusionContext_Compute_Org{};
	Projection::Detour<dwmcore::Symbol_COcclusionContext_SetDeviceTransform, decltype(&MyCOcclusionContext_SetDeviceTransform)> g_COcclusionContext_SetDeviceTransform_Org{};
	Projection::Detour<dwmcore::Symbol_CVisual_CollectOcclusion, decltype(&MyCVisual_CollectOcclusion)> g_CVisual_CollectOcclusion_Org{};
	decltype(&MyCColorBrush_AddOcclusionInformation) g_CColorBrush_AddOcclusionInformation_Org{ nullptr };
	decltype(&MyCColorBrush_AddOcclusionInformation)* g_CColorBrush_AddOcclusionInformation_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCColorBrush_AddOcclusionInformation> g_CColorBrush_AddOcclusionInformation_Hook;
	Projection::Detour<dwmcore::Symbol_COcclusionContext_COcclusionContext, decltype(&MyCOcclusionContext_Destructor)> g_COcclusionContext_Destructor_Org{};

	Projection::Detour<dwmcore::Symbol_COcclusionContext_IsOccluded, decltype(&MyCOcclusionContext_IsOccluded)> g_COcclusionContext_IsOccluded_Org{};
	Projection::Detour<dwmcore::Symbol_CTreeDirty_GetOptimizedRect, decltype(&MyCTreeDirty_GetOptimizedRect)> g_CTreeDirty_GetOptimizedRect_Org{};
	Projection::Detour<dwmcore::Symbol_CDrawingContext_DrawVisualTree, decltype(&MyCDrawingContext_DrawVisualTree)> g_CDrawingContext_DrawVisualTree_Org{};

	Util::ObjectPool<dwmcore::CD2DContext*, CGlassSafetyZoneLayer> g_safetyZonePool{};

	std::unordered_map<const dwmcore::COcclusionContext*, ULONGLONG> g_shrunkCoverageSetMap{};
	void ShrinkOccludersAboveGlass(const dwmcore::COcclusionContext* occlusionContext);

	enum class GlassSafetyZoneMode : UCHAR
	{
		Disabled,
		Visible,
		Always
	};
	GlassSafetyZoneMode g_glassSafetyZoneMode{ GlassSafetyZoneMode::Visible };

	struct UnoccludedDirtyRegionCalculationContext
	{
		const dwmcore::COcclusionContext* occlusionContext;

		void Enter(const dwmcore::COcclusionContext* context)
		{
			if (g_glassSafetyZoneMode != GlassSafetyZoneMode::Disabled && context)
			{
				occlusionContext = context;
			}
		}
		void Leave()
		{
			if (occlusionContext)
			{
				occlusionContext = nullptr;
			}
		}
		bool IsActive()
		{
			return occlusionContext != nullptr;
		}
	};
	using udrcc_leave_scope_exit = wil::unique_any<UnoccludedDirtyRegionCalculationContext*, decltype(&UnoccludedDirtyRegionCalculationContext::Leave), &UnoccludedDirtyRegionCalculationContext::Leave, wil::details::pointer_access_none>;
	[[nodiscard]] inline udrcc_leave_scope_exit EnterUnoccludedDirtyRegionCalculationContext(UnoccludedDirtyRegionCalculationContext* pudrcc, const dwmcore::COcclusionContext* context) noexcept
	{
		pudrcc->Enter(context);
		return udrcc_leave_scope_exit{ pudrcc };
	}
	UnoccludedDirtyRegionCalculationContext g_calculationContext{};

	dwmcore::CShape* g_shape{ nullptr };
	dwmcore::CGeometry* g_geometry{ nullptr };
	dwmcore::CVisual* g_glassVisualForCollectingOcclusion{ nullptr };
}

void GlassIntegrity::ShrinkOccludersAboveGlass(const dwmcore::COcclusionContext* occlusionContext)
{
	const auto frameId = occlusionContext->GetFrameId();
	const auto expansion = GlassKernel::GetBlurRadius();

	if (!expansion)
	{
		return;
	}
	const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(occlusionContext);
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
			!occlusionContext->IsOccluded(
				glassRegion.m_transformedRect,
				glassRegion.m_depth,
				true
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

		for (auto& [occluder, sides] : targetOccluderSet)
		{
			auto& originalRect = occluder->m_originalRect;

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

	collectAndTryShrinkOccluders(coverageSet->GetOccluderArray()->views());
}

HRESULT GlassIntegrity::MyCOcclusionContext_CheckAndRecordOverlayCandidate(
	dwmcore::COcclusionContext* This,
	dwmcore::CVisual* visual,
	dwmcore::CCompositionSurfaceInfo* surface,
	const dwmcore::CMILMatrix* matrix,
	dwmcore::CShape* shape,
	int flags
)
{
	/*if (shape)
	{
		D2D1_RECT_F bounds{};
		shape->GetTightBounds(&bounds, nullptr);
		OutputDebugStringW(
			wil::str_printf<std::wstring>(
				L"MyCOcclusionContext_CheckAndRecordOverlayCandidate called for shape with bounds: left=%f, top=%f, right=%f, bottom=%f\n",
				bounds.left,
				bounds.top,
				bounds.right,
				bounds.bottom
			).c_str()
		);
	}
	else
	{
		OutputDebugStringW(L"MyCOcclusionContext_CheckAndRecordOverlayCandidate called for shape: null\n");
	}*/
	return g_COcclusionContext_CheckAndRecordOverlayCandidate_Org(
		This,
		visual,
		surface,
		matrix,
		shape,
		flags
	);
}

HRESULT GlassIntegrity::MyCCombinedGeometry_ProcessUpdate(
	dwmcore::CCombinedGeometry* This,
	dwmcore::CResourceTable* resourceTable,
	const dwmcore::MILCMD_COMBINEDGEOMETRY* command
)
{
	g_glassStatusByGeometry[This] = command->GeometryCombineMode;

	// command is from kernel, we can't modify it directly,
	// so we create a copy of it on stack and modify the copy before passing to original function
	auto patchedCommand = *command;
	if (command->hGeometry1 == 0 || command->hGeometry2 == 0)
	{
		patchedCommand.GeometryCombineMode = D2D1_COMBINE_MODE_UNION;
	}
	else
	{
		patchedCommand.GeometryCombineMode = D2D1_COMBINE_MODE_INTERSECT;
	}
	return g_CCombinedGeometry_ProcessUpdate_Org(This, resourceTable, &patchedCommand);
}
void GlassIntegrity::MyCVisual_SetClip(
	dwmcore::CVisual* This,
	dwmcore::CGeometry* geometry
)
{
	if (HookHelper::get_vftable_from(This) == dwmcore::CSpriteVisual::vftable)
	{
		if (geometry && HookHelper::get_vftable_from(geometry) == dwmcore::CCombinedGeometry::vftable)
		{
			g_glassVisualSet.insert(This);
		}
		else
		{
			g_glassVisualSet.erase(This);
		}
	}

	return g_CVisual_SetClip_Org(This, geometry);
}

void GlassIntegrity::MyCSpriteVisual_Destructor(dwmcore::CSpriteVisual* This)
{
	g_glassVisualSet.erase(This);
	return g_CSpriteVisual_Destructor_Org(This);
}

bool GlassIntegrity::MyCShape_AllowsOcclusion(dwmcore::CShape* This)
{
	if (
		This == g_shape &&
		HookHelper::get_vftable_from(g_geometry) == dwmcore::CCombinedGeometry::vftable
	)
	{
		return true;
	}

	return g_CShape_AllowsOcclusion_Org(This);
}
HRESULT GlassIntegrity::MyCGeometry_GetShapeData(
	dwmcore::CGeometry* This,
	const D2D1_SIZE_F* size,
	dwmcore::CShapePtr* shape
)
{
	const auto hr = g_CGeometry_GetShapeData_Org(This, size, shape);
	g_geometry = This;
	g_shape = shape->get();

	return hr;
}

void GlassIntegrity::MyCGeometry_Destructor(dwmcore::CGeometry* This)
{
	g_glassStatusByGeometry.erase(This);
	return g_CGeometry_Destructor_Org(This);
}

HRESULT GlassIntegrity::MyCOcclusionContext_Compute(
	dwmcore::COcclusionContext* This,
	const dwmcore::CVisualTree* visualTree,
	const DWM::span<D2D1_RECT_F>& rectangles,
	float unknown,
	const DWM::span<dwmcore::COverlayContext*>& overlays
)
{
	HRESULT hr{ S_OK };
	if (const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(This); glassCoverageSet)
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
			DWM::span{ rectangles.length, extendedRectangles.get() },
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

HRESULT GlassIntegrity::MyCOcclusionContext_SetDeviceTransform(
	dwmcore::COcclusionContext* This,
	const dwmcore::CMILMatrix* matrix
)
{
	if (
		const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(This);
		glassCoverageSet
	)
	{
		glassCoverageSet->SetDeviceTransform(matrix);
	}
	return g_COcclusionContext_SetDeviceTransform_Org(This, matrix);
}

HRESULT GlassIntegrity::MyCVisual_CollectOcclusion(
	dwmcore::CVisual* This,
	dwmcore::COcclusionContext* occlusionContext,
	dwmcore::COcclusionInfo* occlusionInfo
)
{
	g_glassVisualForCollectingOcclusion = g_glassVisualSet.contains(This) ? This : nullptr;
	const auto collectOcclusionScope = wil::scope_exit([]
	{
		g_glassVisualForCollectingOcclusion = nullptr;
	});
	return g_CVisual_CollectOcclusion_Org(This, occlusionContext, occlusionInfo);
}

HRESULT GlassIntegrity::MyCColorBrush_AddOcclusionInformation(
	dwmcore::CColorBrush* This,
	dwmcore::COcclusionContext* occlusionContext,
	const D2D1_SIZE_F& worldSize
)
{
	if (g_glassVisualForCollectingOcclusion)
	{
		HRESULT hr{ S_OK };

		const auto geometry = g_glassVisualForCollectingOcclusion->GetClipNoRef();
		if (!geometry)
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

		auto color = Color::sRGBToscRGB(This->GetColor(), 1.f);
		const auto expansion = GlassKernel::GetBlurRadius();
		const auto active = GlassIntegrity::g_glassStatusByGeometry[geometry].test(0);
		const auto maximized = GlassIntegrity::g_glassStatusByGeometry[geometry].test(1);

		if (
			GlassKernel::CRealizedGlassColorizationParameters realizedGlassColorizationParameters;
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
				occlusionContext->CollectRectangleForOcclusion(rect, false);
			}
		}
		// here are the glass regions
		else if (expansion)
		{
			if (const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(occlusionContext, true); glassCoverageSet)
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

		return S_OK;
	}

	return g_CColorBrush_AddOcclusionInformation_Org(This, occlusionContext, worldSize);
}

void GlassIntegrity::MyCOcclusionContext_Destructor(dwmcore::COcclusionContext* This)
{
	CArrayBasedGlassCoverageSet::Remove(This);
	g_shrunkCoverageSetMap.erase(This);
	return g_COcclusionContext_Destructor_Org(This);
}

bool GlassIntegrity::MyCOcclusionContext_IsOccluded(
	dwmcore::COcclusionContext* This,
	const D2D1_RECT_F& coverage,
	int depth,
	bool ignoreDeviceTransform
)
{
	auto occluded = g_COcclusionContext_IsOccluded_Org(
		This,
		coverage,
		depth,
		ignoreDeviceTransform
	);

	const auto expansion = GlassKernel::GetBlurRadius();
	if (!expansion || !g_calculationContext.IsActive())
	{
		return occluded;
	}

	const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(This);

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

		occluded = g_COcclusionContext_IsOccluded_Org(
			This,
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
D2D1_RECT_F* GlassIntegrity::MyCTreeDirty_GetOptimizedRect(
	dwmcore::CTreeDirty* This,
	D2D1_RECT_F* dirtyRect,
	UINT i,
	const D2D1_RECT_F& bounds,
	const dwmcore::COcclusionContext* occlusionContext,
	const dwmcore::CRegion* region,
	const dwmcore::CMILMatrix* matrix,
	const DWM::span<dwmcore::CVisual>& visuals
)
{
	const auto calculationScope = EnterUnoccludedDirtyRegionCalculationContext(
		&g_calculationContext,
		occlusionContext->GetFrameId() == dwmcore::GetCurrentFrameId() ? occlusionContext : nullptr
	);
	return g_CTreeDirty_GetOptimizedRect_Org(
		This,
		dirtyRect,
		i,
		bounds,
		occlusionContext,
		region,
		matrix,
		visuals
	);
}
HRESULT GlassIntegrity::MyCDrawingContext_DrawVisualTree(
	dwmcore::CDrawingContext* This,
	dwmcore::CVisualTree* tree,
	const D2D1_RECT_F& rectangle,
	const dwmcore::COcclusionContext* occlusionContext,
	int clearMode,
	float padding,
	dwmcore::CVisual* visualOverride
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

		occlusionContext->SetDeviceTransform(This->GetDeviceTransform());
		const auto transformedRect = occlusionContext->PageInPixelsRectToDeviceRect(rectangle);
		const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(occlusionContext);

		if (
			g_glassSafetyZoneMode != GlassSafetyZoneMode::Always &&
			(
				!glassCoverageSet ||
				glassCoverageSet->IsEmpty() ||
				!glassCoverageSet->IsVisible(transformedRect, occlusionContext)
			)
		)
		{
			break;
		}

		ShrinkOccludersAboveGlass(occlusionContext);
		if (GlassKernel::IsCurrentCVIFullyTransparent())
		{
			break;
		}

		const auto d2dContext = This->GetD3DDevice()->GetD2DContext();
		const auto context = d2dContext->GetDeviceContext();

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

		hr = g_CDrawingContext_DrawVisualTree_Org(This, tree, extendedPixelRectangle, occlusionContext, clearMode, padding, visualOverride);

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

	hr = g_CDrawingContext_DrawVisualTree_Org(This, tree, rectangle, occlusionContext, clearMode, padding, visualOverride);

	return hr;
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

	if (!g_CColorBrush_AddOcclusionInformation_Org)
	{
		g_CColorBrush_AddOcclusionInformation_Org_Address =
			dwmcore::CColorBrush_AddOcclusionInformation_VtableSlot.address(dwmcore::CColorBrush::vftable);
		g_CColorBrush_AddOcclusionInformation_Hook.Prepare(
			g_CColorBrush_AddOcclusionInformation_Org_Address,
			&g_CColorBrush_AddOcclusionInformation_Org
		);
		HookHelper::GetCurrentHookTransaction().Apply(g_CColorBrush_AddOcclusionInformation_Hook);
	}

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_COcclusionContext_CheckAndRecordOverlayCandidate_Org, &MyCOcclusionContext_CheckAndRecordOverlayCandidate },

			{ &g_CCombinedGeometry_ProcessUpdate_Org, &MyCCombinedGeometry_ProcessUpdate },
			{ &g_CVisual_SetClip_Org, &MyCVisual_SetClip },
			{ &g_CSpriteVisual_Destructor_Org, &MyCSpriteVisual_Destructor },
			{ &g_CShape_AllowsOcclusion_Org, &MyCShape_AllowsOcclusion },
			{ &g_CGeometry_GetShapeData_Org, &MyCGeometry_GetShapeData },
			{ &g_CGeometry_Destructor_Org, &MyCGeometry_Destructor },

			{ &g_COcclusionContext_Compute_Org, &MyCOcclusionContext_Compute },
			{ &g_COcclusionContext_SetDeviceTransform_Org, &MyCOcclusionContext_SetDeviceTransform },
			{ &g_CVisual_CollectOcclusion_Org, &MyCVisual_CollectOcclusion },
			{ &g_COcclusionContext_Destructor_Org, &MyCOcclusionContext_Destructor },

			{ &g_COcclusionContext_IsOccluded_Org, &MyCOcclusionContext_IsOccluded },
			{ &g_CTreeDirty_GetOptimizedRect_Org, &MyCTreeDirty_GetOptimizedRect },
			{ &g_CDrawingContext_DrawVisualTree_Org, &MyCDrawingContext_DrawVisualTree },
		},
		true
	);
}

void GlassIntegrity::Shutdown()
{
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_COcclusionContext_CheckAndRecordOverlayCandidate_Org, &MyCOcclusionContext_CheckAndRecordOverlayCandidate },

			{ &g_CCombinedGeometry_ProcessUpdate_Org, &MyCCombinedGeometry_ProcessUpdate },
			{ &g_CVisual_SetClip_Org, &MyCVisual_SetClip },
			{ &g_CSpriteVisual_Destructor_Org, &MyCSpriteVisual_Destructor },
			{ &g_CShape_AllowsOcclusion_Org, &MyCShape_AllowsOcclusion },
			{ &g_CGeometry_GetShapeData_Org, &MyCGeometry_GetShapeData },
			{ &g_CGeometry_Destructor_Org, &MyCGeometry_Destructor },

			{ &g_COcclusionContext_Compute_Org, &MyCOcclusionContext_Compute },
			{ &g_COcclusionContext_SetDeviceTransform_Org, &MyCOcclusionContext_SetDeviceTransform },
			{ &g_CVisual_CollectOcclusion_Org, &MyCVisual_CollectOcclusion },
			{ &g_COcclusionContext_Destructor_Org, &MyCOcclusionContext_Destructor },

			{ &g_COcclusionContext_IsOccluded_Org, &MyCOcclusionContext_IsOccluded },
			{ &g_CTreeDirty_GetOptimizedRect_Org, &MyCTreeDirty_GetOptimizedRect },
			{ &g_CDrawingContext_DrawVisualTree_Org, &MyCDrawingContext_DrawVisualTree },
		},
		false
	);

	if (g_CColorBrush_AddOcclusionInformation_Org)
	{
		HookHelper::GetCurrentHookTransaction().Apply(g_CColorBrush_AddOcclusionInformation_Hook);
	}

}

void GlassIntegrity::Cleanup()
{
	CArrayBasedGlassCoverageSet::RemoveAll();
	g_glassVisualSet.clear();
	g_glassStatusByGeometry.clear();
	g_shrunkCoverageSetMap.clear();
	g_safetyZonePool.Cleanup(std::chrono::seconds{ 0 });
}
