#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "GlassEngine.hpp"
#include <bitset>

namespace OpenGlass::uDWM
{
	struct CSolidRectangleVisual;
	struct CBaseGeometryProxy;
}

namespace OpenGlass::GlassInfra
{
	HRESULT UpdateGlassMarker(uDWM::CSolidRectangleVisual* visual, uDWM::CBaseGeometryProxy* geometry, std::bitset<2> status);
	void RedrawTopLevelWindow(uDWM::CTopLevelWindow* window, bool deepRedraw);
	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Shutdown();
	void Cleanup();

	inline uDWM::CTopLevelWindow* g_window{ nullptr };
	abi::ICompositionSurface* GetOrCreateReflectionSurface();
}
