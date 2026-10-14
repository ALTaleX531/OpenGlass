#pragma once

#include <shobjidl.h>

namespace OpenGlass
{
	// IDesktopWallpaper occupies slots 0..18. The private methods below are
	// slots 19..24 in shell32 10.0.17763.737 and 10.0.26100.9549.
	DECLARE_INTERFACE_IID_(IDesktopWallpaperPrivate, IDesktopWallpaper, "C182461F-DFAC-4375-AB6E-4CC45AA7F9CC")
	{
		virtual HRESULT STDMETHODCALLTYPE SetWallpaperNoGP(PCWSTR monitor, PCWSTR wallpaper) = 0;
		virtual HRESULT STDMETHODCALLTYPE PaintWallpaperThumbnail(LONGLONG dc, int width, int height) = 0;
		virtual HRESULT STDMETHODCALLTYPE ComputeImageColorizationColor(DWORD* color) = 0;
		virtual HRESULT STDMETHODCALLTYPE GetMonitorNumber(PCWSTR monitor, UINT* number) = 0;
		virtual HRESULT STDMETHODCALLTYPE Flush() = 0;
		virtual HRESULT STDMETHODCALLTYPE PaintWallpaperBitmapFromShellTask() = 0;
	};
}
