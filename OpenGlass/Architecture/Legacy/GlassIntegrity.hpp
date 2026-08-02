#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "GlassEngine.hpp"
#include "dwmcoreProjection.hpp"

namespace OpenGlass::GlassIntegrity
{
	void DestroyDeviceResources(dwmcore::CD2DContext* d2dContext);

	void FlipOccludersCheckpoint(dwmcore::CArrayBasedCoverageSet* coverageSet);
	inline auto FlipOccludersCheckpointScoped(dwmcore::CArrayBasedCoverageSet* coverageSet)
	{
		FlipOccludersCheckpoint(coverageSet);
		return wil::scope_exit([coverageSet]
		{
			FlipOccludersCheckpoint(coverageSet);
		});
	}

	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Shutdown();
}
