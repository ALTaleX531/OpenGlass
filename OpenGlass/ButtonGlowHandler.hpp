#pragma once
#include "framework.hpp"
#include "GlassEngine.hpp"

namespace OpenGlass::ButtonGlowHandler
{
	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Shutdown();
	void Cleanup();
}
