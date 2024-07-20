#pragma once
#include "framework.hpp"
#include "ConfigurationFramework.hpp"

namespace OpenGlass::ButtonGlowHandler
{
	void UpdateConfiguration(ConfigurationFramework::UpdateType type);

	HRESULT Startup();
	void Shutdown();
}