#include "pch.h"
#include "MsstyleInternals.hpp"
#include "CaptionTextHandler.hpp"
#include "Shared.hpp"
#include "GlassKernel.hpp"
#include "Util.hpp"
#include "uDWMProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "dcompPrivates.hpp"
#include "CustomThemeAtlasLoader.hpp"

using namespace OpenGlass;
namespace OpenGlass::CaptionTextHandler
{
	int WINAPI MyDrawTextW(
		HDC hdc,
		LPCWSTR lpchText,
		int cchText,
		LPRECT lprc,
		UINT format
	);
	HBITMAP WINAPI MyCreateBitmap(
		int nWidth,
		int nHeight,
		UINT nPlanes,
		UINT nBitCount,
		const void* lpBits
	);
	HRESULT MyIWICImagingFactory2_CreateBitmapFromHBITMAP(
		IWICImagingFactory2* This,
		HBITMAP hBitmap,
		HPALETTE hPalette,
		WICBitmapAlphaChannelOption options,
		IWICBitmap** ppIBitmap
	);
	HRESULT MyCText_ValidateResources(uDWM::CText* This);
	HRESULT MyCText_InitializeVisualTreeClone(uDWM::CText* This, uDWM::CText* clonedVisual, UINT cloneOption);
	HRESULT MyCText_CloneVisualTree(uDWM::CText* This, uDWM::CText** clonedVisual, bool unknown1, bool unknown2, bool unknown3);
	HRESULT MyCText_scalar_deleting_destructor(uDWM::CText* This, BYTE flag);
	HRESULT MyCChannel_MatrixTransformUpdate(dwmcore::CChannel* This, UINT handleId, MilMatrix3x2D* matrix);

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
	HRESULT MyCDWriteText_SetSize(uDWM::CDWriteText* This, const SIZE* size);
	HRESULT MyCDWriteText_InitializeVisualTreeClone(uDWM::CDWriteText* This, uDWM::CDWriteText* clonedVisual, UINT cloneOption);
	HRESULT MyCDWriteText_scalar_deleting_destructor(uDWM::CDWriteText* This, BYTE flag);

	decltype(&MyDrawTextW) g_DrawTextW_Org{ nullptr };
	decltype(&MyCreateBitmap) g_CreateBitmap_Org{ nullptr };
	HookHelper::ImportHook g_textImportHooks;
	decltype(&MyIWICImagingFactory2_CreateBitmapFromHBITMAP) g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org{ nullptr };
	Projection::Detour<uDWM::Symbol_CText_ValidateResources, decltype(&MyCText_ValidateResources)> g_CText_ValidateResources_Org{};
	Projection::Detour<uDWM::Symbol_CText_InitializeVisualTreeClone, decltype(&MyCText_InitializeVisualTreeClone)> g_CText_InitializeVisualTreeClone_Org{};
	Projection::Detour<uDWM::Symbol_CText_CloneVisualTree, decltype(&MyCText_CloneVisualTree)> g_CText_CloneVisualTree_Org{};
	decltype(&MyCText_scalar_deleting_destructor) g_CText_scalar_deleting_destructor_Org{ nullptr };
	decltype(&MyCText_scalar_deleting_destructor)* g_CText_scalar_deleting_destructor_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCText_scalar_deleting_destructor> g_CText_scalar_deleting_destructor_Hook;
	Projection::Detour<dwmcore::Symbol_CChannel_MatrixTransformUpdate, decltype(&MyCChannel_MatrixTransformUpdate)> g_CChannel_MatrixTransformUpdate_Org{};
	decltype(&MyIWICImagingFactory2_CreateBitmapFromHBITMAP)* g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyIWICImagingFactory2_CreateBitmapFromHBITMAP> g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Hook;

	decltype(&MyID2D1DeviceContext_DrawTextLayout) g_ID2D1DeviceContext_DrawTextLayout_Org{ nullptr };
	decltype(&MyID2D1DeviceContext_DrawTextLayout)* g_ID2D1DeviceContext_DrawTextLayout_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyID2D1DeviceContext_DrawTextLayout> g_ID2D1DeviceContext_DrawTextLayout_Hook;
	decltype(&MyICompositionGraphicsDevice_CreateDrawingSurface) g_ICompositionGraphicsDevice_CreateDrawingSurface_Org{ nullptr };
	decltype(&MyICompositionGraphicsDevice_CreateDrawingSurface)* g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyICompositionGraphicsDevice_CreateDrawingSurface> g_ICompositionGraphicsDevice_CreateDrawingSurface_Hook;
	decltype(&MyICompositionSurfaceBrush2_put_Offset) g_ICompositionSurfaceBrush2_put_Offset_Org{ nullptr };
	decltype(&MyICompositionSurfaceBrush2_put_Offset)* g_ICompositionSurfaceBrush2_put_Offset_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyICompositionSurfaceBrush2_put_Offset> g_ICompositionSurfaceBrush2_put_Offset_Hook;
	Projection::Detour<uDWM::Symbol_CDWriteText_ValidateVisual, decltype(&MyCDWriteText_ValidateVisual)> g_CDWriteText_ValidateVisual_Org{};
	decltype(&MyCDWriteText_UpdateOffset) g_CDWriteText_UpdateOffset_Org{ nullptr };
	decltype(&MyCDWriteText_UpdateOffset)* g_CDWriteText_UpdateOffset_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCDWriteText_UpdateOffset> g_CDWriteText_UpdateOffset_Hook;
	decltype(&MyCDWriteText_SetSize) g_CDWriteText_SetSize_Org{ nullptr };
	decltype(&MyCDWriteText_SetSize)* g_CDWriteText_SetSize_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCDWriteText_SetSize> g_CDWriteText_SetSize_Hook;
	Projection::Detour<uDWM::Symbol_CDWriteText_InitializeVisualTreeClone, decltype(&MyCDWriteText_InitializeVisualTreeClone)> g_CDWriteText_InitializeVisualTreeClone_Org{};
	decltype(&MyCDWriteText_scalar_deleting_destructor) g_CDWriteText_scalar_deleting_destructor_Org{ nullptr };
	decltype(&MyCDWriteText_scalar_deleting_destructor)* g_CDWriteText_scalar_deleting_destructor_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCDWriteText_scalar_deleting_destructor> g_CDWriteText_scalar_deleting_destructor_Hook;


	static union
	{
		uDWM::CText* g_textVisual;
		uDWM::CDWriteText* g_dwriteTextVisual;
	};
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
	MARGINS g_contentMargins{}, g_sizingMargins{};

	struct CWindowState : Util::ComRefCounted<CWindowState>
	{
		uDWM::CVisual* sourceVisual{};
		bool active{};
		bool maximized{};
		LONG windowRectLeft{};
	};
	using CWindowStatePtr = winrt::com_ptr<CWindowState>;
	std::unordered_map<uDWM::CVisual*, CWindowStatePtr> g_textVisualStateMap{};

	CWindowStatePtr GetOrCreateWindowState(uDWM::CVisual* visual)
	{
		auto& state = g_textVisualStateMap[visual];
		if (!state)
		{
			state = Util::make_com_ptr<CWindowState>();
			state->sourceVisual = visual;
		}

		return state;
	}

	const CWindowState& GetWindowState(uDWM::CVisual* visual)
	{
		static const CWindowState defaultState{};
		const auto it = g_textVisualStateMap.find(visual);
		return it == g_textVisualStateMap.end() || !it->second ? defaultState : *it->second;
	}

	void ShareWindowState(uDWM::CVisual* source, uDWM::CVisual* clone)
	{
		if (source && clone)
		{
			// The source remains authoritative; preview containers must not replace its active state.
			g_textVisualStateMap[clone] = GetOrCreateWindowState(source);
		}
	}
	winrt::com_ptr<ID2D1DCRenderTarget> g_textGlowRT{};
	winrt::com_ptr<ID2D1Bitmap1> g_textGlowD2DBitmap{};
	winrt::com_ptr<ID2D1Effect> g_textGlowEffect{};
	winrt::com_ptr<ID2D1Effect> g_textMorphologyEffect{};

	COLORREF g_textGlowColor{};

	int g_textGlowSize{};
	int g_textGlowIntensity{};
	int g_centerCaption{ 0 };
	int CaptionCenterOffset(double visualWidth, double textWidth, double visualX, double parentWidth, double scale)
	{
		const int localOffset = std::max(
			static_cast<int>((visualWidth - textWidth) / 2.0),
			0
		);
		if (g_centerCaption <= 0 || g_isTrimmed)
			return 0;

		if (g_centerCaption == 1)
			return localOffset;

		// Win8 centers against the parent visual and uses a small scale-adjusted
		// guard band before falling back to the local visual width.
		int parentOffset = static_cast<int>((parentWidth - textWidth) / 2.0) - static_cast<int>(visualX);
		if (static_cast<double>(parentOffset + static_cast<int>(textWidth)) + scale * 5.0 > visualWidth)
			parentOffset = localOffset;

		return std::max(parentOffset, 0);
	}


	void CalculateRealizedTextGlowParams(int textGlowMode);
}

int WINAPI CaptionTextHandler::MyDrawTextW(
	HDC hdc,
	LPCWSTR lpchText,
	int cchText,
	LPRECT lprc,
	UINT format
)
{
	int result{ 0 };
	auto drawTextCallback = [](HDC hdc, LPWSTR pszText, int cchText, LPRECT prc, UINT dwFlags, LPARAM lParam) static -> int
	{
		return *reinterpret_cast<int*>(lParam) = g_DrawTextW_Org(hdc, pszText, cchText, prc, dwFlags);
	};

	if ((format & DT_CALCRECT))
	{
		result = g_DrawTextW_Org(hdc, lpchText, cchText, lprc, format);

		if ((format & DT_END_ELLIPSIS) != 0)
		{
			RECT rawTextRect{};
			g_DrawTextW_Org(hdc, lpchText, cchText, &rawTextRect, DT_CALCRECT | DT_NOPREFIX | DT_SINGLELINE);
			g_isTrimmed = wil::rect_width(*lprc) < wil::rect_width(rawTextRect);
		}

		return result;
	}
	// clear the background, so the text can be shown transparent
	// with this hack, we don't need to hook FillRect any more
	BITMAP bmp{};
	if (GetObjectW(GetCurrentObject(hdc, OBJ_BITMAP), sizeof(bmp), &bmp) && bmp.bmBits)
	{
		memset(bmp.bmBits, 0, 4 * bmp.bmWidth * bmp.bmHeight);
	}

	OffsetRect(lprc, g_textGlowSize, g_textGlowSize);

	const auto& windowState = GetWindowState(g_textVisual);
	const auto textColor = GetTextColor(hdc);
	const auto textColorOverride = windowState.active ? (windowState.maximized ? g_captionActiveColorMaximized : g_captionActiveColor) : (windowState.maximized ? g_captionInactiveColorMaximized : g_captionInactiveColor);
	DTTOPTS options
	{
		sizeof(DTTOPTS),
		DTT_TEXTCOLOR | DTT_COMPOSITED | DTT_CALLBACK | DTT_GLOWSIZE,
		textColorOverride != 0xFFFFFFFF ? textColorOverride : textColor,
		0,
		0,
		0,
		{},
		0,
		0,
		0,
		0,
		FALSE,
		0,
		drawTextCallback,
		(LPARAM)&result
	};

	auto glowDrawRect = *lprc;

	if (Shared::g_textGlowMode == 1 || Shared::g_textGlowMode == 2)
	{
		glowDrawRect.left -= g_contentMargins.cxLeftWidth;
		glowDrawRect.top -= g_contentMargins.cyTopHeight;
		glowDrawRect.right += g_contentMargins.cxRightWidth;
		glowDrawRect.bottom += g_contentMargins.cyBottomHeight;
	}
	else if (LOWORD(Shared::g_textGlowMode) == 3)
	{
		options.iGlowSize = g_textGlowSize;
		glowDrawRect.left -= g_textGlowSize;
		glowDrawRect.top -= g_textGlowSize;
		glowDrawRect.right += g_textGlowSize;
		glowDrawRect.bottom += g_textGlowSize;
	}

	const auto calcGlowClipRect = [&windowState](LPCRECT lprc, RECT& glowClipRect, bool mirrored)
	{
		LONG offset = 0;
		offset += windowState.windowRectLeft;
		offset -= g_textVisual->GetX();
		// Keep the glow clip rect aligned with the same centering offset as the text.
		offset -= CaptionCenterOffset(
			static_cast<DOUBLE>(g_textVisual->GetWidth()),
			static_cast<DOUBLE>(g_textSize.cx),
			static_cast<DOUBLE>(g_textVisual->GetX()),
			static_cast<DOUBLE>(g_textVisual->GetTransformParent()->GetWidth()),
			g_textVisual->GetScale().width
		);
		if (!mirrored)
		{
			glowClipRect.left = std::max(
				glowClipRect.left,
				lprc->left +
				offset
			);
		}
		else
		{
			glowClipRect.right = std::min(
				glowClipRect.right,
				lprc->right -
				offset
			);
		}
	};

	auto glowClipRect = glowDrawRect;
	calcGlowClipRect(lprc, glowClipRect, g_textVisual->IsRTLMirrored());

	SaveDC(hdc);
	const auto dcPaintScope = wil::scope_exit([hdc]
	{
		RestoreDC(hdc, -1);
	});
	IntersectClipRect(hdc, glowClipRect.left, glowClipRect.top, glowClipRect.right, glowClipRect.bottom);

	if (Shared::g_textGlowMode == 1 || Shared::g_textGlowMode == 2)
	{
		if (!g_textGlowRT)
		{
			winrt::com_ptr<ID2D1Factory> factory{};
			uDWM::CDesktopManager::GetInstance()->GetD2DDevice()->GetFactory(factory.put());
			D2D1_RENDER_TARGET_PROPERTIES properties = D2D1::RenderTargetProperties(
				D2D1_RENDER_TARGET_TYPE_SOFTWARE,
				D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
			);
			THROW_IF_FAILED(
				factory->CreateDCRenderTarget(
					&properties,
					g_textGlowRT.put()
				)
			);
		}
		winrt::com_ptr<ID2D1DeviceContext> context{};
		THROW_IF_FAILED(g_textGlowRT->QueryInterface(context.put()));
		if (!g_textGlowD2DBitmap)
		{
			THROW_IF_FAILED(
				context->CreateBitmap(
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

		RECT targetRect
		{
			lprc->left - g_textGlowSize,
			lprc->top - g_textGlowSize,
			lprc->right + g_textGlowSize,
			lprc->bottom + g_textGlowSize
		};
		g_textGlowRT->BindDC(hdc, &targetRect);
		g_textGlowRT->BeginDraw();
		Util::DrawNineGridBitmap(
			context.get(),
			g_textGlowD2DBitmap.get(),
			D2D1::RectF(
				static_cast<float>(glowDrawRect.left),
				static_cast<float>(glowDrawRect.top),
				static_cast<float>(glowDrawRect.right),
				static_cast<float>(glowDrawRect.bottom)
			),
			g_sizingMargins,
			Shared::g_textGlowMode == 2 ? (windowState.active ? (windowState.maximized ? Shared::g_glowOpacityMaximized : Shared::g_glowOpacity) : (windowState.maximized ? Shared::g_glowOpacityInactiveMaximized : Shared::g_glowOpacityInactive)) : 1.f
		);
		LOG_IF_FAILED(g_textGlowRT->EndDraw());
		/*{
			FrameRect(
				hdc,
				&glowClipRect,
				GetStockBrush(WHITE_BRUSH)
			);
		}*/
	}

	wil::unique_htheme hTheme{ OpenThemeData(nullptr, L"CompositedWindow::Window") };
	if (hTheme)
	{
		THROW_IF_FAILED(
			DrawThemeTextEx(
				hTheme.get(),
				hdc,
				0,
				0,
				lpchText,
				cchText,
				format,
				lprc,
				&options
			)
		);
	}
	else
	{
		THROW_HR_IF_NULL(E_FAIL, hTheme);
		result = g_DrawTextW_Org(hdc, lpchText, cchText, lprc, format);
	}

	// override that so we can use the correct param in CDrawImageInstruction::Create
	lprc->left -= g_textGlowSize;
	lprc->top -= g_textGlowSize;
	lprc->right += g_textGlowSize;
	lprc->bottom += g_textGlowSize;

	return result;
}
HBITMAP WINAPI CaptionTextHandler::MyCreateBitmap(
	int nWidth,
	int nHeight,
	[[maybe_unused]] UINT nPlanes,
	[[maybe_unused]] UINT nBitCount,
	[[maybe_unused]] const void* lpBits
)
{
	if (!g_textVisual)
	{
		return g_CreateBitmap_Org(nWidth, nHeight, nPlanes, nBitCount, lpBits);
	}

	g_textSize = { nWidth, nHeight };
	nWidth += g_textGlowSize * 2;
	nHeight += g_textGlowSize * 2;

	PVOID bits{ nullptr };
	BITMAPINFO bitmapInfo{ {sizeof(bitmapInfo.bmiHeader), nWidth, -nHeight, 1, 32, BI_RGB} };
	HBITMAP bitmap{ CreateDIBSection(nullptr, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0) };
	memset(bits, 0, sizeof(nWidth * nHeight * 4));

	return bitmap;
}
HRESULT CaptionTextHandler::MyIWICImagingFactory2_CreateBitmapFromHBITMAP(
	IWICImagingFactory2* This,
	HBITMAP hBitmap,
	HPALETTE hPalette,
	[[maybe_unused]] WICBitmapAlphaChannelOption options,
	IWICBitmap** ppIBitmap
)
{
	return g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org(
		This,
		hBitmap,
		hPalette,
		WICBitmapAlphaChannelOption::WICBitmapUsePremultipliedAlpha,
		ppIBitmap
	);
}

HRESULT CaptionTextHandler::MyCText_ValidateResources(uDWM::CText* This)
{
	if (g_centerCaption)
	{
		// update alignment transform
		This->SetDirtyFlags(0x8000);
		// redraw the text to get the width and height
		This->SetDirtyFlags(0x1000);
	}
	if (!g_CText_scalar_deleting_destructor_Org)
	{
		g_CText_scalar_deleting_destructor_Org_Address = HookHelper::get_vftable_from<decltype(g_CText_scalar_deleting_destructor_Org)>(This);
		g_CText_scalar_deleting_destructor_Hook.AttachOnce(g_CText_scalar_deleting_destructor_Org_Address, &g_CText_scalar_deleting_destructor_Org);
	}
	if (g_window = uDWM::TryGetWindowFromVisual(This); g_window && g_window->GetData())
	{
		const auto windowState = GetOrCreateWindowState(This);
		if (windowState->sourceVisual == This)
		{
			windowState->active = g_window->TreatAsActiveWindow();
			windowState->maximized = g_window->TreatAsMaximized();

			RECT windowRect{};
			g_window->GetActualWindowRect(&windowRect, true, false, true);
			windowState->windowRectLeft = windowRect.left;
		}
	}
	g_textVisual = This;
	const auto hr = g_CText_ValidateResources_Org(This);
	g_textVisual = nullptr;
	g_window = nullptr;

	return hr;
}
HRESULT CaptionTextHandler::MyCText_InitializeVisualTreeClone(uDWM::CText* This, uDWM::CText* clonedVisual, UINT cloneOption)
{
	ShareWindowState(This, clonedVisual);
	const auto result = g_CText_InitializeVisualTreeClone_Org(This, clonedVisual, cloneOption);
	if (FAILED(result))
	{
		g_textVisualStateMap.erase(clonedVisual);
	}
	return result;
}
HRESULT CaptionTextHandler::MyCText_CloneVisualTree(uDWM::CText* This, uDWM::CText** clonedVisual, bool unknown1, bool unknown2, bool unknown3)
{
	const auto result = g_CText_CloneVisualTree_Org(This, clonedVisual, unknown1, unknown2, unknown3);
	if (SUCCEEDED(result) && clonedVisual && *clonedVisual)
	{
		ShareWindowState(This, *clonedVisual);
	}
	return result;
}
HRESULT CaptionTextHandler::MyCText_scalar_deleting_destructor(uDWM::CText* This, BYTE flag)
{
	g_textVisualStateMap.erase(This);
	return g_CText_scalar_deleting_destructor_Org(This, flag);
}
HRESULT CaptionTextHandler::MyCChannel_MatrixTransformUpdate(dwmcore::CChannel* This, UINT handleId, MilMatrix3x2D* matrix)
{
	if (g_textVisual)
	{
		matrix->DX -= static_cast<DOUBLE>(g_textGlowSize);
		matrix->DY -= static_cast<DOUBLE>(g_textGlowSize);

		// Apply the same CenterCaption offset to the legacy text transform.
		const DOUBLE offset = static_cast<DOUBLE>(CaptionCenterOffset(
			static_cast<DOUBLE>(g_textVisual->GetWidth()),
			static_cast<DOUBLE>(g_textSize.cx),
			static_cast<DOUBLE>(g_textVisual->GetX()),
			static_cast<DOUBLE>(g_textVisual->GetTransformParent()->GetWidth()),
			g_textVisual->GetScale().width
		));
		if (offset > 0.0)
		{
			matrix->DX += g_textVisual->IsRTLMirrored() ? -offset : offset;
		}
	}

	return g_CChannel_MatrixTransformUpdate_Org(This, handleId, matrix);
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

	const auto& windowState = GetWindowState(g_dwriteTextVisual);
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

	if (!g_textGlowSize)
	{
		return g_ID2D1DeviceContext_DrawTextLayout_Org(
			This,
			origin,
			textLayout,
			defaultFillBrush,
			options
		);
	}

	origin.x += g_textGlowSize;
	origin.y += g_textGlowSize;

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

	if (LOWORD(Shared::g_textGlowMode) == 3 && g_textGlowIntensity)
	{
		winrt::com_ptr<ID2D1BitmapRenderTarget> bitmapRT{};
		THROW_IF_FAILED(
			This->CreateCompatibleRenderTarget(
				D2D1::SizeF(
					std::ceil(metrics.left + metrics.width) + static_cast<float>(g_textGlowSize * 2),
					std::ceil(metrics.top + metrics.height) + static_cast<float>(g_textGlowSize * 2)
				),
				bitmapRT.put()
			)
		);

		bitmapRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
		bitmapRT->BeginDraw();
		bitmapRT->Clear();
		bitmapRT->DrawTextLayout(
			D2D1::Point2F(),
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
			THROW_IF_FAILED(
				g_textMorphologyEffect->SetValue(
					D2D1_MORPHOLOGY_PROP_WIDTH,
					3 + g_textGlowSize / 12
				)
			);
			THROW_IF_FAILED(
				g_textMorphologyEffect->SetValue(
					D2D1_MORPHOLOGY_PROP_HEIGHT,
					3 + g_textGlowSize / 12
				)
			);
		}
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
				Color::FromAbgr(g_textGlowColor | (std::min(g_textGlowIntensity, 255) << 24), false)
			)
		);
		THROW_IF_FAILED(
			g_textGlowEffect->SetValue(
				D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION,
				std::max(
					0.f,
					(static_cast<float>(g_textGlowSize)) / 3.f + 0.5f
				)
			)
		);
		g_textMorphologyEffect->SetInput(0, bitmap.get());

		This->DrawImage(
			g_textGlowEffect.get(),
			&origin,
			nullptr,
			D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
			D2D1_COMPOSITE_MODE_SOURCE_COPY
		);
		This->DrawImage(
			bitmap.get(),
			&origin,
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
			textBoundingBox.left - static_cast<float>(g_contentMargins.cxLeftWidth),
			textBoundingBox.top - static_cast<float>(g_contentMargins.cyTopHeight),
			textBoundingBox.right + static_cast<float>(g_contentMargins.cxRightWidth),
			textBoundingBox.bottom + static_cast<float>(g_contentMargins.cyBottomHeight)
		};

		const auto calcGlowClipRect = [&windowState](const D2D1_RECT_F& textRect, D2D1_RECT_F& glowClipRect, bool mirrored)
		{
			LONG offset = 0;
			offset += windowState.windowRectLeft;
			offset -= g_dwriteTextVisual->GetX();
			// DWrite glow clipping needs the same offset calculation as the text surface.
			offset -= CaptionCenterOffset(
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
		calcGlowClipRect(textBoundingBox, glowRect, g_dwriteTextVisual->IsRTLMirrored());

		THROW_IF_FAILED(
			Util::DrawNineGridBitmap(
				This,
				g_textGlowD2DBitmap.get(),
				glowRect,
				g_sizingMargins,
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
		sizePixels.Width += g_textGlowSize * 2;
		sizePixels.Height += g_textGlowSize * 2;
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
		value.Y -= g_textGlowSize;
		value.X -= g_textGlowSize;

		// offset, glowSize
		// 40, 17
		// 11, 17
		// CDWriteVisual is rtl mirrored, but CSpriteVisual is not rtl mirrored
		if (auto& offset = const_cast<POINT&>(g_dwriteTextVisual->GetOffset()); g_dwriteTextVisual->IsRTLMirrored())
		{
			if (offset.x > g_textGlowSize)
			{
				value.X += g_textGlowSize;
			}
			else
			{
				value.X += g_textGlowSize + g_textGlowSize - offset.x;
			}
		}
		else
		{
			value.X += offset.x - std::max(offset.x - g_textGlowSize, 0l);
		}

		// Apply the same CenterCaption offset to the DWrite composition surface.
		const float offset = static_cast<float>(CaptionCenterOffset(
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
	// 0x2 redraw text
	// 0x8 offset changed
	// 0x10 rtl mirrored changed
	if ((This->GetDirtyFlags() & (0x8 | 0x10)))
	{
		This->SetDirtyFlags(0x2);
	}
	if ((This->GetDirtyFlags() & 0x2))
	{
		This->SetDirtyFlags(0x8);
	}
	if (!g_CDWriteText_scalar_deleting_destructor_Org)
	{
		g_CDWriteText_scalar_deleting_destructor_Org_Address = HookHelper::get_vftable_from<decltype(g_CDWriteText_scalar_deleting_destructor_Org)>(This);
		g_CDWriteText_scalar_deleting_destructor_Hook.AttachOnce(g_CDWriteText_scalar_deleting_destructor_Org_Address, &g_CDWriteText_scalar_deleting_destructor_Org);
	}
	if (!g_CDWriteText_UpdateOffset_Org)
	{
		const auto CVisual_UpdateOffset_Org = uDWM::Symbol_CVisual_UpdateOffset.get();
		const auto CSpriteVisual_SetSize_Org = uDWM::Symbol_CSpriteVisual_SetSize.get();

		for (auto& vf : std::span{ HookHelper::get_vftable_from(This), 32})
		{
			if (vf == CVisual_UpdateOffset_Org)
			{
				g_CDWriteText_UpdateOffset_Org_Address = reinterpret_cast<decltype(g_CDWriteText_UpdateOffset_Org_Address)>(&vf);
				g_CDWriteText_UpdateOffset_Hook.AttachOnce(g_CDWriteText_UpdateOffset_Org_Address, &g_CDWriteText_UpdateOffset_Org);
			}
			if (vf == CSpriteVisual_SetSize_Org)
			{
				g_CDWriteText_SetSize_Org_Address = reinterpret_cast<decltype(g_CDWriteText_SetSize_Org_Address)>(&vf);
				g_CDWriteText_SetSize_Hook.AttachOnce(g_CDWriteText_SetSize_Org_Address, &g_CDWriteText_SetSize_Org);
			}
		}
	}
	if (g_window = uDWM::TryGetWindowFromVisual(This); g_window && g_window->GetData())
	{
		const auto windowState = GetOrCreateWindowState(This);
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
	if (!g_textGlowSize)
	{
		return g_CDWriteText_UpdateOffset_Org(This);
	}

	// SpriteVisual will crop what exceeds its bounding rectangle,
	// here we make it offset x minus the size of the glow,
	// and add it back later in the ICompositionSurfaceBrush2::put_Offset method
	//
	// This gives us enough space to render the glow.
	auto& offset = const_cast<POINT&>(This->GetOffset());
	const auto actualOffsetX = offset.x;
	if (!This->IsRTLMirrored())
	{
		offset.x = std::max(offset.x - g_textGlowSize, 0l);
	}
	const auto hr = g_CDWriteText_UpdateOffset_Org(This);
	offset.x = actualOffsetX;

	return hr;
}

HRESULT CaptionTextHandler::MyCDWriteText_SetSize(uDWM::CDWriteText* This, const SIZE* size)
{
	if (!g_textGlowSize)
	{
		return g_CDWriteText_SetSize_Org(This, size);
	}

	const auto hr = g_CDWriteText_SetSize_Org(This, size);
	// SpriteVisual will crop what exceeds its bounding rectangle,
	// expand it to ensure enough space to render the glow.
	auto& offset = const_cast<POINT&>(This->GetOffset());
	if (This->IsRTLMirrored())
	{
		This->GetVisualProxy()->SetSize(
			static_cast<double>(size->cx + offset.x - std::max(offset.x - g_textGlowSize, 0l)),
			static_cast<double>(size->cy)
		);
	}
	else
	{
		This->GetVisualProxy()->SetSize(
			static_cast<double>(size->cx + offset.x - std::max(offset.x - g_textGlowSize, 0l) + g_textGlowSize),
			static_cast<double>(size->cy)
		);
	}

	return hr;
}

HRESULT CaptionTextHandler::MyCDWriteText_InitializeVisualTreeClone(uDWM::CDWriteText* This, uDWM::CDWriteText* clonedVisual, UINT cloneOption)
{
	ShareWindowState(This, clonedVisual);
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

void CaptionTextHandler::CalculateRealizedTextGlowParams(int textGlowMode)
{
	if (textGlowMode == 0)
	{
		g_textGlowSize = 0;
	}
	else if (textGlowMode == 1 || textGlowMode == 2)
	{
		const auto themeHandle = CustomThemeAtlasLoader::GetThemeHandle();

		CustomThemeAtlasLoader::MyGetThemeMargins(
			themeHandle,
			nullptr,
			static_cast<int>(DWM_WINDOW_THEME_PART::TEXTGLOW),
			0,
			TMT_SIZINGMARGINS,
			nullptr,
			&g_sizingMargins
		);
		CustomThemeAtlasLoader::MyGetThemeMargins(
			themeHandle,
			nullptr,
			static_cast<int>(DWM_WINDOW_THEME_PART::TEXTGLOW),
			0,
			TMT_CONTENTMARGINS,
			nullptr,
			&g_contentMargins
		);
		g_textGlowSize = std::max(
			{
				g_contentMargins.cxLeftWidth,
				g_contentMargins.cxRightWidth,
				g_contentMargins.cyTopHeight,
				g_contentMargins.cyBottomHeight
			}
		);
	}
	else
	{
		wil::unique_htheme themeHandle{ OpenThemeData(nullptr, L"CompositedWindow::Window") };

		if (g_textGlowSize = HIWORD(textGlowMode); !g_textGlowSize)
		{
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle.get(), static_cast<int>(DWM_WINDOW_THEME_PART::COMMON), 0, TMT_TEXTGLOWSIZE, &g_textGlowSize);
		}
		CustomThemeAtlasLoader::MyGetThemeInt(themeHandle.get(), static_cast<int>(DWM_WINDOW_THEME_PART::COMMON), 0, TMT_GLOWINTENSITY, &g_textGlowIntensity);
		GetThemeColor(themeHandle.get(), static_cast<int>(DWM_WINDOW_THEME_PART::COMMON), 0, TMT_GLOWCOLOR, &g_textGlowColor);

		// debug
		//g_textGlowIntensity = 305;
		//g_textGlowColor = 0xFFFFFF;
	}
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
		CalculateRealizedTextGlowParams(Shared::g_textGlowMode);
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

	if (uDWM::g_versionInfo.build < os::build_w11_22h2)
	{
		wil::unique_hmodule wincodecsMoudle{ LoadLibraryExW(L"WindowsCodecs.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR) };
		FAIL_FAST_LAST_ERROR_IF_NULL_MSG(wincodecsMoudle, "Unable to load WindowsCodecs.dll");
		const auto WICCreateImagingFactory_Proxy_fn = reinterpret_cast<HRESULT(WINAPI*)(UINT, IWICImagingFactory2**)>(
			GetProcAddress(wincodecsMoudle.get(), "WICCreateImagingFactory_Proxy")
		);
		FAIL_FAST_LAST_ERROR_IF_NULL_MSG(WICCreateImagingFactory_Proxy_fn, "WICCreateImagingFactory_Proxy is unavailable");
		winrt::com_ptr<IWICImagingFactory2> wicFactory{ nullptr };
		FAIL_FAST_IF_FAILED_MSG(WICCreateImagingFactory_Proxy_fn(WINCODEC_SDK_VERSION2, wicFactory.put()), "Unable to create the WIC factory for caption hooks");
		g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address = reinterpret_cast<decltype(g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address)>(&HookHelper::get_vftable_from(wicFactory.get())[21]);
		g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Hook.Prepare(g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address, &g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org);
		HookHelper::GetCurrentHookTransaction().Apply(g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Hook);
		g_textImportHooks.Prepare(
			uDWM::g_moduleHandle,
			std::initializer_list<HookHelper::ImportDllDetourInfo>
			{
				{
					"user32.dll",
					std::initializer_list<HookHelper::ImportFunctionDetourInfo>
					{
						HookHelper::MakeImportDetour<&MyDrawTextW>("DrawTextW", &g_DrawTextW_Org)
					}
				},
				{
					"gdi32.dll",
					std::initializer_list<HookHelper::ImportFunctionDetourInfo>
					{
						HookHelper::MakeImportDetour<&MyCreateBitmap>("CreateBitmap", &g_CreateBitmap_Org)
					}
				}
			}
		);
		HookHelper::GetCurrentHookTransaction().Apply(g_textImportHooks);

		const auto build_before_w10_2004 = dwmcore::g_versionInfo.build < os::build_w10_2004;
	HookHelper::ApplyInlineHooks(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CChannel_MatrixTransformUpdate_Org, &MyCChannel_MatrixTransformUpdate },
				{ &g_CText_ValidateResources_Org, &MyCText_ValidateResources },
				{ &g_CText_InitializeVisualTreeClone_Org, &MyCText_InitializeVisualTreeClone, !build_before_w10_2004 },
				{ &g_CText_CloneVisualTree_Org, &MyCText_CloneVisualTree, build_before_w10_2004 }
			},
			true
		);
	}
	else
	{
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

		winrt::com_ptr<IDCompositionDesktopDevicePartner> dcompDevicePartner{ nullptr };
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
				{ &g_CDWriteText_ValidateVisual_Org, &MyCDWriteText_ValidateVisual },
				{ &g_CDWriteText_InitializeVisualTreeClone_Org, &MyCDWriteText_InitializeVisualTreeClone }
			},
			true
		);
	}
}
void CaptionTextHandler::Shutdown()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_CaptionTextHandler))
	{
		return;
	}

	if (uDWM::g_versionInfo.build < os::build_w11_22h2)
	{
		if (g_CText_scalar_deleting_destructor_Org)
		{
			HookHelper::GetCurrentHookTransaction().Apply(g_CText_scalar_deleting_destructor_Hook);
		}

		const auto build_before_w10_2004 = dwmcore::g_versionInfo.build < os::build_w10_2004;
		HookHelper::ApplyInlineHooks(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CChannel_MatrixTransformUpdate_Org, &MyCChannel_MatrixTransformUpdate },
				{ &g_CText_ValidateResources_Org, &MyCText_ValidateResources },
				{ &g_CText_InitializeVisualTreeClone_Org, &MyCText_InitializeVisualTreeClone, !build_before_w10_2004 },
				{ &g_CText_CloneVisualTree_Org, &MyCText_CloneVisualTree, build_before_w10_2004 }
			},
			false
		);

		HookHelper::GetCurrentHookTransaction().Apply(g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Hook);
		HookHelper::GetCurrentHookTransaction().Apply(g_textImportHooks);

	}
	else
	{
		if (g_CDWriteText_scalar_deleting_destructor_Org)
		{
			HookHelper::GetCurrentHookTransaction().Apply(g_CDWriteText_scalar_deleting_destructor_Hook);
		}
		if (g_CDWriteText_UpdateOffset_Org)
		{
			HookHelper::GetCurrentHookTransaction().Apply(g_CDWriteText_UpdateOffset_Hook);
		}
		if (g_CDWriteText_SetSize_Org)
		{
			HookHelper::GetCurrentHookTransaction().Apply(g_CDWriteText_SetSize_Hook);
		}

		HookHelper::ApplyInlineHooks(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CDWriteText_ValidateVisual_Org, &MyCDWriteText_ValidateVisual },
				{ &g_CDWriteText_InitializeVisualTreeClone_Org, &MyCDWriteText_InitializeVisualTreeClone }
			},
			false
		);

		HookHelper::GetCurrentHookTransaction().Apply(g_ID2D1DeviceContext_DrawTextLayout_Hook);
		HookHelper::GetCurrentHookTransaction().Apply(g_ICompositionGraphicsDevice_CreateDrawingSurface_Hook);
		HookHelper::GetCurrentHookTransaction().Apply(g_ICompositionSurfaceBrush2_put_Offset_Hook);

	}
}

void CaptionTextHandler::Cleanup()
{
	g_textVisual = nullptr;
	g_dwriteTextVisual = nullptr;
	DestroyDeviceResources();
	g_textSize = {};
	g_textVisualStateMap.clear();
}
