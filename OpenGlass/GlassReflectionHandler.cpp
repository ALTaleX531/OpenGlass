#include "pch.h"
#include "GlassReflectionHandler.hpp"
#include "uDWMProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "Shared.hpp"
#include "GlassKernel.hpp"

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
