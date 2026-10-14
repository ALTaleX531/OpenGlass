#pragma once

#include <objbase.h>

namespace OpenGlass::ColorHooks
{
	HRESULT WINAPI MyCoCreateInstance(REFCLSID clsid, IUnknown* outer, DWORD context, REFIID iid, void** object);
	inline auto g_CoCreateInstance_Org{ &MyCoCreateInstance };
}
