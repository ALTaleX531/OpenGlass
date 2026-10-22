#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "GlassEngine.hpp"

namespace OpenGlass::GlassInfra
{
	void RedrawTopLevelWindow(uDWM::CTopLevelWindow* window, bool deepRedraw);
	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Shutdown();
	void Cleanup();

	inline uDWM::CTopLevelWindow* g_window{ nullptr };
	abi::ICompositionSurface* GetOrCreateReflectionSurface();
}
