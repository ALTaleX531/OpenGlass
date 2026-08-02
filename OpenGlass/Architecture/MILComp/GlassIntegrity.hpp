#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "GlassEngine.hpp"
#include "dwmcoreProjection.hpp"

namespace OpenGlass::GlassIntegrity
{
	inline std::unordered_set<dwmcore::CVisual*> g_glassVisualSet{};
	inline std::unordered_map<dwmcore::CGeometry*, std::bitset<2>> g_glassStatusByGeometry{};

	void DestroyDeviceResources(dwmcore::CD2DContext* d2dContext);

	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Shutdown();
}
