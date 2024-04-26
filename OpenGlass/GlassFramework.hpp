#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "HookHelper.hpp"
#include "ConfigurationFramework.hpp"

namespace OpenGlass::GlassFramework
{
	void InitializeFromSymbol(std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset);
	void UpdateConfiguration(ConfigurationFramework::UpdateType type);
	
	HRESULT Startup();
	void Shutdown();
}