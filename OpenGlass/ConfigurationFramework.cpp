#include "pch.h"
#include "ConfigurationFramework.hpp"
#include "OcclusionCulling.hpp"
#include "BackdropManager.hpp"
#include "CaptionTextHandler.hpp"
#include "GlassFramework.hpp"
#include "ServiceApi.hpp"

using namespace OpenGlass;
namespace OpenGlass::ConfigurationFramework
{
	wil::unique_hkey g_dwmKey{ nullptr };
	wil::unique_hkey g_personalizeKey{ nullptr };
}

void ConfigurationFramework::Update(UpdateType type)
{
	OcclusionCulling::UpdateConfiguration(type);
	GlassFramework::UpdateConfiguration(type);
	CaptionTextHandler::UpdateConfiguration(type);
}

HKEY ConfigurationFramework::GetDwmKey()
{
	return g_dwmKey.get();
}

HKEY ConfigurationFramework::GetPersonalizeKey()
{
	return g_personalizeKey.get();
}

void ConfigurationFramework::Load()
{
	PipeContent content{ GetCurrentProcessId() };
	HRESULT hr{ Client::RequestUserRegistryKey(content) };
	if (SUCCEEDED(hr))
	{
		g_dwmKey.reset(content.dwmKey);
		g_personalizeKey.reset(content.personalizeKey);
		Update(UpdateType::All);
	}
	else
	{
		LOG_HR(hr);
	}
}
void ConfigurationFramework::Unload()
{
	LOG_IF_FAILED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\DWM", g_dwmKey));
	LOG_IF_FAILED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", g_personalizeKey));
}