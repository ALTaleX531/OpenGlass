#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "GlassEngine.hpp"
#include "dwmcoreProjection.hpp"

namespace OpenGlass::GlassIntegrity
{
	void DestroyDeviceResources(dwmcore::CD2DContext* d2dContext);

	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Shutdown();
	void Cleanup();
}
