#include "pch.h"
#include "CaptionTextHandler.hpp"
#include "Shared.hpp"
#include "GlassKernel.hpp"
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
	HRESULT STDMETHODCALLTYPE MyIWICImagingFactory2_CreateBitmapFromHBITMAP(
		IWICImagingFactory2* This,
		HBITMAP hBitmap,
		HPALETTE hPalette,
		WICBitmapAlphaChannelOption options,
		IWICBitmap** ppIBitmap
	);
	HRESULT STDMETHODCALLTYPE MyCText_ValidateResources(uDWM::CText* This);
	HRESULT STDMETHODCALLTYPE MyCText_InitializeVisualTreeClone(uDWM::CText* This, uDWM::CText* clonedVisual, UINT cloneOption);
	HRESULT STDMETHODCALLTYPE MyCText_scalar_deleting_destructor(uDWM::CText* This, BYTE flag);
	HRESULT STDMETHODCALLTYPE MyCChannel_MatrixTransformUpdate(dwmcore::CChannel* This, UINT handleId, MilMatrix3x2D* matrix);

	void STDMETHODCALLTYPE MyID2D1DeviceContext_DrawTextLayout(
		ID2D1DeviceContext* This,
		D2D1_POINT_2F origin,
		IDWriteTextLayout* textLayout,
		ID2D1Brush* defaultFillBrush,
		D2D1_DRAW_TEXT_OPTIONS options
	);
	HRESULT STDMETHODCALLTYPE MyICompositionGraphicsDevice_CreateDrawingSurface(
		abi::ICompositionGraphicsDevice* This,
		abi::Size sizePixels,
		abi::DirectXPixelFormat pixelFormat,
		abi::DirectXAlphaMode alphaMode,
		abi::ICompositionDrawingSurface** result
	);
	HRESULT STDMETHODCALLTYPE MyICompositionSurfaceBrush2_put_Offset(
		abi::ICompositionSurfaceBrush2* This,
		abi::Vector2 value
	);
	HRESULT STDMETHODCALLTYPE MyCDWriteText_ValidateVisual(uDWM::CDWriteText* This);
	HRESULT STDMETHODCALLTYPE MyCDWriteText_UpdateOffset(uDWM::CDWriteText* This);
	HRESULT STDMETHODCALLTYPE MyCDWriteText_SetSize(uDWM::CDWriteText* This, const SIZE* size);
	HRESULT STDMETHODCALLTYPE MyCDWriteText_InitializeVisualTreeClone(uDWM::CDWriteText* This, uDWM::CDWriteText* clonedVisual, UINT cloneOption);
	HRESULT STDMETHODCALLTYPE MyCDWriteText_scalar_deleting_destructor(uDWM::CDWriteText* This, BYTE flag);

	decltype(&MyDrawTextW) g_DrawTextW_Org{ nullptr };
	decltype(&MyCreateBitmap) g_CreateBitmap_Org{ nullptr };
	decltype(&MyIWICImagingFactory2_CreateBitmapFromHBITMAP) g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org{ nullptr };
	decltype(&MyCText_ValidateResources) g_CText_ValidateResources_Org{ nullptr };
	decltype(&MyCText_InitializeVisualTreeClone) g_CText_InitializeVisualTreeClone_Org{ nullptr };
	decltype(&MyCText_scalar_deleting_destructor) g_CText_scalar_deleting_destructor_Org{ nullptr };
	decltype(&MyCText_scalar_deleting_destructor)* g_CText_scalar_deleting_destructor_Org_Address{ nullptr };
	decltype(&MyCChannel_MatrixTransformUpdate) g_CChannel_MatrixTransformUpdate_Org{ nullptr };
	PVOID* g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address{ nullptr };

	decltype(&MyID2D1DeviceContext_DrawTextLayout) g_ID2D1DeviceContext_DrawTextLayout_Org{ nullptr };
	decltype(&MyID2D1DeviceContext_DrawTextLayout)* g_ID2D1DeviceContext_DrawTextLayout_Org_Address{ nullptr };
	decltype(&MyICompositionGraphicsDevice_CreateDrawingSurface) g_ICompositionGraphicsDevice_CreateDrawingSurface_Org{ nullptr };
	decltype(&MyICompositionGraphicsDevice_CreateDrawingSurface)* g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address{ nullptr };
	decltype(&MyICompositionSurfaceBrush2_put_Offset) g_ICompositionSurfaceBrush2_put_Offset_Org{ nullptr };
	decltype(&MyICompositionSurfaceBrush2_put_Offset)* g_ICompositionSurfaceBrush2_put_Offset_Org_Address{ nullptr };
	decltype(&MyCDWriteText_ValidateVisual) g_CDWriteText_ValidateVisual_Org{ nullptr };
	decltype(&MyCDWriteText_UpdateOffset) g_CDWriteText_UpdateOffset_Org{ nullptr };
	decltype(&MyCDWriteText_UpdateOffset)* g_CDWriteText_UpdateOffset_Org_Address{ nullptr };
	decltype(&MyCDWriteText_SetSize) g_CDWriteText_SetSize_Org{ nullptr };
	decltype(&MyCDWriteText_SetSize)* g_CDWriteText_SetSize_Org_Address{ nullptr };
	decltype(&MyCDWriteText_InitializeVisualTreeClone) g_CDWriteText_InitializeVisualTreeClone_Org{ nullptr };
	decltype(&MyCDWriteText_scalar_deleting_destructor) g_CDWriteText_scalar_deleting_destructor_Org{ nullptr };
	decltype(&MyCDWriteText_scalar_deleting_destructor)* g_CDWriteText_scalar_deleting_destructor_Org_Address{ nullptr };


	static union
	{
		uDWM::CText* g_textVisual;
		uDWM::CDWriteText* g_dwriteTextVisual;
	};
	uDWM::CTopLevelWindow* g_window{ nullptr };

	// original sizes, no glow included
	static union
	{
		SIZE g_textSize;
		abi::Size g_textSizeF;
	};
	COLORREF g_captionActiveColor{};
	COLORREF g_captionInactiveColor{};
	MARGINS g_contentMargins{}, g_sizingMargins{};
	std::unordered_map<uDWM::CVisual*, bool> g_textVisualStateMap{};
	winrt::com_ptr<ID2D1Bitmap1> g_textGlowD2DBitmap{};
	winrt::com_ptr<ID2D1Effect> g_textGlowEffect{};
	winrt::com_ptr<ID2D1Effect> g_textMorphologyEffect{};

	float g_textOpacity{};
	float g_textOpacityInactive{};
	COLORREF g_textGlowColor{};

	int g_textGlowSize{};
	int g_textGlowIntensity{};
	bool g_centerCaption{ false };
	bool g_disableTextHooks{ false };

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
	auto drawTextCallback = [](HDC hdc, LPWSTR pszText, int cchText, LPRECT prc, UINT dwFlags, LPARAM lParam) -> int
	{
		return *reinterpret_cast<int*>(lParam) = g_DrawTextW_Org(hdc, pszText, cchText, prc, dwFlags);
	};

	if ((format & DT_CALCRECT))
	{
		return g_DrawTextW_Org(hdc, lpchText, cchText, lprc, format);
	}
	// clear the background, so the text can be shown transparent
	// with this hack, we don't need to hook FillRect any more
	BITMAP bmp{};
	if (GetObjectW(GetCurrentObject(hdc, OBJ_BITMAP), sizeof(bmp), &bmp) && bmp.bmBits)
	{
		memset(bmp.bmBits, 0, 4 * bmp.bmWidth * bmp.bmHeight);
	}

	OffsetRect(lprc, g_textGlowSize, g_textGlowSize);
	
	const auto textColor = GetTextColor(hdc);
	const auto textColorOverride = g_textVisualStateMap[g_textVisual] ? g_captionActiveColor : g_captionInactiveColor;
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

	const auto calcGlowClipRect = [](LPCRECT lprc, RECT& glowClipRect, bool mirrored)
	{
		auto& visibleMargins = g_window->GetMarginsVisibleOutside(g_window->IsWindowMaximized());
		if (!mirrored)
		{
			glowClipRect.left = std::max(
				glowClipRect.left,
				lprc->left -
				(g_textVisual->GetX() + (g_centerCaption ? static_cast<LONG>(std::round(static_cast<DOUBLE>(g_textVisual->GetWidth() - g_textSize.cx) / 2.)) : (-visibleMargins.cxLeftWidth)))
			);
		}
		else
		{
			glowClipRect.right = std::min(
				glowClipRect.right,
				lprc->right +
				(g_textVisual->GetX() + (g_centerCaption ? static_cast<LONG>(std::round(static_cast<DOUBLE>(g_textVisual->GetWidth() - g_textSize.cx) / 2.)) : (-visibleMargins.cxRightWidth)))
			);
		}
	};

	auto glowDrawRect = *lprc;
	auto glowClipRect = glowDrawRect;

	if (Shared::g_textGlowMode == 1 || Shared::g_textGlowMode == 2)
	{
		glowDrawRect.left -= g_contentMargins.cxLeftWidth;
		glowDrawRect.top -= g_contentMargins.cyTopHeight;
		glowDrawRect.right += g_contentMargins.cxRightWidth;
		glowDrawRect.bottom += g_contentMargins.cyBottomHeight;
		glowClipRect = glowDrawRect;
		calcGlowClipRect(lprc, glowClipRect, g_textVisual->IsRTLMirrored());
	}
	else if (LOWORD(Shared::g_textGlowMode) == 3)
	{
		options.iGlowSize = g_textGlowSize;
		glowDrawRect.left -= g_textGlowSize;
		glowDrawRect.top -= g_textGlowSize;
		glowDrawRect.right += g_textGlowSize;
		glowDrawRect.bottom += g_textGlowSize;
		glowClipRect = glowDrawRect;
		calcGlowClipRect(lprc, glowClipRect, g_textVisual->IsRTLMirrored());
	}

	SaveDC(hdc);
	const auto dcPaintScope = wil::scope_exit([hdc]
	{
		RestoreDC(hdc, -1);
	});
	IntersectClipRect(hdc, glowClipRect.left, glowClipRect.top, glowClipRect.right, glowClipRect.bottom);

	if (Shared::g_textGlowMode == 1 || Shared::g_textGlowMode == 2)
	{
		const auto opacity = Shared::g_textGlowMode == 2 ? static_cast<int>((g_textVisualStateMap[g_dwriteTextVisual] ? g_textOpacity : g_textOpacityInactive) * 255.f) : 255;
		
		Util::DrawNineGridBitmap(
			hdc,
			Shared::g_textGlowBitmap.get(),
			glowDrawRect,
			g_sizingMargins,
			opacity
		);
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
HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyIWICImagingFactory2_CreateBitmapFromHBITMAP(
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

HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCText_ValidateResources(uDWM::CText* This)
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
		g_CText_scalar_deleting_destructor_Org_Address = HookHelper::vftbl_of<decltype(g_CText_scalar_deleting_destructor_Org)>(This);
		HookHelper::WritePointer(
			g_CText_scalar_deleting_destructor_Org_Address, 
			MyCText_scalar_deleting_destructor,
			&g_CText_scalar_deleting_destructor_Org
		);
	}
	if (g_window = uDWM::TryGetWindowFromVisual(This); g_window && g_window->GetData())
	{
		g_textVisualStateMap[This] = g_window->TreatAsActiveWindow();
	}
	g_textVisual = This;
	const auto hr = g_CText_ValidateResources_Org(This);
	g_textVisual = nullptr;
	g_window = nullptr;

	return hr;
}
HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCText_InitializeVisualTreeClone(uDWM::CText* This, uDWM::CText* clonedVisual, UINT cloneOption)
{
	g_textVisualStateMap[clonedVisual] = g_textVisualStateMap[This];
	return g_CText_InitializeVisualTreeClone_Org(This, clonedVisual, cloneOption);
}
HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCText_scalar_deleting_destructor(uDWM::CText* This, BYTE flag)
{
	g_textVisualStateMap.erase(This);
	return g_CText_scalar_deleting_destructor_Org(This, flag);
}
HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCChannel_MatrixTransformUpdate(dwmcore::CChannel* This, UINT handleId, MilMatrix3x2D* matrix)
{
	if (g_textVisual)
	{
		matrix->DX -= static_cast<DOUBLE>(g_textGlowSize);
		matrix->DY -= static_cast<DOUBLE>(g_textGlowSize);

		if (g_centerCaption)
		{
			const auto offset = std::round(static_cast<DOUBLE>(g_textVisual->GetWidth() - g_textSize.cx) / 2.);
			matrix->DX += g_textVisual->IsRTLMirrored() ? -offset : offset;
		}
	}

	return g_CChannel_MatrixTransformUpdate_Org(This, handleId, matrix);
}

void STDMETHODCALLTYPE CaptionTextHandler::MyID2D1DeviceContext_DrawTextLayout(
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

	const auto textColorOverride = g_textVisualStateMap[g_dwriteTextVisual] ? g_captionActiveColor : g_captionInactiveColor;
	if (textColorOverride != 0xFFFFFFFF)
	{
		solidColorBrush->SetColor(Color::FromAbgr(textColorOverride));
	}

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
		const D2D1_RECT_F glowRect
		{
			textBoundingBox.left - static_cast<float>(g_contentMargins.cxLeftWidth),
			textBoundingBox.top - static_cast<float>(g_contentMargins.cyTopHeight),
			textBoundingBox.right + static_cast<float>(g_contentMargins.cxRightWidth),
			textBoundingBox.bottom + static_cast<float>(g_contentMargins.cyBottomHeight)
		};
		THROW_IF_FAILED(
			Util::DrawNineGridBitmap(
				This,
				g_textGlowD2DBitmap.get(),
				glowRect,
				g_sizingMargins,
				Shared::g_textGlowMode == 2 ? (g_textVisualStateMap[g_dwriteTextVisual] ? g_textOpacity : g_textOpacityInactive) : 1.f
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

HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyICompositionGraphicsDevice_CreateDrawingSurface(
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

HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyICompositionSurfaceBrush2_put_Offset(
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

		if (g_centerCaption)
		{
			const auto offset = std::round((static_cast<float>(g_dwriteTextVisual->GetWidth()) - g_textSizeF.Width) / 2.f);
			value.X += g_dwriteTextVisual->IsRTLMirrored() ? -offset : offset;
		}
	}
	return g_ICompositionSurfaceBrush2_put_Offset_Org(
		This,
		value
	);
}

HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCDWriteText_ValidateVisual(uDWM::CDWriteText* This)
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
		g_CDWriteText_scalar_deleting_destructor_Org_Address = HookHelper::vftbl_of<decltype(g_CDWriteText_scalar_deleting_destructor_Org)>(This);
		HookHelper::WritePointer(
			g_CDWriteText_scalar_deleting_destructor_Org_Address,
			MyCDWriteText_scalar_deleting_destructor,
			&g_CDWriteText_scalar_deleting_destructor_Org
		);
	}
	if (!g_CDWriteText_UpdateOffset_Org)
	{
		PVOID CVisual_UpdateOffset_Org{ nullptr };
		PVOID CSpriteVisual_SetSize_Org{ nullptr };
		uDWM::g_projectionArray.ApplyToVariable("CVisual::UpdateOffset", CVisual_UpdateOffset_Org);
		uDWM::g_projectionArray.ApplyToVariable("CSpriteVisual::SetSize", CSpriteVisual_SetSize_Org);

		for (auto& vf : std::span{ HookHelper::vftbl_of(This), 32})
		{
			if (vf == CVisual_UpdateOffset_Org)
			{
				g_CDWriteText_UpdateOffset_Org_Address = reinterpret_cast<decltype(g_CDWriteText_UpdateOffset_Org_Address)>(&vf);
				HookHelper::WritePointer(
					g_CDWriteText_UpdateOffset_Org_Address, 
					MyCDWriteText_UpdateOffset,
					&g_CDWriteText_UpdateOffset_Org
				);
			}
			if (vf == CSpriteVisual_SetSize_Org)
			{
				g_CDWriteText_SetSize_Org_Address = reinterpret_cast<decltype(g_CDWriteText_SetSize_Org_Address)>(&vf);
				HookHelper::WritePointer(
					g_CDWriteText_SetSize_Org_Address,
					MyCDWriteText_SetSize,
					&g_CDWriteText_SetSize_Org
				);
			}
		}
	}
	if (g_window = uDWM::TryGetWindowFromVisual(This); g_window && g_window->GetData())
	{
		g_textVisualStateMap[This] = g_window->TreatAsActiveWindow();
	}
	g_dwriteTextVisual = This;
	const auto hr = g_CDWriteText_ValidateVisual_Org(This);
	g_dwriteTextVisual = nullptr;
	g_window = nullptr;

	return hr;
}

HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCDWriteText_UpdateOffset(uDWM::CDWriteText* This)
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

HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCDWriteText_SetSize(uDWM::CDWriteText* This, const SIZE* size)
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

HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCDWriteText_InitializeVisualTreeClone(uDWM::CDWriteText* This, uDWM::CDWriteText* clonedVisual, UINT cloneOption)
{
	g_textVisualStateMap[clonedVisual] = g_textVisualStateMap[This];
	return g_CDWriteText_InitializeVisualTreeClone_Org(This, clonedVisual, cloneOption);
}

HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCDWriteText_scalar_deleting_destructor(uDWM::CDWriteText* This, BYTE flag)
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
			45,
			0,
			TMT_SIZINGMARGINS,
			nullptr,
			&g_sizingMargins
		);
		CustomThemeAtlasLoader::MyGetThemeMargins(
			themeHandle,
			nullptr,
			45,
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
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle.get(), 0, 0, TMT_TEXTGLOWSIZE, &g_textGlowSize);
		}
		CustomThemeAtlasLoader::MyGetThemeInt(themeHandle.get(), 0, 0, TMT_GLOWINTENSITY, &g_textGlowIntensity);
		GetThemeColor(themeHandle.get(), 0, 0, TMT_GLOWCOLOR, &g_textGlowColor);

		// debug
		//g_textGlowIntensity = 305;
		//g_textGlowColor = 0xFFFFFF;
	}
}

void CaptionTextHandler::DestroyDeviceResources()
{
	if (uDWM::g_buildNumber < os::build_w11_22h2)
	{
		return;
	}

	g_textGlowD2DBitmap = nullptr;
	g_textGlowEffect = nullptr;
	g_textMorphologyEffect = nullptr;
}

void CaptionTextHandler::Update(GlassEngine::UpdateType type)
{
	if (type & GlassEngine::UpdateType::Theme)
	{
		g_textGlowD2DBitmap = nullptr;

		const auto themeHandle = CustomThemeAtlasLoader::GetThemeHandle();
		if (themeHandle)
		{
			int value;
			
			value = 100;
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, 46, 1, TMT_OPACITY, &value);
			g_textOpacity = value / 100.f;

			value = 100;
			CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, 46, 2, TMT_OPACITY, &value);
			g_textOpacityInactive = value / 100.f;
		}
		CalculateRealizedTextGlowParams(Shared::g_textGlowMode);
	}
	if (type & GlassEngine::UpdateType::Backdrop)
	{
		g_centerCaption = static_cast<bool>(GlassEngine::GetDwordFromRegistry(L"CenterCaption", FALSE));
		g_captionActiveColor = GlassEngine::GetDwordFromRegistry(L"ColorizationColorCaption", 0xFFFFFFFF);
		g_captionInactiveColor = GlassEngine::GetDwordFromRegistry(L"ColorizationColorCaptionInactive", g_captionActiveColor);
	}
}

void CaptionTextHandler::Startup()
{
	DWORD value{ 0ul };
	wil::reg::get_value_dword_nothrow(
		GlassEngine::GetDwmLocalMachineKey(),
		L"DisabledHooks",
		&value
	);
	g_disableTextHooks = (value & 1) != 0;

	if (g_disableTextHooks)
	{
		return;
	}

	if (uDWM::g_buildNumber < os::build_w11_22h2)
	{
		wil::unique_hmodule wincodecsMoudle{ LoadLibraryExW(L"WindowsCodecs.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32) };
		THROW_LAST_ERROR_IF_NULL(wincodecsMoudle);
		const auto WICCreateImagingFactory_Proxy_fn = reinterpret_cast<HRESULT(WINAPI*)(UINT, IWICImagingFactory2**)>(
			GetProcAddress(wincodecsMoudle.get(), "WICCreateImagingFactory_Proxy")
		);
		THROW_LAST_ERROR_IF_NULL(WICCreateImagingFactory_Proxy_fn);
		winrt::com_ptr<IWICImagingFactory2> wicFactory{ nullptr };
		THROW_IF_FAILED(WICCreateImagingFactory_Proxy_fn(WINCODEC_SDK_VERSION2, wicFactory.put()));
		g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address = &HookHelper::vftbl_of(wicFactory.get())[21];
		HookHelper::WritePointer(
			g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address,
			MyIWICImagingFactory2_CreateBitmapFromHBITMAP,
			&g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org
		);
		g_DrawTextW_Org = reinterpret_cast<decltype(g_DrawTextW_Org)>(HookHelper::WriteIAT(uDWM::g_moduleHandle, "user32.dll", "DrawTextW", MyDrawTextW));
		g_CreateBitmap_Org = reinterpret_cast<decltype(g_CreateBitmap_Org)>(HookHelper::WriteIAT(uDWM::g_moduleHandle, "gdi32.dll", "CreateBitmap", MyCreateBitmap));

		dwmcore::g_projectionArray.ApplyToVariable("CChannel::MatrixTransformUpdate", g_CChannel_MatrixTransformUpdate_Org);
		uDWM::g_projectionArray.ApplyToVariable("CText::ValidateResources", g_CText_ValidateResources_Org);
		uDWM::g_projectionArray.ApplyToVariable("CText::InitializeVisualTreeClone", g_CText_InitializeVisualTreeClone_Org);
		
		THROW_IF_FAILED(
			HookHelper::Detours::Write([]()
			{
				HookHelper::Detours::Attach(&g_CChannel_MatrixTransformUpdate_Org, MyCChannel_MatrixTransformUpdate);
				HookHelper::Detours::Attach(&g_CText_ValidateResources_Org, MyCText_ValidateResources);
				HookHelper::Detours::Attach(&g_CText_InitializeVisualTreeClone_Org, MyCText_InitializeVisualTreeClone);
			})
		);
	}
	else
	{
		winrt::com_ptr<ID2D1DeviceContext> context{};
		THROW_IF_FAILED(
			uDWM::CDesktopManager::GetInstance()->GetD2DDevice()->CreateDeviceContext(
				D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
				context.put()
			)
		);

		g_ID2D1DeviceContext_DrawTextLayout_Org_Address = &HookHelper::vftbl_of<decltype(g_ID2D1DeviceContext_DrawTextLayout_Org)>(context.get())[28];
		HookHelper::WritePointer(
			g_ID2D1DeviceContext_DrawTextLayout_Org_Address,
			MyID2D1DeviceContext_DrawTextLayout,
			&g_ID2D1DeviceContext_DrawTextLayout_Org
		);

		winrt::com_ptr<IDCompositionDesktopDevicePartner> dcompDevicePartner{ nullptr };
		THROW_IF_FAILED(uDWM::CDesktopManager::GetInstance()->GetDCompDevice()->QueryInterface(dcompDevicePartner.put()));
		winrt::com_ptr<abi::ICompositionGraphicsDevice> graphicsDevice{ nullptr };

		const auto compositor = dcompDevicePartner.as<abi::ICompositor>();

		THROW_IF_FAILED(
			compositor.as<abi::ICompositorInterop>()->CreateGraphicsDevice(
				uDWM::CDesktopManager::GetInstance()->GetD2DDevice(),
				graphicsDevice.put()
			)
		);

		g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address = &HookHelper::vftbl_of<decltype(g_ICompositionGraphicsDevice_CreateDrawingSurface_Org)>(graphicsDevice.get())[6];
		HookHelper::WritePointer(
			g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address,
			MyICompositionGraphicsDevice_CreateDrawingSurface,
			&g_ICompositionGraphicsDevice_CreateDrawingSurface_Org
		);

		winrt::com_ptr<abi::ICompositionSurfaceBrush> surfaceBrush{ nullptr };
		THROW_IF_FAILED(compositor->CreateSurfaceBrush(surfaceBrush.put()));

		g_ICompositionSurfaceBrush2_put_Offset_Org_Address = &HookHelper::vftbl_of<decltype(g_ICompositionSurfaceBrush2_put_Offset_Org)>(surfaceBrush.as<abi::ICompositionSurfaceBrush2>().get())[11];
		HookHelper::WritePointer(
			g_ICompositionSurfaceBrush2_put_Offset_Org_Address,
			MyICompositionSurfaceBrush2_put_Offset,
			&g_ICompositionSurfaceBrush2_put_Offset_Org
		);

		uDWM::g_projectionArray.ApplyToVariable("CDWriteText::ValidateVisual", g_CDWriteText_ValidateVisual_Org);
		uDWM::g_projectionArray.ApplyToVariable("CDWriteText::InitializeVisualTreeClone", g_CDWriteText_InitializeVisualTreeClone_Org);

		THROW_IF_FAILED(
			HookHelper::Detours::Write([]()
			{
				HookHelper::Detours::Attach(&g_CDWriteText_ValidateVisual_Org, MyCDWriteText_ValidateVisual);
				HookHelper::Detours::Attach(&g_CDWriteText_InitializeVisualTreeClone_Org, MyCDWriteText_InitializeVisualTreeClone);
			})
		);
	}
}
void CaptionTextHandler::Shutdown()
{
	if (g_disableTextHooks)
	{
		return;
	}

	if (uDWM::g_buildNumber < os::build_w11_22h2)
	{
		if (g_CText_scalar_deleting_destructor_Org)
		{
			HookHelper::WritePointer(
				g_CText_scalar_deleting_destructor_Org_Address,
				g_CText_scalar_deleting_destructor_Org
			);
		}

		THROW_IF_FAILED(
			HookHelper::Detours::Write([]()
			{
				HookHelper::Detours::Detach(&g_CChannel_MatrixTransformUpdate_Org, MyCChannel_MatrixTransformUpdate);
				HookHelper::Detours::Detach(&g_CText_ValidateResources_Org, MyCText_ValidateResources);
				HookHelper::Detours::Detach(&g_CText_InitializeVisualTreeClone_Org, MyCText_InitializeVisualTreeClone);
			})
		);

		HookHelper::WritePointer(
			g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address,
			g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org
		);
		HookHelper::WriteIAT(uDWM::g_moduleHandle, "user32.dll", "DrawTextW", g_DrawTextW_Org);
		HookHelper::WriteIAT(uDWM::g_moduleHandle, "gdi32.dll", "CreateBitmap", g_CreateBitmap_Org);

		g_textVisual = nullptr;
	}
	else
	{
		if (g_CDWriteText_scalar_deleting_destructor_Org)
		{
			HookHelper::WritePointer(
				g_CDWriteText_scalar_deleting_destructor_Org_Address,
				g_CDWriteText_scalar_deleting_destructor_Org
			);
		}
		if (g_CDWriteText_UpdateOffset_Org)
		{
			HookHelper::WritePointer(
				g_CDWriteText_UpdateOffset_Org_Address,
				g_CDWriteText_UpdateOffset_Org
			);
		}
		if (g_CDWriteText_SetSize_Org)
		{
			HookHelper::WritePointer(
				g_CDWriteText_SetSize_Org_Address,
				g_CDWriteText_SetSize_Org
			);
		}

		THROW_IF_FAILED(
			HookHelper::Detours::Write([]()
			{
				HookHelper::Detours::Detach(&g_CDWriteText_ValidateVisual_Org, MyCDWriteText_ValidateVisual);
				HookHelper::Detours::Detach(&g_CDWriteText_InitializeVisualTreeClone_Org, MyCDWriteText_InitializeVisualTreeClone);
			})
		);

		HookHelper::WritePointer(
			g_ID2D1DeviceContext_DrawTextLayout_Org_Address,
			g_ID2D1DeviceContext_DrawTextLayout_Org
		);
		HookHelper::WritePointer(
			g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address,
			g_ICompositionGraphicsDevice_CreateDrawingSurface_Org
		);
		HookHelper::WritePointer(
			g_ICompositionSurfaceBrush2_put_Offset_Org_Address,
			g_ICompositionSurfaceBrush2_put_Offset_Org
		);

		g_dwriteTextVisual = nullptr;
		DestroyDeviceResources();
	}

	g_textSize = {};
	g_textVisualStateMap.clear();
}
