#include "ColorHooks.Shell.hpp"
#include "DwmSettings.hpp"
#include "ShellColorRefresh.hpp"
#include "DesktopWallpaper.hpp"
#include <shobjidl.h>
#include <wil/cppwinrt.h>
#include <wil/registry.h>

using namespace OpenGlass;

namespace winrt
{
	// QI for either inherited interface must retain the proxy's COM identity.
	template<>
	bool is_guid_of<OpenGlass::IDesktopWallpaperPrivate>(guid const& iid) noexcept
	{
		return is_guid_of<OpenGlass::IDesktopWallpaperPrivate, IDesktopWallpaper>(iid);
	}

	template<>
	bool is_guid_of<IPersistStream>(guid const& iid) noexcept
	{
		return is_guid_of<IPersistStream, IPersist>(iid);
	}
}

namespace OpenGlass::ColorHooks
{
	class CDesktopWallpaper : public winrt::implements<CDesktopWallpaper, IDesktopWallpaperPrivate, IPersistStream, winrt::non_agile, winrt::no_weak_ref>
	{
		winrt::com_ptr<IDesktopWallpaperPrivate> m_wallpaper;
		winrt::com_ptr<IPersistStream> m_persist;

	public:
		CDesktopWallpaper(winrt::com_ptr<IDesktopWallpaperPrivate> wallpaper);

		HRESULT STDMETHODCALLTYPE SetWallpaper(PCWSTR monitor, PCWSTR wallpaper) noexcept override;
		HRESULT STDMETHODCALLTYPE GetWallpaper(PCWSTR monitor, PWSTR* wallpaper) noexcept override;
		HRESULT STDMETHODCALLTYPE GetMonitorDevicePathAt(UINT index, PWSTR* monitor) noexcept override;
		HRESULT STDMETHODCALLTYPE GetMonitorDevicePathCount(UINT* count) noexcept override;
		HRESULT STDMETHODCALLTYPE GetMonitorRECT(PCWSTR monitor, RECT* rect) noexcept override;
		HRESULT STDMETHODCALLTYPE SetBackgroundColor(COLORREF color) noexcept override;
		HRESULT STDMETHODCALLTYPE GetBackgroundColor(COLORREF* color) noexcept override;
		HRESULT STDMETHODCALLTYPE SetPosition(DESKTOP_WALLPAPER_POSITION position) noexcept override;
		HRESULT STDMETHODCALLTYPE GetPosition(DESKTOP_WALLPAPER_POSITION* position) noexcept override;
		HRESULT STDMETHODCALLTYPE SetSlideshow(IShellItemArray* items) noexcept override;
		HRESULT STDMETHODCALLTYPE GetSlideshow(IShellItemArray** items) noexcept override;
		HRESULT STDMETHODCALLTYPE SetSlideshowOptions(DESKTOP_SLIDESHOW_OPTIONS options, UINT interval) noexcept override;
		HRESULT STDMETHODCALLTYPE GetSlideshowOptions(DESKTOP_SLIDESHOW_OPTIONS* options, UINT* interval) noexcept override;
		HRESULT STDMETHODCALLTYPE AdvanceSlideshow(PCWSTR monitor, DESKTOP_SLIDESHOW_DIRECTION direction) noexcept override;
		HRESULT STDMETHODCALLTYPE GetStatus(DESKTOP_SLIDESHOW_STATE* state) noexcept override;
		HRESULT STDMETHODCALLTYPE Enable(BOOL enable) noexcept override;

		HRESULT STDMETHODCALLTYPE SetWallpaperNoGP(PCWSTR monitor, PCWSTR wallpaper) noexcept override;
		HRESULT STDMETHODCALLTYPE PaintWallpaperThumbnail(LONGLONG dc, int width, int height) noexcept override;
		HRESULT STDMETHODCALLTYPE ComputeImageColorizationColor(DWORD* color) noexcept override;
		HRESULT STDMETHODCALLTYPE GetMonitorNumber(PCWSTR monitor, UINT* number) noexcept override;
		HRESULT STDMETHODCALLTYPE Flush() noexcept override;
		HRESULT STDMETHODCALLTYPE PaintWallpaperBitmapFromShellTask() noexcept override;

		// The desktop background page requests IPersistStream from the same
		// private interface to capture and restore its wallpaper checkpoint.
		HRESULT STDMETHODCALLTYPE GetClassID(CLSID* clsid) noexcept override;
		HRESULT STDMETHODCALLTYPE IsDirty() noexcept override;
		HRESULT STDMETHODCALLTYPE Load(IStream* stream) noexcept override;
		HRESULT STDMETHODCALLTYPE Save(IStream* stream, BOOL clearDirty) noexcept override;
		HRESULT STDMETHODCALLTYPE GetSizeMax(ULARGE_INTEGER* size) noexcept override;
	};
}

ColorHooks::CDesktopWallpaper::CDesktopWallpaper(winrt::com_ptr<IDesktopWallpaperPrivate> wallpaper) :
	m_wallpaper(std::move(wallpaper)),
	m_persist(m_wallpaper.as<IPersistStream>())
{
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::SetWallpaper(PCWSTR monitor, PCWSTR wallpaper) noexcept
{
	return m_wallpaper->SetWallpaper(monitor, wallpaper);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetWallpaper(PCWSTR monitor, PWSTR* wallpaper) noexcept
{
	return m_wallpaper->GetWallpaper(monitor, wallpaper);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetMonitorDevicePathAt(UINT index, PWSTR* monitor) noexcept
{
	return m_wallpaper->GetMonitorDevicePathAt(index, monitor);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetMonitorDevicePathCount(UINT* count) noexcept
{
	return m_wallpaper->GetMonitorDevicePathCount(count);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetMonitorRECT(PCWSTR monitor, RECT* rect) noexcept
{
	return m_wallpaper->GetMonitorRECT(monitor, rect);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::SetBackgroundColor(COLORREF color) noexcept
{
	return m_wallpaper->SetBackgroundColor(color);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetBackgroundColor(COLORREF* color) noexcept
{
	return m_wallpaper->GetBackgroundColor(color);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::SetPosition(DESKTOP_WALLPAPER_POSITION position) noexcept
{
	return m_wallpaper->SetPosition(position);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetPosition(DESKTOP_WALLPAPER_POSITION* position) noexcept
{
	return m_wallpaper->GetPosition(position);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::SetSlideshow(IShellItemArray* items) noexcept
{
	return m_wallpaper->SetSlideshow(items);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetSlideshow(IShellItemArray** items) noexcept
{
	return m_wallpaper->GetSlideshow(items);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::SetSlideshowOptions(DESKTOP_SLIDESHOW_OPTIONS options, UINT interval) noexcept
{
	return m_wallpaper->SetSlideshowOptions(options, interval);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetSlideshowOptions(DESKTOP_SLIDESHOW_OPTIONS* options, UINT* interval) noexcept
{
	return m_wallpaper->GetSlideshowOptions(options, interval);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::AdvanceSlideshow(PCWSTR monitor, DESKTOP_SLIDESHOW_DIRECTION direction) noexcept
{
	return m_wallpaper->AdvanceSlideshow(monitor, direction);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetStatus(DESKTOP_SLIDESHOW_STATE* state) noexcept
{
	return m_wallpaper->GetStatus(state);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::Enable(BOOL enable) noexcept
{
	return m_wallpaper->Enable(enable);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::SetWallpaperNoGP(PCWSTR monitor, PCWSTR wallpaper) noexcept
{
	return m_wallpaper->SetWallpaperNoGP(monitor, wallpaper);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::PaintWallpaperThumbnail(LONGLONG dc, int width, int height) noexcept
{
	return m_wallpaper->PaintWallpaperThumbnail(dc, width, height);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::ComputeImageColorizationColor(DWORD* color) noexcept try
{
	RETURN_HR_IF_NULL(E_POINTER, color);
	DWORD imageColor{};
	RETURN_IF_FAILED(m_wallpaper->ComputeImageColorizationColor(&imageColor));
	RETURN_IF_FAILED(ShellColorRefresh::Request(GetShellWindow()));
	const auto appliedColor = wil::reg::get_value_dword(HKEY_CURRENT_USER, DwmSettings::g_keyPath, L"ColorizationColor");
	*color = (imageColor & 0xFF000000) | (appliedColor & 0x00FFFFFF);
	return S_OK;
}
CATCH_RETURN();

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetMonitorNumber(PCWSTR monitor, UINT* number) noexcept
{
	return m_wallpaper->GetMonitorNumber(monitor, number);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::Flush() noexcept
{
	return m_wallpaper->Flush();
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::PaintWallpaperBitmapFromShellTask() noexcept
{
	return m_wallpaper->PaintWallpaperBitmapFromShellTask();
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetClassID(CLSID* clsid) noexcept
{
	return m_persist->GetClassID(clsid);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::IsDirty() noexcept
{
	return m_persist->IsDirty();
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::Load(IStream* stream) noexcept
{
	return m_persist->Load(stream);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::Save(IStream* stream, BOOL clearDirty) noexcept
{
	return m_persist->Save(stream, clearDirty);
}

HRESULT STDMETHODCALLTYPE ColorHooks::CDesktopWallpaper::GetSizeMax(ULARGE_INTEGER* size) noexcept
{
	return m_persist->GetSizeMax(size);
}

HRESULT WINAPI ColorHooks::MyCoCreateInstance(REFCLSID clsid, IUnknown* outer, DWORD context, REFIID iid, void** object) try
{
	const auto result = g_CoCreateInstance_Org(clsid, outer, context, iid, object);
	RETURN_IF_FAILED(result);
	if (clsid == __uuidof(DesktopWallpaper) && iid == __uuidof(IDesktopWallpaperPrivate))
	{
		winrt::com_ptr<IDesktopWallpaperPrivate> wallpaper{};
		wallpaper.attach(static_cast<IDesktopWallpaperPrivate*>(std::exchange(*object, nullptr)));
		*object = winrt::make<CDesktopWallpaper>(std::move(wallpaper)).detach();
	}
	return result;
}
CATCH_RETURN();
