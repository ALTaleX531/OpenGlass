#include "pch.h"
#include "BackdropFactory.hpp"
#include "BlurBackdrop.hpp"
#include "AeroBackdrop.hpp"
#include "AcrylicBackdrop.hpp"
#include "MicaBackdrop.hpp"

using namespace OpenGlass;
namespace OpenGlass::BackdropFactory
{
	enum class BackdropType : CHAR
	{
		Blur,
		Aero,
		Acrylic,
		Mica,
		Invalid
	};
	BackdropType g_type{ BackdropType::Blur };
	float g_blurAmount{ 9.f };
	float g_luminosity{ 0.15f };
	float g_glassOpacity{ 0.63f };
	// exclusively used by acrylic backdrop
	float g_materialOpacity{ 0.02f };
	std::wstring g_materialTexturePath{};
	wuc::CompositionDrawingSurface g_materialTextureSurface{ nullptr };
	wuc::CompositionSurfaceBrush g_materialTextureBrush{ nullptr };
	// exclusively used by aero backdrop
	float g_colorBalance{ 0.f };
	float g_afterglowBalance{ 0.43f };

	std::chrono::steady_clock::time_point g_currentTimeStamp{};
	std::unordered_map<DWORD, wuc::CompositionBrush> g_backdropActiveBrushMap{};
	std::unordered_map<DWORD, wuc::CompositionBrush> g_backdropInactiveBrushMap{};

	wuc::CompositionSurfaceBrush CreateMaterialTextureBrush();
}

wuc::CompositionBrush BackdropFactory::GetOrCreateBackdropBrush(
	const wuc::Compositor& compositor,
	DWORD color,
	bool active,
	uDwm::ACCENT_POLICY* policy
)
{
	// these backdrops has no difference other than color
	if (g_type != BackdropType::Aero)
	{
		active = false;
	}

	auto& map{ active ? g_backdropActiveBrushMap : g_backdropInactiveBrushMap };
	auto it{ map.find(color) };
	// find cached brush
	if (it != map.end())
	{
		if (it->second.Compositor() == compositor)
		{
			return it->second;
		}
		else
		{
			Shutdown();
		}
	}

	wuc::CompositionBrush brush{ nullptr };
	if (policy)
	{
		color = policy->dwGradientColor | 0xFF000000;
		if (policy->AccentState == 3 && !(policy->AccentFlags & 2) && os::buildNumber < os::build_w11_22h2)
		{
			color = 0;
		}

		if (
			policy->AccentState == 4 &&
			policy->dwGradientColor == 0
		)
		{
			brush = compositor.CreateColorBrush({});
		}
		if (policy->AccentState == 2)
		{
			brush = compositor.CreateColorBrush(Utils::FromAbgr(policy->dwGradientColor));
		}
		if (policy->AccentState == 1)
		{
			brush = compositor.CreateColorBrush(Utils::FromAbgr(color));
		}

		if (brush)
		{
			return brush;
		}
	}

	auto winrtColor{ Utils::FromAbgr(color) };
	auto glassOpacity{ policy ? static_cast<float>(policy->dwGradientColor >> 24 & 0xFF) / 255.f : g_glassOpacity };
	switch (g_type)
	{
		case BackdropType::Blur:
		{
			brush = BlurBackdrop::CreateBrush(
				compositor,
				winrtColor,
				glassOpacity,
				g_blurAmount
			);
			break;
		}
		case BackdropType::Aero:
		{
			brush = AeroBackdrop::CreateBrush(
				compositor,
				winrtColor,
				winrtColor,
				policy ? static_cast<float>(policy->dwGradientColor >> 24 & 0xFF) / 255.f : g_colorBalance,
				g_afterglowBalance,
				active ? g_glassOpacity : 0.4f * g_glassOpacity + 0.6f,
				g_blurAmount
			);
			break;
		}
		case BackdropType::Acrylic: 
		{
			auto useLuminosity
			{
				policy &&
				os::buildNumber >= os::build_w11_21h2 &&
				(policy->AccentFlags & 2) != 0 &&
				(
					(policy->AccentState == 3 && os::buildNumber > os::build_w11_21h2) ||
					(policy->AccentState == 4)
				)
			};
			auto luminosity{ policy ? (useLuminosity ? std::optional{ 1.03f } : std::nullopt) : g_luminosity };
			brush = AcrylicBackdrop::CreateBrush(
				compositor,
				g_materialTextureBrush,
				AcrylicBackdrop::GetEffectiveTintColor(winrtColor, glassOpacity, luminosity),
				AcrylicBackdrop::GetEffectiveLuminosityColor(winrtColor, glassOpacity, luminosity),
				g_blurAmount,
				g_materialOpacity
			);
			break;
		}
		case BackdropType::Mica:
		{
			brush = MicaBackdrop::CreateBrush(
				compositor,
				winrtColor,
				winrtColor,
				policy ? static_cast<float>(policy->dwGradientColor >> 24 & 0xFF) / 255.f : g_glassOpacity,
				g_luminosity
			);
			break;
		}
		default:
			brush = compositor.CreateColorBrush(Utils::FromAbgr(policy ? policy->dwGradientColor : color));
			break;
	}

	if (!policy)
	{
		map.insert_or_assign(color, brush);
	}

	return brush;
}

std::chrono::steady_clock::time_point BackdropFactory::GetBackdropBrushTimeStamp()
{
	return g_currentTimeStamp;
}

void BackdropFactory::Shutdown()
{
	g_backdropActiveBrushMap.clear();
	g_backdropInactiveBrushMap.clear();
	g_materialTextureBrush = nullptr;
	g_materialTextureSurface = nullptr;
}

void BackdropFactory::UpdateConfiguration(ConfigurationFramework::UpdateType type)
{
	if (type & ConfigurationFramework::UpdateType::Backdrop)
	{
		g_blurAmount = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"BlurDeviation", 30)) / 10.f * 3.f, 0.f, 250.f);
		g_glassOpacity = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"GlassOpacity", 63)) / 100.f, 0.f, 1.f);
		g_luminosity = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"GlassLuminosity", 15)) / 100.f, 0.f, 1.f);

		auto colorBalance{ ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"ColorizationColorBalanceOverride")};
		if (!colorBalance.has_value())
		{
			colorBalance = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationColorBalance", 0);
		}
		g_colorBalance = std::clamp(static_cast<float>(colorBalance.value()) / 100.f, 0.f, 1.f);
		auto afterglowBalance{ ConfigurationFramework::DwmTryDwordFromHKCUAndHKLM(L"ColorizationAfterglowBalanceOverride") };
		if (!afterglowBalance.has_value())
		{
			afterglowBalance = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"ColorizationAfterglowBalance", 43);
		}
		g_afterglowBalance = std::clamp(static_cast<float>(afterglowBalance.value()) / 100.f, 0.f, 1.f);

		WCHAR materialTexturePath[MAX_PATH + 1]{};
		ConfigurationFramework::DwmGetStringFromHKCUAndHKLM(L"CustomThemeMaterial", materialTexturePath);
		if (g_materialTexturePath != materialTexturePath)
		{
			g_materialTexturePath = materialTexturePath;
			g_materialTextureBrush = nullptr;
		}

		g_materialOpacity = std::clamp(static_cast<float>(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"MaterialOpacity", 2)) / 100.f, 0.f, 1.f);
		if (!g_materialTextureBrush)
		{
			g_materialTextureBrush = CreateMaterialTextureBrush();
		}

		g_type = static_cast<BackdropType>(std::clamp(ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"GlassType", 0), 0ul, 4ul));
		// mica is not available in windows 10
		if (os::buildNumber < os::build_w11_21h2 && g_type == BackdropType::Mica)
		{
			g_type = BackdropType::Invalid;
		}

		g_backdropActiveBrushMap.clear();
		g_backdropInactiveBrushMap.clear();
		g_currentTimeStamp = std::chrono::steady_clock::now();
	}
}

wuc::CompositionSurfaceBrush BackdropFactory::CreateMaterialTextureBrush()
{
	winrt::com_ptr<IStream> stream{ nullptr };
	winrt::com_ptr<dcomp::IDCompositionDesktopDevicePartner> dcompDevice{ nullptr };
	winrt::copy_from_abi(dcompDevice, uDwm::CDesktopManager::s_pDesktopManagerInstance->GetDCompDevice());
	auto compositor{ dcompDevice.as<wuc::Compositor>() };

	if (g_materialTexturePath.empty() || !PathFileExistsW(g_materialTexturePath.data()))
	{
		wil::unique_hmodule wuxcModule{ LoadLibraryExW(L"Windows.UI.Xaml.Controls.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE) };
		THROW_LAST_ERROR_IF_NULL(wuxcModule);
		auto resourceHandle{ FindResourceW(wuxcModule.get(), MAKEINTRESOURCE(2000), RT_RCDATA) };
		THROW_LAST_ERROR_IF_NULL(resourceHandle);
		auto globalHandle{ LoadResource(wuxcModule.get(), resourceHandle)};
		THROW_LAST_ERROR_IF_NULL(globalHandle);
		auto cleanUp = wil::scope_exit([&]
		{
			if (globalHandle)
			{
				UnlockResource(globalHandle);
				FreeResource(globalHandle);
			}
		});
		DWORD resourceSize{ SizeofResource(wuxcModule.get(), resourceHandle)};
		THROW_LAST_ERROR_IF(resourceSize == 0);
		auto resourceAddress{ reinterpret_cast<PBYTE>(LockResource(globalHandle)) };
		stream = { SHCreateMemStream(resourceAddress, resourceSize), winrt::take_ownership_from_abi };
	}
	else
	{
		wil::unique_hfile file{ CreateFileW(g_materialTexturePath.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0) };
		THROW_LAST_ERROR_IF(!file.is_valid());

		LARGE_INTEGER fileSize{};
		THROW_IF_WIN32_BOOL_FALSE(GetFileSizeEx(file.get(), &fileSize));

		auto buffer{ std::make_unique<BYTE[]>(static_cast<size_t>(fileSize.QuadPart)) };
		THROW_IF_WIN32_BOOL_FALSE(ReadFile(file.get(), buffer.get(), static_cast<DWORD>(fileSize.QuadPart), nullptr, nullptr));
		stream = { SHCreateMemStream(buffer.get(), static_cast<UINT>(fileSize.QuadPart)), winrt::take_ownership_from_abi };
	}
	THROW_LAST_ERROR_IF_NULL(stream);

	winrt::com_ptr<IWICImagingFactory2> wicFactory{ nullptr };
	wicFactory.copy_from(uDwm::CDesktopManager::s_pDesktopManagerInstance->GetWICFactory());
	winrt::com_ptr<IWICBitmapDecoder> wicDecoder{ nullptr };
	THROW_IF_FAILED(wicFactory->CreateDecoderFromStream(stream.get(), &GUID_VendorMicrosoft, WICDecodeMetadataCacheOnDemand, wicDecoder.put()));
	winrt::com_ptr<IWICBitmapFrameDecode> wicFrame{ nullptr };
	THROW_IF_FAILED(wicDecoder->GetFrame(0, wicFrame.put()));
	winrt::com_ptr<IWICFormatConverter> wicConverter{ nullptr };
	THROW_IF_FAILED(wicFactory->CreateFormatConverter(wicConverter.put()));
	winrt::com_ptr<IWICPalette> wicPalette{ nullptr };
	THROW_IF_FAILED(
		wicConverter->Initialize(
			wicFrame.get(),
			GUID_WICPixelFormat32bppPBGRA,
			WICBitmapDitherTypeNone,
			wicPalette.get(),
			0, WICBitmapPaletteTypeCustom
		)
	);
	winrt::com_ptr<IWICBitmap> wicBitmap{ nullptr };
	THROW_IF_FAILED(wicFactory->CreateBitmapFromSource(wicConverter.get(), WICBitmapCreateCacheOption::WICBitmapNoCache, wicBitmap.put()));

	UINT width{ 0 }, height{ 0 };
	THROW_IF_FAILED(wicBitmap->GetSize(&width, &height));

	if (!g_materialTextureSurface)
	{
		wuc::CompositionGraphicsDevice graphicsDevice{ nullptr };
		THROW_IF_FAILED(
			compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>()->CreateGraphicsDevice(
				uDwm::CDesktopManager::s_pDesktopManagerInstance->GetD2DDevice(),
				reinterpret_cast<ABI::Windows::UI::Composition::ICompositionGraphicsDevice**>(winrt::put_abi(graphicsDevice))
			)
		);
		g_materialTextureSurface = graphicsDevice.CreateDrawingSurface(
			{ static_cast<float>(width), static_cast<float>(height) },
			wgd::DirectXPixelFormat::R16G16B16A16Float,
			wgd::DirectXAlphaMode::Premultiplied
		);
	}
	else
	{
		g_materialTextureSurface.Resize(
			{
				static_cast<int>(width),
				static_cast<int>(height)
			}
		);
	}

	auto drawingSurfaceInterop{ g_materialTextureSurface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>() };
	POINT offset{ 0, 0 };
	winrt::com_ptr<ID2D1DeviceContext> d2dContext{ nullptr };
	THROW_IF_FAILED(
		drawingSurfaceInterop->BeginDraw(nullptr, IID_PPV_ARGS(d2dContext.put()), &offset)
	);
	d2dContext->Clear();
	winrt::com_ptr<ID2D1Bitmap1> d2dBitmap{ nullptr };
	d2dContext->CreateBitmapFromWicBitmap(
		wicBitmap.get(),
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_NONE,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
		),
		d2dBitmap.put()
	);
	d2dContext->DrawBitmap(d2dBitmap.get());
	THROW_IF_FAILED(
		drawingSurfaceInterop->EndDraw()
	);

	return compositor.CreateSurfaceBrush(g_materialTextureSurface);
}