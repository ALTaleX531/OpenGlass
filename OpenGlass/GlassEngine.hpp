#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "DwmSettings.hpp"
#include "uDWMProjection.hpp"

namespace OpenGlass::GlassEngine
{
	using DwmSettings::UpdateType;

	HKEY GetDwmKey();
	HKEY GetMachineDwmKey();
	HKEY GetPersonalizeKey();

	inline DWORD GetDwordFromRegistry(PCWSTR keyName, DWORD defaultValue = 0) noexcept
	{
		return DwmSettings::GetDword(GetDwmKey(), GetMachineDwmKey(), keyName, defaultValue);
	}

	inline DWORD GetOverridableDwordFromRegistry(
		PCWSTR baseKeyName,
		PCWSTR overrideKeyName,
		DWORD defaultValue = 0
	) noexcept
	{
		return DwmSettings::GetOverridableDword(GetDwmKey(), GetMachineDwmKey(), baseKeyName, overrideKeyName, defaultValue);
	}

	template <size_t Length>
	inline void GetStringFromRegistry(PCWSTR keyName, WCHAR(&value)[Length]) noexcept
	{
		DwmSettings::GetString(GetDwmKey(), GetMachineDwmKey(), keyName, value);
	}

	void LoadRegistry(bool redrawNow = true);
	void UnloadRegistry();

	void SetDwmNotificationWindow(HWND hWnd);
	HWND GetDwmNotificationWindow();

	void Update(UpdateType type, bool redrawNow = true);
	void Startup();
	void Activate();
	void Shutdown();
}
