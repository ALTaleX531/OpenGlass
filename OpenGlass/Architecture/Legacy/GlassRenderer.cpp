#include "pch.h"
#include "HookHelper.hpp"
#include "uDWMProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "Shared.hpp"
#include "GlassKernel.hpp"
#include "GlassRenderer.hpp"
#include "GlassRealizer.hpp"
#include "D3DGlassRealizer.hpp"
#include "ReflectionRealizer.hpp"
#include "MaterialRealizer.hpp"
#include "D2DPrivates.hpp"
#include "GlassCoverageSet.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassRenderer
{
	template <typename T>
	HRESULT MyCRenderData_TryDrawCommandAsDrawList(
		dwmcore::CRenderData* This,
		dwmcore::CDrawingContext* drawingContext,
		UINT commandType,
		DWM::span<const BYTE>* resources,
		bool* succeeded,
		T&& callback
	);
	HRESULT MyCRenderData_TryDrawCommandAsDrawList_Win10(
		dwmcore::CRenderData* This,
		dwmcore::CDrawingContext* drawingContext,
		dwmcore::CDrawListCache* drawListCache,
		dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
		bool unknwon,
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
	HRESULT MyCDrawingContext_DrawGeometry(
		dwmcore::IDrawingContext* This,
		dwmcore::CLegacyMilBrush* brush,
		dwmcore::CGeometry* geometry
	);
	void MyID2D1DeviceContext_FillGeometry(
		ID2D1DeviceContext* This,
		ID2D1Geometry* geometry,
		ID2D1Brush* brush,
		ID2D1Brush* opacityBrush
	);

	Projection::Detour<dwmcore::Symbol_CRenderData_TryDrawCommandAsDrawList_Pre_22000, decltype(&MyCRenderData_TryDrawCommandAsDrawList_Win10)> g_CRenderData_TryDrawCommandAsDrawList_Win10_Org{};
	Projection::Detour<dwmcore::Symbol_CRenderData_TryDrawCommandAsDrawList_22000, decltype(&MyCRenderData_TryDrawCommandAsDrawList_Win11)> g_CRenderData_TryDrawCommandAsDrawList_Win11_Org{};
	Projection::Detour<dwmcore::Symbol_CRenderData_DrawImageResource_FillMode_Pre_19041, decltype(&MyCRenderData_DrawImageResource_FillMode_Pre_W10_2004)> g_CRenderData_DrawImageResource_FillMode_Pre_W10_2004_Org{};
	Projection::Detour<dwmcore::Symbol_CRenderData_DrawImageResource_FillMode_19041, decltype(&MyCRenderData_DrawImageResource_FillMode_Win10)> g_CRenderData_DrawImageResource_FillMode_Win10_Org{};
	Projection::Detour<dwmcore::Symbol_CRenderData_DrawImageResource_FillMode_22000, decltype(&MyCRenderData_DrawImageResource_FillMode_Win11_Pre_24H2)> g_CRenderData_DrawImageResource_FillMode_Win11_Pre_24H2_Org{};
	Projection::Detour<dwmcore::Symbol_CRenderData_DrawImageResource_FillMode_26100_2454, decltype(&MyCRenderData_DrawImageResource_FillMode_Win11_24H2)> g_CRenderData_DrawImageResource_FillMode_Win11_24H2_Org{};
	decltype(&MyCDrawingContext_DrawGeometry) g_CDrawingContext_DrawGeometry_Org{ nullptr };
	decltype(&MyCDrawingContext_DrawGeometry)* g_CDrawingContext_DrawGeometry_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCDrawingContext_DrawGeometry> g_CDrawingContext_DrawGeometry_Hook;
	decltype(&MyID2D1DeviceContext_FillGeometry) g_ID2D1DeviceContext_FillGeometry_Org{ nullptr };
	decltype(&MyID2D1DeviceContext_FillGeometry)* g_ID2D1DeviceContext_FillGeometry_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyID2D1DeviceContext_FillGeometry> g_ID2D1DeviceContext_FillGeometry_Hook;

	enum RenderFlag
	{
		RenderFlag_SolidColor,
		RenderFlag_Backdrop,
		RenderFlag_Material,
		RenderFlag_Reflection
	};

	struct CDeviceResources
	{
		winrt::com_ptr<ID2D1SolidColorBrush> m_brush{};
		std::variant<std::monostate, CGlassRealizer, CD3DGlassRealizer> m_glassRealizer{};
		CReflectionRealizer m_reflectionRealizer{};
		CMaterialRealizer m_materialRealizer{};
	};

	ReflectionContext g_reflectionContext{};

	Shared::GlassType g_type{ Shared::GlassType::Invalid };
	CAeroParams g_params{};

	MaterialContext g_materialContext{};

	std::span<const D2D1_RECT_F> g_rectangleSpan{};
	D2D1_RECT_F g_drawingWorldBounds{};

	std::unordered_map<dwmcore::CD2DContext*, CDeviceResources> g_deviceResources{};
	CDeviceResources* g_currentDeviceResources{};
	dwmcore::CDrawingContext* g_drawingContextNoRef{};
	std::bitset<4> g_renderFlag{};
	bool g_colorIsOpaque{};
	bool g_shapeIsRectangles{};
	bool g_renderTargetIgnoresAlpha{};

	bool g_fixLivePreviewRendering{ false };
	UINT g_drawGeometryCommandType{};

	constexpr size_t MAX_RECTANGLES_ON_STACK{ 16 };
	dwmcore::CShapePtr GetShapeRenderingData(
		const dwmcore::CDrawingContext* drawingContext,
		const dwmcore::CShape* shape, 
		const dwmcore::CMILMatrix* matrix,
		D2D1_RECT_F& drawingWorldBounds,
		D2D1_RECT_F& shapeWorldBounds,
		std::unique_ptr<D2D1_RECT_F[]>& rectangles,
		D2D1_RECT_F rectanglesOnStack[MAX_RECTANGLES_ON_STACK],
		UINT& rectanglesCount,
		std::span<const D2D1_RECT_F>& rectangleSpan,
		bool& shapeIsRectangles
	)
	{
		drawingWorldBounds = {};
		rectangles.reset();
		rectanglesCount = 0;
		shapeIsRectangles = false;
		dwmcore::CShapePtr renderingShape{};
		if (
			FAILED(
				drawingContext->GetUnOccludedWorldShape(
					shape,
					drawingContext->GetD2DContextOwner()->GetCurrentZ(),
					renderingShape.put()
				)
			) ||
			!renderingShape
		) [[unlikely]]
		{
			shape->CopyShape(matrix, renderingShape.put());
		}

		D2D1_RECT_F* buffer = nullptr;

		if (renderingShape && !renderingShape->IsEmpty()) [[likely]]
		{
			shape->GetTightBounds(&shapeWorldBounds, matrix);
			drawingContext->GetClipBoundsWorld(drawingWorldBounds, false);
			
			if (renderingShape->IsRectangles(&rectanglesCount)) [[likely]]
			{
				buffer = rectanglesCount > MAX_RECTANGLES_ON_STACK ? (
					rectangles = std::make_unique_for_overwrite<D2D1_RECT_F[]>(rectanglesCount),
					rectangles.get()
				) : rectanglesOnStack;
				if (
					renderingShape->GetRectangles(
						buffer,
						rectanglesCount
					)
				) [[likely]]
				{
					shapeIsRectangles = true;
				}
			}

			if (!shapeIsRectangles) [[unlikely]]
			{
				rectanglesCount = 1;
				rectangles.reset();
				buffer = rectanglesOnStack;
				renderingShape->GetTightBounds(rectanglesOnStack, nullptr);
			}
		}

		rectangleSpan = { buffer, rectanglesCount };
		return renderingShape;
	}

}

template <typename T>
HRESULT GlassRenderer::MyCRenderData_TryDrawCommandAsDrawList(
	dwmcore::CRenderData* This,
	dwmcore::CDrawingContext* drawingContext,
	UINT commandType,
	DWM::span<const BYTE>* resources,
	bool* succeeded,
	T&& callback
)
{
	const auto command = reinterpret_cast<const dwmcore::CDrawGeometryCommand*>(resources->data);
	if (
		commandType == g_drawGeometryCommandType &&
		resources->length == sizeof(dwmcore::CDrawGeometryCommand) &&
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

	if (
		commandType == g_drawGeometryCommandType &&
		resources->length == sizeof(dwmcore::CDrawGeometryCommand) &&
		(
			(
				HookHelper::get_vftable_from(This->GetResources()->data[command->brushIndex]) == dwmcore::CSolidColorLegacyMilBrush::vftable
			) ||
			(
				HookHelper::get_vftable_from(This->GetResources()->data[command->brushIndex]) == dwmcore::CImageLegacyMilBrush::vftable &&
				static_cast<dwmcore::CImageLegacyMilBrush*>(This->GetResources()->data[command->brushIndex])->GetImageSource() == nullptr &&
				wil::rect_is_empty(static_cast<dwmcore::CImageLegacyMilBrush*>(This->GetResources()->data[command->brushIndex])->GetViewbox())
			)
		)
	)
	{
		if (!g_CDrawingContext_DrawGeometry_Org)
		{
			g_CDrawingContext_DrawGeometry_Org_Address = dwmcore::IDrawingContext_DrawGeometry_VtableSlot.address(
				HookHelper::get_vftable_from(drawingContext->GetInterface())
			);
			g_CDrawingContext_DrawGeometry_Hook.AttachOnce(g_CDrawingContext_DrawGeometry_Org_Address, &g_CDrawingContext_DrawGeometry_Org);
		}

		*succeeded = false;
		return S_OK;
	}

	return callback();
}
HRESULT GlassRenderer::MyCRenderData_TryDrawCommandAsDrawList_Win10(
	dwmcore::CRenderData* This,
	dwmcore::CDrawingContext* drawingContext,
	dwmcore::CDrawListCache* drawListCache,
	dwmcore::CDrawListEntryBuilder* drawListEntryBuilder,
	bool unknwon,
	UINT commandType,
	DWM::span<const BYTE>* resources,
	bool* succeeded
)
{
	return MyCRenderData_TryDrawCommandAsDrawList(
		This,
		drawingContext,
		commandType,
		resources,
		succeeded,
		[=]()
		{
			return g_CRenderData_TryDrawCommandAsDrawList_Win10_Org(
				This,
				drawingContext,
				drawListCache,
				drawListEntryBuilder,
				unknwon,
				commandType,
				resources,
				succeeded
			);
		}
	);
}
HRESULT GlassRenderer::MyCRenderData_TryDrawCommandAsDrawList_Win11(
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
		drawingContext,
		commandType,
		resources,
		succeeded,
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

const D2D1_RECT_F* GlassRenderer::AdjustLivePreviewSourceRect(const D2D1_RECT_F* srcRect) noexcept
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

HRESULT GlassRenderer::MyCRenderData_DrawImageResource_FillMode_Win10(
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

HRESULT GlassRenderer::MyCRenderData_DrawImageResource_FillMode_Pre_W10_2004(
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

HRESULT GlassRenderer::MyCRenderData_DrawImageResource_FillMode_Win11_Pre_24H2(
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

HRESULT GlassRenderer::MyCRenderData_DrawImageResource_FillMode_Win11_24H2(
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

HRESULT GlassRenderer::MyCDrawingContext_DrawGeometry(
	dwmcore::IDrawingContext* This,
	dwmcore::CLegacyMilBrush* brush,
	dwmcore::CGeometry* geometry
)
{
	const auto drawingContext = This->GetDrawingContext();
	if (dwmcore::g_versionInfo.build < os::build_w10_2004 && drawingContext->IsBounding())
	{
		return g_CDrawingContext_DrawGeometry_Org(
			This,
			brush,
			geometry
		);
	}

	dwmcore::CShapePtr geometryShape{};
	if (
		FAILED(geometry->GetShapeData(nullptr, &geometryShape)) ||
		!geometryShape ||
		geometryShape->IsEmpty()
	)
	{
		return S_OK;
	}

	const auto occlusionContext = drawingContext->GetOcclusionContext();
	const auto d2dContext = drawingContext->GetD3DDevice()->GetD2DContext();
	const auto context = d2dContext->GetDeviceContext();
	const auto matrix = drawingContext->GetWorldTransform();
	
	if (dwmcore::g_versionInfo.build < os::build_w10_2004)
	{
		if (!drawingContext->GetRenderTarget())
		{
			return S_OK;
		}
	}
	else
	{
		if (!drawingContext->GetDeviceTarget())
		{
			return S_OK;
		}
	}
	if (!context)
	{
		return S_OK;
	}

	UINT rectanglesCount{};
	std::unique_ptr<D2D1_RECT_F[]> rectangles{};
	D2D1_RECT_F shapeWorldBounds{};
	D2D1_RECT_F rectanglesOnStack[MAX_RECTANGLES_ON_STACK]{};

	const auto renderingShape = GetShapeRenderingData(
		drawingContext,
		geometryShape.get(),
		matrix,
		g_drawingWorldBounds,
		shapeWorldBounds,
		rectangles,
		rectanglesOnStack,
		rectanglesCount,
		g_rectangleSpan,
		g_shapeIsRectangles
	);

	if (!renderingShape || renderingShape->IsEmpty())
	{
		return S_OK;
	}

	D2D1_RECT_F renderingShapeBounds{};
	RETURN_IF_FAILED(renderingShape->GetTightBounds(&renderingShapeBounds, nullptr));
	RectF::IntersectUnsafe(g_drawingWorldBounds, renderingShapeBounds);
	if (
		occlusionContext &&
		occlusionContext->GetArrayBasedCoverageSet()->IsCovered(
			g_drawingWorldBounds,
			drawingContext->GetD2DContextOwner()->GetCurrentZ()
		)
	)
	{
		return S_OK;
	}

	g_drawingContextNoRef = drawingContext;
	g_currentDeviceResources = &g_deviceResources.try_emplace(d2dContext).first->second;
	g_renderTargetIgnoresAlpha = false;
	const auto transformScope = wil::scope_exit([drawingContext]
	{
		g_renderTargetIgnoresAlpha = false;
		g_currentDeviceResources = nullptr;
		g_drawingContextNoRef = nullptr;
	});

	D2D1_COLOR_F color{};
	
	if (HookHelper::get_vftable_from(brush) == dwmcore::CImageLegacyMilBrush::vftable)
	{
		const auto imageBrush = static_cast<dwmcore::CImageLegacyMilBrush*>(brush);
		const float opacity = imageBrush->GetOpacityValue();

		if (opacity == 0.f)
		{
			return S_OK;
		}
		
		g_reflectionContext.opacity = opacity;
		g_reflectionContext.worldTransform = matrix;
		g_reflectionContext.viewport = &imageBrush->GetViewport();
		g_renderFlag.set(RenderFlag_Reflection, true);
	}

	if (HookHelper::get_vftable_from(brush) == dwmcore::CSolidColorLegacyMilBrush::vftable)
	{
		const auto solidColorBrush = static_cast<dwmcore::CSolidColorLegacyMilBrush*>(brush);
		const auto sdrBoost = drawingContext->GetSDRBoost();
		color = solidColorBrush->GetRealizedColor();
		
		if (color.a == 0.f)
		{
			return S_OK;
		}

		d2dContext->EnsureBeginDraw();
		bool colorSpaceIsScRGB = false;
		if (
			winrt::com_ptr<ID2D1Bitmap1> renderTargetBitmap{ nullptr };
			SUCCEEDED(Util::GetTargetBitmapFromD2DContext(context, renderTargetBitmap))
		)
		{
			const auto pixelFormat = renderTargetBitmap->GetPixelFormat();
			if (
				const auto format = pixelFormat.format;
				format == DXGI_FORMAT_R16G16B16A16_FLOAT ||
				format == DXGI_FORMAT_R32G32B32A32_FLOAT
			)
			{
				colorSpaceIsScRGB = true;
			}
			g_renderTargetIgnoresAlpha = pixelFormat.alphaMode == D2D1_ALPHA_MODE_IGNORE;
		}
		
		const auto expansion = GlassKernel::GetBlurRadius();
		const auto glassCoverageSet = CArrayBasedGlassCoverageSet::GetOrCreate(occlusionContext->GetArrayBasedCoverageSet());
		const auto reinterpreter = GlassKernel::AlphaChannelReinterpreter(color.a);

		// try render glass
		if (reinterpreter.GetIsValid())
		{
			const auto active = reinterpreter.GetIsActive();
			const auto maximized = reinterpreter.GetIsMaximized();
			g_renderFlag.set(RenderFlag_Material, true);
			g_materialContext.opacity = Shared::g_materialIntensity;

			const auto realizedGlassColorizationParameters = GlassKernel::RealizeWindowColorization(
				GlassKernel::GetBaseColor(Shared::IsTransparencyDisabled(), maximized),
				GlassKernel::GetSourceColor(active),
				GlassKernel::GetColorizationOpacity(active, maximized),
				Shared::IsTransparencyDisabled(),
				false
			);
			color = realizedGlassColorizationParameters.GetEffectivescRGBBlendColor(sdrBoost);
			if (!colorSpaceIsScRGB)
			{
				color = Color::scRGBTosRGB(color, sdrBoost);
			}

			if (
				!(
					Shared::IsTransparencyDisabled() ||
					Shared::IsGlassFullyOpaque(
						realizedGlassColorizationParameters.color.a,
						realizedGlassColorizationParameters.blurBalance,
						realizedGlassColorizationParameters.afterglowBalance
					)
				) &&
				expansion &&
				glassCoverageSet
			)
			{
				g_type = Shared::g_type;
				g_params.blurAmount = Shared::g_blurAmount;
				g_params.optimization = Shared::g_blurOptimization;
				if (colorSpaceIsScRGB)
				{
					g_params.color = Color::sRGBToscRGB(realizedGlassColorizationParameters.color, sdrBoost);
					// according to windows 7 implementation
					// afterglow is always in sRGB color space
					// even when the render target is in scRGB color space.
					g_params.afterglow = realizedGlassColorizationParameters.afterglow;
					//g_params.afterglow = Color::sRGBToscRGB(realizedGlassColorizationParameters.afterglow, 1.f);
				}
				else
				{
					g_params.color = realizedGlassColorizationParameters.color;
					g_params.afterglow = realizedGlassColorizationParameters.afterglow;
				}
				if (g_type == Shared::GlassType::Blur)
				{
					g_params.color.r *= g_params.color.a;
					g_params.color.g *= g_params.color.a;
					g_params.color.b *= g_params.color.a;
				}
				else if (g_type == Shared::GlassType::Aero)
				{
					g_params.color.r *= realizedGlassColorizationParameters.colorBalance;
					g_params.color.g *= realizedGlassColorizationParameters.colorBalance;
					g_params.color.b *= realizedGlassColorizationParameters.colorBalance;
					g_params.color.a = 1.f;

					g_params.afterglow.r *= realizedGlassColorizationParameters.afterglowBalance;
					g_params.afterglow.g *= realizedGlassColorizationParameters.afterglowBalance;
					g_params.afterglow.b *= realizedGlassColorizationParameters.afterglowBalance;
					g_params.afterglow.a = 1.f;

					g_params.fallback = color;
					g_params.blurBalance = realizedGlassColorizationParameters.blurBalance;
				}

				if (
					std::holds_alternative<std::monostate>(g_currentDeviceResources->m_glassRealizer) ||
					Shared::g_useD3DRendering != std::holds_alternative<CD3DGlassRealizer>(g_currentDeviceResources->m_glassRealizer)
				)
				{
					if (Shared::g_useD3DRendering)
					{
						g_currentDeviceResources->m_glassRealizer.emplace<CD3DGlassRealizer>();
					}
					else
					{
						g_currentDeviceResources->m_glassRealizer.emplace<CGlassRealizer>();
					}
				}
				g_renderFlag.set(RenderFlag_Backdrop, true);
			}
		}
		else
		{
			if (!colorSpaceIsScRGB)
			{
				color = Color::scRGBTosRGB(color, sdrBoost);
			}
		}

		if (!g_renderFlag.test(RenderFlag_Backdrop))
		{
			g_colorIsOpaque = color.a == 1.f;
			g_renderFlag.set(RenderFlag_SolidColor, true);
		}
	}

	if (g_renderFlag.any())
	{
		if (!g_currentDeviceResources->m_brush)
		{
			RETURN_IF_FAILED(context->CreateSolidColorBrush(color, g_currentDeviceResources->m_brush.put()));
		}
		else
		{
			g_currentDeviceResources->m_brush->SetColor(color);
		}

		if (!g_ID2D1DeviceContext_FillGeometry_Org)
		{
			g_ID2D1DeviceContext_FillGeometry_Org_Address = reinterpret_cast<decltype(g_ID2D1DeviceContext_FillGeometry_Org_Address)>(&HookHelper::get_vftable_from(context)[0x17]);
			g_ID2D1DeviceContext_FillGeometry_Hook.AttachOnce(g_ID2D1DeviceContext_FillGeometry_Org_Address, &g_ID2D1DeviceContext_FillGeometry_Org);
		}

		return drawingContext->FillShapeWithBrush(renderingShape.get(), g_currentDeviceResources->m_brush.get());
	}

	return S_OK;
}

void GlassRenderer::MyID2D1DeviceContext_FillGeometry(
	ID2D1DeviceContext* This,
	ID2D1Geometry* geometry,
	ID2D1Brush* brush,
	ID2D1Brush* opacityBrush
)
{
	const auto fillGeometryScope = wil::scope_exit([] { g_renderFlag.reset(); });
	if (
		!g_currentDeviceResources ||
		opacityBrush
	)
	{
		return g_ID2D1DeviceContext_FillGeometry_Org(This, geometry, brush, opacityBrush);
	}

	const auto primitiveBlend = This->GetPrimitiveBlend();
	const auto antialiasMode = This->GetAntialiasMode();
	D2D1_MATRIX_3X2_F matrix{};
	This->GetTransform(&matrix);
	This->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
	This->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
	This->SetTransform(D2D1::IdentityMatrix());

	bool ignoreLayer{ g_shapeIsRectangles };
	if (!ignoreLayer)
	{
		auto layerOptions = D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND;
		if (g_renderTargetIgnoresAlpha)
		{
			layerOptions = static_cast<D2D1_LAYER_OPTIONS1>(
				layerOptions | D2D1_LAYER_OPTIONS1_IGNORE_ALPHA
			);
		}
		This->PushLayer(
			D2D1::LayerParameters1(
				g_drawingWorldBounds,
				geometry,
				D2D1_ANTIALIAS_MODE_ALIASED,
				D2D1::IdentityMatrix(),
				1.f,
				nullptr,
				layerOptions
			),
			nullptr
		);
	}
	if (g_renderFlag.test(RenderFlag_SolidColor))
	{
		const auto primitiveBlendSolidColor = This->GetPrimitiveBlend();
		if (!g_colorIsOpaque)
		{
			This->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
		}
		if (!ignoreLayer)
		{
			This->FillRectangle(g_drawingWorldBounds, brush);
		}
		else
		{
			g_ID2D1DeviceContext_FillGeometry_Org(This, geometry, brush, opacityBrush);
		}
		if (!g_colorIsOpaque)
		{
			This->SetPrimitiveBlend(primitiveBlendSolidColor);
		}
	}
	if (g_renderFlag.test(RenderFlag_Backdrop))
	{
		LOG_IF_FAILED(This->Flush());

		if (std::holds_alternative<CGlassRealizer>(g_currentDeviceResources->m_glassRealizer))
		{
			LOG_IF_FAILED(
				std::get<CGlassRealizer>(g_currentDeviceResources->m_glassRealizer).Render(
					This,
					geometry,
					g_drawingWorldBounds,
					g_rectangleSpan,
					g_params,
					g_type
				)
			);
		}
		else
		{
			auto& d3dGlassRealizer = std::get<CD3DGlassRealizer>(g_currentDeviceResources->m_glassRealizer);
			if (uDWM::g_versionInfo.build < os::build_w10_2004)
			{
				const auto renderTarget = g_drawingContextNoRef->GetRenderTarget();
				const auto d3dSurface = renderTarget->GetTargetSurfaceNoRef();

				const auto texture2D = d3dSurface->GetTexture2D();
				const auto renderTargetView = d3dSurface->GetRenderTargetView();
				const auto shaderResourceView = d3dSurface->GetShaderResourceView();

				LOG_IF_FAILED(
					d3dGlassRealizer.Render(
						g_drawingContextNoRef->GetD3DDevice()->GetDevice(),
						g_drawingContextNoRef->GetD3DDevice()->GetImmediateContext(),
						texture2D,
						renderTargetView,
						shaderResourceView,
						geometry,
						g_drawingWorldBounds,
						g_rectangleSpan,
						g_params,
						g_type
					)
				);
			}
			else
			{
				const auto deviceTarget = g_drawingContextNoRef->GetDeviceTarget();
				const auto deviceTexture = deviceTarget->GetDeviceTexture();

				auto texture2D = deviceTexture->GetTexture2D();
				const auto renderTargetView = deviceTarget->GetRenderTargetView();
				const auto shaderResourceView = deviceTexture->GetShaderResourceView();

				winrt::com_ptr<ID3D11Texture2D> backBufferTexture{};
				if (!texture2D)
				{
					winrt::com_ptr<ID2D1Bitmap1> renderTargetBitmap{};
					THROW_IF_FAILED(Util::GetTargetBitmapFromD2DContext(This, renderTargetBitmap));
					THROW_IF_FAILED(Util::GetTextureFromD2DBitmap(renderTargetBitmap.get(), backBufferTexture));
					texture2D = backBufferTexture.get();
				}

				LOG_IF_FAILED(
					d3dGlassRealizer.Render(
						g_drawingContextNoRef->GetD3DDevice()->GetDevice(),
						g_drawingContextNoRef->GetD3DDevice()->GetImmediateContext(),
						texture2D,
						renderTargetView,
						shaderResourceView,
						geometry,
						g_drawingWorldBounds,
						g_rectangleSpan,
						g_params,
						g_type
					)
				);
			}
		}

	}
	if (g_renderFlag.test(RenderFlag_Material))
	{
		LOG_IF_FAILED(
			g_currentDeviceResources->m_materialRealizer.Render(
				This,
				g_rectangleSpan,
				g_materialContext
			)
		);
	}
	if (g_renderFlag.test(RenderFlag_Reflection))
	{
		const auto primitiveBlendReflection = This->GetPrimitiveBlend();
		This->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
		LOG_IF_FAILED(
			g_currentDeviceResources->m_reflectionRealizer.Render(
				This,
				g_rectangleSpan,
				g_reflectionContext
			)
		);
		This->SetPrimitiveBlend(primitiveBlendReflection);
	}
	if (!ignoreLayer)
	{
		This->PopLayer();
	}

	This->SetAntialiasMode(antialiasMode);
	This->SetPrimitiveBlend(primitiveBlend);
	This->SetTransform(matrix);
}

void GlassRenderer::DestroyDeviceResources(dwmcore::CD2DContext* d2dContext)
{
	g_deviceResources.erase(d2dContext);
}

void GlassRenderer::Update(GlassEngine::UpdateType type)
{
	if (type & GlassEngine::UpdateType::Theme)
	{
		WCHAR materialTexturePath[MAX_PATH]{};
		GlassEngine::GetStringFromRegistry(L"CustomThemeMaterial", materialTexturePath);
		PathUnquoteSpacesW(materialTexturePath);
		Shared::g_materialTexturePath.assign(materialTexturePath);

		for (auto& [_, deviceResources] : g_deviceResources)
		{
			deviceResources.m_reflectionRealizer.Reset();
			deviceResources.m_materialRealizer.Reset();
		}
	}
	if (type & GlassEngine::UpdateType::Backdrop)
	{
		auto value = GlassEngine::GetDwordFromRegistry(L"GlassOpacity", 63);
		Shared::g_glassOpacity = std::clamp(static_cast<float>(value) / 100.f, 0.f, 1.f);
		Shared::g_glassOpacityInactive = std::clamp(static_cast<float>(GlassEngine::GetDwordFromRegistry(L"GlassOpacityInactive", value)) / 100.f, 0.f, 1.f);
		
		value = GlassEngine::GetOverridableDwordFromRegistry(L"ColorizationAfterglow", L"ColorizationAfterglowOverride");
		Shared::g_afterglow = Color::FromArgb(value);

		value = GlassEngine::GetOverridableDwordFromRegistry(L"ColorizationBlurBalance", L"ColorizationBlurBalanceOverride");
		Shared::g_blurBalance = std::clamp(static_cast<float>(value) / 100.f, 0.f, 1.f);

		value = GlassEngine::GetOverridableDwordFromRegistry(L"ColorizationColorBalance", L"ColorizationColorBalanceOverride");
		Shared::g_colorBalance = std::clamp(static_cast<float>(value) / 100.f, 0.f, 1.f);

		value = GlassEngine::GetOverridableDwordFromRegistry(L"ColorizationAfterglowBalance", L"ColorizationAfterglowBalanceOverride");
		Shared::g_afterglowBalance = std::clamp(static_cast<float>(value) / 100.f, 0.f, 1.f);
	
		value = GlassEngine::GetDwordFromRegistry(L"MaterialOpacity");
		Shared::g_materialIntensity = std::clamp(static_cast<float>(value) / 100.f, 0.f, 1.f);
	}
}

void GlassRenderer::Startup()
{
#ifdef _DEBUG
#endif

	if (!g_drawGeometryCommandType)
	{
		switch (dwmcore::g_versionInfo.build)
		{
		case os::build_w10_1809:
		{
			g_drawGeometryCommandType = 460;
			break;
		}
		case os::build_w10_1903:
		{
			g_drawGeometryCommandType = 535;
			break;
		}
		case os::build_w10_2004:
		{
			g_drawGeometryCommandType = 461;
			break;
		}
		case os::build_w11_21h2:
		{
			g_drawGeometryCommandType = 456;
			break;
		}
		case os::build_w11_22h2:
		{
			g_drawGeometryCommandType = 444;
			break;
		}
		// Microsoft has been changing things so frequently since 24H2 that
		// the command types can be completely different under the same build number, 
		// they are absolutely insane
		case os::build_w11_24h2:
		{
			break;
		}
		default:
			break;
		}
	}
	if (!g_drawGeometryCommandType)
	{
		auto CRenderDataBuilder_DrawGeometry_Instructions = static_cast<UCHAR*>(
			dwmcore::Symbol_CRenderDataBuilder_DrawGeometry.get()
		);

		UCHAR g_movDrawGeometryCommandType_Instructions[]
		{
			// mov dword ptr [rdx+rcx+4], ???
			0xC7, 0x44, 0x0A, 0x04, // 0x00, 0x00, 0x00, 0x00
		};
		auto i = 128;
		do
		{
			
			if (
				memcmp(
					CRenderDataBuilder_DrawGeometry_Instructions,
					g_movDrawGeometryCommandType_Instructions,
					sizeof(g_movDrawGeometryCommandType_Instructions)
				) == 0
			)
			{
				g_drawGeometryCommandType = *reinterpret_cast<UINT*>(CRenderDataBuilder_DrawGeometry_Instructions + sizeof(g_movDrawGeometryCommandType_Instructions));
				break;
			}

			CRenderDataBuilder_DrawGeometry_Instructions += 1;
			i--;
		} while (i);
	}

	const auto build_before_w11_24h2 = Util::VersionBefore<os::build_w11_24h2, os::revision_24h2_rtm_1>(dwmcore::g_versionInfo.build, dwmcore::g_versionInfo.revision);
	const auto build_before_w11_21h2 = dwmcore::g_versionInfo.build < os::build_w11_21h2;
	const auto build_before_w10_2004 = dwmcore::g_versionInfo.build < os::build_w10_2004;
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CRenderData_TryDrawCommandAsDrawList_Win10_Org, &MyCRenderData_TryDrawCommandAsDrawList_Win10, build_before_w11_21h2 },
			{ &g_CRenderData_TryDrawCommandAsDrawList_Win11_Org, &MyCRenderData_TryDrawCommandAsDrawList_Win11, !build_before_w11_21h2 },

			{ &g_CRenderData_DrawImageResource_FillMode_Pre_W10_2004_Org, &MyCRenderData_DrawImageResource_FillMode_Pre_W10_2004, build_before_w10_2004 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win10_Org, &MyCRenderData_DrawImageResource_FillMode_Win10, !build_before_w10_2004 && build_before_w11_21h2 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win11_Pre_24H2_Org, &MyCRenderData_DrawImageResource_FillMode_Win11_Pre_24H2, !build_before_w11_21h2 && build_before_w11_24h2 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win11_24H2_Org, &MyCRenderData_DrawImageResource_FillMode_Win11_24H2, !build_before_w11_24h2 },
		},
		true
	);
}

void GlassRenderer::Shutdown()
{
	const auto build_before_w11_24h2 = Util::VersionBefore<os::build_w11_24h2, os::revision_24h2_rtm_1>(dwmcore::g_versionInfo.build, dwmcore::g_versionInfo.revision);
	const auto build_before_w11_21h2 = dwmcore::g_versionInfo.build < os::build_w11_21h2;
	const auto build_before_w10_2004 = dwmcore::g_versionInfo.build < os::build_w10_2004;
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CRenderData_TryDrawCommandAsDrawList_Win10_Org, &MyCRenderData_TryDrawCommandAsDrawList_Win10, build_before_w11_21h2 },
			{ &g_CRenderData_TryDrawCommandAsDrawList_Win11_Org, &MyCRenderData_TryDrawCommandAsDrawList_Win11, !build_before_w11_21h2 },

			{ &g_CRenderData_DrawImageResource_FillMode_Pre_W10_2004_Org, &MyCRenderData_DrawImageResource_FillMode_Pre_W10_2004, build_before_w10_2004 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win10_Org, &MyCRenderData_DrawImageResource_FillMode_Win10, !build_before_w10_2004 && build_before_w11_21h2 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win11_Pre_24H2_Org, &MyCRenderData_DrawImageResource_FillMode_Win11_Pre_24H2, !build_before_w11_21h2 && build_before_w11_24h2 },
			{ &g_CRenderData_DrawImageResource_FillMode_Win11_24H2_Org, &MyCRenderData_DrawImageResource_FillMode_Win11_24H2, !build_before_w11_24h2 },
		},
		false
	);

	if (g_ID2D1DeviceContext_FillGeometry_Org)
	{
		HookHelper::GetCurrentHookTransaction().Apply(g_ID2D1DeviceContext_FillGeometry_Hook);
	}
	if (g_CDrawingContext_DrawGeometry_Org)
	{
		HookHelper::GetCurrentHookTransaction().Apply(g_CDrawingContext_DrawGeometry_Hook);
	}

}

void GlassRenderer::Cleanup()
{
	g_deviceResources.clear();
}
