#include "pch.h"
#include "MsstyleInternals.hpp"
#include "CaptionTextHandler.hpp"
#include "CaptionText.hpp"
#include "Shared.hpp"
#include "Util.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "CustomThemeAtlasLoader.hpp"

using namespace OpenGlass;
namespace OpenGlass::CaptionTextHandler
{
	void MyID2D1DeviceContext_DrawTextLayout(
		ID2D1DeviceContext* This,
		D2D1_POINT_2F origin,
		IDWriteTextLayout* textLayout,
		ID2D1Brush* defaultFillBrush,
		D2D1_DRAW_TEXT_OPTIONS options
	);
	HRESULT MyICompositionGraphicsDevice_CreateDrawingSurface(
		abi::ICompositionGraphicsDevice* This,
		abi::Size sizePixels,
		abi::DirectXPixelFormat pixelFormat,
		abi::DirectXAlphaMode alphaMode,
		abi::ICompositionDrawingSurface** result
	);
	HRESULT MyICompositionSurfaceBrush2_put_Offset(
		abi::ICompositionSurfaceBrush2* This,
		abi::Vector2 value
	);
	HRESULT MyCDWriteText_ValidateVisual(uDWM::CDWriteText* This);
	HRESULT MyCDWriteText_UpdateOffset(uDWM::CDWriteText* This);
	HRESULT MyCDWriteText_SendSetSize(uDWM::CDWriteText* This, const SIZE* size);
	HRESULT MyCDWriteText_InitializeVisualTreeClone(uDWM::CDWriteText* This, uDWM::CDWriteText* clonedVisual, UINT cloneOption);
	HRESULT MyCDWriteText_scalar_deleting_destructor(uDWM::CDWriteText* This, BYTE flag);

	decltype(&MyID2D1DeviceContext_DrawTextLayout) g_ID2D1DeviceContext_DrawTextLayout_Org{ nullptr };
	decltype(&MyID2D1DeviceContext_DrawTextLayout)* g_ID2D1DeviceContext_DrawTextLayout_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyID2D1DeviceContext_DrawTextLayout> g_ID2D1DeviceContext_DrawTextLayout_Hook;
	decltype(&MyICompositionGraphicsDevice_CreateDrawingSurface) g_ICompositionGraphicsDevice_CreateDrawingSurface_Org{ nullptr };
	decltype(&MyICompositionGraphicsDevice_CreateDrawingSurface)* g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyICompositionGraphicsDevice_CreateDrawingSurface> g_ICompositionGraphicsDevice_CreateDrawingSurface_Hook;
	decltype(&MyICompositionSurfaceBrush2_put_Offset) g_ICompositionSurfaceBrush2_put_Offset_Org{ nullptr };
	decltype(&MyICompositionSurfaceBrush2_put_Offset)* g_ICompositionSurfaceBrush2_put_Offset_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyICompositionSurfaceBrush2_put_Offset> g_ICompositionSurfaceBrush2_put_Offset_Hook;
	Projection::Detour<uDWM::Symbol_CDWriteText_ValidateVisual, &MyCDWriteText_ValidateVisual> g_CDWriteText_ValidateVisual_Org{};
	decltype(&MyCDWriteText_UpdateOffset) g_CDWriteText_UpdateOffset_Org{ nullptr };
	decltype(&MyCDWriteText_UpdateOffset)* g_CDWriteText_UpdateOffset_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCDWriteText_UpdateOffset> g_CDWriteText_UpdateOffset_Hook;
	decltype(&MyCDWriteText_SendSetSize) g_CDWriteText_SendSetSize_Org{ nullptr };
	decltype(&MyCDWriteText_SendSetSize)* g_CDWriteText_SendSetSize_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCDWriteText_SendSetSize> g_CDWriteText_SendSetSize_Hook;
	Projection::Detour<uDWM::Symbol_CDWriteText_InitializeVisualTreeClone, &MyCDWriteText_InitializeVisualTreeClone> g_CDWriteText_InitializeVisualTreeClone_Org{};
	decltype(&MyCDWriteText_scalar_deleting_destructor) g_CDWriteText_scalar_deleting_destructor_Org{ nullptr };
	decltype(&MyCDWriteText_scalar_deleting_destructor)* g_CDWriteText_scalar_deleting_destructor_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCDWriteText_scalar_deleting_destructor> g_CDWriteText_scalar_deleting_destructor_Hook;

	uDWM::CDWriteText* g_dwriteTextVisual;
	uDWM::CTopLevelWindow* g_window{ nullptr };

	bool g_isTrimmed{ false };
	// original sizes, no glow included
	static union
	{
		SIZE g_textSize;
		abi::Size g_textSizeF;
	};
	COLORREF g_captionActiveColor{};
	COLORREF g_captionInactiveColor{};
	COLORREF g_captionActiveColorMaximized{};
	COLORREF g_captionInactiveColorMaximized{};

	CaptionText::WindowStateMap g_textVisualStateMap{};

	winrt::com_ptr<ID2D1DCRenderTarget> g_textGlowRT{};
	winrt::com_ptr<ID2D1Bitmap1> g_textGlowD2DBitmap{};
	winrt::com_ptr<ID2D1Effect> g_textGlowEffect{};
	winrt::com_ptr<ID2D1Effect> g_textMorphologyEffect{};


	CaptionText::TextGlowParameters g_textGlowParameters{};
	int g_centerCaption{ 0 };

}

void CaptionTextHandler::MyID2D1DeviceContext_DrawTextLayout(
	ID2D1DeviceContext* This,
	D2D1_POINT_2F origin,
	IDWriteTextLayout* textLayout,
	ID2D1Brush* defaultFillBrush,
	D2D1_DRAW_TEXT_OPTIONS options
)
{
	if (!g_dwriteTextVisual)
	{
		return g_ID2D1DeviceContext_DrawTextLayout_Org(
			This,
			origin,
			textLayout,
			defaultFillBrush,
			options
		);
	}

	winrt::com_ptr<ID2D1SolidColorBrush> solidColorBrush{};
	if (FAILED(defaultFillBrush->QueryInterface(solidColorBrush.put())))
	{
		return g_ID2D1DeviceContext_DrawTextLayout_Org(
			This,
			origin,
			textLayout,
			defaultFillBrush,
			options
		);
	}
	const auto color = solidColorBrush->GetColor();
	const auto cleanup = wil::scope_exit([&]
	{
		solidColorBrush->SetColor(color);
	});

	const auto& windowState = CaptionText::GetWindowState(g_textVisualStateMap, g_dwriteTextVisual);
	const auto textColorOverride = windowState.active ? (windowState.maximized ? g_captionActiveColorMaximized : g_captionActiveColor) : (windowState.maximized ? g_captionInactiveColorMaximized : g_captionInactiveColor);
	if (textColorOverride != 0xFFFFFFFF)
	{
		solidColorBrush->SetColor(Color::FromAbgr(textColorOverride));
	}

	DWRITE_LINE_METRICS lineMetrics{};
	UINT32 lineCount{};
	THROW_IF_FAILED(
		textLayout->GetLineMetrics(
			&lineMetrics,
			1,
			&lineCount
		)
	);
	g_isTrimmed = lineMetrics.isTrimmed;

	if (!g_textGlowParameters.size)
	{
		return g_ID2D1DeviceContext_DrawTextLayout_Org(
			This,
			origin,
			textLayout,
			defaultFillBrush,
			options
		);
	}

	origin.x += g_textGlowParameters.size;
	origin.y += g_textGlowParameters.size;

	DWRITE_TEXT_METRICS metrics{};
	THROW_IF_FAILED(
		textLayout->GetMetrics(
			&metrics
		)
	);

	if (!metrics.width || !metrics.height)
	{
		return g_ID2D1DeviceContext_DrawTextLayout_Org(
			This,
			origin,
			textLayout,
			defaultFillBrush,
			options
		);
	}

	if (LOWORD(Shared::g_textGlowMode) == 3 && g_textGlowParameters.intensity)
	{
		DWRITE_OVERHANG_METRICS overhangs{};
		THROW_IF_FAILED(textLayout->GetOverhangMetrics(&overhangs));
		const auto textBounds = CaptionText::CalculateDWriteBitmapBounds(metrics, overhangs);
		const D2D1_POINT_2F bitmapOrigin{ origin.x + textBounds.left, origin.y + textBounds.top };

		winrt::com_ptr<ID2D1BitmapRenderTarget> bitmapRT{};
		THROW_IF_FAILED(
			This->CreateCompatibleRenderTarget(
				D2D1::SizeF(
					textBounds.right - textBounds.left,
					textBounds.bottom - textBounds.top
				),
				bitmapRT.put()
			)
		);

		bitmapRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
		bitmapRT->BeginDraw();
		bitmapRT->Clear();
		bitmapRT->DrawTextLayout(
			D2D1::Point2F(-textBounds.left, -textBounds.top),
			textLayout,
			defaultFillBrush,
			options
		);
		THROW_IF_FAILED(bitmapRT->EndDraw());

		winrt::com_ptr<ID2D1Bitmap> bitmap{};
		THROW_IF_FAILED(bitmapRT->GetBitmap(bitmap.put()));

		if (!g_textMorphologyEffect)
		{
			THROW_IF_FAILED(
				This->CreateEffect(
					CLSID_D2D1Morphology,
					g_textMorphologyEffect.put()
				)
			);
			THROW_IF_FAILED(
				g_textMorphologyEffect->SetValue(
					D2D1_MORPHOLOGY_PROP_MODE,
					D2D1_MORPHOLOGY_MODE_DILATE
				)
			);
		}
		THROW_IF_FAILED(
			g_textMorphologyEffect->SetValue(
				D2D1_MORPHOLOGY_PROP_WIDTH,
				3 + g_textGlowParameters.size / 12
			)
		);
		THROW_IF_FAILED(
			g_textMorphologyEffect->SetValue(
				D2D1_MORPHOLOGY_PROP_HEIGHT,
				3 + g_textGlowParameters.size / 12
			)
		);
		if (!g_textGlowEffect)
		{
			THROW_IF_FAILED(
				This->CreateEffect(
					CLSID_D2D1Shadow,
					g_textGlowEffect.put()
				)
			);
			THROW_IF_FAILED(
				g_textGlowEffect->SetValue(
					D2D1_SHADOW_PROP_OPTIMIZATION,
					D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED
				)
			);
			g_textGlowEffect->SetInputEffect(0, g_textMorphologyEffect.get());
		}
		THROW_IF_FAILED(
			g_textGlowEffect->SetValue(
				D2D1_SHADOW_PROP_COLOR,
				Color::FromAbgr(g_textGlowParameters.color | (std::min(g_textGlowParameters.intensity, 255) << 24), false)
			)
		);
		THROW_IF_FAILED(
			g_textGlowEffect->SetValue(
				D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION,
				std::max(
					0.f,
					(static_cast<float>(g_textGlowParameters.size)) / 3.f + 0.5f
				)
			)
		);
		g_textMorphologyEffect->SetInput(0, bitmap.get());

		// With no source rectangle, DrawImage preserves the effect's negative bounds.
		This->DrawImage(
			g_textGlowEffect.get(),
			&bitmapOrigin,
			nullptr,
			D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
			D2D1_COMPOSITE_MODE_SOURCE_COPY
		);
		This->DrawImage(
			bitmap.get(),
			&bitmapOrigin,
			nullptr,
			D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR
		);
		return;
	}
	if (Shared::g_textGlowMode == 1 || Shared::g_textGlowMode == 2)
	{
		if (!g_textGlowD2DBitmap)
		{
			THROW_IF_FAILED(
				This->CreateBitmap(
					D2D1::SizeU(
						Shared::g_textGlowBitmapInfo.bmiHeader.biWidth,
						-Shared::g_textGlowBitmapInfo.bmiHeader.biHeight
					),
					Shared::g_textGlowBitmapPixels,
					Shared::g_textGlowBitmapInfo.bmiHeader.biWidth * 4,
					D2D1::BitmapProperties1(
						D2D1_BITMAP_OPTIONS_NONE,
						D2D1::PixelFormat(
							DXGI_FORMAT_B8G8R8A8_UNORM,
							D2D1_ALPHA_MODE_PREMULTIPLIED
						)
					),
					g_textGlowD2DBitmap.put()
				)
			);
		}

		DWRITE_OVERHANG_METRICS overhangs{};
		THROW_IF_FAILED(
			textLayout->GetOverhangMetrics(
				&overhangs
			)
		);
		const D2D1_RECT_F textBoundingBox
		{
			origin.x + (std::floor(-overhangs.left) - 1.f),
			origin.y + std::floor(metrics.top) - 1.f,
			origin.x + (std::floor(-overhangs.left) - 1.f) + g_textSizeF.Width,
			origin.y + std::floor(metrics.top + metrics.height) + 1.f
		};
		D2D1_RECT_F glowRect
		{
			textBoundingBox.left - static_cast<float>(g_textGlowParameters.contentMargins.cxLeftWidth),
			textBoundingBox.top - static_cast<float>(g_textGlowParameters.contentMargins.cyTopHeight),
			textBoundingBox.right + static_cast<float>(g_textGlowParameters.contentMargins.cxRightWidth),
			textBoundingBox.bottom + static_cast<float>(g_textGlowParameters.contentMargins.cyBottomHeight)
		};

		const auto calcGlowClipRect = [&windowState](const D2D1_RECT_F& textRect, D2D1_RECT_F& glowClipRect, bool mirrored)
		{
			LONG offset = 0;
			offset += windowState.windowRectLeft;
			offset -= g_dwriteTextVisual->GetX();
			// DWrite glow clipping needs the same offset calculation as the text surface.
			offset -= CaptionText::CenterOffset(g_centerCaption, g_isTrimmed,
				static_cast<double>(g_dwriteTextVisual->GetWidth()),
				static_cast<double>(g_textSizeF.Width),
				static_cast<double>(g_dwriteTextVisual->GetX()),
				static_cast<double>(g_dwriteTextVisual->GetTransformParent()->GetWidth()),
				g_dwriteTextVisual->GetScale().width
			);
			if (!mirrored)
			{
				glowClipRect.left = std::max(
					glowClipRect.left,
					textRect.left +
					offset
				);
			}
			else
			{
				glowClipRect.right = std::min(
					glowClipRect.right,
					textRect.right -
					offset
				);
			}
		};
		auto glowClipRect = glowRect;
		calcGlowClipRect(textBoundingBox, glowClipRect, g_dwriteTextVisual->IsRTLMirrored());
		This->PushAxisAlignedClip(glowClipRect, D2D1_ANTIALIAS_MODE_ALIASED);
		const auto clipScope = wil::scope_exit([&]
		{
			This->PopAxisAlignedClip();
		});

		THROW_IF_FAILED(
			Util::DrawNineGridBitmap(
				This,
				g_textGlowD2DBitmap.get(),
				glowRect,
				g_textGlowParameters.sizingMargins,
				Shared::g_textGlowMode == 2 ? (windowState.active ? (windowState.maximized ? Shared::g_glowOpacityMaximized : Shared::g_glowOpacity) : (windowState.maximized ? Shared::g_glowOpacityInactiveMaximized : Shared::g_glowOpacityInactive)) : 1.f
			)
		);
		/*{
			winrt::com_ptr<ID2D1SolidColorBrush> brush{};
			THROW_IF_FAILED(This->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Red), brush.put()));
			This->DrawRectangle(
				glowRect,
				brush.get(),
				1.f,
				nullptr
			);
		}*/
	}

	return g_ID2D1DeviceContext_DrawTextLayout_Org(
		This,
		origin,
		textLayout,
		defaultFillBrush,
		options
	);
}

HRESULT CaptionTextHandler::MyICompositionGraphicsDevice_CreateDrawingSurface(
	abi::ICompositionGraphicsDevice* This,
	abi::Size sizePixels,
	abi::DirectXPixelFormat pixelFormat,
	abi::DirectXAlphaMode alphaMode,
	abi::ICompositionDrawingSurface** result
)
{
	if (g_dwriteTextVisual)
	{
		g_textSizeF = sizePixels;
		sizePixels.Width += g_textGlowParameters.size * 2;
		sizePixels.Height += g_textGlowParameters.size * 2;
	}
	return g_ICompositionGraphicsDevice_CreateDrawingSurface_Org(
		This,
		sizePixels,
		pixelFormat,
		alphaMode,
		result
	);
}

HRESULT CaptionTextHandler::MyICompositionSurfaceBrush2_put_Offset(
	abi::ICompositionSurfaceBrush2* This,
	abi::Vector2 value
)
{
	if (g_dwriteTextVisual)
	{
		value.Y -= g_textGlowParameters.size;
		value.X -= g_textGlowParameters.size;

		// offset, glowSize
		// 40, 17
		// 11, 17
		// CDWriteVisual is rtl mirrored, but CSpriteVisual is not rtl mirrored
		if (const auto& offset = g_dwriteTextVisual->GetOffset(); g_dwriteTextVisual->IsRTLMirrored())
		{
			if (offset.x > g_textGlowParameters.size)
			{
				value.X += g_textGlowParameters.size;
			}
			else
			{
				value.X += g_textGlowParameters.size + g_textGlowParameters.size - offset.x;
			}
		}
		else
		{
			value.X += offset.x - std::max(offset.x - g_textGlowParameters.size, 0l);
		}

		// Apply the same CenterCaption offset to the DWrite composition surface.
		const float offset = static_cast<float>(CaptionText::CenterOffset(g_centerCaption, g_isTrimmed,
			static_cast<double>(g_dwriteTextVisual->GetWidth()),
			static_cast<double>(g_textSizeF.Width),
			static_cast<double>(g_dwriteTextVisual->GetX()),
			static_cast<double>(g_dwriteTextVisual->GetTransformParent()->GetWidth()),
			g_dwriteTextVisual->GetScale().width
		));
		if (offset > 0.0f)
		{
			value.X += g_dwriteTextVisual->IsRTLMirrored() ? -offset : offset;
		}
	}
	return g_ICompositionSurfaceBrush2_put_Offset_Org(
		This,
		value
	);
}

HRESULT CaptionTextHandler::MyCDWriteText_ValidateVisual(uDWM::CDWriteText* This)
{
	// update offset
	This->SetDirtyFlags(0x4);
	if (!g_CDWriteText_scalar_deleting_destructor_Org)
	{
		g_CDWriteText_scalar_deleting_destructor_Org_Address = HookHelper::get_vftable_from<decltype(g_CDWriteText_scalar_deleting_destructor_Org)>(This);
		g_CDWriteText_scalar_deleting_destructor_Hook.AttachOnce(g_CDWriteText_scalar_deleting_destructor_Org_Address, &g_CDWriteText_scalar_deleting_destructor_Org);
	}
	if (!g_CDWriteText_UpdateOffset_Org)
	{
		const auto CVisual_UpdateOffset_Org = uDWM::Symbol_CVisual_UpdateOffset.get();
		const auto CSpriteVisual_SendSetSize_Org = uDWM::Symbol_CSpriteVisual_SendSetSize.get();

		for (auto& vf : std::span{ HookHelper::get_vftable_from(This), 32})
		{
			if (vf == CVisual_UpdateOffset_Org)
			{
				g_CDWriteText_UpdateOffset_Org_Address = reinterpret_cast<decltype(g_CDWriteText_UpdateOffset_Org_Address)>(&vf);
				g_CDWriteText_UpdateOffset_Hook.AttachOnce(g_CDWriteText_UpdateOffset_Org_Address, &g_CDWriteText_UpdateOffset_Org);
			}
			if (vf == CSpriteVisual_SendSetSize_Org)
			{
				g_CDWriteText_SendSetSize_Org_Address = reinterpret_cast<decltype(g_CDWriteText_SendSetSize_Org_Address)>(&vf);
				g_CDWriteText_SendSetSize_Hook.AttachOnce(g_CDWriteText_SendSetSize_Org_Address, &g_CDWriteText_SendSetSize_Org);
			}
		}
	}
	if (g_window = uDWM::TryGetWindowFromVisual(This); g_window && g_window->GetData())
	{
		const auto windowState = CaptionText::GetOrCreateWindowState(g_textVisualStateMap, This);
		if (windowState->sourceVisual == This)
		{
			windowState->active = g_window->TreatAsActiveWindow();
			windowState->maximized = g_window->TreatAsMaximized();

			RECT windowRect{};
			g_window->GetActualWindowRect(&windowRect, true, false, true);
			windowState->windowRectLeft = windowRect.left;
		}
	}
	g_dwriteTextVisual = This;
	const auto hr = g_CDWriteText_ValidateVisual_Org(This);
	g_dwriteTextVisual = nullptr;
	g_window = nullptr;

	return hr;
}

HRESULT CaptionTextHandler::MyCDWriteText_UpdateOffset(uDWM::CDWriteText* This)
{
	if (!g_textGlowParameters.size)
	{
		return g_CDWriteText_UpdateOffset_Org(This);
	}

	// SpriteVisual will crop what exceeds its bounding rectangle,
	// here we make it offset x minus the size of the glow,
	// and add it back later in the ICompositionSurfaceBrush2::put_Offset method
	//
	// This gives us enough space to render the glow.
	auto& offset = This->GetMutableOffset();
	const auto actualOffsetX = offset.x;
	if (!This->IsRTLMirrored())
	{
		offset.x = std::max(offset.x - g_textGlowParameters.size, 0l);
	}
	const auto hr = g_CDWriteText_UpdateOffset_Org(This);
	offset.x = actualOffsetX;

	return hr;
}

HRESULT CaptionTextHandler::MyCDWriteText_SendSetSize(uDWM::CDWriteText* This, const SIZE* size)
{
	if (!g_textGlowParameters.size)
	{
		return g_CDWriteText_SendSetSize_Org(This, size);
	}

	const auto hr = g_CDWriteText_SendSetSize_Org(This, size);
	// SpriteVisual will crop what exceeds its bounding rectangle,
	// expand it to ensure enough space to render the glow.
	auto& offset = This->GetMutableOffset();
	if (This->IsRTLMirrored())
	{
		This->GetVisualProxy()->SetSize(
			static_cast<double>(size->cx + offset.x - std::max(offset.x - g_textGlowParameters.size, 0l)),
			static_cast<double>(size->cy)
		);
	}
	else
	{
		This->GetVisualProxy()->SetSize(
			static_cast<double>(size->cx + offset.x - std::max(offset.x - g_textGlowParameters.size, 0l) + g_textGlowParameters.size),
			static_cast<double>(size->cy)
		);
	}

	return hr;
}

HRESULT CaptionTextHandler::MyCDWriteText_InitializeVisualTreeClone(uDWM::CDWriteText* This, uDWM::CDWriteText* clonedVisual, UINT cloneOption)
{
	CaptionText::ShareWindowState(g_textVisualStateMap, This, clonedVisual);
	const auto result = g_CDWriteText_InitializeVisualTreeClone_Org(This, clonedVisual, cloneOption);
	if (FAILED(result))
	{
		g_textVisualStateMap.erase(clonedVisual);
	}
	return result;
}

HRESULT CaptionTextHandler::MyCDWriteText_scalar_deleting_destructor(uDWM::CDWriteText* This, BYTE flag)
{
	g_textVisualStateMap.erase(This);
	return g_CDWriteText_scalar_deleting_destructor_Org(This, flag);
}

void CaptionTextHandler::DestroyDeviceResources()
{
	g_textGlowRT = nullptr;
	g_textGlowD2DBitmap = nullptr;

	if (uDWM::g_versionInfo.build < os::build_w11_22h2)
	{
		return;
	}
	g_textGlowEffect = nullptr;
	g_textMorphologyEffect = nullptr;
}

void CaptionTextHandler::Update(GlassEngine::UpdateType type)
{
	if (type & GlassEngine::UpdateType::Theme)
	{
		g_textGlowD2DBitmap = nullptr;
		CaptionText::CalculateRealizedTextGlowParams(Shared::g_textGlowMode, g_textGlowParameters);
	}
	if (type & GlassEngine::UpdateType::Backdrop || type & GlassEngine::UpdateType::Theme)
	{
		g_centerCaption = std::clamp(static_cast<int>(GlassEngine::GetDwordFromRegistry(L"CenterCaption", FALSE)), 0, 2);
		g_captionActiveColor = GlassEngine::GetDwordFromRegistry(L"ColorizationColorCaption", 0xFFFFFFFD);
		g_captionInactiveColor = GlassEngine::GetDwordFromRegistry(L"ColorizationColorCaptionInactive", g_captionActiveColor);
		g_captionActiveColorMaximized = GlassEngine::GetDwordFromRegistry(L"ColorizationColorCaptionMaximized", g_captionActiveColor);
		g_captionInactiveColorMaximized = GlassEngine::GetDwordFromRegistry(L"ColorizationColorCaptionInactiveMaximized", g_captionInactiveColor);

		const auto themeHandle = CustomThemeAtlasLoader::GetThemeHandle();
		if (themeHandle)
		{
			if (g_captionActiveColor == 0xFFFFFFFE)
			{
				CustomThemeAtlasLoader::MyGetThemeColor(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 1, TMT_TEXTCOLOR, &g_captionActiveColor);
			}
			else if (g_captionActiveColor == 0xFFFFFFFD)
			{
				g_captionActiveColor = RGB(0, 0, 0);
			}
			if (g_captionInactiveColor == 0xFFFFFFFE)
			{
				CustomThemeAtlasLoader::MyGetThemeColor(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 2, TMT_TEXTCOLOR, &g_captionInactiveColor);
			}
			else if (g_captionInactiveColor == 0xFFFFFFFD)
			{
				g_captionInactiveColor = RGB(0, 0, 0);
			}
			if (g_captionActiveColorMaximized == 0xFFFFFFFE)
			{
				CustomThemeAtlasLoader::MyGetThemeColor(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 3, TMT_TEXTCOLOR, &g_captionActiveColorMaximized);
			}
			else if (g_captionActiveColorMaximized == 0xFFFFFFFD)
			{
				if (Shared::g_type == Shared::GlassType::Aero)
				{
					g_captionActiveColorMaximized = RGB(0, 0, 0);
				}
				else
				{
					g_captionActiveColorMaximized = RGB(255, 255, 255);
				}
			}
			if (g_captionInactiveColorMaximized == 0xFFFFFFFE)
			{
				CustomThemeAtlasLoader::MyGetThemeColor(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 4, TMT_TEXTCOLOR, &g_captionInactiveColorMaximized);
			}
			else if (g_captionInactiveColorMaximized == 0xFFFFFFFD)
			{
				if (Shared::g_type == Shared::GlassType::Aero)
				{
					g_captionInactiveColorMaximized = RGB(0, 0, 0);
				}
				else
				{
					g_captionInactiveColorMaximized = RGB(255, 255, 255);
				}
			}

			int value;

			value = 100;
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 1, TMT_OPACITY, &value);
			Shared::g_glowOpacity = value / 100.f;

			value = 100;
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 2, TMT_OPACITY, &value);
			Shared::g_glowOpacityInactive = value / 100.f;

			value = 100;
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 3, TMT_OPACITY, &value);
			Shared::g_glowOpacityMaximized = value / 100.f;

			value = 100;
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 4, TMT_OPACITY, &value);
			Shared::g_glowOpacityInactiveMaximized = value / 100.f;
		}
	}
}

void CaptionTextHandler::Startup()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_CaptionTextHandler))
	{
		return;
	}

	winrt::com_ptr<ID2D1DeviceContext> context{};
	FAIL_FAST_IF_FAILED_MSG(
		uDWM::CDesktopManager::GetInstance()->GetD2DDevice()->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			context.put()
		),
		"Unable to create the caption D2D device context"
	);

	g_ID2D1DeviceContext_DrawTextLayout_Org_Address = &HookHelper::get_vftable_from<decltype(g_ID2D1DeviceContext_DrawTextLayout_Org)>(context.get())[28];
	g_ID2D1DeviceContext_DrawTextLayout_Hook.Prepare(g_ID2D1DeviceContext_DrawTextLayout_Org_Address, &g_ID2D1DeviceContext_DrawTextLayout_Org);
	HookHelper::GetCurrentHookTransaction().Apply(g_ID2D1DeviceContext_DrawTextLayout_Hook);

	winrt::com_ptr<IDCompositionDesktopDevicePartner6> dcompDevicePartner{ nullptr };
	FAIL_FAST_IF_FAILED_MSG(uDWM::CDesktopManager::GetInstance()->GetInteropCompositorDCompDevicePartner()->QueryInterface(dcompDevicePartner.put()), "Unable to obtain IDCompositionDesktopDevicePartner");
	winrt::com_ptr<abi::ICompositionGraphicsDevice> graphicsDevice{ nullptr };

	winrt::com_ptr<abi::ICompositor> compositor{};
	FAIL_FAST_IF_FAILED_MSG(dcompDevicePartner->QueryInterface(compositor.put()), "Unable to obtain ICompositor");

	winrt::com_ptr<abi::ICompositorInterop> compositorInterop{};
	FAIL_FAST_IF_FAILED_MSG(compositor->QueryInterface(compositorInterop.put()), "Unable to obtain ICompositorInterop");
	FAIL_FAST_IF_FAILED_MSG(
		compositorInterop->CreateGraphicsDevice(
			uDWM::CDesktopManager::GetInstance()->GetD2DDevice(),
			graphicsDevice.put()
		),
		"Unable to create the caption composition graphics device"
	);

	g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address = &HookHelper::get_vftable_from<decltype(g_ICompositionGraphicsDevice_CreateDrawingSurface_Org)>(graphicsDevice.get())[6];
	g_ICompositionGraphicsDevice_CreateDrawingSurface_Hook.Prepare(g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address, &g_ICompositionGraphicsDevice_CreateDrawingSurface_Org);
	HookHelper::GetCurrentHookTransaction().Apply(g_ICompositionGraphicsDevice_CreateDrawingSurface_Hook);

	winrt::com_ptr<abi::ICompositionSurfaceBrush> surfaceBrush{ nullptr };
	FAIL_FAST_IF_FAILED_MSG(compositor->CreateSurfaceBrush(surfaceBrush.put()), "Unable to create the caption surface brush");

	winrt::com_ptr<abi::ICompositionSurfaceBrush2> surfaceBrush2{ nullptr };
	FAIL_FAST_IF_FAILED_MSG(surfaceBrush->QueryInterface(surfaceBrush2.put()), "Unable to obtain ICompositionSurfaceBrush2");
	g_ICompositionSurfaceBrush2_put_Offset_Org_Address = &HookHelper::get_vftable_from<decltype(g_ICompositionSurfaceBrush2_put_Offset_Org)>(surfaceBrush2.get())[11];
	g_ICompositionSurfaceBrush2_put_Offset_Hook.Prepare(g_ICompositionSurfaceBrush2_put_Offset_Org_Address, &g_ICompositionSurfaceBrush2_put_Offset_Org);
	HookHelper::GetCurrentHookTransaction().Apply(g_ICompositionSurfaceBrush2_put_Offset_Hook);

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CDWriteText_ValidateVisual_Org },
			{ &g_CDWriteText_InitializeVisualTreeClone_Org }
		}
	);
}
void CaptionTextHandler::Shutdown()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_CaptionTextHandler))
	{
		return;
	}

	if (g_CDWriteText_scalar_deleting_destructor_Org)
	{
		HookHelper::GetCurrentHookTransaction().Apply(g_CDWriteText_scalar_deleting_destructor_Hook);
	}
	if (g_CDWriteText_UpdateOffset_Org)
	{
		HookHelper::GetCurrentHookTransaction().Apply(g_CDWriteText_UpdateOffset_Hook);
	}
	if (g_CDWriteText_SendSetSize_Org)
	{
		HookHelper::GetCurrentHookTransaction().Apply(g_CDWriteText_SendSetSize_Hook);
	}

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CDWriteText_ValidateVisual_Org },
			{ &g_CDWriteText_InitializeVisualTreeClone_Org }
		}
	);

	HookHelper::GetCurrentHookTransaction().Apply(g_ID2D1DeviceContext_DrawTextLayout_Hook);
	HookHelper::GetCurrentHookTransaction().Apply(g_ICompositionGraphicsDevice_CreateDrawingSurface_Hook);
	HookHelper::GetCurrentHookTransaction().Apply(g_ICompositionSurfaceBrush2_put_Offset_Hook);

}

void CaptionTextHandler::Cleanup()
{
	g_dwriteTextVisual = nullptr;
	DestroyDeviceResources();
	g_textSize = {};
	g_textVisualStateMap.clear();
}
