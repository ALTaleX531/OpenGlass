#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "uDwmProjection.hpp"
#include "ConfigurationFramework.hpp"

namespace OpenGlass::OcclusionCulling
{
	void InitializeFromSymbol(std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset);
	void UpdateConfiguration(ConfigurationFramework::UpdateType type);
	
	HRESULT Startup();
	void Shutdown();
}