#pragma once
#include "framework.hpp"
#include "cpprt.hpp"

namespace OpenGlass::GlassService
{
	// client side apis
	enum class RequestType : UCHAR
	{
		OpenUserRegistry
	};
	struct RequestBuffer
	{
		RequestType type;
		HKEY dwmKey;
		HKEY personalizeKey;
	};
	HRESULT SendRequest(RequestBuffer& content);
	bool IsActive();
	bool IsDwmProcess(HANDLE processHandle);

	// service side apis
	enum class ThreadStatus
	{
		Paused,
		Running,
		Stopped
	};
	struct ThreadControl
	{
		HANDLE readyEvent{};
		HANDLE stopEvent{};
		HANDLE runEvent{};
		HANDLE wakeEvent{};
		HANDLE dependencyReadyEvent{};
	};
	HRESULT ControlThread(
		const ThreadControl& control,
		ThreadStatus newStatus
	);

	DWORD WINAPI ServerThreadEntryPoint(PVOID);
	DWORD WINAPI InjectionThreadEntryPoint(LPVOID);
}
