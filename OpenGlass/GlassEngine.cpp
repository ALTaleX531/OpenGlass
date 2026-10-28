#include "pch.h"
#include "Shared.hpp"
#include "GlassEngine.hpp"
#include "CaptionTextHandler.hpp"
#include "CaptionMetricsTweaker.hpp"
#include "ButtonGlowHandler.hpp"
#include "GlassKernel.hpp"
#include "GlassInfra.hpp"
#include "AccentOverrider.hpp"
#include "GlassFrameHandler.hpp"
#include "GlassFrameDemodernizer.hpp"
#include "GlassFrameEnhancer.hpp"
#include "GlassFrameIntegrity.hpp"
#include "GlassReflectionHandler.hpp"
#include "GlassRenderer.hpp"
#include "GlassIntegrity.hpp"
#include "CustomThemeAtlasLoader.hpp"
#include "GlassService.hpp"
#include "SystemSettings.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassEngine
{
	wil::unique_hkey g_dwmKey{ nullptr };
	wil::unique_hkey g_machineDwmKey{ nullptr };
	wil::unique_hkey g_personalizeKey{ nullptr };
	HWND g_notificationWindow{ nullptr };
}

HKEY GlassEngine::GetDwmKey()
{
	return g_dwmKey.get();
}

HKEY GlassEngine::GetMachineDwmKey()
{
	return g_machineDwmKey.get();
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
	}
	// Apply machine settings and defaults even when the user keys are unavailable.
	Update(UpdateType::All, redrawNow);
}
void GlassEngine::UnloadRegistry()
{
	g_dwmKey.reset();
	LOG_IF_FAILED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, DwmSettings::g_keyPath, g_machineDwmKey));
	LOG_IF_FAILED(wil::reg::open_unique_key_nothrow(HKEY_LOCAL_MACHINE, Util::g_personalizeKeyPath, g_personalizeKey));
}

void GlassEngine::SetDwmNotificationWindow(HWND hWnd)
{
	g_notificationWindow = hWnd;
}

HWND GlassEngine::GetDwmNotificationWindow()
{
	return g_notificationWindow;
}

void GlassEngine::Update(UpdateType type, bool redrawNow)
{
	{
		const auto lock = wil::EnterCriticalSection(uDWM::CDesktopManager::s_csDwmInstance);
		GlassInfra::Update(type);
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
		GlassFrameIntegrity::Update(type);
		GlassReflectionHandler::Update(type);
		if (redrawNow)
		{
			GlassKernel::RedrawAllTopLevelWindow(false);
#if defined(OPENGLASS_DWM_MILCOMP)
			LOG_IF_FAILED(uDWM::CDesktopManager::GetInstance()->GetCompositor()->ForceRender());
#endif
		}
	}

	LOG_IF_FAILED(DwmFlush());
}

void GlassEngine::Startup()
{
	Shared::g_disabledHooks = GetDwordFromRegistry(L"DisabledHooks", 0);

	// wait for compositor thread to be idle before starting up,
	// to avoid racing with it and causing potential deadlock or other issues
	LOG_IF_FAILED(DwmFlush());

	HookHelper::GetHookRundown().Open();
	{
		HookHelper::HookTransaction transaction{ HookHelper::HookMode::Install };
		GlassInfra::Startup();
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
			GlassFrameIntegrity::Startup();
			GlassFrameDemodernizer::Startup();
			GlassReflectionHandler::Startup();
			transaction.Commit();
		}
	}
	{
		const auto lock = wil::EnterCriticalSection(uDWM::CDesktopManager::s_csDwmInstance);
		// The first redraw can discover lazy hooks. End the installation
		// transaction before allowing their independent transactions to run.
		GlassKernel::RedrawAllTopLevelWindow(true);
	}
#if defined(OPENGLASS_DWM_MILCOMP)
	LOG_IF_FAILED(uDWM::CDesktopManager::GetInstance()->GetCompositor()->ForceRender());
#endif
}
void GlassEngine::Activate()
{
	CustomThemeAtlasLoader::Activate();
}
void GlassEngine::Shutdown()
{
	HookHelper::GetHookRundown().BeginShutdown();
	LOG_IF_FAILED(DwmFlush());

	{
		HookHelper::HookTransaction transaction{ HookHelper::HookMode::Remove };
		GlassReflectionHandler::Shutdown();
		GlassFrameDemodernizer::Shutdown();
		GlassFrameIntegrity::Shutdown();
		GlassFrameEnhancer::Shutdown();
		GlassFrameHandler::Shutdown();
		AccentOverrider::Shutdown();
		ButtonGlowHandler::Shutdown();
		CaptionMetricsTweaker::Shutdown();
		CaptionTextHandler::Shutdown();
		CustomThemeAtlasLoader::Shutdown();
		GlassRenderer::Shutdown();
		GlassIntegrity::Shutdown();
		GlassInfra::Shutdown();
		{
			const auto lock = wil::EnterCriticalSection(uDWM::CDesktopManager::s_csDwmInstance);
			transaction.Commit();
		}
	}

	HookHelper::GetHookRundown().WaitForDrain(std::chrono::seconds{ 1 });

	{
		const auto lock = wil::EnterCriticalSection(uDWM::CDesktopManager::s_csDwmInstance);
		GlassReflectionHandler::Cleanup();
		GlassFrameDemodernizer::Cleanup();
		GlassFrameIntegrity::Cleanup();
		GlassFrameEnhancer::Cleanup();
		GlassFrameHandler::Cleanup();
		AccentOverrider::Cleanup();
		ButtonGlowHandler::Cleanup();
		CaptionMetricsTweaker::Cleanup();
		CaptionTextHandler::Cleanup();
	}

	CustomThemeAtlasLoader::Deactivate();

	{
		const auto lock = wil::EnterCriticalSection(uDWM::CDesktopManager::s_csDwmInstance);
		GlassKernel::RedrawAllTopLevelWindow(true, true);
		GlassRenderer::Cleanup();
		GlassIntegrity::Cleanup();
		GlassInfra::Cleanup();
	}

#if defined(OPENGLASS_DWM_MILCOMP)
	LOG_IF_FAILED(uDWM::CDesktopManager::GetInstance()->GetCompositor()->ForceRender());
#endif
}
