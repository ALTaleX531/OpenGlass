#pragma once
#include "framework.hpp"
#include "GlassEngine.hpp"

namespace OpenGlass::CaptionTextHandler
{
	void DestroyDeviceResources();
	
	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Shutdown();
	void Cleanup();
}
