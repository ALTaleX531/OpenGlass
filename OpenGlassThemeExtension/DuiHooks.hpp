#pragma once

#include "Core.hpp"
#include "HookHelper.hpp"

namespace OpenGlass::DuiHooks
{
	[[nodiscard]] HookHelper::ImportHook* Prepare(HookType type);
}
