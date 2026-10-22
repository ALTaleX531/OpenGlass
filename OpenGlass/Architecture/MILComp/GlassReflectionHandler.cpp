#include "pch.h"
#include "GlassReflectionHandler.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "Shared.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassReflectionHandler
{

}

void GlassReflectionHandler::Update([[maybe_unused]] GlassEngine::UpdateType type)
{
}

void GlassReflectionHandler::Startup()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_GlassReflectionHandler))
	{
		return;
	}
}

void GlassReflectionHandler::Shutdown()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_GlassReflectionHandler))
	{
		return;
	}
}

void GlassReflectionHandler::Cleanup()
{
}
