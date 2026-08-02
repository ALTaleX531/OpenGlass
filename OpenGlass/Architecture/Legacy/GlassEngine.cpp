#include "pch.h"
#include "Shared.hpp"
#include "GlassEngine.hpp"
#include "CaptionTextHandler.hpp"
#include "CaptionMetricsTweaker.hpp"
#include "ButtonGlowHandler.hpp"
#include "GlassKernel.hpp"
#include "AccentOverrider.hpp"
#include "GlassFrameHandler.hpp"
#include "GlassFrameDemodernizer.hpp"
#include "GlassFrameEnhancer.hpp"
#include "GlassReflectionHandler.hpp"
#include "GlassRenderer.hpp"
#include "GlassIntegrity.hpp"
#include "CustomThemeAtlasLoader.hpp"
#include "GlassService.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassEngine
{
	wil::unique_hkey g_dwmKey{ nullptr };
	wil::unique_hkey g_personalizeKey{ nullptr };
}

HKEY GlassEngine::GetDwmKey()
{
	return g_dwmKey.get();
}

HKEY GlassEngine::GetPersonalizeKey()
{
	return g_personalizeKey.get();
}

void GlassEngine::LoadRegistry(bool redrawNow)
{
	UnloadRegistry();

	GlassService::RequestBuffer content{ GlassService::RequestType::OpenUserRegistry };
	const auto hr = GlassService::SendRequest(content);
	if (SUCCEEDED(hr))
	{
		if (content.dwmKey)
		{
			g_dwmKey.reset(content.dwmKey);
		}
		if (content.personalizeKey)
		{
			g_personalizeKey.reset(content.personalizeKey);
		}
		Update(UpdateType::All, redrawNow);
	}
}
void GlassEngine::UnloadRegistry()
{
	wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\DWM", g_dwmKey);
	wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", g_personalizeKey);
}

void GlassEngine::Update(UpdateType type, bool redrawNow)
{
	{
		const auto lock = wil::EnterCriticalSection(uDWM::CDesktopManager::s_csDwmInstance);
		GlassKernel::Update(type);
		GlassIntegrity::Update(type);
		GlassRenderer::Update(type);

		CustomThemeAtlasLoader::Update(type);
		CaptionTextHandler::Update(type);
		CaptionMetricsTweaker::Update(type);
		ButtonGlowHandler::Update(type);
		AccentOverrider::Update(type);
		GlassFrameHandler::Update(type);
		GlassFrameDemodernizer::Update(type);
		GlassFrameEnhancer::Update(type);
		GlassReflectionHandler::Update(type);
		if (redrawNow)
		{
			GlassKernel::RedrawAllTopLevelWindow(false);
		}
	}
	THROW_IF_FAILED(DwmFlush());
}

void GlassEngine::Startup()
{
	Shared::g_disabledHooks = GetDwordFromRegistry(L"DisabledHooks", 0);
	GlassKernel::Startup();
	GlassIntegrity::Startup();
	GlassRenderer::Startup();

	{
		const auto lock = wil::EnterCriticalSection(uDWM::CDesktopManager::s_csDwmInstance);
		CustomThemeAtlasLoader::Startup();
		CaptionTextHandler::Startup();
		CaptionMetricsTweaker::Startup();
		ButtonGlowHandler::Startup();
		AccentOverrider::Startup();
		GlassFrameHandler::Startup();
		GlassFrameEnhancer::Startup();
		GlassFrameDemodernizer::Startup();
		GlassReflectionHandler::Startup();
		GlassKernel::RedrawAllTopLevelWindow(true);
	}
}
void GlassEngine::Shutdown()
{
	{
		const auto lock = wil::EnterCriticalSection(uDWM::CDesktopManager::s_csDwmInstance);
		GlassReflectionHandler::Shutdown();
		GlassFrameDemodernizer::Shutdown();
		GlassFrameEnhancer::Shutdown();
		GlassFrameHandler::Shutdown();
		AccentOverrider::Shutdown();
		ButtonGlowHandler::Shutdown();
		CaptionMetricsTweaker::Shutdown();
		CaptionTextHandler::Shutdown();
		CustomThemeAtlasLoader::Shutdown();
		GlassKernel::RedrawAllTopLevelWindow(true);
	}

	for (int i = 0; i < 10; i++)
	{
		THROW_IF_FAILED(DwmFlush());
	}

	GlassRenderer::Shutdown();
	GlassIntegrity::Shutdown();
	GlassKernel::Shutdown();
}
