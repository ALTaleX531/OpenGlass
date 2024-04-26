#pragma once
#include "framework.hpp"
#include "cpprt.hpp"

namespace OpenGlass
{
	struct PipeContent
	{
		DWORD processId;
		HKEY dwmKey;
		HKEY personalizeKey;
	};
	namespace Client
	{
		HRESULT RequestUserRegistryKey(PipeContent& content);
	}
	namespace Server
	{
		HRESULT DuplicateUserRegistryKeyToDwm(PipeContent& content);
		HRESULT InjectDllToDwm(DWORD processId, bool inject, bool* actionPerformed = nullptr);
		DWORD InjectionThreadProc(LPVOID);
		HRESULT Run();
	}
}