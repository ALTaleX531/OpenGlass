#pragma once
#include "framework.hpp"
#include "winrt.hpp"
#include "ConfigurationFramework.hpp"

namespace OpenGlass::BackdropFactory
{
	wuc::CompositionBrush GetOrCreateBackdropBrush(
		const wuc::Compositor& compositor,
		DWORD color,
		bool active,
		uDwm::ACCENT_POLICY* policy = nullptr
	);
	std::chrono::steady_clock::time_point GetBackdropBrushTimeStamp();

	void Shutdown();
	void UpdateConfiguration(ConfigurationFramework::UpdateType type);
}