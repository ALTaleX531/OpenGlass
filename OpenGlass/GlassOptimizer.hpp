#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "uDwmProjection.hpp"
#include "ConfigurationFramework.hpp"

namespace OpenGlass::GlassOptimizer
{
	void UpdateConfiguration(ConfigurationFramework::UpdateType type);
	
	HRESULT Startup();
	void Shutdown();
}