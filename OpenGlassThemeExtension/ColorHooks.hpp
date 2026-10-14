#pragma once

#include "Core.hpp"
#include "Colorization.hpp"
#include "HookHelper.hpp"

namespace OpenGlass::ColorHooks
{
	[[nodiscard]] HookHelper::ImportHook* Prepare(HookType type);
	HRESULT WINAPI MyDwmpSetColorizationParameters(const Colorization::ColorizationParameters* input, BOOL skipPersistence);
	HRESULT WINAPI MyDwmpGetColorizationParameters(Colorization::ColorizationParameters* output);
}
