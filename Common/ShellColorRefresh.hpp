#pragma once
#include <windows.h>

namespace OpenGlass::ShellColorRefresh
{
	inline HRESULT Request(HWND shell) noexcept
	{
		if (!shell) return HRESULT_FROM_WIN32(ERROR_INVALID_WINDOW_HANDLE);
		DWORD_PTR result{};
		SetLastError(ERROR_SUCCESS);
		// UpdateWallpaperTransition(14, 0xFFFFFFFF) posts this exact request.
		// Its zero-parameter path only consumes a pending color; it cannot refresh
		// an unchanged wallpaper after Manual. Send synchronously so successful
		// requests finish before another color choice. Allow sent-message
		// dispatch while waiting: Explorer can broadcast back to this window.
		if (!SendMessageTimeoutW(shell, 0x52C, 14, static_cast<LPARAM>(0xFFFFFFFFull),
			SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 5000, &result))
		{
			const auto error = GetLastError();
			return error ? HRESULT_FROM_WIN32(error) : E_FAIL;
		}
		return static_cast<HRESULT>(result);
	}
}
