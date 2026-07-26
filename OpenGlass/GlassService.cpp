#include "pch.h"
#include "resource.h"
#include "Util.hpp"
#include "GlassService.hpp"
#include "HookHelper.hpp"
#include "OpenGlass.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassService
{
	typedef wil::unique_any<PACL, decltype(&::LocalFree), ::LocalFree> unique_hlocal_acl;
	constexpr auto c_pipeName = L"\\\\.\\pipe\\Global\\OpenGlassHostPipe";
	struct CDwmProcessInfo
	{
		std::chrono::steady_clock::time_point injectionTimeStamp;
		wil::unique_handle processHandle;
		UINT retryCount{ 0 };

		CDwmProcessInfo(std::chrono::steady_clock::time_point t, HANDLE h) : injectionTimeStamp(t), processHandle(h), retryCount(0) {}
	};
	std::unordered_map<DWORD, CDwmProcessInfo> g_dwmInjectionMap{};
	std::unordered_set<DWORD> g_dwmInjectionBlackList{};

	ThreadStatus g_injectionThreadStatus{ ThreadStatus::Stopped };
	ThreadStatus g_serverThreadStatus{ ThreadStatus::Stopped };
	DWORD g_injectionThreadId{};
	DWORD g_serverThreadId{};
	HANDLE g_serverNamedPipeHandle{};

	bool IsOpenGlassAlreadyLoaded(DWORD processId);
	HRESULT InjectOpenGlassDLL(DWORD processId, bool inject);
	HRESULT OpenUserRegistryForDwm(RequestBuffer& content, DWORD processId);
	HRESULT RunInjectionThread();
	HRESULT RunServerThread();

	// Check if the crash dialog is disabled via registry
	FORCEINLINE bool IsCrashDialogDisabled()
	{
		DWORD value{ 1 };
		if (SUCCEEDED(wil::reg::get_value_dword_nothrow(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\DWM", L"DisableCrashDialog", &value)))
		{
			return value == 1;
		}
		return false; // Default: dialog is enabled
	}
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
			TOKEN_ALL_ACCESS,
			nullptr,
			SecurityImpersonation,
			TokenImpersonation,
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
	return HookHelper::GetRemoteModuleBase(processId, Util::g_thisModulePath.c_str());
}

bool GlassService::IsDwmProcess(HANDLE processHandle) try
{
	const auto dwmPath = wil::QueryFullProcessImageNameW<std::wstring, MAX_PATH>(processHandle);
	if (_wcsicmp(dwmPath.c_str(), wil::ExpandEnvironmentStringsW<std::wstring, MAX_PATH>(L"%WINDIR%\\system32\\dwm.exe").c_str()) != 0)
	{
		return false;
	}

	wil::unique_handle token{ nullptr };
	THROW_IF_WIN32_BOOL_FALSE(OpenProcessToken(processHandle, TOKEN_QUERY | TOKEN_DUPLICATE, token.put()));

	wil::unique_handle impersonationToken;
	THROW_IF_WIN32_BOOL_FALSE(DuplicateToken(token.get(), SecurityIdentification, impersonationToken.put()));

	BOOL isWindowManager{ FALSE };
	wil::unique_sid sid{ nullptr };
	SID_IDENTIFIER_AUTHORITY authority{ SECURITY_NT_AUTHORITY };
	THROW_IF_WIN32_BOOL_FALSE(AllocateAndInitializeSid(&authority, 2, SECURITY_WINDOW_MANAGER_BASE_RID, SECURITY_NULL_RID, 0, 0, 0, 0, 0, 0, &sid));
	THROW_IF_WIN32_BOOL_FALSE(CheckTokenMembership(impersonationToken.get(), sid.get(), &isWindowManager));

	return static_cast<bool>(isWindowManager);
}
catch (...)
{
	return false;
}

HRESULT GlassService::InjectOpenGlassDLL(DWORD processId, bool inject)
{
	wil::unique_handle processHandle{ OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId) };
	RETURN_LAST_ERROR_IF_NULL(processHandle);

	const auto initOffset = static_cast<ULONG_PTR>(
		reinterpret_cast<ULONG_PTR>(OpenGlass::InitializationThreadEntryPoint) -
		reinterpret_cast<ULONG_PTR>(wil::GetModuleInstanceHandle())
	);
	const auto uninitOffset = static_cast<ULONG_PTR>(
		reinterpret_cast<ULONG_PTR>(OpenGlass::UnInitializationThreadEntryPoint) -
		reinterpret_cast<ULONG_PTR>(wil::GetModuleInstanceHandle())
	);

	const auto CreateRemoteThreadWithNTAPI = [&](LPTHREAD_START_ROUTINE startRoutine, void* parameter) -> wil::unique_handle
	{
		wil::unique_handle threadHandle{};
		const auto status = NtCreateThreadEx(
			threadHandle.put(),
			THREAD_ALL_ACCESS,
			nullptr,
			processHandle.get(),
			reinterpret_cast<PUSER_THREAD_START_ROUTINE>(startRoutine),
			parameter,
			0,
			0,
			0,
			0,
			nullptr
		);
		LOG_IF_NTSTATUS_FAILED(status);

		return threadHandle;
	};

	if (inject)
	{
		const auto remoteLoadLibraryW = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");

		{
			const auto pathSize = (Util::g_thisModulePath.size() + 1ull) * sizeof(WCHAR);
			void* remotePath = VirtualAllocEx(processHandle.get(), nullptr, pathSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			RETURN_LAST_ERROR_IF_NULL(remotePath);
			const auto remotePathCleanup = wil::scope_exit([&]
			{
				LOG_IF_WIN32_BOOL_FALSE(VirtualFreeEx(processHandle.get(), remotePath, 0, MEM_RELEASE));
			});

			SIZE_T bytesWritten{};
			RETURN_IF_WIN32_BOOL_FALSE(
				WriteProcessMemory(
					processHandle.get(),
					remotePath,
					Util::g_thisModulePath.c_str(),
					pathSize,
					&bytesWritten
				)
			);
			RETURN_HR_IF(E_FAIL, bytesWritten != pathSize);

			const auto loadThread = CreateRemoteThreadWithNTAPI(reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteLoadLibraryW), remotePath);
			RETURN_LAST_ERROR_IF_NULL(loadThread);

			if (WaitForSingleObject(loadThread.get(), 3'000) != WAIT_OBJECT_0)
			{
				LOG_IF_WIN32_BOOL_FALSE(TerminateThread(loadThread.get(), HRESULT_FROM_WIN32(ERROR_POSSIBLE_DEADLOCK)));
				return HRESULT_FROM_WIN32(ERROR_POSSIBLE_DEADLOCK);
			}
		}

		HMODULE remoteDllBase = HookHelper::GetRemoteModuleBase(processId, Util::g_thisModulePath.c_str());
		RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND), remoteDllBase);

		const auto remoteInit = reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<uint8_t*>(remoteDllBase) + initOffset);
		const auto initThread = CreateRemoteThreadWithNTAPI(remoteInit, nullptr);
		RETURN_LAST_ERROR_IF_NULL(initThread);
	}
	else
	{
		const auto remoteDllBase = HookHelper::GetRemoteModuleBase(processId, Util::g_thisModulePath.c_str());
		if (!remoteDllBase)
		{
			return S_OK;
		}

		const auto remoteUninit = reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<uint8_t*>(remoteDllBase) + uninitOffset);
		const auto uninitThread = CreateRemoteThreadWithNTAPI(remoteUninit, nullptr);
		RETURN_LAST_ERROR_IF_NULL(uninitThread);
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
				[](ULONG_PTR status) static
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

	auto WalkDwmProcesses = [](std::function<bool(DWORD)>&& callback) static
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

	HRESULT hr{ S_OK };
	do
	{
		// cleanup unused session records
		auto currentTimeStamp = std::chrono::steady_clock::now();
		for (auto it = g_dwmInjectionMap.begin(); it != g_dwmInjectionMap.end(); )
		{
			const auto& [injectionTimeStamp, __, ___] = it->second;
			if (currentTimeStamp - injectionTimeStamp >= std::chrono::minutes{ 3 })
			{
				it = g_dwmInjectionMap.erase(it);
			}
			else
			{
				it++;
			}
		}
		if (g_injectionThreadStatus == ThreadStatus::Paused)
		{
			goto wait_until_next_cycle;
		}

		WalkDwmProcesses([&hr](DWORD processId) -> bool
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
			DWORD bytesReturned{};
			wil::unique_wtsmem_ptr<WTS_CONNECTSTATE_CLASS> buffer{};
			if (
				!WTSQuerySessionInformationW(
					WTS_CURRENT_SERVER_HANDLE,
					sessionId,
					WTSConnectState,
					reinterpret_cast<LPWSTR*>(&buffer),
					&bytesReturned
				) ||
				!buffer ||
				(
					*buffer != WTSActive &&
					*buffer != WTSConnected
				)
			)
			{
				return true;
			}

			{
				wil::unique_handle processHandle{ OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId) };
				if (!processHandle)
				{
					return true;
				}
				if (!IsDwmProcess(processHandle.get()) || g_dwmInjectionBlackList.find(processId) != g_dwmInjectionBlackList.end())
				{
					return true;
				}
			}

			if (!IsOpenGlassAlreadyLoaded(processId))
			{
				const auto currentTimeStamp = std::chrono::steady_clock::now();
				const auto it = g_dwmInjectionMap.find(sessionId);
				if (it != g_dwmInjectionMap.end())
				{
					auto& [injectionTimeStamp, processHandle, retryCount] = it->second;

					DWORD exitCode{ 0 };
					LOG_IF_WIN32_BOOL_FALSE(GetExitCodeProcess(processHandle.get(), &exitCode));
					// DWM constantly crashes or manual fast fail triggered by user
					if (currentTimeStamp - injectionTimeStamp <= std::chrono::seconds{ 15 } || exitCode == 0xC0000409)
					{
						// Only show the crash dialog if not disabled via registry
						if (!IsCrashDialogDisabled())
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
							g_injectionThreadStatus = response == IDABORT ? ThreadStatus::Stopped : ThreadStatus::Running;

							if (g_injectionThreadStatus == ThreadStatus::Stopped)
							{
								hr = E_ABORT;
								return false;
							}
							if (response == IDIGNORE)
							{
								g_dwmInjectionMap.erase(it);
								g_dwmInjectionBlackList.emplace(processId);
								return false;
							}
						}
						else
						{
							// Crash dialog disabled - automatically retry
							if (++retryCount >= 3)
							{
								// Max retries reached - add to blacklist
								g_dwmInjectionMap.erase(it);
								g_dwmInjectionBlackList.emplace(processId);
								return true;
							}
							Sleep(1000);
							g_dwmInjectionMap.erase(it);
							return true;
						}
					}
				}

				if (const auto hresult = InjectOpenGlassDLL(processId, true); SUCCEEDED(hresult))
				{
					g_dwmInjectionMap.insert_or_assign(sessionId, CDwmProcessInfo{ currentTimeStamp, OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId) });
				}
			}

			return true;
		});

	wait_until_next_cycle:
		SleepEx(g_injectionThreadStatus == ThreadStatus::Paused ? INFINITE : 2000ul, TRUE);
	}
	while (g_injectionThreadStatus != ThreadStatus::Stopped);

	WalkDwmProcesses([](DWORD processId) static -> bool
	{
		if (IsOpenGlassAlreadyLoaded(processId))
		{
			InjectOpenGlassDLL(processId, false);
		}

		return true;
	});

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
			FALSE
		};
		pipe.reset(
			CreateNamedPipeW(
				c_pipeName,
				PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
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

				{
					wil::unique_handle processHandle{ OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, clientProcessId) };
					if (!processHandle)
					{
						goto on_named_pipe_disconnected;
					}
					if (!IsDwmProcess(processHandle.get()))
					{
						// If the client process is not a verified DWM process, ignore request.
						goto on_named_pipe_disconnected;
					}
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

bool GlassService::IsActive()
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
