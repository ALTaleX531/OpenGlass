#include "pch.h"
#include "ButtonGlowHandler.hpp"

using namespace OpenGlass;
namespace OpenGlass::ButtonGlowHandler
{
	static int MINMAXBUTTONGLOW = 93; //16 in windows 7
	static int CLOSEBUTTONGLOW = 92; //11 in windows 7
	static int TOOLCLOSEBUTTONGLOW = 94; //47 in windows 7

	inline __int64(__fastcall* CTopLevelWindow__CreateBitmapFromAtlas)(HTHEME hTheme, int iPartId, MARGINS* outMargins, void** outBitmapSource);
	HRESULT CTopLevelWindow__CreateButtonGlowsFromAtlas(HTHEME hTheme)
	{
#ifdef DEBUG
		OutputDebugStringW(std::format(L"CTopLevelWindow__CreateButtonGlowsFromAtlas").c_str());
#endif
		MARGINS margins{};
		HRESULT hr = S_OK;
		void* OutBitmapSourceBlue = nullptr;
		void* OutBitmapSourceRed = nullptr;
		void* OutBitmapSourceTool = nullptr;

		hr = CTopLevelWindow__CreateBitmapFromAtlas(hTheme, MINMAXBUTTONGLOW, &margins, &OutBitmapSourceBlue);
		if (hr < 0)
			return hr;
		*(MARGINS*)(__int64(OutBitmapSourceBlue) + 0x30) = margins; //offset is the same as w7, so it shouldnt change

		hr = CTopLevelWindow__CreateBitmapFromAtlas(hTheme, CLOSEBUTTONGLOW, &margins, &OutBitmapSourceRed);
		if (hr < 0)
			return hr;
		*(MARGINS*)(__int64(OutBitmapSourceRed) + 0x30) = margins;

		hr = CTopLevelWindow__CreateBitmapFromAtlas(hTheme, TOOLCLOSEBUTTONGLOW, &margins, &OutBitmapSourceTool);
		if (hr < 0)
			return hr;
		*(MARGINS*)(__int64(OutBitmapSourceTool) + 0x30) = margins;

		for (int i = 0; i < 4; ++i)
		{
			auto frame = (*uDwm::CTopLevelWindow::s_rgpwfWindowFrames)[i];
			*(void**)(__int64(frame) + 0xD0) = OutBitmapSourceBlue;
			*(void**)(__int64(frame) + 0xC8) = OutBitmapSourceRed;
		}
		for (int i = 4; i < 6; ++i)
		{
			auto frame = (*uDwm::CTopLevelWindow::s_rgpwfWindowFrames)[i];
			*(void**)(__int64(frame) + 0xD0) = OutBitmapSourceTool;
			*(void**)(__int64(frame) + 0xC8) = OutBitmapSourceTool;
		}
		return hr;
	}

	HRESULT (__fastcall* CTopLevelWindow_CreateGlyphsFromAtlas)(HTHEME hTheme);
	HRESULT __fastcall CTopLevelWindow_CreateGlyphsFromAtlas_Hook(HTHEME hTheme)
	{
		CTopLevelWindow__CreateButtonGlowsFromAtlas(hTheme);
		return CTopLevelWindow_CreateGlyphsFromAtlas(hTheme);
	}

	void UpdateConfiguration(ConfigurationFramework::UpdateType type)
	{
		MINMAXBUTTONGLOW = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"MINMAXBUTTONGLOWid",93);
		CLOSEBUTTONGLOW = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"CLOSEBUTTONGLOWid",92);
		TOOLCLOSEBUTTONGLOW = ConfigurationFramework::DwmGetDwordFromHKCUAndHKLM(L"TOOLCLOSEBUTTONGLOWid",94);
	}

	HRESULT Startup()
	{
		uDwm::GetAddressFromSymbolMap("CTopLevelWindow::CreateBitmapFromAtlas", CTopLevelWindow__CreateBitmapFromAtlas);
		uDwm::GetAddressFromSymbolMap("CTopLevelWindow::CreateGlyphsFromAtlas", CTopLevelWindow_CreateGlyphsFromAtlas);
		HookHelper::Detours::Write([]()
			{
				HookHelper::Detours::Attach(&CTopLevelWindow_CreateGlyphsFromAtlas, CTopLevelWindow_CreateGlyphsFromAtlas_Hook);
			});

		return S_OK;
	}

	void Shutdown()
	{
		HookHelper::Detours::Write([]()
			{
				HookHelper::Detours::Detach(&CTopLevelWindow_CreateGlyphsFromAtlas, CTopLevelWindow_CreateGlyphsFromAtlas_Hook);
			});
	}
}
