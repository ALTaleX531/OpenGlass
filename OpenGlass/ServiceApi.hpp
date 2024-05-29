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
		bool IsDllAlreadyLoadedByDwm(DWORD processId);
		HRESULT InjectDllToDwm(DWORD processId, bool inject);
		DWORD InjectionThreadProc(LPVOID);
		HRESULT Run();
	}
}