#include "pch.h"
#include "HookHelper.hpp"
#include "OSHelper.hpp"
#include "uDwmProjection.hpp"
#include "CaptionTextHandler.hpp"
#include "CustomMsstyleLoader.hpp"

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
	HRESULT STDMETHODCALLTYPE MyCText_ValidateResources(uDwm::CText* This);
	HRESULT STDMETHODCALLTYPE MyCText_SetSize(uDwm::CText* This, const SIZE* size);
	HRESULT STDMETHODCALLTYPE MyCMatrixTransformProxy_Update(struct CMatrixTransformProxy* This, MilMatrix3x2D* matrix);
	HRESULT STDMETHODCALLTYPE MyCChannel_MatrixTransformUpdate(dwmcore::CChannel* This, UINT handleIndex, MilMatrix3x2D* matrix);

	decltype(&MyDrawTextW) g_DrawTextW_Org{ nullptr };
	decltype(&MyCreateBitmap) g_CreateBitmap_Org{ nullptr };
	decltype(&MyIWICImagingFactory2_CreateBitmapFromHBITMAP) g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org{ nullptr };
	decltype(&MyCText_ValidateResources) g_CText_ValidateResources_Org{ nullptr };
	decltype(&MyCText_SetSize) g_CText_SetSize_Org{ nullptr };
	decltype(&MyCMatrixTransformProxy_Update) g_CMatrixTransformProxy_Update_Org{ nullptr };
	decltype(&MyCChannel_MatrixTransformUpdate) g_CChannel_MatrixTransformUpdate_Org{ nullptr };
	PVOID* g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address{ nullptr };

	uDwm::CText* g_textVisual{ nullptr };
	LONG g_textWidth{ 0 };
	LONG g_textHeight{ 0 };
	std::optional<COLORREF> g_captionColor{};

	constexpr int standardGlowSize{ 15 };
	int g_textGlowSize{ standardGlowSize };
	bool g_centerCaption{ false };
	bool g_disableTextHooks{ false };
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
		result = g_DrawTextW_Org(hdc, lpchText, cchText, lprc, format);

		g_textWidth = wil::rect_width(*lprc);
		// reservice space for the glow
		lprc->right += g_textGlowSize * 2;
		lprc->bottom += g_textGlowSize * 2;
		// if the text height exceed the height of the text visual
		// then later it will be set to the height of the visual
		g_textHeight = wil::rect_height(*lprc);

		return result;
	}
	// clear the background, so the text can be shown transparent
	// with this hack, we don't need to hook FillRect any more
	BITMAP bmp{};
	if (GetObjectW(GetCurrentObject(hdc, OBJ_BITMAP), sizeof(bmp), &bmp) && bmp.bmBits)
	{
		memset(bmp.bmBits, 0, 4 * bmp.bmWidth * bmp.bmHeight);
	}

	// override that so we can use the correct param in CDrawImageInstruction::Create
	lprc->bottom = lprc->top + g_textHeight;
#ifdef _DEBUG
	{
		FrameRect(hdc, lprc, GetStockBrush(WHITE_BRUSH));
	}
#endif // _DEBUG
	lprc->left += g_textGlowSize;
	lprc->top += g_textGlowSize;
	lprc->right -= g_textGlowSize;
	lprc->bottom -= g_textGlowSize;

#ifdef _DEBUG
	{
		FrameRect(hdc, lprc, GetStockBrush(WHITE_BRUSH));
	}
#endif // _DEBUG

	DTTOPTS options
	{
		sizeof(DTTOPTS),
		DTT_TEXTCOLOR | DTT_COMPOSITED | DTT_CALLBACK | DTT_GLOWSIZE,
		g_captionColor ? g_captionColor.value() : GetTextColor(hdc),
		0,
		0,
		0,
		{},
		0,
		0,
		0,
		0,
		FALSE,
		g_textGlowSize,
		drawTextCallback,
		(LPARAM)&result
	};
	wil::unique_htheme hTheme{ CustomMsstyleLoader::OpenActualThemeData(L"Composited::Window")};

	if (hTheme)
	{
		LOG_IF_FAILED(DrawThemeTextEx(hTheme.get(), hdc, 0, 0, lpchText, cchText, format, lprc, &options));
	}
	else
	{
		LOG_HR_IF_NULL(E_FAIL, hTheme);
		result = g_DrawTextW_Org(hdc, lpchText, cchText, lprc, format);
	}

	lprc->left -= g_textGlowSize;
	lprc->top -= g_textGlowSize;
	lprc->right += g_textGlowSize;
	lprc->bottom += g_textGlowSize;

	return result;
}
HBITMAP WINAPI CaptionTextHandler::MyCreateBitmap(
	int nWidth,
	int nHeight,
	UINT /*nPlanes*/,
	UINT /*nBitCount*/,
	const void* /*lpBits*/
)
{
	if (g_textHeight)
	{
		nHeight = g_textHeight;
	}
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
	WICBitmapAlphaChannelOption /*options*/,
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


HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCText_ValidateResources(uDwm::CText* This)
{
	g_textVisual = This;
	HRESULT hr{ g_CText_ValidateResources_Org(This) };
	g_textVisual = nullptr;

	return hr;
}
HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCText_SetSize(uDwm::CText* This, const SIZE* size)
{
	if (!g_centerCaption)
	{
		return g_CText_SetSize_Org(This, size);
	}

	auto oldWidth = This->GetWidth();
	HRESULT hr{ g_CText_SetSize_Org(This, size) };

	if (oldWidth != size->cx)
	{
		This->SetDirtyFlags(0x8000);
	}

	return hr;
}

HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCMatrixTransformProxy_Update(struct CMatrixTransformProxy* This, MilMatrix3x2D* matrix)
{
	if (g_textVisual)
	{
		matrix->DX -= static_cast<DOUBLE>(g_textGlowSize) * (g_textVisual->IsRTL() ? -1.f : 1.f);
		if (g_centerCaption)
		{
			auto offset = floor(static_cast<DOUBLE>(g_textVisual->GetWidth() - g_textWidth) / 2.);
			matrix->DX += g_textVisual->IsRTL() ? -offset : offset;
		}
		matrix->DY = static_cast<DOUBLE>(static_cast<LONG>(static_cast<DOUBLE>(g_textVisual->GetHeight() - g_textHeight) / 2. - 0.5));
	}

	return g_CMatrixTransformProxy_Update_Org(This, matrix);
}
HRESULT STDMETHODCALLTYPE CaptionTextHandler::MyCChannel_MatrixTransformUpdate(dwmcore::CChannel* This, UINT handleIndex, MilMatrix3x2D* matrix)
{
	if (g_textVisual)
	{
		matrix->DX -= static_cast<DOUBLE>(g_textGlowSize) * (g_textVisual->IsRTL() ? -1.f : 1.f);
		if (g_centerCaption)
		{
			auto offset = floor(static_cast<DOUBLE>(g_textVisual->GetWidth() - g_textWidth) / 2.);
			matrix->DX += g_textVisual->IsRTL() ? -offset : offset;
		}
		matrix->DY = static_cast<DOUBLE>(static_cast<LONG>(static_cast<DOUBLE>(g_textVisual->GetHeight() - g_textHeight) / 2. - 0.5));
	}

	return g_CChannel_MatrixTransformUpdate_Org(This, handleIndex, matrix);
}

void CaptionTextHandler::UpdateConfiguration(ConfigurationFramework::UpdateType type)
{
	if (type & ConfigurationFramework::UpdateType::Backdrop)
	{
		g_centerCaption = static_cast<bool>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"CenterCaption", FALSE));
		g_textGlowSize = standardGlowSize;
		auto glowSize = ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"TextGlowSize");
		if (!glowSize.has_value())
		{
			glowSize = ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"TextGlowMode");
			if (glowSize.has_value())
			{
				g_textGlowSize = HIWORD(glowSize.value());
			}
		}
		else
		{
			g_textGlowSize = glowSize.value();
		}
		g_captionColor = ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"ColorizationColorCaption");
	}
}

HRESULT CaptionTextHandler::Startup()
{
	DWORD value{ 0ul };
	LOG_IF_FAILED_WITH_EXPECTED(
		wil::reg::get_value_dword_nothrow(
			ConfigurationFramework::GetDwmKey(),
			L"DisabledHooks",
			reinterpret_cast<DWORD*>(&value)
		),
		HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
	);
	g_disableTextHooks = (value & 1) != 0;

	if (!g_disableTextHooks)
	{
		if (os::buildNumber < os::build_w11_22h2)
		{
			uDwm::GetAddressFromSymbolMap("CText::ValidateResources", g_CText_ValidateResources_Org);
			uDwm::GetAddressFromSymbolMap("CText::SetSize", g_CText_SetSize_Org);
			uDwm::GetAddressFromSymbolMap("CMatrixTransformProxy::Update", g_CMatrixTransformProxy_Update_Org);
			dwmcore::GetAddressFromSymbolMap("CChannel::MatrixTransformUpdate", g_CChannel_MatrixTransformUpdate_Org);

			wil::unique_hmodule wincodecsMoudle{ LoadLibraryExW(L"WindowsCodecs.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32) };
			RETURN_LAST_ERROR_IF_NULL(wincodecsMoudle);
			const auto WICCreateImagingFactory_Proxy_fn = reinterpret_cast<HRESULT(WINAPI*)(UINT, IWICImagingFactory2**)>(
				GetProcAddress(wincodecsMoudle.get(), "WICCreateImagingFactory_Proxy")
				);
			RETURN_LAST_ERROR_IF_NULL(WICCreateImagingFactory_Proxy_fn);
			winrt::com_ptr<IWICImagingFactory2> wicFactory{ nullptr };
			RETURN_IF_FAILED(WICCreateImagingFactory_Proxy_fn(WINCODEC_SDK_VERSION2, wicFactory.put()));
			g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address = &HookHelper::vtbl_of(wicFactory.get())[21];
			g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org = HookHelper::WritePointer(
				g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address,
				MyIWICImagingFactory2_CreateBitmapFromHBITMAP
			);
			RETURN_LAST_ERROR_IF_NULL(g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org);

			g_DrawTextW_Org = reinterpret_cast<decltype(g_DrawTextW_Org)>(HookHelper::WriteIAT(uDwm::g_moduleHandle, "user32.dll", "DrawTextW", MyDrawTextW));
			g_CreateBitmap_Org = reinterpret_cast<decltype(g_CreateBitmap_Org)>(HookHelper::WriteIAT(uDwm::g_moduleHandle, "gdi32.dll", "CreateBitmap", MyCreateBitmap));

			HookHelper::Detours::Write([]()
			{
				if (os::buildNumber >= os::build_w10_1903)
				{
					HookHelper::Detours::Attach(&g_CMatrixTransformProxy_Update_Org, MyCMatrixTransformProxy_Update);
				}
				else
				{
					HookHelper::Detours::Attach(&g_CChannel_MatrixTransformUpdate_Org, MyCChannel_MatrixTransformUpdate);
				}
				HookHelper::Detours::Attach(&g_CText_SetSize_Org, MyCText_SetSize);
				HookHelper::Detours::Attach(&g_CText_ValidateResources_Org, MyCText_ValidateResources);
			});
		}
	}

	return S_OK;
}
void CaptionTextHandler::Shutdown()
{
	if (!g_disableTextHooks)
	{
		if (os::buildNumber < os::build_w11_22h2)
		{
			HookHelper::Detours::Write([]()
			{
				HookHelper::Detours::Detach(&g_CText_ValidateResources_Org, MyCText_ValidateResources);
				HookHelper::Detours::Detach(&g_CText_SetSize_Org, MyCText_SetSize);
				if (os::buildNumber >= os::build_w10_1903)
				{
					HookHelper::Detours::Detach(&g_CMatrixTransformProxy_Update_Org, MyCMatrixTransformProxy_Update);
				}
				else
				{
					HookHelper::Detours::Detach(&g_CChannel_MatrixTransformUpdate_Org, MyCChannel_MatrixTransformUpdate);
				}
			});

			if (g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address)
			{
				HookHelper::WritePointer(
					g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org_Address,
					g_IWICImagingFactory2_CreateBitmapFromHBITMAP_Org
				);
			}
			if (g_DrawTextW_Org)
			{
				HookHelper::WriteIAT(uDwm::g_moduleHandle, "user32.dll", "DrawTextW", g_DrawTextW_Org);
			}
			if (g_CreateBitmap_Org)
			{
				HookHelper::WriteIAT(uDwm::g_moduleHandle, "gdi32.dll", "CreateBitmap", g_CreateBitmap_Org);
			}

			g_textVisual = nullptr;
			g_textWidth = 0;
			g_textHeight = 0;
		}
	}
}