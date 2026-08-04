#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "GlassEngine.hpp"

namespace OpenGlass::CustomThemeAtlasLoader
{
	HRESULT MyGetThemeStream(
		HTHEME hTheme,
		int iPartId,
		int iStateId,
		int iPropId,
		VOID** ppvStream,
		DWORD* pcbStream,
		HINSTANCE hInst
	);
	HRESULT MyGetThemeMargins(
		HTHEME hTheme,
		HDC hdc,
		int iPartId,
		int iStateId,
		int iPropId,
		LPCRECT prc,
		MARGINS* pMargins
	);
	HRESULT MyGetThemeRect(
		HTHEME hTheme,
		int iPartId,
		int iStateId,
		int iPropId,
		LPRECT pRect
	);
	HRESULT MyGetThemeInt(
		HTHEME hTheme,
		int iPartId,
		int iStateId,
		int iPropId,
		int* piVal
	);
	HRESULT MyGetThemeColor(
		HTHEME hTheme,
		int iPartId,
		int iStateId,
		int iPropId,
		COLORREF* pColor
	);
	HTHEME GetThemeHandle();

	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Activate();
	void Shutdown();
	void Deactivate();
}
