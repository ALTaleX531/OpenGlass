#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "GlassEngine.hpp"

namespace OpenGlass::GlassFrameEnhancer
{
	// Legacy Accent cloning shares the outer live-preview inventory choice.
	bool IsGlassOnlyLivePreviewClone();

	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Shutdown();
	void Cleanup();
}
