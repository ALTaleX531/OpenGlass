#include "pch.h"
#include "MsstyleInternals.hpp"
#include "Shared.hpp"
#include "HookHelper.hpp"
#include "uDWMProjection.hpp"
#include "CustomThemeAtlasLoader.hpp"
#include "PngAssetValidation.hpp"
#include "ThemeAtlasLayout.hpp"

using namespace OpenGlass;
namespace OpenGlass::CustomThemeAtlasLoader
{
	decltype(&MyGetThemeStream) g_GetThemeStream_Org{};
	decltype(&MyGetThemeMargins) g_GetThemeMargins_Org{};
	decltype(&MyGetThemeRect) g_GetThemeRect_Org{};
	decltype(&MyGetThemeInt) g_GetThemeInt_Org{};
	decltype(&MyGetThemeColor) g_GetThemeColor_Org{};

	constexpr size_t make_theme_atlas_layout_key(
		int iPartId,
		int iStateId,
		int iPropId
	)
	{
		const auto u1 = static_cast<size_t>(static_cast<uint16_t>(iPartId));
		const auto u2 = static_cast<size_t>(static_cast<uint16_t>(iStateId));
		const auto ui = static_cast<size_t>(static_cast<uint32_t>(iPropId));
		return (u1 << 48) | (u2 << 32) | ui;
	}
	HRESULT LoadThemeAtlas(LPCWSTR themeAtlasPath);
	HRESULT LoadThemeAtlasLayout(LPCWSTR themeAtlasLayoutPath);
	HRESULT LoadTextGlowFromThemeAtlas();
	void LoadTheme(
		LPCWSTR themeAtlasPath,
		LPCWSTR themeAtlasLayoutPath
	);

	enum class CompatibilityQuirk
	{
		None,
		RS1
	};
	auto g_themeAtlasLayoutQuirk = CompatibilityQuirk::None;
	std::unordered_map<size_t, MARGINS> g_themeAtlasLayoutMap{};
	std::unique_ptr<BYTE[]> g_themeAtlasStream{};
	UINT g_themeAtlasStreamSize{};
	std::optional<PngAssetValidation::ImageInfo> g_themeAtlasInfo;
	wil::unique_htheme g_themeHandle{};
	HookHelper::ImportHook g_themeImportHook;
	void UnloadTheme()
	{
		Shared::g_textGlowBitmap.reset();

		g_themeAtlasStream.reset();
		g_themeAtlasStreamSize = 0;
		g_themeAtlasInfo.reset();
		g_themeAtlasLayoutMap.clear();
		g_themeAtlasLayoutQuirk = CompatibilityQuirk::None;
		Shared::g_dontDeflateInactiveFrameGeometry = true;
		Shared::g_captionHeight = std::nullopt;

		g_themeHandle.reset();
	}
}

HRESULT CustomThemeAtlasLoader::MyGetThemeStream(
	HTHEME hTheme,
	int iPartId,
	int iStateId,
	int iPropId,
	VOID** ppvStream,
	DWORD* pcbStream,
	HINSTANCE hInst
)
{
	if (
		hTheme != g_themeHandle.get() ||
		iPartId ||
		iStateId ||
		iPropId != TMT_DISKSTREAM ||
		!g_themeAtlasStreamSize
	)
	{
		return g_GetThemeStream_Org(hTheme, iPartId, iStateId, iPropId, ppvStream, pcbStream, hInst);
	}

	*ppvStream = g_themeAtlasStream.get();
	*pcbStream = g_themeAtlasStreamSize;
	return S_OK;
}

HRESULT CustomThemeAtlasLoader::MyGetThemeMargins(
	HTHEME hTheme,
	HDC hdc,
	int iPartId,
	int iStateId,
	int iPropId,
	LPCRECT prc,
	MARGINS* pMargins
)
{
	decltype(g_themeAtlasLayoutMap)::iterator it;
	if (
		hTheme != g_themeHandle.get() ||
		(
			it = g_themeAtlasLayoutMap.find(
				make_theme_atlas_layout_key(
					iPartId,
					iStateId,
					iPropId
				)
			)
		) == g_themeAtlasLayoutMap.end()
	)
	{
		return g_GetThemeMargins_Org(hTheme, hdc, iPartId, iStateId, iPropId, prc, pMargins);
	}

	*pMargins = it->second;
	return S_OK;
}

HRESULT CustomThemeAtlasLoader::MyGetThemeRect(
	HTHEME hTheme,
	int iPartId,
	int iStateId,
	int iPropId,
	LPRECT pRect
)
{
	decltype(g_themeAtlasLayoutMap)::iterator it;
	if (
		hTheme != g_themeHandle.get() ||
		(
			it = g_themeAtlasLayoutMap.find(
				make_theme_atlas_layout_key(
					iPartId,
					iStateId,
					iPropId
				)
			)
		) == g_themeAtlasLayoutMap.end()
	)
	{
		return g_GetThemeRect_Org(hTheme, iPartId, iStateId, iPropId, pRect);
	}

	memcpy_s(
		pRect,
		sizeof(*pRect),
		&it->second,
		sizeof(it->second)
	);
	return S_OK;
}

// openglass exclusive
HRESULT CustomThemeAtlasLoader::MyGetThemeInt(
	HTHEME hTheme,
	int iPartId,
	int iStateId,
	int iPropId,
	int* piVal
)
{
	decltype(g_themeAtlasLayoutMap)::iterator it;
	if (
		hTheme != g_themeHandle.get() ||
		(
			it = g_themeAtlasLayoutMap.find(
				make_theme_atlas_layout_key(
					iPartId,
					iStateId,
					iPropId
				)
			)
		) == g_themeAtlasLayoutMap.end()
	)
	{
		return g_GetThemeInt_Org(hTheme, iPartId, iStateId, iPropId, piVal);
	}

	*piVal = it->second.cxLeftWidth;
	return S_OK;
}

HRESULT CustomThemeAtlasLoader::MyGetThemeColor(
	HTHEME hTheme,
	int iPartId,
	int iStateId,
	int iPropId,
	COLORREF* pColor
)
{
	decltype(g_themeAtlasLayoutMap)::iterator it;
	if (
		hTheme != g_themeHandle.get() ||
		(
			it = g_themeAtlasLayoutMap.find(
				make_theme_atlas_layout_key(
					iPartId,
					iStateId,
					iPropId
				)
			)
		) == g_themeAtlasLayoutMap.end()
	)
	{
		return g_GetThemeColor_Org(hTheme, iPartId, iStateId, iPropId, pColor);
	}

	*pColor = RGB(it->second.cxLeftWidth, it->second.cxRightWidth, it->second.cyTopHeight);
	return S_OK;
}
HTHEME CustomThemeAtlasLoader::GetThemeHandle()
{
	return g_themeHandle.get();
}

HRESULT CustomThemeAtlasLoader::LoadThemeAtlas(LPCWSTR themeAtlasPath)
{
	if (
		wcslen(themeAtlasPath) != 0 &&
		!PathIsRelativeW(themeAtlasPath) &&
		!PathIsNetworkPathW(themeAtlasPath) &&
		PathFileExistsW(themeAtlasPath)
	)
	{
		wil::unique_hfile file{ CreateFileW(themeAtlasPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0) };
		RETURN_LAST_ERROR_IF(!file.is_valid());

		LARGE_INTEGER fileSize{};
		RETURN_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file.get(), &fileSize));

		RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), fileSize.QuadPart <= 0 || fileSize.QuadPart > PngAssetValidation::MaximumFileSize);
		const UINT candidateSize = static_cast<UINT>(fileSize.QuadPart);
		auto candidate = std::make_unique<BYTE[]>(candidateSize);
		DWORD bytesRead{};
		RETURN_IF_WIN32_BOOL_FALSE(ReadFile(file.get(), candidate.get(), candidateSize, &bytesRead, nullptr));
		RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_HANDLE_EOF), bytesRead != candidateSize);

		PngAssetValidation::ImageInfo candidateInfo{};
		RETURN_IF_FAILED(PngAssetValidation::ValidateStructure(
			{ reinterpret_cast<const std::byte*>(candidate.get()), candidateSize },
			candidateInfo
		));
		winrt::com_ptr<IStream> stream{ SHCreateMemStream(candidate.get(), candidateSize), winrt::take_ownership_from_abi };
		RETURN_HR_IF_NULL(E_OUTOFMEMORY, stream);
		winrt::com_ptr<IWICImagingFactory> factory{ nullptr };
		factory.copy_from(uDWM::CDesktopManager::GetInstance()->GetWICFactory());
		winrt::com_ptr<IWICFormatConverter> converter{ nullptr };
		RETURN_IF_FAILED(PngAssetValidation::CreateValidatedWicSource(
			factory.get(),
			stream.get(),
			&candidateInfo,
			converter.put()
		));

		g_themeAtlasStreamSize = candidateSize;
		g_themeAtlasStream = std::move(candidate);
		g_themeAtlasInfo = candidateInfo;
	}
	else
	{
		WCHAR themeFileName[MAX_PATH]{};
		RETURN_IF_FAILED(
			GetCurrentThemeName(
				themeFileName,
				std::size(themeFileName),
				nullptr,
				0,
				nullptr,
				0
			)
		);

		wil::unique_hmodule themeResource{ LoadLibraryExW(themeFileName, nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_SEARCH_SYSTEM32) };
		RETURN_LAST_ERROR_IF_NULL(themeResource);

		PVOID streamAddress;
		DWORD streamSize;
		RETURN_IF_FAILED(
			GetThemeStream(
				g_themeHandle.get(),
				static_cast<int>(DWM_WINDOW_THEME_PART::COMMON),
				0,
				TMT_DISKSTREAM,
				&streamAddress,
				&streamSize,
				themeResource.get()
			)
		);

		g_themeAtlasStreamSize = streamSize;
		g_themeAtlasStream = std::make_unique<BYTE[]>(g_themeAtlasStreamSize);
		g_themeAtlasInfo.reset();
		memcpy_s(
			g_themeAtlasStream.get(),
			g_themeAtlasStreamSize,
			streamAddress,
			streamSize
		);
	}

	return S_OK;
}

HRESULT CustomThemeAtlasLoader::LoadThemeAtlasLayout(LPCWSTR themeAtlasLayoutPath) try
{
	if (!PathFileExistsW(themeAtlasLayoutPath)) return S_FALSE;

	wil::unique_hfile file{ CreateFileW(themeAtlasLayoutPath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr) };
	RETURN_LAST_ERROR_IF(!file.is_valid());
	LARGE_INTEGER fileSize{};
	RETURN_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file.get(), &fileSize));
	RETURN_HR_IF(
		HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE),
		fileSize.QuadPart < 0 || fileSize.QuadPart > ThemeAtlasLayout::MaximumFileSize
	);

	std::vector<std::byte> bytes(static_cast<std::size_t>(fileSize.QuadPart));
	if (!bytes.empty())
	{
		DWORD bytesRead{};
		RETURN_IF_WIN32_BOOL_FALSE(ReadFile(file.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &bytesRead, nullptr));
		RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_HANDLE_EOF), bytesRead != static_cast<DWORD>(bytes.size()));
	}

	ThemeAtlasLayout::Document document;
	RETURN_IF_FAILED(ThemeAtlasLayout::Parse(bytes, document));

	auto layoutMap = g_themeAtlasLayoutMap;
	auto quirk = g_themeAtlasLayoutQuirk;
	auto dontDeflateInactiveFrameGeometry = Shared::g_dontDeflateInactiveFrameGeometry;
	auto captionHeight = Shared::g_captionHeight;
	for (const auto& record : document.records)
	{
		if (const auto mapping = std::get_if<ThemeAtlasLayout::Mapping>(&record))
		{
			auto part = mapping->part;
			if (quirk == CompatibilityQuirk::RS1 && part > 10) --part;
			layoutMap.try_emplace(
				make_theme_atlas_layout_key(part, mapping->state, mapping->property),
				MARGINS
				{
					mapping->value[0],
					mapping->value[1],
					mapping->value[2],
					mapping->value[3]
				}
			);
			continue;
		}

		const auto& property = std::get<ThemeAtlasLayout::Property>(record);
		if (property.name == "DontDeflateInactiveFrameGeometry")
		{
			dontDeflateInactiveFrameGeometry = property.value != 0;
		}
		else if (property.name == "RS1Compatibility")
		{
			if (property.value) quirk = CompatibilityQuirk::RS1;
		}
		else if (property.name == "CaptionHeight")
		{
			captionHeight = static_cast<DWORD>(property.value);
		}
	}

	g_themeAtlasLayoutMap = std::move(layoutMap);
	g_themeAtlasLayoutQuirk = quirk;
	Shared::g_dontDeflateInactiveFrameGeometry = dontDeflateInactiveFrameGeometry;
	Shared::g_captionHeight = captionHeight;
	return S_OK;
}
CATCH_RETURN()

HRESULT CustomThemeAtlasLoader::LoadTextGlowFromThemeAtlas()
{
	RECT textGlowAtlasRect{};
	RETURN_IF_FAILED(
		MyGetThemeRect(
			g_themeHandle.get(),
			static_cast<int>(DWM_WINDOW_THEME_PART::TEXTGLOW),
			0,
			TMT_ATLASRECT,
			&textGlowAtlasRect
		)
	);
	RETURN_IF_WIN32_BOOL_FALSE(InflateRect(&textGlowAtlasRect, -1, -1));
	winrt::com_ptr<IStream> stream{ SHCreateMemStream(g_themeAtlasStream.get(), g_themeAtlasStreamSize), winrt::take_ownership_from_abi };
	RETURN_HR_IF_NULL(E_OUTOFMEMORY, stream);
	winrt::com_ptr<IWICImagingFactory> wicFactory{ nullptr };
	wicFactory.copy_from(uDWM::CDesktopManager::GetInstance()->GetWICFactory());
	winrt::com_ptr<IWICFormatConverter> wicConverter{ nullptr };
	RETURN_IF_FAILED(PngAssetValidation::CreateValidatedWicSource(
		wicFactory.get(),
		stream.get(),
		g_themeAtlasInfo ? &*g_themeAtlasInfo : nullptr,
		wicConverter.put()
	));

	Shared::g_textGlowBitmapInfo =
	{
		{
			sizeof(Shared::g_textGlowBitmapInfo.bmiHeader),
			wil::rect_width(textGlowAtlasRect),
			-wil::rect_height(textGlowAtlasRect),
			1,
			32,
			BI_RGB
		}
	};
	Shared::g_textGlowBitmap.reset(
		CreateDIBSection(
			nullptr,
			&Shared::g_textGlowBitmapInfo,
			DIB_RGB_COLORS,
			&Shared::g_textGlowBitmapPixels,
			nullptr,
			0
		)
	);
	RETURN_LAST_ERROR_IF_NULL(Shared::g_textGlowBitmap);

	WICRect copyRect{ textGlowAtlasRect.left, textGlowAtlasRect.top, Shared::g_textGlowBitmapInfo.bmiHeader.biWidth, -Shared::g_textGlowBitmapInfo.bmiHeader.biHeight };
	RETURN_IF_FAILED(
		wicConverter->CopyPixels(
			&copyRect,
			Shared::g_textGlowBitmapInfo.bmiHeader.biWidth * 4,
			Shared::g_textGlowBitmapInfo.bmiHeader.biWidth * std::abs(Shared::g_textGlowBitmapInfo.bmiHeader.biHeight) * 4,
			reinterpret_cast<BYTE*>(Shared::g_textGlowBitmapPixels)
		)
	);

	return S_OK;
}

void CustomThemeAtlasLoader::LoadTheme(LPCWSTR themeAtlasPath, LPCWSTR themeAtlasLayoutPath)
{
	g_themeHandle.reset(OpenThemeData(nullptr, L"DWMWindow"));

	if (SUCCEEDED(LoadThemeAtlas(themeAtlasPath)))
	{
		LOG_IF_FAILED(LoadThemeAtlasLayout(themeAtlasLayoutPath));
		LOG_IF_FAILED(LoadTextGlowFromThemeAtlas());
	}
}

void CustomThemeAtlasLoader::Update(GlassEngine::UpdateType type)
{
	if (type & GlassEngine::UpdateType::Theme)
	{
		WCHAR themeAtlasPath[MAX_PATH]{}, themeAtlasLayoutPath[MAX_PATH]{};
		GlassEngine::GetStringFromRegistry(
			L"CustomThemeAtlas",
			themeAtlasPath
		);
		PathUnquoteSpacesW(themeAtlasPath);
		wcscpy_s(themeAtlasLayoutPath, themeAtlasPath);
		wcscat_s(themeAtlasLayoutPath, L".layout");

		UnloadTheme();
		LoadTheme(
			themeAtlasPath,
			themeAtlasLayoutPath
		);
	}
	if (type & GlassEngine::UpdateType::Backdrop || type & GlassEngine::UpdateType::Theme)
	{
		DWORD active = 0, inactive = 0, maximized = 0, inactiveMaximized = 0, transparent = 0, opaque = 0;
		const auto themeHandle = CustomThemeAtlasLoader::GetThemeHandle();

		active = GlassEngine::GetDwordFromRegistry(L"ColorizationGlassReflectionOpacity", 0xFFFFFFFE);
		inactive = GlassEngine::GetDwordFromRegistry(L"ColorizationGlassReflectionOpacityInactive", active);
		maximized = GlassEngine::GetDwordFromRegistry(L"ColorizationGlassReflectionOpacityMaximized", active);
		inactiveMaximized = GlassEngine::GetDwordFromRegistry(L"ColorizationGlassReflectionOpacityInactiveMaximized", inactive);

		if (active == 0xFFFFFFFE)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				active = 90;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				active = 90;
			}
		}
		else if (active == 0xFFFFFFFF)
		{
			if (themeHandle)
			{
				active = 90;
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::SQUEEGEREFLECTIONMAP), 1, TMT_OPACITY, reinterpret_cast<int*>(&active));
			}
		}
		if (inactive == 0xFFFFFFFE)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				inactive = 40;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				inactive = 75;
			}
		}
		else if (inactive == 0xFFFFFFFF)
		{
			if (themeHandle)
			{
				inactive = 75;
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::SQUEEGEREFLECTIONMAP), 2, TMT_OPACITY, reinterpret_cast<int*>(&inactive));
			}
		}
		if (maximized == 0xFFFFFFFE)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				maximized = 50;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				maximized = 90;
			}
		}
		else if (maximized == 0xFFFFFFFF)
		{
			if (themeHandle)
			{
				maximized = 90;
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::SQUEEGEREFLECTIONMAP), 3, TMT_OPACITY, reinterpret_cast<int*>(&maximized));
			}
		}
		if (inactiveMaximized == 0xFFFFFFFE)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				inactiveMaximized = 20;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				inactiveMaximized = 75;
			}
		}
		else if (inactiveMaximized == 0xFFFFFFFF)
		{
			if (themeHandle)
			{
				inactiveMaximized = 75;
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::SQUEEGEREFLECTIONMAP), 4, TMT_OPACITY, reinterpret_cast<int*>(&inactiveMaximized));
			}
		}

		Shared::g_reflectionOpacity = std::clamp(static_cast<float>(active) / 100.f, 0.f, 1.f);
		Shared::g_reflectionOpacityInactive = std::clamp(static_cast<float>(inactive) / 100.f, 0.f, 1.f);
		Shared::g_reflectionOpacityMaximized = std::clamp(static_cast<float>(maximized) / 100.f, 0.f, 1.f);
		Shared::g_reflectionOpacityInactiveMaximized = std::clamp(static_cast<float>(inactiveMaximized) / 100.f, 0.f, 1.f);

		active = 0, inactive = 0, maximized = 0, inactiveMaximized = 0;
		active = GlassEngine::GetDwordFromRegistry(L"ColorizationOpacity", 0xFFFFFFFE);
		inactive = GlassEngine::GetDwordFromRegistry(L"ColorizationOpacityInactive", active);
		maximized = GlassEngine::GetDwordFromRegistry(L"ColorizationOpacityMaximized", active);
		inactiveMaximized = GlassEngine::GetDwordFromRegistry(L"ColorizationOpacityInactiveMaximized", inactive);

		if (active == 0xFFFFFFFE)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				active = 100;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				active = 100;
			}
		}
		else if (active == 0xFFFFFFFF)
		{
			if (themeHandle)
			{
				active = 100;
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 1, TMT_COLORIZATIONOPACITY, reinterpret_cast<int*>(&active));
			}
		}
		if (inactive == 0xFFFFFFFE)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				inactive = 55;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				inactive = 40;
			}
		}
		else if (inactive == 0xFFFFFFFF)
		{
			if (themeHandle)
			{
				inactive = 40;
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 2, TMT_COLORIZATIONOPACITY, reinterpret_cast<int*>(&inactive));
			}
		}
		if (maximized == 0xFFFFFFFE)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				maximized = 75;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				maximized = 100;
			}
		}
		else if (maximized == 0xFFFFFFFF)
		{
			if (themeHandle)
			{
				maximized = 100;
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 3, TMT_COLORIZATIONOPACITY, reinterpret_cast<int*>(&maximized));
			}
		}
		if (inactiveMaximized == 0xFFFFFFFE)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				inactiveMaximized = 75;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				inactiveMaximized = 40;
			}
		}
		else if (inactiveMaximized == 0xFFFFFFFF)
		{
			if (themeHandle)
			{
				inactiveMaximized = 40;
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 4, TMT_COLORIZATIONOPACITY, reinterpret_cast<int*>(&inactiveMaximized));
			}
		}

		Shared::g_colorizationOpacity = std::clamp(static_cast<float>(active) / 100.f, 0.f, 1.f);
		Shared::g_colorizationOpacityInactive = std::clamp(static_cast<float>(inactive) / 100.f, 0.f, 1.f);
		Shared::g_colorizationOpacityMaximized = std::clamp(static_cast<float>(maximized) / 100.f, 0.f, 1.f);
		Shared::g_colorizationOpacityInactiveMaximized = std::clamp(static_cast<float>(inactiveMaximized) / 100.f, 0.f, 1.f);

		active = 0, inactive = 0, maximized = 0, inactiveMaximized = 0;
		transparent = GlassEngine::GetDwordFromRegistry(L"ColorizationBaseTransparent", 0xFFFFFFFE);
		maximized = GlassEngine::GetDwordFromRegistry(L"ColorizationBaseMaximized", 0xFFFFFFFE);
		opaque = GlassEngine::GetDwordFromRegistry(L"ColorizationBaseOpaque", 0xFFFFFFFE);

		if (transparent == 0xFFFFFFFE)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				transparent = 0x00000000;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				transparent = 0x00000000;
			}
		}
		else if (transparent == 0xFFFFFFFF)
		{
			if (themeHandle)
			{
				transparent = 0x00000000;
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 1, TMT_COLORIZATIONCOLOR, reinterpret_cast<int*>(&transparent));
			}
		}
		if (maximized == 0xFFFFFFFE)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				maximized = 0xFF000000;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				maximized = 0x00000000;
			}
		}
		else if (maximized == 0xFFFFFFFF)
		{
			if (themeHandle)
			{
				maximized = 0x00000000;
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::TOPFRAME), 3, TMT_COLORIZATIONCOLOR, reinterpret_cast<int*>(&maximized));
			}
		}
		if (opaque == 0xFFFFFFFE)
		{
			if (Shared::g_type == Shared::GlassType::Blur)
			{
				opaque = 0xFFDFDFDF;
			}
			else if (Shared::g_type == Shared::GlassType::Aero)
			{
				opaque = 0xFFDFDFDF;
			}
		}
		else if (opaque == 0xFFFFFFFF)
		{
			if (themeHandle)
			{
				opaque = 0xFFDFDFDF;
				CustomThemeAtlasLoader::MyGetThemeInt(themeHandle, static_cast<int>(DWM_WINDOW_THEME_PART::COMMON), 0, TMT_COLORIZATIONCOLOR, reinterpret_cast<int*>(&opaque));
			}
		}

		Shared::g_colorizationBaseTransparent = Color::FromArgb(transparent, false);
		Shared::g_colorizationBaseMaximized = Color::FromArgb(maximized, false);
		Shared::g_colorizationBaseOpaque = Color::FromArgb(opaque, false);
	}
}

void CustomThemeAtlasLoader::Startup()
{
	const auto build_before_w10_2004 = uDWM::g_versionInfo.build < os::build_w10_2004;
	g_themeImportHook.Prepare(
		uDWM::g_moduleHandle,
		std::initializer_list<HookHelper::ImportDllDetourInfo>
		{
			{
				"ext-ms-win-uxtheme-themes-l1-1-0.dll",
				std::initializer_list<HookHelper::ImportFunctionDetourInfo>
				{
					HookHelper::MakeImportDetour<&MyGetThemeMargins>("GetThemeMargins", &g_GetThemeMargins_Org),
					HookHelper::MakeImportDetour<&MyGetThemeInt>("GetThemeInt", &g_GetThemeInt_Org),
					HookHelper::MakeImportDetour<&MyGetThemeColor>("GetThemeColor", &g_GetThemeColor_Org)
				}
			},
			{
				build_before_w10_2004 ? "UxTheme.dll" : "ext-ms-win-uxtheme-themes-l1-1-2.dll",
				std::initializer_list<HookHelper::ImportFunctionDetourInfo>
				{
					HookHelper::MakeImportDetour<&MyGetThemeStream>("GetThemeStream", &g_GetThemeStream_Org),
					HookHelper::MakeImportDetour<&MyGetThemeRect>("GetThemeRect", &g_GetThemeRect_Org)
				}
			}
		},
		true
	);
	HookHelper::GetCurrentHookTransaction().Apply(g_themeImportHook);
}
void CustomThemeAtlasLoader::Activate()
{
	PostMessageW(FindWindowW(L"DWM", nullptr), WM_THEMECHANGED, 0, 0);
}
void CustomThemeAtlasLoader::Shutdown()
{
	HookHelper::GetCurrentHookTransaction().Apply(g_themeImportHook);
}
void CustomThemeAtlasLoader::Deactivate()
{
	SendMessageW(FindWindowW(L"DWM", nullptr), WM_THEMECHANGED, 0, 0);
	UnloadTheme();
}
