#include "pch.h"
#include "MsstyleInternals.hpp"
#include "CaptionTextHandler.hpp"
#include "Shared.hpp"
#include "GlassKernel.hpp"
#include "uDWMProjection.hpp"
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
	decltype(&MyICompositionGraphicsDevice_CreateDrawingSurface) g_ICompositionGraphicsDevice_CreateDrawingSurface_Org{ nullptr };
	decltype(&MyICompositionGraphicsDevice_CreateDrawingSurface)* g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address{ nullptr };
	decltype(&MyICompositionSurfaceBrush2_put_Offset) g_ICompositionSurfaceBrush2_put_Offset_Org{ nullptr };
	decltype(&MyICompositionSurfaceBrush2_put_Offset)* g_ICompositionSurfaceBrush2_put_Offset_Org_Address{ nullptr };
	decltype(&MyCDWriteText_ValidateVisual) g_CDWriteText_ValidateVisual_Org{ nullptr };
	decltype(&MyCDWriteText_UpdateOffset) g_CDWriteText_UpdateOffset_Org{ nullptr };
	decltype(&MyCDWriteText_UpdateOffset)* g_CDWriteText_UpdateOffset_Org_Address{ nullptr };
	decltype(&MyCDWriteText_SendSetSize) g_CDWriteText_SendSetSize_Org{ nullptr };
	decltype(&MyCDWriteText_SendSetSize)* g_CDWriteText_SendSetSize_Org_Address{ nullptr };
	decltype(&MyCDWriteText_InitializeVisualTreeClone) g_CDWriteText_InitializeVisualTreeClone_Org{ nullptr };
	decltype(&MyCDWriteText_scalar_deleting_destructor) g_CDWriteText_scalar_deleting_destructor_Org{ nullptr };
	decltype(&MyCDWriteText_scalar_deleting_destructor)* g_CDWriteText_scalar_deleting_destructor_Org_Address{ nullptr };

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
	MARGINS g_contentMargins{}, g_sizingMargins{};
	struct CWindowState
	{
		bool frozen{};
		bool active{};
		bool maximized{};
		LONG windowRectLeft{};
	};
	std::unordered_map<uDWM::CVisual*, CWindowState> g_textVisualStateMap{};
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

	const auto& windowState = g_textVisualStateMap[g_dwriteTextVisual];
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
	// update offset
	This->SetDirtyFlags(0x4);
	if (!g_CDWriteText_scalar_deleting_destructor_Org)
	{
		g_CDWriteText_scalar_deleting_destructor_Org_Address = HookHelper::get_vftable_from<decltype(g_CDWriteText_scalar_deleting_destructor_Org)>(This);
		HookHelper::PatchPointerT(
			g_CDWriteText_scalar_deleting_destructor_Org_Address,
			MyCDWriteText_scalar_deleting_destructor,
			&g_CDWriteText_scalar_deleting_destructor_Org
		);
	}
	if (!g_CDWriteText_UpdateOffset_Org)
	{
		PVOID CVisual_UpdateOffset_Org{ nullptr };
		PVOID CSpriteVisual_SendSetSize_Org{ nullptr };
		uDWM::g_projectionArray.ApplyToVariable("CVisual::UpdateOffset", CVisual_UpdateOffset_Org);
		uDWM::g_projectionArray.ApplyToVariable("CSpriteVisual::SendSetSize", CSpriteVisual_SendSetSize_Org);

		for (auto& vf : std::span{ HookHelper::get_vftable_from(This), 32})
		{
			if (vf == CVisual_UpdateOffset_Org)
			{
				g_CDWriteText_UpdateOffset_Org_Address = reinterpret_cast<decltype(g_CDWriteText_UpdateOffset_Org_Address)>(&vf);
				HookHelper::PatchPointerT(
					g_CDWriteText_UpdateOffset_Org_Address,
					MyCDWriteText_UpdateOffset,
					&g_CDWriteText_UpdateOffset_Org
				);
			}
			if (vf == CSpriteVisual_SendSetSize_Org)
			{
				g_CDWriteText_SendSetSize_Org_Address = reinterpret_cast<decltype(g_CDWriteText_SendSetSize_Org_Address)>(&vf);
				HookHelper::PatchPointerT(
					g_CDWriteText_SendSetSize_Org_Address,
					MyCDWriteText_SendSetSize,
					&g_CDWriteText_SendSetSize_Org
				);
			}
		}
	}
	if (g_window = uDWM::TryGetWindowFromVisual(This); g_window && g_window->GetData())
	{
		auto& windowState = g_textVisualStateMap[This];

		if (!windowState.frozen)
		{
			windowState.active = g_window->TreatAsActiveWindow();
			windowState.maximized = g_window->TreatAsMaximized();

			RECT windowRect{};
			g_window->GetActualWindowRect(&windowRect, true, false, true);
			windowState.windowRectLeft = windowRect.left;
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

HRESULT CaptionTextHandler::MyCDWriteText_SendSetSize(uDWM::CDWriteText* This, const SIZE* size)
{
	if (!g_textGlowSize)
	{
		return g_CDWriteText_SendSetSize_Org(This, size);
	}

	const auto hr = g_CDWriteText_SendSetSize_Org(This, size);
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
	auto& clonedWindowState = g_textVisualStateMap[clonedVisual];
	clonedWindowState = g_textVisualStateMap[This];
	clonedWindowState.frozen = true;
	return g_CDWriteText_InitializeVisualTreeClone_Org(This, clonedVisual, cloneOption);
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

	winrt::com_ptr<ID2D1DeviceContext> context{};
	THROW_IF_FAILED(
		uDWM::CDesktopManager::GetInstance()->GetD2DDevice()->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			context.put()
		)
	);

	g_ID2D1DeviceContext_DrawTextLayout_Org_Address = &HookHelper::get_vftable_from<decltype(g_ID2D1DeviceContext_DrawTextLayout_Org)>(context.get())[28];
	HookHelper::PatchPointerT(
		g_ID2D1DeviceContext_DrawTextLayout_Org_Address,
		MyID2D1DeviceContext_DrawTextLayout,
		&g_ID2D1DeviceContext_DrawTextLayout_Org
	);

	winrt::com_ptr<IDCompositionDesktopDevicePartner6> dcompDevicePartner{ nullptr };
	THROW_IF_FAILED(uDWM::CDesktopManager::GetInstance()->GetInteropCompositorDCompDevicePartner()->QueryInterface(dcompDevicePartner.put()));
	winrt::com_ptr<abi::ICompositionGraphicsDevice> graphicsDevice{ nullptr };

	winrt::com_ptr<abi::ICompositor> compositor{};
	THROW_IF_FAILED(dcompDevicePartner->QueryInterface(compositor.put()));

	winrt::com_ptr<abi::ICompositorInterop> compositorInterop{};
	THROW_IF_FAILED(compositor->QueryInterface(compositorInterop.put()));
	THROW_IF_FAILED(
		compositorInterop->CreateGraphicsDevice(
			uDWM::CDesktopManager::GetInstance()->GetD2DDevice(),
			graphicsDevice.put()
		)
	);

	g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address = &HookHelper::get_vftable_from<decltype(g_ICompositionGraphicsDevice_CreateDrawingSurface_Org)>(graphicsDevice.get())[6];
	HookHelper::PatchPointerT(
		g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address,
		MyICompositionGraphicsDevice_CreateDrawingSurface,
		&g_ICompositionGraphicsDevice_CreateDrawingSurface_Org
	);

	winrt::com_ptr<abi::ICompositionSurfaceBrush> surfaceBrush{ nullptr };
	THROW_IF_FAILED(compositor->CreateSurfaceBrush(surfaceBrush.put()));

	winrt::com_ptr<abi::ICompositionSurfaceBrush2> surfaceBrush2{ nullptr };
	THROW_IF_FAILED(surfaceBrush->QueryInterface(surfaceBrush2.put()));
	g_ICompositionSurfaceBrush2_put_Offset_Org_Address = &HookHelper::get_vftable_from<decltype(g_ICompositionSurfaceBrush2_put_Offset_Org)>(surfaceBrush2.get())[11];
	HookHelper::PatchPointerT(
		g_ICompositionSurfaceBrush2_put_Offset_Org_Address,
		MyICompositionSurfaceBrush2_put_Offset,
		&g_ICompositionSurfaceBrush2_put_Offset_Org
	);

	uDWM::g_projectionArray.ApplyToVariable("CDWriteText::ValidateVisual", g_CDWriteText_ValidateVisual_Org);
	uDWM::g_projectionArray.ApplyToVariable("CDWriteText::InitializeVisualTreeClone", g_CDWriteText_InitializeVisualTreeClone_Org);

	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CDWriteText_ValidateVisual_Org, &MyCDWriteText_ValidateVisual },
			{ &g_CDWriteText_InitializeVisualTreeClone_Org, &MyCDWriteText_InitializeVisualTreeClone }
		},
		true
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
		HookHelper::PatchPointerT(
			g_CDWriteText_scalar_deleting_destructor_Org_Address,
			g_CDWriteText_scalar_deleting_destructor_Org
		);
	}
	if (g_CDWriteText_UpdateOffset_Org)
	{
		HookHelper::PatchPointerT(
			g_CDWriteText_UpdateOffset_Org_Address,
			g_CDWriteText_UpdateOffset_Org
		);
	}
	if (g_CDWriteText_SendSetSize_Org)
	{
		HookHelper::PatchPointerT(
			g_CDWriteText_SendSetSize_Org_Address,
			g_CDWriteText_SendSetSize_Org
		);
	}

	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CDWriteText_ValidateVisual_Org, &MyCDWriteText_ValidateVisual },
			{ &g_CDWriteText_InitializeVisualTreeClone_Org, &MyCDWriteText_InitializeVisualTreeClone }
		},
		false
	);

	SwitchToThread();

	HookHelper::PatchPointerT(
		g_ID2D1DeviceContext_DrawTextLayout_Org_Address,
		g_ID2D1DeviceContext_DrawTextLayout_Org
	);
	HookHelper::PatchPointerT(
		g_ICompositionGraphicsDevice_CreateDrawingSurface_Org_Address,
		g_ICompositionGraphicsDevice_CreateDrawingSurface_Org
	);
	HookHelper::PatchPointerT(
		g_ICompositionSurfaceBrush2_put_Offset_Org_Address,
		g_ICompositionSurfaceBrush2_put_Offset_Org
	);

	g_dwriteTextVisual = nullptr;

	DestroyDeviceResources();
	g_textSize = {};
	g_textVisualStateMap.clear();
}
