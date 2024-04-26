#pragma once
#include "framework.hpp"
#include "ConfigurationFramework.hpp"

namespace OpenGlass::CaptionTextHandler
{
	void InitializeFromSymbol(std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset);
	void UpdateConfiguration(ConfigurationFramework::UpdateType type);

	HRESULT Startup();
	void Shutdown();
}