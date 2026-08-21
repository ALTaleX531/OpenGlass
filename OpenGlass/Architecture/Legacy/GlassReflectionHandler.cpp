#include "pch.h"
#include "GlassReflectionHandler.hpp"
#include "uDWMProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "Shared.hpp"
#include "GlassReflectionBrush.hpp"
#include "GlassKernel.hpp"
#include "DetourChains.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassReflectionHandler
{
	template <typename T>
	HRESULT MyCRenderData_TryDrawCommandAsDrawList(
		dwmcore::CRenderData* This,
		UINT commandType,
		DWM::span<const BYTE>* resources,
		T&& callback
	);
	HRESULT MyCRenderData_DrawImageResource_FillMode_Win10(
		dwmcore::CRenderData* This,
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
		bool unknown,
		dwmcore::CImageSource* imageSource,
		const D2D1_RECT_F* srcRect,
		const D2D1_RECT_F* dstRect,
		float opacity
	);
	HRESULT MyCRenderData_DrawImageResource_FillMode_Pre_W10_2004(
		dwmcore::CRenderData* This,
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
		bool unknown,
		dwmcore::CImageSource* imageSource,
		const D2D1_RECT_F* srcRect,
		const D2D1_RECT_F* dstRect,
		float opacity,
		const D2D1_RECT_F* unknownRect
	);
	HRESULT MyCRenderData_DrawImageResource_FillMode_Win11_Pre_24H2(
		dwmcore::CRenderData* This,
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
		dwmcore::CImageSource* imageSource,
		const D2D1_RECT_F* srcRect,
		const D2D1_RECT_F* dstRect,
		float opacity
	);
	HRESULT MyCRenderData_DrawImageResource_FillMode_Win11_24H2(
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
		dwmcore::CImageSource* imageSource,
		const D2D1_RECT_F* srcRect,
		const D2D1_RECT_F* dstRect,
		float opacity
	);
	const D2D1_RECT_F* AdjustLivePreviewSourceRect(const D2D1_RECT_F* srcRect) noexcept;

	void MyCAnimatedGlassSheet_OnRectUpdated(uDWM::CAnimatedGlassSheet* This, LPCRECT lprc);
	void MyCAnimatedGlassSheet_Destructor(uDWM::CAnimatedGlassSheet* This);

	HRESULT MyCLivePreview__FadeOutToGlass(uDWM::CLivePreview* This);
	HRESULT MyCLivePreview__UpdateInstructions(uDWM::CLivePreview* This);
	HRESULT MyCLivePreview__UpdateResourcesForMonitorHelper(
		uDWM::CLivePreview* This,
		const uDWM::CTopLevelWindow* window,
		uDWM::LivePreviewResource* resource
	);
	
	DetourChains::RenderDataTryDrawCommandAsDrawListWin10ReflectionNode g_CRenderData_TryDrawCommandAsDrawList_Win10_Org{};
	DetourChains::RenderDataTryDrawCommandAsDrawListWin11ReflectionNode g_CRenderData_TryDrawCommandAsDrawList_Win11_Org{};
	Projection::Detour<dwmcore::Symbol_CRenderData_DrawImageResource_FillMode_Pre_19041, &MyCRenderData_DrawImageResource_FillMode_Pre_W10_2004> g_CRenderData_DrawImageResource_FillMode_Pre_W10_2004_Org{};
	Projection::Detour<dwmcore::Symbol_CRenderData_DrawImageResource_FillMode_19041, &MyCRenderData_DrawImageResource_FillMode_Win10> g_CRenderData_DrawImageResource_FillMode_Win10_Org{};
	Projection::Detour<dwmcore::Symbol_CRenderData_DrawImageResource_FillMode_22000, &MyCRenderData_DrawImageResource_FillMode_Win11_Pre_24H2> g_CRenderData_DrawImageResource_FillMode_Win11_Pre_24H2_Org{};
	Projection::Detour<dwmcore::Symbol_CRenderData_DrawImageResource_FillMode_26100_2454, &MyCRenderData_DrawImageResource_FillMode_Win11_24H2> g_CRenderData_DrawImageResource_FillMode_Win11_24H2_Org{};

	Projection::Detour<uDWM::Symbol_CAnimatedGlassSheet_OnRectUpdated, &MyCAnimatedGlassSheet_OnRectUpdated> g_CAnimatedGlassSheet_OnRectUpdated_Org{};
	Projection::Detour<uDWM::Symbol_CAnimatedGlassSheet__CAnimatedGlassSheet, &MyCAnimatedGlassSheet_Destructor> g_CAnimatedGlassSheet_Destructor_Org{};

	Projection::Detour<uDWM::Symbol_CLivePreview__FadeOutToGlass, &MyCLivePreview__FadeOutToGlass> g_CLivePreview__FadeOutToGlass_Org{};
	Projection::Detour<uDWM::Symbol_CLivePreview__UpdateInstructions, &MyCLivePreview__UpdateInstructions> g_CLivePreview__UpdateInstructions_Org{};
	Projection::Detour<uDWM::Symbol_CLivePreview__UpdateResourcesForMonitorHelper, &MyCLivePreview__UpdateResourcesForMonitorHelper> g_CLivePreview__UpdateResourcesForMonitorHelper_Org{};

	bool g_fixLivePreviewRendering{ false };
	
	class CAnimatedReflectionSheet : public winrt::implements<CAnimatedReflectionSheet, IUnknown, winrt::non_agile, winrt::no_weak_ref>
	{
		uDWM::CAnimatedGlassSheet* m_sheet{ nullptr };
		winrt::com_ptr<uDWM::CRenderDataVisual> m_visual{ nullptr };
		winrt::com_ptr<uDWM::CRgnGeometryProxy> m_geometry{ nullptr };
	public:
		CAnimatedReflectionSheet(uDWM::CAnimatedGlassSheet* sheet) : m_sheet{ sheet } {};
		virtual ~CAnimatedReflectionSheet()
		{
			if (m_visual)
			{
				m_sheet->GetVisualCollection()->Remove(m_visual.get());
				GlassReflectionBrush::Remove(m_visual.get());
			}
		}
		static HRESULT Create(uDWM::CAnimatedGlassSheet* glassSheet, CAnimatedReflectionSheet** outputSheet)
		{
			auto reflectionSheet = winrt::make_self<CAnimatedReflectionSheet>(glassSheet);
			RETURN_IF_FAILED(reflectionSheet->Initialize());
			*outputSheet = reflectionSheet.detach();
			return S_OK;
		}

		HRESULT Initialize()
		{
			RETURN_IF_FAILED(
				uDWM::CRenderDataVisual::Create(
					m_visual.put()
				)
			);
			m_visual->SetInsetFromParent({});
			const auto brush = GlassReflectionBrush::GetOrCreate(m_visual.get(), 0, true);
			if (!brush)
			{
				return E_FAIL;
			}
			
			wil::unique_hrgn emptyRegion{ CreateRectRgn(0, 0, 0, 0) };
			RETURN_LAST_ERROR_IF_NULL(emptyRegion);

			RETURN_IF_FAILED(
				uDWM::ResourceHelper::CreateGeometryFromHRGN(
					emptyRegion.get(),
					m_geometry.put()
				)
			);
			winrt::com_ptr<uDWM::CDrawGeometryInstruction> instruction{ nullptr };
			RETURN_IF_FAILED(
				uDWM::CDrawGeometryInstruction::Create(
					brush.get(),
					m_geometry.get(),
					instruction.put()
				)
			);
			RETURN_IF_FAILED(m_visual->AddInstruction(instruction.get()));
			RETURN_IF_FAILED(
				m_sheet->GetVisualCollection()->InsertRelative(
					m_visual.get(),
					nullptr,
					false,
					true
				)
			);

			return S_OK;
		}
		HRESULT OnRectUpdated(LPCRECT lprc)
		{
			const auto brush = GlassReflectionBrush::GetOrCreate(m_visual.get(), 0);
			RETURN_IF_FAILED(
				brush->Update(
					(Shared::g_reflectionPolicy & Shared::ReflectionPolicy::AnimatedGlassSheet) ? 
					1.f : 
					0.f,
					GlassReflectionBrush::CalculateTargetViewport(
						{ lprc->left, lprc->top }
					),
					D2D1::RectF(),
					nullptr,
					DWM::MilBrushMappingMode::Absolute,
					DWM::MilBrushMappingMode::Absolute,
					nullptr,
					nullptr,
					DWM::MilStretch::None,
					DWM::MilTileMode::Extend,
					DWM::MilHorizontalAlignment::Left,
					DWM::MilVerticalAlignment::Top,
					nullptr
				)
			);
			m_visual->_ValidateVisual();

			RETURN_IF_FAILED(
				uDWM::ResourceHelper::CreateGeometryFromHRGN(
					wil::unique_hrgn
					{
						CreateRoundRectRgn(
							0 - m_sheet->GetAtlasPaddingLeft(),
							0 - m_sheet->GetAtlasPaddingTop(),
							wil::rect_width(*lprc) - m_sheet->GetAtlasPaddingRight(),
							wil::rect_height(*lprc) - m_sheet->GetAtlasPaddingBottom(),
							Shared::g_roundRectRadius,
							Shared::g_roundRectRadius
						)
					}.get(),
					reinterpret_cast<uDWM::CRgnGeometryProxy**>(&m_geometry)
				)
			);
			return S_OK;
		}
	};
	std::unordered_map<uDWM::CAnimatedGlassSheet*, winrt::com_ptr<CAnimatedReflectionSheet>> g_sheetMap{};
}

template <typename T>
HRESULT GlassReflectionHandler::MyCRenderData_TryDrawCommandAsDrawList(
	dwmcore::CRenderData* This,
	UINT commandType,
	DWM::span<const BYTE>* resources,
	T&& callback
)
{
	const auto isDrawGeometryCommand = GlassKernel::IsDrawGeometryCommand(commandType, resources);
	const auto command = reinterpret_cast<const dwmcore::CDrawGeometryCommand*>(resources->data);
	if (
		isDrawGeometryCommand &&
		HookHelper::get_vftable_from(This->GetResources()->data[command->brushIndex]) == dwmcore::CImageLegacyMilBrush::vftable &&
		static_cast<dwmcore::CImageLegacyMilBrush*>(This->GetResources()->data[command->brushIndex])->GetImageSource() &&
		HookHelper::get_vftable_from(This->GetResources()->data[command->geometryIndex]) == dwmcore::CRectangleGeometry::vftable &&
		static_cast<dwmcore::CImageLegacyMilBrush*>(This->GetResources()->data[command->brushIndex])->GetFloatResource()
	)
	{
		g_fixLivePreviewRendering = true;
	}
	const auto cleanup = wil::scope_exit([]
	{
		g_fixLivePreviewRendering = false;
	});

	return callback();
}

HRESULT GlassReflectionHandler::MyCRenderData_TryDrawCommandAsDrawList_Win10(
	dwmcore::CRenderData* This,
	dwmcore::CDrawingContext* drawingContext,
	dwmcore::CDrawListCache* drawListCache,
	dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
	bool unknown,
	UINT commandType,
	DWM::span<const BYTE>* resources,
	bool* succeeded
)
{
	return MyCRenderData_TryDrawCommandAsDrawList(
		This,
		commandType,
		resources,
		[=]()
		{
			return g_CRenderData_TryDrawCommandAsDrawList_Win10_Org(
				This,
				drawingContext,
				drawListCache,
				drawListEntryBuilder,
				unknown,
				commandType,
				resources,
				succeeded
			);
		}
	);
}

HRESULT GlassReflectionHandler::MyCRenderData_TryDrawCommandAsDrawList_Win11(
	dwmcore::CRenderData* This,
	dwmcore::CDrawingContext* drawingContext,
	dwmcore::CDrawListCache* drawListCache,
	dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
	UINT commandType,
	DWM::span<const BYTE>* resources,
	bool* succeeded
)
{
	return MyCRenderData_TryDrawCommandAsDrawList(
		This,
		commandType,
		resources,
		[=]()
		{
			return g_CRenderData_TryDrawCommandAsDrawList_Win11_Org(
				This,
				drawingContext,
				drawListCache,
				drawListEntryBuilder,
				commandType,
				resources,
				succeeded
			);
		}
	);
}

const D2D1_RECT_F* GlassReflectionHandler::AdjustLivePreviewSourceRect(const D2D1_RECT_F* srcRect) noexcept
{
	// The DWM team changed the implementation of dwmcore!CRenderData::TryDrawCommandAsDrawList,
	// which is why Aero Peek is glitching since Windows 10 1803.
	//
	// https://github.com/microsoft/Windows-Dev-Performance/issues/12
	//
	// I still can't really figure out
	// why they're passing the geometry's bounding rectangle to srcRect parameter.
	//
	// But since Windows 11 24H2,
	// they stop this buggy behavior when MilStretch is set to Uniform or UniformToFill.
	if (g_fixLivePreviewRendering)
	{
		return nullptr;
	}

	return srcRect;
}

HRESULT GlassReflectionHandler::MyCRenderData_DrawImageResource_FillMode_Win10(
	dwmcore::CRenderData* This,
	dwmcore::CDrawingContext* drawingContext,
	dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
	bool unknown,
	dwmcore::CImageSource* imageSource,
	const D2D1_RECT_F* srcRect,
	const D2D1_RECT_F* dstRect,
	float opacity
)
{
	return g_CRenderData_DrawImageResource_FillMode_Win10_Org(
		This,
		drawingContext,
		drawListEntryBuilder,
		unknown,
		imageSource,
		AdjustLivePreviewSourceRect(srcRect),
		dstRect,
		opacity
	);
}

HRESULT GlassReflectionHandler::MyCRenderData_DrawImageResource_FillMode_Pre_W10_2004(
	dwmcore::CRenderData* This,
	dwmcore::CDrawingContext* drawingContext,
	dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
	bool unknown,
	dwmcore::CImageSource* imageSource,
	const D2D1_RECT_F* srcRect,
	const D2D1_RECT_F* dstRect,
	float opacity,
	const D2D1_RECT_F* unknownRect
)
{
	return g_CRenderData_DrawImageResource_FillMode_Pre_W10_2004_Org(
		This,
		drawingContext,
		drawListEntryBuilder,
		unknown,
		imageSource,
		AdjustLivePreviewSourceRect(srcRect),
		dstRect,
		opacity,
		unknownRect
	);
}

HRESULT GlassReflectionHandler::MyCRenderData_DrawImageResource_FillMode_Win11_Pre_24H2(
	dwmcore::CRenderData* This,
	dwmcore::CDrawingContext* drawingContext,
	dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
	dwmcore::CImageSource* imageSource,
	const D2D1_RECT_F* srcRect,
	const D2D1_RECT_F* dstRect,
	float opacity
)
{
	return g_CRenderData_DrawImageResource_FillMode_Win11_Pre_24H2_Org(
		This,
		drawingContext,
		drawListEntryBuilder,
		imageSource,
		AdjustLivePreviewSourceRect(srcRect),
		dstRect,
		opacity
	);
}

HRESULT GlassReflectionHandler::MyCRenderData_DrawImageResource_FillMode_Win11_24H2(
	dwmcore::CDrawingContext* drawingContext,
	dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
	dwmcore::CImageSource* imageSource,
	const D2D1_RECT_F* srcRect,
	const D2D1_RECT_F* dstRect,
	float opacity
)
{
	return g_CRenderData_DrawImageResource_FillMode_Win11_24H2_Org(
		drawingContext,
		drawListEntryBuilder,
		imageSource,
		AdjustLivePreviewSourceRect(srcRect),
		dstRect,
		opacity
	);
}

void GlassReflectionHandler::MyCAnimatedGlassSheet_OnRectUpdated(uDWM::CAnimatedGlassSheet* This, LPCRECT lprc)
{
	winrt::com_ptr<CAnimatedReflectionSheet> reflectionSheet{};
	if (const auto it = g_sheetMap.find(This); it != g_sheetMap.end())
	{
		reflectionSheet = it->second;
	}
	else
	{
		if (SUCCEEDED(CAnimatedReflectionSheet::Create(This, reflectionSheet.put())))
		{
			g_sheetMap.emplace(This, reflectionSheet);
		}
	}

	if (reflectionSheet)
	{
		reflectionSheet->OnRectUpdated(lprc);
	}

	return g_CAnimatedGlassSheet_OnRectUpdated_Org(This, lprc);
}
void GlassReflectionHandler::MyCAnimatedGlassSheet_Destructor(uDWM::CAnimatedGlassSheet* This)
{
	g_sheetMap.erase(This);
	return g_CAnimatedGlassSheet_Destructor_Org(This);
}

// here restores
// CLivePreview::_UpdateGlassVisual
// CLivePreview::_UpdateInstructions
HRESULT GlassReflectionHandler::MyCLivePreview__FadeOutToGlass(uDWM::CLivePreview* This)
{
	RETURN_IF_FAILED(This->_UpdateResources());

	for (auto& visual : This->GetLivePreviewVisualArray()->views())
	{
		auto& windowFrames = visual.windowFrames;
		if (windowFrames)
		{
			windowFrames->GetTransformParent()->GetVisualCollection()->Remove(windowFrames);
			windowFrames->Release();
			windowFrames = nullptr;
		}
		if (!windowFrames)
		{
			visual.data->GetWindow()->CloneVisualTreeForLivePreview(true, &windowFrames);
			This->GetGlassVisual()->GetVisualCollection()->InsertRelative(
				windowFrames,
				nullptr,
				false,
				true
			);
		}
	}
	RETURN_IF_FAILED(This->ClearInstructions());
	RETURN_IF_FAILED(This->GetGlassVisual()->ClearInstructions());
	for (const auto& resource : This->GetLivePreviewResourceArray()->views())
	{
		//if (resource.IsWindowBoundingRectNotEmpty())
		if (
			!IsRectEmpty(resource.GetWindowBoundingRect())
		)
		{
			winrt::com_ptr<uDWM::CDrawGeometryInstruction> instruction{ nullptr };
			if (
				resource.GetWindowVisualBrush() &&
				resource.GetWindowBoundingGeometry()
			)
			{
				RETURN_IF_FAILED(
					uDWM::CDrawGeometryInstruction::Create(
						resource.GetWindowVisualBrush(),
						resource.GetWindowBoundingGeometry(),
						instruction.put()
					)
				);
				RETURN_IF_FAILED(This->AddInstruction(instruction.get()));
			}

			if (
				resource.GetGlassVisualBrush() &&
				resource.GetGlassBoundingGeometry()
			)
			{
				RETURN_IF_FAILED(
					uDWM::CDrawGeometryInstruction::Create(
						resource.GetGlassVisualBrush(),
						resource.GetGlassBoundingGeometry(),
						instruction.put()
					)
				);
				RETURN_IF_FAILED(This->AddInstruction(instruction.get()));
			}
		}
		//if (resource.IsGlassBoundingRectNotEmpty())
		if (
			!IsRectEmpty(resource.GetGlassBoundingRect()) && 
			resource.GetReflectionGeometry()
		)
		{
			if (
				const auto brush = GlassReflectionBrush::GetOrCreate(
					This,
					0,
					true
				);
				brush
			)
			{
				RETURN_IF_FAILED(
					brush->Update(
						(Shared::g_reflectionPolicy & Shared::ReflectionPolicy::LivePreview) ?
						1.f :
						0.f,
						GlassReflectionBrush::CalculateTargetViewport(
							This->GetGlassVisual()->GetLocalToParentVisualOffset(This->GetTransformParent())
						),
						D2D1::RectF(),
						nullptr,
						DWM::MilBrushMappingMode::Absolute,
						DWM::MilBrushMappingMode::Absolute,
						nullptr,
						nullptr,
						DWM::MilStretch::None,
						DWM::MilTileMode::Extend,
						DWM::MilHorizontalAlignment::Left,
						DWM::MilVerticalAlignment::Top,
						nullptr
					)
				);

				winrt::com_ptr<uDWM::CDrawGeometryInstruction> instruction{ nullptr };
				RETURN_IF_FAILED(
					uDWM::CDrawGeometryInstruction::Create(
						brush.get(),
						resource.GetReflectionGeometry(),
						instruction.put()
					)
				);
				RETURN_IF_FAILED(This->GetGlassVisual()->AddInstruction(instruction.get()));
			}
		}
	}

	return g_CLivePreview__FadeOutToGlass_Org(This);
}
HRESULT GlassReflectionHandler::MyCLivePreview__UpdateInstructions(uDWM::CLivePreview* This)
{
	const auto hr = g_CLivePreview__UpdateInstructions_Org(This);

	for (const auto& resource : This->GetLivePreviewResourceArray()->views())
	{
		//if (resource.IsGlassBoundingRectNotEmpty())
		if (
			!IsRectEmpty(resource.GetGlassBoundingRect()) && 
			resource.GetReflectionGeometry()
		)
		{
			if (
				const auto brush = GlassReflectionBrush::GetOrCreate(
					This,
					0,
					true
				);
				brush
			)
			{
				RETURN_IF_FAILED(
					brush->Update(
						(Shared::g_reflectionPolicy & Shared::ReflectionPolicy::LivePreview) ?
						1.f :
						0.f,
						GlassReflectionBrush::CalculateTargetViewport(
							This->GetGlassVisual()->GetLocalToParentVisualOffset(This->GetTransformParent())
						),
						D2D1::RectF(),
						nullptr,
						DWM::MilBrushMappingMode::Absolute,
						DWM::MilBrushMappingMode::Absolute,
						nullptr,
						nullptr,
						DWM::MilStretch::None,
						DWM::MilTileMode::Extend,
						DWM::MilHorizontalAlignment::Left,
						DWM::MilVerticalAlignment::Top,
						nullptr
					)
				);

				winrt::com_ptr<uDWM::CDrawGeometryInstruction> instruction{ nullptr };
				RETURN_IF_FAILED(
					uDWM::CDrawGeometryInstruction::Create(
						brush.get(),
						resource.GetReflectionGeometry(),
						instruction.put()
					)
				);
				RETURN_IF_FAILED(This->GetGlassVisual()->AddInstruction(instruction.get()));
			}
		}
	}

	return hr;
}

HRESULT GlassReflectionHandler::MyCLivePreview__UpdateResourcesForMonitorHelper(
	uDWM::CLivePreview* This,
	const uDWM::CTopLevelWindow* window,
	uDWM::LivePreviewResource* resource
)
{
	const auto build_before_w11_24h2 = uDWM::g_versionInfo.build < os::build_w11_24h2;

	if (!build_before_w11_24h2)
	{
		GlassKernel::g_redirectFirstCreateRectRgnCall = true;
		GlassKernel::g_window = window;
	}
	const auto result = g_CLivePreview__UpdateResourcesForMonitorHelper_Org(This, window, resource);
	if (!build_before_w11_24h2)
	{
		GlassKernel::g_window = nullptr;
		GlassKernel::g_redirectFirstCreateRectRgnCall = std::nullopt;
	}

	return result;
}

void GlassReflectionHandler::Update([[maybe_unused]] GlassEngine::UpdateType type)
{
}

void GlassReflectionHandler::Startup()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_GlassReflectionHandler))
	{
		return;
	}

	const auto build_before_w11_24h2 = Util::VersionBefore<os::build_w11_24h2, os::revision_24h2_rtm_1>(dwmcore::g_versionInfo.build, dwmcore::g_versionInfo.revision);
	const auto dwmcore_build_before_w11_21h2 = dwmcore::g_versionInfo.build < os::build_w11_21h2;
	const auto udwm_build_before_w11_21h2 = uDWM::g_versionInfo.build < os::build_w11_21h2;
	const auto build_before_w10_2004 = dwmcore::g_versionInfo.build < os::build_w10_2004;
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CRenderData_TryDrawCommandAsDrawList_Win10_Org, dwmcore_build_before_w11_21h2 },
			{ &g_CRenderData_TryDrawCommandAsDrawList_Win11_Org, !dwmcore_build_before_w11_21h2 },
			{ &g_CRenderData_DrawImageResource_FillMode_Pre_W10_2004_Org, build_before_w10_2004 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win10_Org, !build_before_w10_2004 && dwmcore_build_before_w11_21h2 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win11_Pre_24H2_Org, !dwmcore_build_before_w11_21h2 && build_before_w11_24h2 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win11_24H2_Org, !build_before_w11_24h2 },

			{ &g_CAnimatedGlassSheet_OnRectUpdated_Org, udwm_build_before_w11_21h2 },
			{ &g_CAnimatedGlassSheet_Destructor_Org, udwm_build_before_w11_21h2 },
			{ &g_CLivePreview__UpdateInstructions_Org, udwm_build_before_w11_21h2 },
			{ &g_CLivePreview__UpdateResourcesForMonitorHelper_Org, uDWM::g_versionInfo.build >= os::build_w11_24h2 },

			{ &g_CLivePreview__FadeOutToGlass_Org, !udwm_build_before_w11_21h2 }
		},
		true
	);
}

void GlassReflectionHandler::Shutdown()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_GlassReflectionHandler))
	{
		return;
	}

	const auto build_before_w11_24h2 = Util::VersionBefore<os::build_w11_24h2, os::revision_24h2_rtm_1>(dwmcore::g_versionInfo.build, dwmcore::g_versionInfo.revision);
	const auto dwmcore_build_before_w11_21h2 = dwmcore::g_versionInfo.build < os::build_w11_21h2;
	const auto udwm_build_before_w11_21h2 = uDWM::g_versionInfo.build < os::build_w11_21h2;
	const auto build_before_w10_2004 = dwmcore::g_versionInfo.build < os::build_w10_2004;
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CRenderData_TryDrawCommandAsDrawList_Win10_Org, dwmcore_build_before_w11_21h2 },
			{ &g_CRenderData_TryDrawCommandAsDrawList_Win11_Org, !dwmcore_build_before_w11_21h2 },
			{ &g_CRenderData_DrawImageResource_FillMode_Pre_W10_2004_Org, build_before_w10_2004 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win10_Org, !build_before_w10_2004 && dwmcore_build_before_w11_21h2 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win11_Pre_24H2_Org, !dwmcore_build_before_w11_21h2 && build_before_w11_24h2 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win11_24H2_Org, !build_before_w11_24h2 },

			{ &g_CAnimatedGlassSheet_OnRectUpdated_Org, udwm_build_before_w11_21h2 },
			{ &g_CAnimatedGlassSheet_Destructor_Org, udwm_build_before_w11_21h2 },
			{ &g_CLivePreview__UpdateInstructions_Org, udwm_build_before_w11_21h2 },
			{ &g_CLivePreview__UpdateResourcesForMonitorHelper_Org, uDWM::g_versionInfo.build >= os::build_w11_24h2 },

			{ &g_CLivePreview__FadeOutToGlass_Org, !udwm_build_before_w11_21h2 }
		},
		false
	);

}

void GlassReflectionHandler::Cleanup()
{
	g_sheetMap.clear();
}
