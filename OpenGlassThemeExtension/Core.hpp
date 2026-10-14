#pragma once

#define NOMINMAX
#include <windows.h>

namespace OpenGlass
{
	enum class HookType : DWORD
	{
		CPL,
		UI,
		Max
	};
}
