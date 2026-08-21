#pragma once
#include "ProjectionHelper.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"

namespace OpenGlass::GlassFrameDemodernizer
{
	HRESULT MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This);
	HRESULT MyCTopLevelWindow_UpdateNCAreaBackground(uDWM::CTopLevelWindow* This);
}

namespace OpenGlass::GlassFrameHandler
{
	HRESULT MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This);
	HRESULT MyCTopLevelWindow_UpdateNCAreaBackground(uDWM::CTopLevelWindow* This);
}

namespace OpenGlass::GlassReflectionHandler
{
	HRESULT MyCRenderData_TryDrawCommandAsDrawList_Win10(
		dwmcore::CRenderData* This,
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CDrawListCache* drawListCache,
		dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
		bool unknown,
		UINT commandType,
		DWM::span<const BYTE>* resources,
		bool* succeeded
	);
	HRESULT MyCRenderData_TryDrawCommandAsDrawList_Win11(
		dwmcore::CRenderData* This,
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CDrawListCache* drawListCache,
		dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
		UINT commandType,
		DWM::span<const BYTE>* resources,
		bool* succeeded
	);
}

namespace OpenGlass::GlassRenderer
{
	HRESULT MyCRenderData_TryDrawCommandAsDrawList_Win10(
		dwmcore::CRenderData* This,
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CDrawListCache* drawListCache,
		dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
		bool unknown,
		UINT commandType,
		DWM::span<const BYTE>* resources,
		bool* succeeded
	);
	HRESULT MyCRenderData_TryDrawCommandAsDrawList_Win11(
		dwmcore::CRenderData* This,
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CDrawListCache* drawListCache,
		dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
		UINT commandType,
		DWM::span<const BYTE>* resources,
		bool* succeeded
	);
}

namespace OpenGlass::DetourChains
{
	// Replacements are listed in invocation order, not component startup order.
	using TopLevelWindowValidateVisual = Projection::ChainDetour<
		uDWM::Symbol_CTopLevelWindow_ValidateVisual,
		&GlassFrameDemodernizer::MyCTopLevelWindow_ValidateVisual,
		&GlassFrameHandler::MyCTopLevelWindow_ValidateVisual
	>;
	using TopLevelWindowValidateVisualDemodernizerNode = TopLevelWindowValidateVisual::Node<0>;
	using TopLevelWindowValidateVisualFrameHandlerNode = TopLevelWindowValidateVisual::Node<1>;

	using TopLevelWindowUpdateNCAreaBackground = Projection::ChainDetour<
		uDWM::Symbol_CTopLevelWindow_UpdateNCAreaBackground,
		&GlassFrameDemodernizer::MyCTopLevelWindow_UpdateNCAreaBackground,
		&GlassFrameHandler::MyCTopLevelWindow_UpdateNCAreaBackground
	>;
	using TopLevelWindowUpdateNCAreaBackgroundDemodernizerNode = TopLevelWindowUpdateNCAreaBackground::Node<0>;
	using TopLevelWindowUpdateNCAreaBackgroundFrameHandlerNode = TopLevelWindowUpdateNCAreaBackground::Node<1>;

	using RenderDataTryDrawCommandAsDrawListWin10 = Projection::ChainDetour<
		dwmcore::Symbol_CRenderData_TryDrawCommandAsDrawList_Pre_22000,
		&GlassReflectionHandler::MyCRenderData_TryDrawCommandAsDrawList_Win10,
		&GlassRenderer::MyCRenderData_TryDrawCommandAsDrawList_Win10
	>;
	using RenderDataTryDrawCommandAsDrawListWin10ReflectionNode = RenderDataTryDrawCommandAsDrawListWin10::Node<0>;
	using RenderDataTryDrawCommandAsDrawListWin10RendererNode = RenderDataTryDrawCommandAsDrawListWin10::Node<1>;

	using RenderDataTryDrawCommandAsDrawListWin11 = Projection::ChainDetour<
		dwmcore::Symbol_CRenderData_TryDrawCommandAsDrawList_22000,
		&GlassReflectionHandler::MyCRenderData_TryDrawCommandAsDrawList_Win11,
		&GlassRenderer::MyCRenderData_TryDrawCommandAsDrawList_Win11
	>;
	using RenderDataTryDrawCommandAsDrawListWin11ReflectionNode = RenderDataTryDrawCommandAsDrawListWin11::Node<0>;
	using RenderDataTryDrawCommandAsDrawListWin11RendererNode = RenderDataTryDrawCommandAsDrawListWin11::Node<1>;
}
