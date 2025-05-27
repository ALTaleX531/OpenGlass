#include "pch.h"
#include "resource.h"
#include "Util.hpp"
#include "GlassService.hpp"
#include "HookHelper.hpp"
#include "OpenGlass.hpp"
#include "module.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassService
{
	typedef wil::unique_any<PACL, decltype(&::LocalFree), ::LocalFree> unique_hlocal_acl;
	constexpr auto c_pipeName = L"\\\\.\\pipe\\Global\\OpenGlassLegacyHostPipe";
	std::unordered_map<DWORD, std::chrono::steady_clock::time_point> g_dwmInjectionMap{};

	ThreadStatus g_injectionThreadStatus{ ThreadStatus::Stopped };
	ThreadStatus g_serverThreadStatus{ ThreadStatus::Stopped };
	DWORD g_injectionThreadId{};
	DWORD g_serverThreadId{};
	HANDLE g_serverNamedPipeHandle{};

	bool IsOpenGlassAlreadyLoaded(DWORD processId);
	bool IsDwmProcess(DWORD processId);
	HRESULT InjectOpenGlassDLL(DWORD processId, bool inject);
	HRESULT OpenUserRegistryForDwm(RequestBuffer& content, DWORD processId);
	HRESULT RunInjectionThread();
	HRESULT RunServerThread();
}

HRESULT GlassService::OpenUserRegistryForDwm(RequestBuffer& content, DWORD processId) try
{
	wil::unique_handle processHandle{ OpenProcess(PROCESS_DUP_HANDLE, FALSE, processId) };
	THROW_LAST_ERROR_IF_NULL(processHandle);
	DWORD sessionId{ 0 };
	THROW_IF_WIN32_BOOL_FALSE(ProcessIdToSessionId(processId, &sessionId));
	wil::unique_handle token{ nullptr };
	THROW_IF_WIN32_BOOL_FALSE(WTSQueryUserToken(sessionId, &token));
	wil::unique_handle duplicatedToken{ nullptr };
	THROW_IF_WIN32_BOOL_FALSE(
		DuplicateTokenEx(
			token.get(),
			TOKEN_ASSIGN_PRIMARY | TOKEN_ALL_ACCESS,
			nullptr,
			SecurityImpersonation,
			TokenPrimary,
			&duplicatedToken
		)
	);
	THROW_IF_WIN32_BOOL_FALSE(ImpersonateLoggedOnUser(duplicatedToken.get()));
	wil::unique_hkey userKey{ nullptr };
	{
		const auto revertCleanUp = wil::scope_exit([] { THROW_IF_WIN32_BOOL_FALSE(RevertToSelf()); });
		THROW_IF_FAILED(HRESULT_FROM_WIN32(RegOpenCurrentUser(KEY_READ, &userKey)));
	}
	wil::unique_hkey key{ nullptr };
	THROW_IF_FAILED(wil::reg::open_unique_key_nothrow(userKey.get(), L"Software\\Microsoft\\Windows\\DWM", key));
	THROW_IF_WIN32_BOOL_FALSE(
		DuplicateHandle(
			GetCurrentProcess(),
			key.release(),
			processHandle.get(),
			reinterpret_cast<PHANDLE>(&content.dwmKey),
			0,
			FALSE,
			DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE
		)
	);
	THROW_IF_FAILED(wil::reg::open_unique_key_nothrow(userKey.get(), L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", key));
	THROW_IF_WIN32_BOOL_FALSE(
		DuplicateHandle(
			GetCurrentProcess(),
			key.release(),
			processHandle.get(),
			reinterpret_cast<PHANDLE>(&content.personalizeKey),
			0,
			FALSE,
			DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE
		)
	);

	return S_OK;
}
CATCH_RETURN()

bool GlassService::IsOpenGlassAlreadyLoaded(DWORD processId)
{
	wil::unique_handle processHandle{ OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId) };
	return HookHelper::GetProcessModule(processHandle.get(), Util::g_thisModulePath.c_str()) != nullptr;
}

bool GlassService::IsDwmProcess(DWORD processId)
{
	wil::unique_handle processHandle{ OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId) };
	if (!processHandle)
	{
		return false;
	}

	try
	{
		const auto dwmPath = wil::QueryFullProcessImageNameW<std::wstring, MAX_PATH>(processHandle.get());
		if (_wcsicmp(dwmPath.c_str(), wil::ExpandEnvironmentStringsW<std::wstring, MAX_PATH>(L"%WINDIR%\\system32\\dwm.exe").c_str()) != 0)
		{
			return false;
		}
	}
	catch (...)
	{
		return false;
	}

	return true;
}

HRESULT GlassService::InjectOpenGlassDLL(DWORD processId, bool inject)
{
	wil::unique_handle processHandle{ OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId) };

	[[maybe_unused]] const auto bufferSize = (Util::g_thisModulePath.size() + 1ull) * sizeof(WCHAR);
	PVOID remoteAddress{ nullptr };
	if (inject)
	{
		remoteAddress = VirtualAllocEx(processHandle.get(), nullptr, bufferSize, MEM_COMMIT, PAGE_READWRITE);
		RETURN_LAST_ERROR_IF_NULL(remoteAddress);
	}
	else
	{
		remoteAddress = HookHelper::GetProcessModule(processHandle.get(), Util::g_thisModulePath.c_str());
	}
	[[maybe_unused]] const auto cleanup = wil::scope_exit([&processHandle, remoteAddress, inject]
		{
			if (inject && remoteAddress)
			{
				VirtualFreeEx(processHandle.get(), remoteAddress, 0, MEM_RELEASE);
			}
		});

	if (inject)
	{
		RETURN_IF_WIN32_BOOL_FALSE(WriteProcessMemory(processHandle.get(), remoteAddress, static_cast<LPCVOID>(Util::g_thisModulePath.c_str()), bufferSize, nullptr));
	}

	static const auto s_pfnNtCreateThreadEx = reinterpret_cast<NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, LPVOID, HANDLE, LPTHREAD_START_ROUTINE, LPVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, LPVOID)>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateThreadEx"));

	wil::unique_handle threadHandle{ nullptr };
	const auto startRoutine =
		inject ?
		reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryW) :
		reinterpret_cast<LPTHREAD_START_ROUTINE>(
			reinterpret_cast<ULONG_PTR>(OpenGlass::UnInitializationThreadEntryPoint)-
			reinterpret_cast<ULONG_PTR>(wil::GetModuleInstanceHandle()) +
			reinterpret_cast<ULONG_PTR>(remoteAddress)
			);
	NTSTATUS ntstatus
	{
		s_pfnNtCreateThreadEx(
			threadHandle.put(),
			PROCESS_ALL_ACCESS,
			nullptr,
			processHandle.get(),
			startRoutine,
			remoteAddress,
			0,
			0,
			0,
			0,
			nullptr
		)
	};
	RETURN_IF_NTSTATUS_FAILED(ntstatus);

	if (inject)
	{
		const auto waitResult = WaitForSingleObject(threadHandle.get(), 1000);
		if (waitResult == WAIT_TIMEOUT)
		{
#pragma warning(suppress:6258)
			RETURN_IF_WIN32_BOOL_FALSE(TerminateThread(threadHandle.get(), HRESULT_FROM_WIN32(ERROR_POSSIBLE_DEADLOCK)));
			return HRESULT_FROM_WIN32(ERROR_POSSIBLE_DEADLOCK);
		}
		if (waitResult != WAIT_OBJECT_0)
		{
			RETURN_LAST_ERROR();
		}

		const auto remoteDllAddress = HookHelper::GetProcessModule(processHandle.get(), Util::g_thisModulePath.c_str());
		if (!remoteDllAddress)
		{
			RETURN_HR(E_ACCESSDENIED);
		}

		ntstatus = s_pfnNtCreateThreadEx(
			threadHandle.put(),
			PROCESS_ALL_ACCESS,
			nullptr,
			processHandle.get(),
			reinterpret_cast<LPTHREAD_START_ROUTINE>(
				reinterpret_cast<ULONG_PTR>(OpenGlass::InitializationThreadEntryPoint) -
				reinterpret_cast<ULONG_PTR>(wil::GetModuleInstanceHandle()) +
				reinterpret_cast<ULONG_PTR>(remoteDllAddress)
				),
			nullptr,
			0,
			0,
			0,
			0,
			nullptr
		);
		RETURN_IF_NTSTATUS_FAILED(ntstatus);
	}

	return S_OK;
}

HRESULT GlassService::ControlThread(
	HANDLE threadHandle,
	ThreadStatus newStatus
)
{
	const auto threadId = GetThreadId(threadHandle);
	if (threadId == g_injectionThreadId)
	{
		RETURN_LAST_ERROR_IF(
			QueueUserAPC(
				[](ULONG_PTR status)
				{
					g_injectionThreadStatus = static_cast<ThreadStatus>(status);
				},
				threadHandle,
				static_cast<ULONG_PTR>(newStatus)
			) == 0
		);
	}
	else if (threadId == g_serverThreadId)
	{
		if (newStatus != ThreadStatus::Stopped)
		{
			return HRESULT_FROM_WIN32(ERROR_BAD_DRIVER_LEVEL);
		}

		g_serverThreadStatus = newStatus;
		CancelIoEx(
			g_serverNamedPipeHandle,
			nullptr
		);
	}
	else
	{
		return E_ACCESSDENIED;
	}

	return S_OK;
}

HRESULT GlassService::RunInjectionThread()
{
	RETURN_IF_FAILED(SetThreadDescription(GetCurrentThread(), L"OpenGlass Injection Thread"));

	RETURN_IF_FAILED(RoInitialize(RO_INIT_MULTITHREADED));
	const wil::unique_rouninitialize_call wrtScope{};

	g_dwmInjectionMap.clear();
	g_injectionThreadStatus = ThreadStatus::Running;
	g_injectionThreadId = GetCurrentThreadId();
	const auto injectionThreadScope = wil::scope_exit([]
	{
		g_injectionThreadId = 0;
		g_injectionThreadStatus = ThreadStatus::Stopped;
	});

	while (!WaitNamedPipeW(c_pipeName, NMPWAIT_NOWAIT))
	{
		Sleep(1);
	}

	#pragma warning(suppress:6387)
	const auto RtlAdjustPrivilege = reinterpret_cast<NTSTATUS(NTAPI*)(int, BOOLEAN, BOOLEAN, PBOOLEAN)>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlAdjustPrivilege"));
	constexpr auto SE_DEBUG_PRIVILEGE = 0x14;
	BOOLEAN result = false;
	#pragma warning(suppress:6387)
	RETURN_IF_NTSTATUS_FAILED(RtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, true, false, &result));

	auto WalkDwmProcesses = [](std::function<bool(DWORD)>&& callback)
	{
		wil::unique_handle snapshot{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
		PROCESSENTRY32W pe{ sizeof(pe) };
		RETURN_IF_WIN32_BOOL_FALSE(Process32FirstW(snapshot.get(), &pe));

		do 
		{ 
			if (!_wcsicmp(pe.szExeFile, L"dwm.exe")) 
			{ 
				if (!callback(pe.th32ProcessID))
				{
					break;
				}
			} 
		} 
		while (Process32NextW(snapshot.get(), &pe));
		return S_OK;
	};

	FILE* logfp = fopen("c:\\openglass\\log.txt", "w");

	HRESULT hr{ S_OK };
	do
	{
		// cleanup unused session records
		auto currentTimeStamp = std::chrono::steady_clock::now();
		for (auto it = g_dwmInjectionMap.begin(); it != g_dwmInjectionMap.end(); )
		{
			const auto& [injectionSessionId, injectionTimeStamp] = *it;
			if (currentTimeStamp - injectionTimeStamp >= std::chrono::minutes{ 1 })
			{
				it = g_dwmInjectionMap.erase(it);
			}
			else
			{
				it++;
			}
		}

		PWTS_SESSION_INFO pSessionInfo = nullptr;
		DWORD count = 0;

		if (g_injectionThreadStatus == ThreadStatus::Paused)
		{
			goto wait_until_next_cycle;
		}

		if (WTSEnumerateSessions(WTS_CURRENT_SERVER_HANDLE, 0, 1, &pSessionInfo, &count)) {
			for (DWORD i = 0; i < count; ++i) {
				DWORD activeSessionId = pSessionInfo[i].SessionId;
				DWORD bytesReturned = 0;
				WTS_CONNECTSTATE_CLASS* state = nullptr;
				if (WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, activeSessionId, WTSConnectState, reinterpret_cast<LPTSTR*>(&state), &bytesReturned)) {
					if (*state == WTSActive || *state == WTSConnected) {
						WalkDwmProcesses([&hr, activeSessionId](DWORD processId) -> bool
							{
								if (g_injectionThreadStatus == ThreadStatus::Paused)
								{
									return false;
								}

								DWORD sessionId{ 0 };
								if (!ProcessIdToSessionId(processId, &sessionId))
								{
									return true;
								}
								if (activeSessionId != sessionId)
								{
									return true;
								}

								if (!IsDwmProcess(processId))
								{
									return true;
								}

								if (!IsOpenGlassAlreadyLoaded(processId))
								{
									auto currentTimeStamp = std::chrono::steady_clock::now();

									const auto it = g_dwmInjectionMap.find(sessionId);
									if (it != g_dwmInjectionMap.end())
									{
										const auto& [injectionSessionId, injectionTimeStamp] = *it;

										// DWM constantly crashes
										if (currentTimeStamp - injectionTimeStamp <= std::chrono::seconds{ 30 })
										{
											auto title = Util::GetResourceStringView<IDS_STRING101>();
											auto content = Util::GetResourceStringView<IDS_STRING109>();
											DWORD response{ IDTIMEOUT };
											WTSSendMessageW(
												WTS_CURRENT_SERVER_HANDLE,
												sessionId,
												const_cast<LPWSTR>(title.data()),
												static_cast<DWORD>(title.size() * sizeof(WCHAR)),
												const_cast<LPWSTR>(content.data()),
												static_cast<DWORD>(content.size() * sizeof(WCHAR)),
												MB_ICONERROR | MB_ABORTRETRYIGNORE,
												0,
												&response,
												TRUE
											);
											g_injectionThreadStatus = response == IDABORT ? ThreadStatus::Paused : ThreadStatus::Running;

											if (g_injectionThreadStatus == ThreadStatus::Paused)
											{
												hr = E_ABORT;
												ShutdownService();
												return false;
											}
											if (response == IDIGNORE)
											{
												g_dwmInjectionMap.erase(it);
												return false;
											}
										}
									}

									if (const auto hresult = InjectOpenGlassDLL(processId, true); SUCCEEDED(hresult))
									{
										g_dwmInjectionMap.insert_or_assign(sessionId, currentTimeStamp);
									}
								}

								return true;
							});
					}
				}
			}
			WTSFreeMemory(pSessionInfo);
		}

	wait_until_next_cycle:
		SleepEx(g_injectionThreadStatus == ThreadStatus::Paused ? INFINITE : 2000ul, TRUE);
	} 
	while (g_injectionThreadStatus != ThreadStatus::Stopped);

	WalkDwmProcesses([](DWORD processId) -> bool
	{
		if (IsOpenGlassAlreadyLoaded(processId))
		{
			InjectOpenGlassDLL(processId, false);
		}

		return true;
	});

	fclose(logfp);
	
	return hr;
}

HRESULT GlassService::RunServerThread()
{
	RETURN_IF_FAILED(SetThreadDescription(GetCurrentThread(), L"OpenGlass Server Thread"));
	RETURN_IF_WIN32_BOOL_FALSE(SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS));

	RETURN_IF_FAILED(RoInitialize(RO_INIT_MULTITHREADED));
	const wil::unique_rouninitialize_call wrtScope{};

	g_serverThreadStatus = ThreadStatus::Running;
	g_serverThreadId = GetCurrentThreadId();
	const auto serverThreadScope = wil::scope_exit([]
	{
		g_serverThreadId = 0;
		g_serverThreadStatus = ThreadStatus::Stopped;
		g_serverNamedPipeHandle = nullptr;
	});

	wil::unique_hfile pipe{ INVALID_HANDLE_VALUE };
	{
		wil::unique_sid sid{ nullptr };
		SID_IDENTIFIER_AUTHORITY authority{ SECURITY_NT_AUTHORITY };
		RETURN_IF_WIN32_BOOL_FALSE(
			AllocateAndInitializeSid(
				&authority,
				2,
				SECURITY_WINDOW_MANAGER_BASE_RID,
				SECURITY_NULL_RID,
				0,
				0,
				0,
				0,
				0,
				0,
				sid.put()
			)
		);
		EXPLICIT_ACCESS_W explicitAccess
		{
			FILE_ALL_ACCESS,
			SET_ACCESS,
			NO_INHERITANCE,
			{
				.TrusteeForm{ TRUSTEE_IS_SID },
				.TrusteeType{ TRUSTEE_IS_GROUP },
				.ptstrName{ reinterpret_cast<LPWCH>(sid.get()) }
			}
		};
		unique_hlocal_acl acl{ nullptr };
		RETURN_IF_WIN32_ERROR(
			SetEntriesInAclW(
				1ul,
				&explicitAccess,
				nullptr,
				acl.put()
			)
		);
		SECURITY_DESCRIPTOR descriptor{};
		RETURN_IF_WIN32_BOOL_FALSE(
			InitializeSecurityDescriptor(
				&descriptor,
				SECURITY_DESCRIPTOR_REVISION
			)
		);
		RETURN_IF_WIN32_BOOL_FALSE(
			SetSecurityDescriptorDacl(
				&descriptor,
				TRUE,
				acl.get(),
				FALSE
			)
		);
		SECURITY_ATTRIBUTES attributes
		{
			sizeof(SECURITY_ATTRIBUTES),
			&descriptor,
			TRUE
		};
		pipe.reset(
			CreateNamedPipeW(
				c_pipeName,
				PIPE_ACCESS_DUPLEX,
				PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
				PIPE_UNLIMITED_INSTANCES,
				1024ul,
				1024ul,
				50ul,
				&attributes
			)
		);
		RETURN_LAST_ERROR_IF(!pipe);
	}
	g_serverNamedPipeHandle = pipe.get();

	do
	{
		DWORD error{ 0ul };
		if (ConnectNamedPipe(pipe.get(), nullptr) || (error = GetLastError()) == ERROR_PIPE_CONNECTED)
		{
			try
			{
				ULONG clientProcessId{};
				THROW_IF_WIN32_BOOL_FALSE(GetNamedPipeClientProcessId(pipe.get(), &clientProcessId));
				
				if (!IsDwmProcess(clientProcessId))
				{
					goto on_named_pipe_disconnected;
				}

				RequestBuffer content{};
				THROW_IF_WIN32_BOOL_FALSE(ReadFile(pipe.get(), &content, sizeof(content), nullptr, nullptr));
				THROW_IF_FAILED(OpenUserRegistryForDwm(content, clientProcessId));
				THROW_IF_WIN32_BOOL_FALSE(WriteFile(pipe.get(), &content, sizeof(content), nullptr, nullptr));
				THROW_IF_WIN32_BOOL_FALSE(FlushFileBuffers(pipe.get()));
			}
			catch(...) {}

on_named_pipe_disconnected:
			RETURN_IF_WIN32_BOOL_FALSE(DisconnectNamedPipe(pipe.get()));
		}
		SwitchToThread();
	}
	while (g_serverThreadStatus != ThreadStatus::Stopped);

	return S_OK;
}

HRESULT GlassService::SendRequest(RequestBuffer& content)
{
	wil::unique_hfile pipe
	{
		CreateFile2(
			c_pipeName,
			GENERIC_READ | GENERIC_WRITE,
			0,
			OPEN_EXISTING,
			nullptr
		)
	};
	while (!pipe.is_valid())
	{
		DWORD error{ GetLastError() };
		if (error != ERROR_PIPE_BUSY)
		{
			RETURN_WIN32(error == ERROR_FILE_NOT_FOUND ? ERROR_SERVICE_NOT_ACTIVE : error);
		}

		if (!WaitNamedPipeW(c_pipeName, 3000ul))
		{
			error = GetLastError();
			RETURN_WIN32(error == ERROR_SEM_TIMEOUT ? ERROR_SERVICE_REQUEST_TIMEOUT : error);
		}
		pipe.reset(
			CreateFile2(
				c_pipeName,
				GENERIC_READ | GENERIC_WRITE,
				0,
				OPEN_EXISTING,
				nullptr
			)
		);
	}

	RETURN_IF_WIN32_BOOL_FALSE(WriteFile(pipe.get(), &content, sizeof(content), nullptr, nullptr));
	RETURN_IF_WIN32_BOOL_FALSE(ReadFile(pipe.get(), &content, sizeof(content), nullptr, nullptr));

	return S_OK;
}

bool GlassService::IsRunning()
{
	return static_cast<bool>(WaitNamedPipeW(c_pipeName, NMPWAIT_NOWAIT));
}

DWORD GlassService::InjectionThreadEntryPoint(LPVOID)
{
	return RunInjectionThread();
}

DWORD WINAPI GlassService::ServerThreadEntryPoint(PVOID)
{
	return RunServerThread();
}