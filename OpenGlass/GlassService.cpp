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

		CDwmProcessInfo(std::chrono::steady_clock::time_point t, HANDLE h) : injectionTimeStamp(t), processHandle(h) {}
	};
	std::unordered_map<DWORD, CDwmProcessInfo> g_dwmInjectionMap{};
	std::unordered_set<DWORD> g_dwmInjectionBlackList{};


	bool IsOpenGlassAlreadyLoaded(DWORD processId);
	HRESULT InjectOpenGlassDLL(DWORD processId, bool inject);
	HRESULT OpenUserRegistryForDwm(RequestBuffer& content, DWORD processId);
	HRESULT RunInjectionThread(const ThreadControl& control);
	HRESULT RunServerThread(const ThreadControl& control);
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
	const auto WaitForRemoteThread = [](HANDLE threadHandle, DWORD* exitCode = nullptr) -> HRESULT
	{
		const auto waitResult = WaitForSingleObject(threadHandle, INFINITE);
		RETURN_LAST_ERROR_IF(waitResult == WAIT_FAILED);
		RETURN_HR_IF(E_UNEXPECTED, waitResult != WAIT_OBJECT_0);

		if (exitCode)
		{
			RETURN_IF_WIN32_BOOL_FALSE(GetExitCodeThread(threadHandle, exitCode));
		}

		return S_OK;
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
			// Keep the parameter alive until LoadLibraryW returns. Terminating a loader
			// thread can strand the target process under the loader lock.
			RETURN_IF_FAILED(WaitForRemoteThread(loadThread.get()));
		}

		HMODULE remoteDllBase = HookHelper::GetRemoteModuleBase(processId, Util::g_thisModulePath.c_str());
		RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND), remoteDllBase);

		const auto remoteInit = reinterpret_cast<LPTHREAD_START_ROUTINE>(reinterpret_cast<uint8_t*>(remoteDllBase) + initOffset);
		const auto initThread = CreateRemoteThreadWithNTAPI(remoteInit, nullptr);
		RETURN_LAST_ERROR_IF_NULL(initThread);

		// Injection is not complete until Startup has finished publishing its hooks.
		DWORD exitCode{};
		RETURN_IF_FAILED(WaitForRemoteThread(initThread.get(), &exitCode));
		RETURN_IF_FAILED(static_cast<HRESULT>(exitCode));
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

		// FreeLibraryAndExitThread marks the end of the DLL's owned lifetime.
		DWORD exitCode{};
		RETURN_IF_FAILED(WaitForRemoteThread(uninitThread.get(), &exitCode));
		RETURN_IF_FAILED(static_cast<HRESULT>(exitCode));
	}

	return S_OK;
}

HRESULT GlassService::ControlThread(
	const ThreadControl& control,
	ThreadStatus newStatus
)
{
	RETURN_HR_IF(E_INVALIDARG, !control.stopEvent);

	switch (newStatus)
	{
	case ThreadStatus::Stopped:
		RETURN_IF_WIN32_BOOL_FALSE(SetEvent(control.stopEvent));
		if (control.wakeEvent)
		{
			RETURN_IF_WIN32_BOOL_FALSE(SetEvent(control.wakeEvent));
		}
		break;

	case ThreadStatus::Paused:
		RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BAD_DRIVER_LEVEL), !control.runEvent || !control.wakeEvent);
		RETURN_IF_WIN32_BOOL_FALSE(ResetEvent(control.runEvent));
		RETURN_IF_WIN32_BOOL_FALSE(SetEvent(control.wakeEvent));
		break;

	case ThreadStatus::Running:
		RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BAD_DRIVER_LEVEL), !control.runEvent || !control.wakeEvent);
		RETURN_IF_WIN32_BOOL_FALSE(SetEvent(control.runEvent));
		RETURN_IF_WIN32_BOOL_FALSE(SetEvent(control.wakeEvent));
		break;

	default:
		return E_INVALIDARG;
	}

	return S_OK;
}

HRESULT GlassService::RunInjectionThread(const ThreadControl& control)
{
	RETURN_HR_IF(
		E_INVALIDARG,
		!control.readyEvent ||
		!control.stopEvent ||
		!control.runEvent ||
		!control.wakeEvent ||
		!control.dependencyReadyEvent
	);
	RETURN_IF_FAILED(SetThreadDescription(GetCurrentThread(), L"OpenGlass Injection Thread"));

	RETURN_IF_FAILED(RoInitialize(RO_INIT_MULTITHREADED));
	const wil::unique_rouninitialize_call wrtScope{};

	const HANDLE startupEvents[]{ control.stopEvent, control.dependencyReadyEvent };
	const auto startupWait = WaitForMultipleObjects(
		static_cast<DWORD>(std::size(startupEvents)),
		startupEvents,
		FALSE,
		INFINITE
	);
	RETURN_LAST_ERROR_IF(startupWait == WAIT_FAILED);
	RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_CANCELLED), startupWait == WAIT_OBJECT_0);
	RETURN_HR_IF(E_UNEXPECTED, startupWait != WAIT_OBJECT_0 + 1);

	g_dwmInjectionMap.clear();
	RETURN_IF_WIN32_BOOL_FALSE(SetEvent(control.readyEvent));

	const auto GetThreadStatus = [&control]() -> ThreadStatus
	{
		if (WaitForSingleObject(control.stopEvent, 0) == WAIT_OBJECT_0)
		{
			return ThreadStatus::Stopped;
		}
		return WaitForSingleObject(control.runEvent, 0) == WAIT_OBJECT_0 ?
			ThreadStatus::Running :
			ThreadStatus::Paused;
	};
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
			const auto& [injectionTimeStamp, _] = it->second;
			if (currentTimeStamp - injectionTimeStamp >= std::chrono::minutes{ 3 })
			{
				it = g_dwmInjectionMap.erase(it);
			}
			else
			{
				it++;
			}
		}
		if (GetThreadStatus() != ThreadStatus::Running)
		{
			goto wait_until_next_cycle;
		}

		WalkDwmProcesses([&](DWORD processId) -> bool
		{
			if (GetThreadStatus() != ThreadStatus::Running)
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
					const auto& [injectionTimeStamp, processHandle] = it->second;

					DWORD exitCode{ 0 };
					LOG_IF_WIN32_BOOL_FALSE(GetExitCodeProcess(processHandle.get(), &exitCode));
					// DWM constantly crashes or manual fast fail triggered by user
					if (
						(
							currentTimeStamp - injectionTimeStamp <= std::chrono::seconds{ 15 } &&
							// DWM shutdown by session manager
							exitCode != 0xD00002FE
						) ||
						exitCode == 0xC0000409
					)
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
						if (response == IDABORT)
						{
							LOG_IF_WIN32_BOOL_FALSE(SetEvent(control.stopEvent));
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
				}

				if (const auto hresult = InjectOpenGlassDLL(processId, true); SUCCEEDED(hresult))
				{
					g_dwmInjectionMap.insert_or_assign(sessionId, CDwmProcessInfo{ currentTimeStamp, OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId) });
				}
			}

			return true;
		});

	wait_until_next_cycle:
		const auto status = GetThreadStatus();
		if (status == ThreadStatus::Paused)
		{
			const HANDLE waitEvents[]{ control.stopEvent, control.runEvent };
			const auto waitResult = WaitForMultipleObjects(
				static_cast<DWORD>(std::size(waitEvents)),
				waitEvents,
				FALSE,
				INFINITE
			);
			if (waitResult == WAIT_FAILED)
			{
				hr = HRESULT_FROM_WIN32(GetLastError());
				break;
			}
		}
		else if (status == ThreadStatus::Running)
		{
			const HANDLE waitEvents[]{ control.stopEvent, control.wakeEvent };
			const auto waitResult = WaitForMultipleObjects(
				static_cast<DWORD>(std::size(waitEvents)),
				waitEvents,
				FALSE,
				2000ul
			);
			if (waitResult == WAIT_FAILED)
			{
				hr = HRESULT_FROM_WIN32(GetLastError());
				break;
			}
		}
	}
	while (GetThreadStatus() != ThreadStatus::Stopped);

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

static HRESULT WaitForPipeOperation(
	HANDLE pipe,
	OVERLAPPED& overlapped,
	HANDLE stopEvent,
	DWORD* bytesTransferred
)
{
	const HANDLE waitHandles[]{ stopEvent, overlapped.hEvent };
	const auto waitResult = WaitForMultipleObjects(
		static_cast<DWORD>(std::size(waitHandles)),
		waitHandles,
		FALSE,
		INFINITE
	);
	RETURN_LAST_ERROR_IF(waitResult == WAIT_FAILED);
	if (waitResult == WAIT_OBJECT_0)
	{
		CancelIoEx(pipe, &overlapped);
		DWORD ignored{};
		GetOverlappedResult(pipe, &overlapped, &ignored, TRUE);
		return HRESULT_FROM_WIN32(ERROR_CANCELLED);
	}
	RETURN_HR_IF(E_UNEXPECTED, waitResult != WAIT_OBJECT_0 + 1);

	DWORD transferred{};
	RETURN_IF_WIN32_BOOL_FALSE(GetOverlappedResult(pipe, &overlapped, &transferred, FALSE));
	if (bytesTransferred)
	{
		*bytesTransferred = transferred;
	}
	return S_OK;
}

static HRESULT ConnectPipeClient(HANDLE pipe, HANDLE stopEvent)
{
	wil::unique_handle operationEvent{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
	RETURN_LAST_ERROR_IF_NULL(operationEvent);
	OVERLAPPED overlapped{ .hEvent = operationEvent.get() };

	if (ConnectNamedPipe(pipe, &overlapped))
	{
		return S_OK;
	}
	const auto error = GetLastError();
	if (error == ERROR_PIPE_CONNECTED)
	{
		return S_OK;
	}
	RETURN_HR_IF(HRESULT_FROM_WIN32(error), error != ERROR_IO_PENDING);
	return WaitForPipeOperation(pipe, overlapped, stopEvent, nullptr);
}

static HRESULT TransferPipeMessage(
	HANDLE pipe,
	void* buffer,
	DWORD bufferSize,
	bool write,
	HANDLE stopEvent
)
{
	wil::unique_handle operationEvent{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
	RETURN_LAST_ERROR_IF_NULL(operationEvent);
	OVERLAPPED overlapped{ .hEvent = operationEvent.get() };

	const auto started = write ?
		WriteFile(pipe, buffer, bufferSize, nullptr, &overlapped) :
		ReadFile(pipe, buffer, bufferSize, nullptr, &overlapped);
	if (!started)
	{
		const auto error = GetLastError();
		RETURN_HR_IF(HRESULT_FROM_WIN32(error), error != ERROR_IO_PENDING);
	}

	DWORD transferred{};
	RETURN_IF_FAILED(WaitForPipeOperation(pipe, overlapped, stopEvent, &transferred));
	RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BAD_LENGTH), transferred != bufferSize);
	return S_OK;
}
HRESULT GlassService::RunServerThread(const ThreadControl& control)
{
	RETURN_HR_IF(E_INVALIDARG, !control.readyEvent || !control.stopEvent);
	RETURN_IF_FAILED(SetThreadDescription(GetCurrentThread(), L"OpenGlass Server Thread"));
	RETURN_IF_WIN32_BOOL_FALSE(SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS));

	RETURN_IF_FAILED(RoInitialize(RO_INIT_MULTITHREADED));
	const wil::unique_rouninitialize_call wrtScope{};

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
				PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
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
	RETURN_IF_WIN32_BOOL_FALSE(SetEvent(control.readyEvent));

	while (WaitForSingleObject(control.stopEvent, 0) != WAIT_OBJECT_0)
	{
		const auto connectResult = ConnectPipeClient(pipe.get(), control.stopEvent);
		if (connectResult == HRESULT_FROM_WIN32(ERROR_CANCELLED))
		{
			break;
		}
		RETURN_IF_FAILED(connectResult);

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
			THROW_IF_FAILED(TransferPipeMessage(pipe.get(), &content, sizeof(content), false, control.stopEvent));
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), content.type != RequestType::OpenUserRegistry);
			THROW_IF_FAILED(OpenUserRegistryForDwm(content, clientProcessId));
			THROW_IF_FAILED(TransferPipeMessage(pipe.get(), &content, sizeof(content), true, control.stopEvent));
			THROW_IF_WIN32_BOOL_FALSE(FlushFileBuffers(pipe.get()));
		}
		catch(...) {}

	on_named_pipe_disconnected:
		RETURN_IF_WIN32_BOOL_FALSE(DisconnectNamedPipe(pipe.get()));
	}

	return S_OK;
}

HRESULT GlassService::SendRequest(RequestBuffer& content)
{
	RETURN_HR_IF(E_INVALIDARG, content.type != RequestType::OpenUserRegistry);

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

	DWORD bytesTransferred{};
	RETURN_IF_WIN32_BOOL_FALSE(WriteFile(pipe.get(), &content, sizeof(content), &bytesTransferred, nullptr));
	RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BAD_LENGTH), bytesTransferred != sizeof(content));
	RETURN_IF_WIN32_BOOL_FALSE(ReadFile(pipe.get(), &content, sizeof(content), &bytesTransferred, nullptr));
	RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_BAD_LENGTH), bytesTransferred != sizeof(content));
	RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), content.type != RequestType::OpenUserRegistry);

	return S_OK;
}

bool GlassService::IsActive()
{
	return static_cast<bool>(WaitNamedPipeW(c_pipeName, NMPWAIT_NOWAIT));
}

DWORD GlassService::InjectionThreadEntryPoint(LPVOID parameter)
{
	if (!parameter)
	{
		return static_cast<DWORD>(E_INVALIDARG);
	}
	return RunInjectionThread(*static_cast<ThreadControl*>(parameter));
}

DWORD WINAPI GlassService::ServerThreadEntryPoint(PVOID parameter)
{
	if (!parameter)
	{
		return static_cast<DWORD>(E_INVALIDARG);
	}
	return RunServerThread(*static_cast<ThreadControl*>(parameter));
}
