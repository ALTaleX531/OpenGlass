#pragma once
#include "resource.h"
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "uDwmProjection.hpp"

namespace OpenGlass
{
	class CGlassReflectionVisual
	{
		static inline wuc::CompositionDrawingSurface s_reflectionSurface{ nullptr };
		static inline std::wstring s_reflectionPath{};
		static inline winrt::com_ptr<dcomp::IDCompositionDesktopDevicePartner> s_dcompDevice{ nullptr };
		static inline float s_parallaxIntensity{ 0.1f };
		static inline float s_intensity{ 0.f };

		uDwm::CTopLevelWindow* m_window{ nullptr };
		uDwm::CWindowData* m_data{ nullptr };
		wuc::SpriteVisual m_visual{ nullptr };
		wuc::CompositionSurfaceBrush m_brush{ nullptr };
		HMONITOR m_monitor{ nullptr };
		RECT m_monitorRect{};
		RECT m_windowRect{};
		float m_parallaxIntensity{ 0.f };
		wfn::float3 m_offsetToWindow{};
		bool m_forceUpdate{ false };
		bool m_cloned{ false };
	public:
		CGlassReflectionVisual(uDwm::CTopLevelWindow* window, uDwm::CWindowData* data, bool cloned = false) : m_window{ window }, m_data{ data }, m_cloned{ cloned } {}
		~CGlassReflectionVisual()
		{
			UninitializeVisual();
		}
		wuc::SpriteVisual GetVisual() const
		{
			return m_visual;
		}

		void InitializeVisual(const wuc::Compositor& compositor)
		{
			m_brush = compositor.CreateSurfaceBrush();
			m_brush.Stretch(wuc::CompositionStretch::None);
			m_brush.HorizontalAlignmentRatio(0.f);
			m_brush.VerticalAlignmentRatio(0.f);
			m_visual = compositor.CreateSpriteVisual();
			m_visual.Brush(m_brush);
			m_visual.RelativeSizeAdjustment({ 1.f, 1.f });
		}
		void UninitializeVisual()
		{
			m_brush = nullptr;
			m_visual = nullptr;
		}
		void SyncReflectionData(const CGlassReflectionVisual& reflecctionVisual)
		{
			m_brush.Surface(reflecctionVisual.m_brush.Surface());
			m_brush.Scale(reflecctionVisual.m_brush.Scale());
			m_brush.Offset(reflecctionVisual.m_brush.Offset());
			m_visual.Opacity(reflecctionVisual.m_visual.Opacity());
		}
		void NotifyOffsetToWindow(wfn::float3 offset)
		{
			if (m_cloned) { return; }
			if (m_offsetToWindow != offset)
			{
				m_offsetToWindow = offset;
				m_forceUpdate = true;
			}
		}
		void ValidateVisual() try
		{
			if (m_cloned) { return; }

			EnsureGlassSurface();
			if (m_brush.Surface() != s_reflectionSurface)
			{
				m_forceUpdate = true;
				m_brush.Surface(s_reflectionSurface);
			}
			if (m_visual.Opacity() != s_intensity)
			{
				m_visual.Opacity(s_intensity);
				if (s_intensity == 0.f)
				{
					m_visual.IsVisible(false);
				}
				else
				{
					m_visual.IsVisible(true);
				}
			}
			if (!m_visual.IsVisible())
			{
				return;
			}

			RECT windowRect{};
			THROW_HR_IF_NULL(E_INVALIDARG, m_window->GetActualWindowRect(&windowRect, false, true, false));

			HWND hwnd{ m_data->GetHwnd() };
			HMONITOR monitor{ MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) };
			THROW_LAST_ERROR_IF_NULL(monitor);

			MONITORINFO mi{ sizeof(MONITORINFO) };
			THROW_IF_WIN32_BOOL_FALSE(GetMonitorInfoW(monitor, &mi));

			bool scallingChanged{ false };
			bool offsetChanged{ false };
			if (
				m_forceUpdate ||
				m_monitor != monitor ||
				!EqualRect(&m_monitorRect, &mi.rcMonitor) ||
				m_parallaxIntensity != s_parallaxIntensity
			)
			{
				scallingChanged = true;
			}
			if (
				scallingChanged || 
				m_forceUpdate ||
				(m_windowRect.left != windowRect.left) || 
				(m_windowRect.top != windowRect.top) || 
				m_parallaxIntensity != s_parallaxIntensity
			)
			{
				offsetChanged = true;
			}
			m_monitor = monitor;
			m_monitorRect = mi.rcMonitor;
			m_windowRect = windowRect;
			m_parallaxIntensity = s_parallaxIntensity;
			m_forceUpdate = false;

			auto surfaceSize{ s_reflectionSurface.SizeInt32() };
			if (scallingChanged)
			{
				m_brush.Scale(
					winrt::Windows::Foundation::Numerics::float2
					{ 
						(static_cast<float>(wil::rect_width(m_monitorRect)) / static_cast<float>(surfaceSize.Width)) + 0.01f,
						(static_cast<float>(wil::rect_height(m_monitorRect)) / static_cast<float>(surfaceSize.Height)) + 0.01f
					}
				);
			}
			if (offsetChanged)
			{
				// when window is maximized, windowRect.left and windowRect.top are both 0
				// it is not the real window position, needs to add border margins
				m_brush.Offset(
					winrt::Windows::Foundation::Numerics::float2
					{
						-static_cast<float>((windowRect.left) + (!IsMaximized(hwnd) ? m_offsetToWindow.x : 0) - mi.rcMonitor.left) * (1.f - m_parallaxIntensity),
						-static_cast<float>((windowRect.top) + (!IsMaximized(hwnd) ? m_offsetToWindow.y : 0) - mi.rcMonitor.top),
					}
				);
			}
		}
		CATCH_LOG_RETURN()

		static void UpdateIntensity(float intensity)
		{
			if (s_intensity != intensity)
			{
				s_intensity = intensity;
			}
		}
		static void UpdateParallaxIntensity(float intensity)
		{
			if (s_parallaxIntensity != intensity)
			{
				s_parallaxIntensity = intensity;
			}
		}
		static void EnsureGlassSurface()
		{
			if (!s_reflectionSurface || !uDwm::CheckDeviceState(s_dcompDevice))
			{
				s_dcompDevice.copy_from(uDwm::CDesktopManager::s_pDesktopManagerInstance->GetDCompDevice());
				UpdateReflectionSurface(s_reflectionPath.data());
			}
		}
		static void UpdateReflectionSurface(std::wstring_view reflectionPath) try
		{
			winrt::com_ptr<IStream> stream{ nullptr };

			if (s_reflectionPath != reflectionPath)
			{
				s_reflectionPath = reflectionPath;
			}
			if (reflectionPath.empty() || !PathFileExistsW(reflectionPath.data()))
			{
				HMODULE currentModule{ wil::GetModuleInstanceHandle() };
				auto resourceHandle{ FindResourceW(currentModule, MAKEINTRESOURCE(IDB_REFLECTION), L"PNG") };
				THROW_LAST_ERROR_IF_NULL(resourceHandle);
				auto globalHandle{ LoadResource(currentModule, resourceHandle) };
				THROW_LAST_ERROR_IF_NULL(globalHandle);
				auto cleanUp = wil::scope_exit([&]
				{
					if (globalHandle)
					{
						UnlockResource(globalHandle);
						FreeResource(globalHandle);
					}
				});
				DWORD resourceSize{ SizeofResource(currentModule, resourceHandle) };
				THROW_LAST_ERROR_IF(resourceSize == 0);
				auto resourceAddress{ reinterpret_cast<PBYTE>(LockResource(globalHandle)) };
				stream = { SHCreateMemStream(resourceAddress, resourceSize), winrt::take_ownership_from_abi };
			}
			else
			{
				wil::unique_hfile file{ CreateFileW(reflectionPath.data(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, 0)};
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

			auto compositor{ s_dcompDevice.as<wuc::Compositor>() };
			wuc::CompositionGraphicsDevice graphicsDevice{ nullptr };
			THROW_IF_FAILED(
				compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>()->CreateGraphicsDevice(
					uDwm::CDesktopManager::s_pDesktopManagerInstance->GetD2DDevice(),
					reinterpret_cast<ABI::Windows::UI::Composition::ICompositionGraphicsDevice**>(winrt::put_abi(graphicsDevice))
				)
			);
			s_reflectionSurface = graphicsDevice.CreateDrawingSurface(
				{ static_cast<float>(width), static_cast<float>(height) },
				wgd::DirectXPixelFormat::B8G8R8A8UIntNormalized,
				wgd::DirectXAlphaMode::Premultiplied
			);
			auto drawingSurfaceInterop{ s_reflectionSurface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>() };
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
		}
		CATCH_LOG_RETURN()
	};
}