#include "pch.h"
#include "ConfigurationFramework.hpp"
#include "CaptionTextHandler.hpp"
#include "ButtonGlowHandler.hpp"
#include "GlassFramework.hpp"
#include "GlassRenderer.hpp"
#include "CustomMsstyleLoader.hpp"
#include "ServiceApi.hpp"

using namespace OpenGlass;
namespace OpenGlass::ConfigurationFramework
{
	wil::unique_hkey g_dwmKey{ nullptr };
	wil::unique_hkey g_personalizeKey{ nullptr };
}

void ConfigurationFramework::Update(UpdateType type)
{
	GlassFramework::UpdateConfiguration(type);
	GlassRenderer::UpdateConfiguration(type);
	CaptionTextHandler::UpdateConfiguration(type);
	CustomMsstyleLoader::UpdateConfiguration(type);
	ButtonGlowHandler::UpdateConfiguration(type);
	DwmFlush();
}

HKEY ConfigurationFramework::GetDwmKey()
{
	return g_dwmKey.get();
}

HKEY ConfigurationFramework::GetPersonalizeKey()
{
	return g_personalizeKey.get();
}

void ConfigurationFramework::Load(bool updateNow)
{
	PipeContent content{ GetCurrentProcessId() };
	HRESULT hr{ Client::RequestUserRegistryKey(content) };
	if (SUCCEEDED(hr))
	{
		g_dwmKey.reset(content.dwmKey);
		g_personalizeKey.reset(content.personalizeKey);
		if (updateNow)
		{
			Update(UpdateType::All);
		}
	}
	else
	{
		LOG_HR(hr);
	}
}
void ConfigurationFramework::Unload()
{
	HRESULT hr{ S_OK };

	hr = wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\DWM", g_dwmKey);
	LOG_IF_FAILED_WITH_EXPECTED(hr, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
	hr = wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", g_personalizeKey);
	LOG_IF_FAILED_WITH_EXPECTED(hr, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
}