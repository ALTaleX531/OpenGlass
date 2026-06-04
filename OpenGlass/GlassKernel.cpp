#include "pch.h"
#include "MsstyleInternals.hpp"
#include "GlassKernel.hpp"
#include "uDWMProjection.hpp"
#include "Shared.hpp"
#include "dwmcoreProjection.hpp"
#include "ReflectionVisual.hpp"
#include "GlassRenderer.hpp"
#include "CaptionTextHandler.hpp"
#include "CustomThemeAtlasLoader.hpp"
#include "GlassRealizer.hpp"
#include "D3DGlassRealizer.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassKernel
{
	HRESULT MyCCachedVisualImage_CCachedTarget_Update(
		dwmcore::CCachedVisualImage::CCachedTarget* This,
		const D2D1_RECT_F& rect,
		DWM::MilStretch mode,
		const struct RenderTargetInfo& info
	);
	HRESULT MyCDrawingContext_PreSubgraph(
		dwmcore::CDrawingContext* This,
		dwmcore::CVisualTree* visualTree,
		bool* conditionalBreak
	);
	HRESULT MyCD2DContext_DestroyDeviceResources(dwmcore::CD2DContext* This);
	HRESULT MyCXXX_ReleaseXXX(PVOID This);

	decltype(&MyCCachedVisualImage_CCachedTarget_Update) g_CCachedVisualImage_CCachedTarget_Update_Org{ nullptr };
	decltype(&MyCDrawingContext_PreSubgraph) g_CDrawingContext_PreSubgraph_Org{ nullptr };
	decltype(&MyCD2DContext_DestroyDeviceResources) g_CD2DContext_DestroyDeviceResources_Org{ nullptr };
	decltype(&MyCXXX_ReleaseXXX) g_CXXX_ReleaseXXX_Org{ nullptr };

	size_t g_CVIHierarchy{};
	HWND g_hwnd{};

	void RedrawTopLevelWindow(uDWM::CTopLevelWindow* window, bool deepRedraw)
	{
		if (window)
		{
			if (const auto dwriteTextVisual = window->GetDWriteTextVisual(); dwriteTextVisual)
			{
				dwriteTextVisual->SetDirtyFlags(0x2); // device lost/update layout
				dwriteTextVisual->SetDirtyFlags(0x4); // update offset
				dwriteTextVisual->SetDirtyFlags(0x200); // update dwrite objects
			}
			if (deepRedraw)
			{
				// clear extra reflection draw geometry command
				if (const auto legacyVisual = window->GetLegacyVisual(); legacyVisual)
				{
					legacyVisual->ClearAll();
				}
			}
			// update nc background
			window->SetDirtyFlags(0x20000);
			// update window visuals
			window->SetDirtyFlags(0x40000);
			// update blur behind
			window->OnBlurBehindUpdated();
			// update system backdrop
			window->OnSystemBackdropUpdated();
		}
	}

	winrt::com_ptr<abi::ICompositionSurface> g_reflectionSurface{ nullptr };
	abi::ICompositionSurface* GetOrCreateReflectionSurface()
	{
		if (!g_reflectionSurface)
		{
			LOG_IF_FAILED(CReflectionVisual::CreateSurface(g_reflectionSurface.put()));
		}

		return g_reflectionSurface.get();
	}
}

HRESULT GlassKernel::MyCCachedVisualImage_CCachedTarget_Update(
	dwmcore::CCachedVisualImage::CCachedTarget* This,
	const D2D1_RECT_F& rect,
	DWM::MilStretch mode,
	const struct RenderTargetInfo& info
)
{
	g_hwnd = nullptr;
	g_CVIHierarchy += 1;
	const auto hr = g_CCachedVisualImage_CCachedTarget_Update_Org(
		This,
		rect,
		mode,
		info
	);
	g_CVIHierarchy -= 1;
	g_hwnd = nullptr;
	return hr;
}

HRESULT GlassKernel::MyCDrawingContext_PreSubgraph(
	dwmcore::CDrawingContext* This,
	dwmcore::CVisualTree* visualTree,
	bool* conditionalBreak
)
{
	const auto isKeyPressed = [](int key)
	{
		return (GetAsyncKeyState(key) & 0x8000);
	};
	if (
		isKeyPressed(VK_CONTROL) &&
		isKeyPressed(VK_SHIFT) &&
		(isKeyPressed(VK_LWIN) || isKeyPressed(VK_RWIN)) &&
		(isKeyPressed('X') || isKeyPressed('Q'))
	)
	{
		__fastfail(HRESULT_FROM_WIN32(ERROR_POSSIBLE_DEADLOCK));
	}

	if (g_CVIHierarchy && !g_hwnd)
	{
		const auto visual = This->GetCurrentVisual();
		if (visual)
		{
			const auto hwnd = visual->GetHwnd();
			if (hwnd)
			{
				if (false)
				{
					if (
						(
							(
								RegisterWindowMessageW(L"WorkerW") == GetClassWord(g_hwnd, GCW_ATOM) ||
								RegisterWindowMessageW(L"Progman") == GetClassWord(g_hwnd, GCW_ATOM)
							) &&
							FindWindowExW(g_hwnd, nullptr, L"SHELLDLL_DefView", nullptr)
						) ||
						(
							RegisterWindowMessageW(L"SHELLDLL_DefView") == GetClassWord(g_hwnd, GCW_ATOM) &&
							(
								RegisterWindowMessageW(L"WorkerW") == GetClassWord(GetParent(g_hwnd), GCW_ATOM) ||
								RegisterWindowMessageW(L"Progman") == GetClassWord(GetParent(g_hwnd), GCW_ATOM)
							)
						)
					)
					{
						const auto d2dContext = This->GetD3DDevice()->GetD2DContext();
						const auto context = d2dContext->GetDeviceContext();
						LOG_IF_FAILED(This->ApplyRenderStateInternal(false));
						LOG_IF_FAILED(This->FlushD2D());
						d2dContext->EnsureBeginDraw();

						if (!context)
						{
							return g_CDrawingContext_PreSubgraph_Org(This, visualTree, conditionalBreak);
						}

						static winrt::com_ptr<ID2D1Bitmap> s_textBitmap{};
						static winrt::com_ptr<ID2D1Image> s_textGlowImage{};

						static dwmcore::CD2DContext* s_d2dContext{ nullptr };
						if (s_d2dContext != d2dContext)
						{
							s_textBitmap = nullptr;
							s_textGlowImage = nullptr;
							s_d2dContext = d2dContext;
						}

						static D2D1_RECT_F s_textLayoutBox{};
						static winrt::com_ptr<IDWriteFactory> s_dwriteFactory{ nullptr };
						static winrt::com_ptr<IDWriteTextFormat> s_dwriteTextFormat{ nullptr };
						static winrt::com_ptr<IDWriteTextLayout> s_dwriteTextLayout{ nullptr };

						if (!s_dwriteFactory)
						{
							THROW_IF_FAILED(
								DWriteCreateFactory(
									DWRITE_FACTORY_TYPE_SHARED,
									__uuidof(IDWriteFactory),
									reinterpret_cast<IUnknown**>(s_dwriteFactory.put())
								)
							);
							THROW_IF_FAILED(
								s_dwriteFactory->CreateTextFormat(
									L"Segoe UI",
									nullptr,
									DWRITE_FONT_WEIGHT_REGULAR,
									DWRITE_FONT_STYLE_NORMAL,
									DWRITE_FONT_STRETCH_NORMAL,
									16.f,
									L"en-us",
									s_dwriteTextFormat.put()
								)
							);
							THROW_IF_FAILED(s_dwriteTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING));
							THROW_IF_FAILED(s_dwriteTextFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
							std::wstring watermarkText
							{
								L"Aero Glass For Windows 10+ (Release Preview) \n"
								L"https://github.com/ALTaleX531/OpenGlass "
							};
							THROW_IF_FAILED(
								s_dwriteFactory->CreateTextLayout(
									watermarkText.c_str(),
									static_cast<UINT32>(watermarkText.size()),
									s_dwriteTextFormat.get(),
									0.f,
									0.f,
									s_dwriteTextLayout.put()
								)
							);
							DWRITE_TEXT_METRICS metrics{};
							THROW_IF_FAILED(s_dwriteTextLayout->GetMetrics(&metrics));
							s_textLayoutBox =
							{
								0.f,
								0.f,
								metrics.widthIncludingTrailingWhitespace,
								metrics.height
							};
							THROW_IF_FAILED(s_dwriteTextLayout->SetMaxWidth(wil::rect_width(s_textLayoutBox)));
						}

						winrt::com_ptr<ID2D1Effect> textGlowEffect{};
						winrt::com_ptr<ID2D1Effect> textMorphologyEffect{};
						if (!s_textBitmap || !s_textGlowImage)
						{
							THROW_IF_FAILED(
								context->CreateEffect(
									CLSID_D2D1Morphology,
									textMorphologyEffect.put()
								)
							);
							THROW_IF_FAILED(
								textMorphologyEffect->SetValue(
									D2D1_MORPHOLOGY_PROP_MODE,
									D2D1_MORPHOLOGY_MODE_DILATE
								)
							);
							THROW_IF_FAILED(
								textMorphologyEffect->SetValue(
									D2D1_MORPHOLOGY_PROP_WIDTH,
									3
								)
							);
							THROW_IF_FAILED(
								textMorphologyEffect->SetValue(
									D2D1_MORPHOLOGY_PROP_HEIGHT,
									3
								)
							);

							THROW_IF_FAILED(
								context->CreateEffect(
									CLSID_D2D1Shadow,
									textGlowEffect.put()
								)
							);
							THROW_IF_FAILED(
								textGlowEffect->SetValue(
									D2D1_PROPERTY_CACHED,
									TRUE
								)
							);
							THROW_IF_FAILED(
								textGlowEffect->SetValue(
									D2D1_SHADOW_PROP_OPTIMIZATION,
									D2D1_GAUSSIANBLUR_OPTIMIZATION_SPEED
								)
							);
							THROW_IF_FAILED(
								textGlowEffect->SetValue(
									D2D1_SHADOW_PROP_COLOR,
									D2D1::ColorF(D2D1::ColorF::Black)
								)
							);
							THROW_IF_FAILED(
								textGlowEffect->SetValue(
									D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION,
									4.f
								)
							);
							textGlowEffect->SetInputEffect(0, textMorphologyEffect.get());

							winrt::com_ptr<ID2D1BitmapRenderTarget> bitmapRT{};
							THROW_IF_FAILED(
								context->CreateCompatibleRenderTarget(
									D2D1::SizeF(
										wil::rect_width(s_textLayoutBox) + 24.f,
										wil::rect_height(s_textLayoutBox) + 24.f
									),
									bitmapRT.put()
								)
							);
							winrt::com_ptr<ID2D1SolidColorBrush> watermarkBrush{};
							THROW_IF_FAILED(bitmapRT->CreateSolidColorBrush(D2D1::ColorF(0xFFFFFF), watermarkBrush.put()));

							bitmapRT->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
							bitmapRT->BeginDraw();
							bitmapRT->Clear();
							bitmapRT->DrawTextLayout(
								D2D1::Point2F(12.f, 12.f),
								s_dwriteTextLayout.get(),
								watermarkBrush.get(),
								D2D1_DRAW_TEXT_OPTIONS_NONE
							);

							THROW_IF_FAILED(bitmapRT->EndDraw());
							THROW_IF_FAILED(bitmapRT->GetBitmap(s_textBitmap.put()));
							textMorphologyEffect->SetInput(0, s_textBitmap.get());
							textGlowEffect->GetOutput(s_textGlowImage.put());
						}

						MONITORINFO monitorInfo{ sizeof(monitorInfo) };
						THROW_IF_WIN32_BOOL_FALSE(GetMonitorInfoW(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &monitorInfo));
						D2D1_POINT_2F origin
						{
							static_cast<float>(monitorInfo.rcWork.right) - 9.f - wil::rect_width(s_textLayoutBox) - 24.f,
							static_cast<float>(monitorInfo.rcWork.bottom) - 9.f - wil::rect_height(s_textLayoutBox) - 24.f,
						};
						const auto deviceTransform = This->GetDeviceTransform()->GetD2DMatrix();
						origin = D2D1::Matrix3x2F::ReinterpretBaseType(&deviceTransform)->TransformPoint(origin);
						context->DrawImage(
							s_textGlowImage.get(),
							&origin,
							nullptr,
							D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR
						);
						context->DrawImage(
							s_textBitmap.get(),
							&origin,
							nullptr,
							D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR
						);
						context->Flush();
					}
				}

				g_hwnd = hwnd;
			}
		}
	}

	return g_CDrawingContext_PreSubgraph_Org(This, visualTree, conditionalBreak);
}

HRESULT GlassKernel::MyCD2DContext_DestroyDeviceResources(dwmcore::CD2DContext* This)
{
	GlassRenderer::DestroyDeviceResources(This);
	return g_CD2DContext_DestroyDeviceResources_Org(This);
}

HRESULT GlassKernel::MyCXXX_ReleaseXXX(PVOID This)
{
	CaptionTextHandler::DestroyDeviceResources();
	g_reflectionSurface = nullptr;
	return g_CXXX_ReleaseXXX_Org(This);
}

void GlassKernel::RedrawAllTopLevelWindow(bool deepRedraw)
{
	ULONG_PTR desktopID{ 0 };
	Util::GetDesktopID(1, &desktopID);
	const auto windowList = uDWM::CDesktopManager::GetInstance()->GetWindowList()->GetWindowListForDesktop(desktopID);
	for (auto i = windowList->Blink; i != windowList; i = i->Blink)
	{
		RedrawTopLevelWindow(
			reinterpret_cast<uDWM::CWindowData*>(i)->GetWindow(),
			deepRedraw
		);
	}
}

float GlassKernel::GetBlurRadius()
{
	if (Shared::IsTransparencyDisabled())
	{
		return 0.f;
	}
	if (Shared::g_useD3DRendering)
	{
		return CD3DGlassRealizer::GetBlurRadius(Shared::g_blurAmount);
	}

	return CGlassRealizer::GetBlurRadius(Shared::g_blurAmount);
}

float GlassKernel::GetColorizationOpacity(bool active, bool maximized)
{
	if (active && !maximized)
	{
		return Shared::g_colorizationOpacity;
	}
	else if (active && maximized)
	{
		return Shared::g_colorizationOpacityMaximized;
	}
	else if (!active && !maximized)
	{
		return Shared::g_colorizationOpacityInactive;
	}
	else if (!active && maximized)
	{
		return Shared::g_colorizationOpacityInactiveMaximized;
	}

	return 0.f;
}
D2D1_COLOR_F GlassKernel::GetBaseColor(bool opaque, bool maximized)
{
	D2D1_COLOR_F color{};

	if (Shared::g_opaqueBlendPriority == Shared::OpaqueBlendPriority::Vista)
	{
		if (maximized)
		{
			color = Shared::g_colorizationBaseMaximized;
		}
		else if (opaque)
		{
			color = Shared::g_colorizationBaseOpaque;
		}
		else
		{
			color = Shared::g_colorizationBaseTransparent;
		}
	}
	else
	{
		if (opaque)
		{
			color = Shared::g_colorizationBaseOpaque;
		}
		else if (maximized)
		{
			color = Shared::g_colorizationBaseMaximized;
		}
		else
		{
			color = Shared::g_colorizationBaseTransparent;
		}
	}

	return color;
}
D2D1_COLOR_F GlassKernel::GetSourceColor(bool active)
{
	if (Shared::g_type == Shared::GlassType::Blur)
	{
		const auto& color = active ? Shared::g_color : Shared::g_colorInactive;
		return
		{
			color.r,
			color.g,
			color.b,
			active ? Shared::g_glassOpacity : Shared::g_glassOpacityInactive
		};
	}
	else if (Shared::g_type == Shared::GlassType::Aero)
	{
		return
		{
			Shared::g_color.r,
			Shared::g_color.g,
			Shared::g_color.b,
			1.f
		};
	}

	return {};
}

D2D1_COLOR_F GlassKernel::CRealizedGlassColorizationParameters::GetEffectivescRGBBlendColor(float sdrBoost) const
{
	const auto scRGBColor = Color::sRGBToscRGB(color, 0.f);
	const auto scRGBAfterglow = Color::sRGBToscRGB(afterglow, 0.f);

	if (Shared::g_type == Shared::GlassType::Aero)
	{
		// dwmcore!CCapturedGlassColorizationParameters::GetEffectivescRGBBlendColor (Windows 7)
		D2D1_COLOR_F effectiveBlendColor = {};
		if (Shared::IsGlassFullyOpaque(0.f, blurBalance, afterglowBalance))
		{
			effectiveBlendColor.r = scRGBColor.r * colorBalance;
			effectiveBlendColor.g = scRGBColor.g * colorBalance;
			effectiveBlendColor.b = scRGBColor.b * colorBalance;
			effectiveBlendColor.a = 1.f;
		}
		else
		{
			const auto alpha = std::max(1.f - blurBalance, 0.1f);
			effectiveBlendColor =
			{
				std::clamp((scRGBColor.r * colorBalance + scRGBAfterglow.r * afterglowBalance * 0.6f) / alpha, 0.f, 1.f),
				std::clamp((scRGBColor.g * colorBalance + scRGBAfterglow.g * afterglowBalance * 0.6f) / alpha, 0.f, 1.f),
				std::clamp((scRGBColor.b * colorBalance + scRGBAfterglow.b * afterglowBalance * 0.6f) / alpha, 0.f, 1.f),
				alpha
			};
		}

		if (sdrBoost > 1.f)
		{
			effectiveBlendColor.r *= sdrBoost;
			effectiveBlendColor.g *= sdrBoost;
			effectiveBlendColor.b *= sdrBoost;
		}
		return effectiveBlendColor;
	}

	if (sdrBoost > 1.f)
	{
		return D2D1::ColorF(
			scRGBColor.r * sdrBoost,
			scRGBColor.g * sdrBoost,
			scRGBColor.b * sdrBoost,
			scRGBColor.a
		);
	}
	return scRGBColor;
}
GlassKernel::CRealizedGlassColorizationParameters GlassKernel::RealizeWindowColorization(
	const D2D1_COLOR_F& baseColor,
	const D2D1_COLOR_F& srcColor,
	float colorizationOpacity,
	bool opaque,
	bool livePreview
)
{
	CRealizedGlassColorizationParameters parameters{};

	if (Shared::g_type == Shared::GlassType::Blur)
	{
		parameters.color = srcColor;
		parameters.color.a *= colorizationOpacity;

		parameters.color.r = (1.f - parameters.color.a) * baseColor.r * baseColor.a + parameters.color.r * parameters.color.a;
		parameters.color.g = (1.f - parameters.color.a) * baseColor.g * baseColor.a + parameters.color.g * parameters.color.a;
		parameters.color.b = (1.f - parameters.color.a) * baseColor.b * baseColor.a + parameters.color.b * parameters.color.a;
		parameters.color.a = (1.f - parameters.color.a) * baseColor.a + parameters.color.a;
		if (parameters.color.a)
		{
			parameters.color.r /= parameters.color.a;
			parameters.color.g /= parameters.color.a;
			parameters.color.b /= parameters.color.a;
		}
	}
	else if (Shared::g_type == Shared::GlassType::Aero)
	{
		// uDWM!CGlassColorizationParameters::AdjustWindowColorization (Windows 7)
		parameters.afterglowBalance = Shared::g_afterglowBalance * (1.f - baseColor.a);
		parameters.blurBalance = Shared::g_blurBalance * (1.f - baseColor.a);

		parameters.afterglow = Shared::g_afterglow;
		parameters.color = srcColor;

		float colorBalance = Shared::g_colorBalance * colorizationOpacity;

		parameters.color.r = (1.f - colorBalance) * baseColor.r * baseColor.a + parameters.color.r * colorBalance;
		parameters.color.g = (1.f - colorBalance) * baseColor.g * baseColor.a + parameters.color.g * colorBalance;
		parameters.color.b = (1.f - colorBalance) * baseColor.b * baseColor.a + parameters.color.b * colorBalance;
		parameters.color.a = (1.f - colorBalance) * baseColor.a + parameters.color.a * colorBalance;
		if (parameters.color.a)
		{
			parameters.color.r /= parameters.color.a;
			parameters.color.g /= parameters.color.a;
			parameters.color.b /= parameters.color.a;
		}

		if (opaque)
		{
			parameters.blurBalance = 0.f;
		}
		else if (!livePreview)
		{
			parameters.blurBalance = 1.f - (1.f - parameters.blurBalance) * colorizationOpacity;
		}

		parameters.colorBalance = parameters.color.a;
	}

	return parameters;
}

float GlassKernel::GetAdjustedReflectionIntensity(bool active, bool maximized)
{
	float baseOpacity = 0.f;

	if (active && !maximized)
	{
		baseOpacity = Shared::g_reflectionOpacity;
	}
	else if (active && maximized)
	{
		baseOpacity = Shared::g_reflectionOpacityMaximized;
	}
	else if (!active && !maximized)
	{
		baseOpacity = Shared::g_reflectionOpacityInactive;
	}
	else if (!active && maximized)
	{
		baseOpacity = Shared::g_reflectionOpacityInactiveMaximized;
	}

	return std::clamp(baseOpacity * Shared::g_reflectionIntensity / 0.5f, 0.f, 1.f);
}

bool GlassKernel::IsCurrentCVIFullyTransparent()
{
	return g_CVIHierarchy && !g_hwnd;
}

void GlassKernel::Update(GlassEngine::UpdateType type)
{
	if (type & GlassEngine::UpdateType::Theme)
	{
		Shared::g_textGlowMode = GlassEngine::GetDwordFromRegistry(L"TextGlowMode", 1);

		WCHAR reflectionTexturePath[MAX_PATH]{};
		GlassEngine::GetStringFromRegistry(L"CustomThemeReflection", reflectionTexturePath);
		PathUnquoteSpacesW(reflectionTexturePath);
		Shared::g_reflectionTexturePath.assign(reflectionTexturePath);

		g_reflectionSurface = nullptr;
	}
	if (type & GlassEngine::UpdateType::Framework)
	{
		Shared::g_disableOnBattery = static_cast<bool>(
			GlassEngine::GetDwordFromRegistry(
				L"DisableGlassOnBattery",
				1
			)
		);
	}
	if (type & GlassEngine::UpdateType::Backdrop)
	{
		Shared::g_type = static_cast<Shared::GlassType>(std::clamp(GlassEngine::GetDwordFromRegistry(L"GlassType", 0), 0ul, 1ul));
		Shared::g_transparencyEnabled = GlassEngine::GetPersonalizeKey() ? Util::IsTransparencyEnabled(GlassEngine::GetPersonalizeKey()) : true;

		DWORD value = 0;
		Shared::g_reflectionIntensity = std::clamp(static_cast<float>(GlassEngine::GetDwordFromRegistry(L"ColorizationGlassReflectionIntensity")) / 100.f, 0.f, 1.f);

		Shared::g_reflectionParallaxIntensity = std::clamp(static_cast<float>(GlassEngine::GetDwordFromRegistry(L"ColorizationGlassReflectionParallaxIntensity", 13)) / 100.f, 0.f, 1.f);
		Shared::g_reflectionPolicy = static_cast<Shared::ReflectionPolicy>(GlassEngine::GetDwordFromRegistry(L"ColorizationGlassReflectionPolicy", 0xFFFFFFFF));
		Shared::g_blurAmount = std::clamp(static_cast<float>(GlassEngine::GetDwordFromRegistry(L"BlurDeviation", 30)) / 10.f * 3.f, 0.f, 250.f);
		Shared::g_blurOptimization = static_cast<D2D1_GAUSSIANBLUR_OPTIMIZATION>(std::clamp(GlassEngine::GetDwordFromRegistry(L"BlurOptimization", 0), 0ul, 2ul));
		Shared::g_roundRectRadius = static_cast<int>(GlassEngine::GetDwordFromRegistry(L"RoundRectRadius"));
		
		value = GlassEngine::GetDwordFromRegistry(L"ColorizationColorOverride", GlassEngine::GetDwordFromRegistry(L"ColorizationColor", 0xFFFFFFFF));
		Shared::g_color = Color::FromArgb(value);
		Shared::g_colorInactive = Color::FromArgb(GlassEngine::GetDwordFromRegistry(L"ColorizationColorInactive", value));

		value = GlassEngine::GetDwordFromRegistry(L"ColorizationOpaqueBlendPriority", 0xFFFFFFFF);
		Shared::g_opaqueBlend = static_cast<int>(GlassEngine::GetDwordFromRegistry(L"ColorizationOpaqueBlend"));
		if (value == 0xFFFFFFFF)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				value = 0;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				value = 1;
			}
		}
		Shared::g_opaqueBlendPriority = static_cast<Shared::OpaqueBlendPriority>(std::clamp(value, 0ul, 1ul));

		Shared::g_useD3DRendering = static_cast<bool>(GlassEngine::GetDwordFromRegistry(L"UseDirect3DRendering", 0));
	}
}

void GlassKernel::Startup()
{
	dwmcore::g_projectionArray.ApplyToVariable("CCachedVisualImage::CCachedTarget::Update", g_CCachedVisualImage_CCachedTarget_Update_Org);
	dwmcore::g_projectionArray.ApplyToVariable("CDrawingContext::PreSubgraph", g_CDrawingContext_PreSubgraph_Org);
	dwmcore::g_projectionArray.ApplyToVariable("CD2DContext::DestroyDeviceResources", g_CD2DContext_DestroyDeviceResources_Org);
	uDWM::g_projectionArray.ApplyToVariable("CGraphicsDeviceManager::ReleaseGraphicsDevice", g_CXXX_ReleaseXXX_Org);
	
	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CCachedVisualImage_CCachedTarget_Update_Org, &MyCCachedVisualImage_CCachedTarget_Update },
			{ &g_CDrawingContext_PreSubgraph_Org, &MyCDrawingContext_PreSubgraph },
			{ &g_CD2DContext_DestroyDeviceResources_Org, &MyCD2DContext_DestroyDeviceResources },
			{ &g_CXXX_ReleaseXXX_Org, &MyCXXX_ReleaseXXX }
		},
		true
	);
}

void GlassKernel::Shutdown()
{
	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CCachedVisualImage_CCachedTarget_Update_Org, &MyCCachedVisualImage_CCachedTarget_Update },
			{ &g_CDrawingContext_PreSubgraph_Org, &MyCDrawingContext_PreSubgraph },
			{ &g_CD2DContext_DestroyDeviceResources_Org, &MyCD2DContext_DestroyDeviceResources },
			{ &g_CXXX_ReleaseXXX_Org, &MyCXXX_ReleaseXXX }
		},
		false
	);

	SwitchToThread();

	CReflectionVisual::RemoveAll();
	g_reflectionSurface = nullptr;
}
