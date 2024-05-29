#pragma once
#include "framework.hpp"
#include "ConfigurationFramework.hpp"

namespace OpenGlass::CaptionTextHandler
{
	void UpdateConfiguration(ConfigurationFramework::UpdateType type);

	HRESULT Startup();
	void Shutdown();
}