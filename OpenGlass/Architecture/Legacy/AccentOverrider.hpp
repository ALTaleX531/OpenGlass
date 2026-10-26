#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "GlassEngine.hpp"

namespace OpenGlass::AccentOverrider
{
	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Shutdown();
	void Cleanup();
}
