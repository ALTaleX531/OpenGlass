#include "pch.h"
#include "resource.h"
#include "Utils.hpp"
#include "ServiceApi.hpp"
#include "HookHelper.hpp"

namespace OpenGlass
{
	constexpr std::wstring_view pipe_name{ L"\\\\.\\pipe\\OpenGlassHostPipe" };
	std::unordered_map<DWORD, std::chrono::steady_clock::time_point> g_dwmInjectionMap{};
	std::chrono::steady_clock::time_point g_dwmInjectionCheckPoint{};
	bool g_serverClosed{ false };
}
using namespace OpenGlass;

HRESULT Server::DuplicateUserRegistryKeyToDwm(PipeContent& content) try
{
	wil::unique_handle processHandle{ OpenProcess(PROCESS_DUP_HANDLE, FALSE, content.processId) };
	THROW_LAST_ERROR_IF_NULL(processHandle);
	DWORD sessionId{ 0 };
	THROW_IF_WIN32_BOOL_FALSE(ProcessIdToSessionId(content.processId, &sessionId));
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
		auto revertCleanUp = wil::scope_exit([] { THROW_IF_WIN32_BOOL_FALSE(RevertToSelf()); });
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

HRESULT Server::InjectDllToDwm(DWORD processId, bool inject, bool* actionPerformed)
{
	static auto s_OpenGlassDllPath{ wil::GetModuleFileNameW<std::wstring, MAX_PATH + 1>(wil::GetModuleInstanceHandle()) };
	wil::unique_handle processHandle{ OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, processId) };
	HMODULE moduleHandle{ HookHelper::GetProcessModule(processHandle.get(), s_OpenGlassDllPath.c_str())};
	if (inject && moduleHandle)
	{
		return S_OK;
	}

	OutputDebugStringW(std::format(L"dwm {}. (PID: {})\n", inject ? L"injected" : L"uninjected", processId).c_str());
	auto bufferSize{ (s_OpenGlassDllPath.size() + 1) * sizeof(WCHAR) };
	auto remoteAddress{ inject ? VirtualAllocEx(processHandle.get(), nullptr, bufferSize, MEM_COMMIT, PAGE_READWRITE) : nullptr };
	if (inject)
	{
		RETURN_LAST_ERROR_IF_NULL(remoteAddress);
	}
	auto cleanUp = wil::scope_exit([&processHandle, &remoteAddress]
	{
		if (remoteAddress)
		{
			VirtualFreeEx(processHandle.get(), remoteAddress, 0, MEM_RELEASE);
			remoteAddress = nullptr;
		}
	});

	auto startRoutine
	{ 
		inject ? 
		reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryW) :
		reinterpret_cast<LPTHREAD_START_ROUTINE>(FreeLibrary)
	};
	if (inject)
	{
		RETURN_IF_WIN32_BOOL_FALSE(WriteProcessMemory(processHandle.get(), remoteAddress, static_cast<LPCVOID>(s_OpenGlassDllPath.c_str()), bufferSize, nullptr));
	}
	wil::unique_handle threadHandle{ nullptr };
	static const auto s_pfnNtCreateThreadEx{ reinterpret_cast<NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, LPVOID, HANDLE, LPTHREAD_START_ROUTINE, LPVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, LPVOID)>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateThreadEx")) };
	NTSTATUS ntstatus{ s_pfnNtCreateThreadEx(&threadHandle, PROCESS_ALL_ACCESS, nullptr, processHandle.get(), startRoutine, inject ? remoteAddress : moduleHandle, 0x0, 0x0, 0x0, 0x0, nullptr)};
	RETURN_IF_NTSTATUS_FAILED(ntstatus);
	RETURN_LAST_ERROR_IF(WaitForSingleObject(threadHandle.get(), 1000) != WAIT_OBJECT_0);
	if (actionPerformed)
	{
		*actionPerformed = true;
	}

	return S_OK;
}

DWORD Server::InjectionThreadProc(LPVOID)
{
	constexpr auto SE_DEBUG_PRIVILEGE{ 0x14 };
	static const auto s_pfnRtlAdjustPrivilege{ reinterpret_cast<NTSTATUS(NTAPI*)(int, BOOLEAN, BOOLEAN, PBOOLEAN)>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlAdjustPrivilege")) };
	
	BOOLEAN result = false; 
	s_pfnRtlAdjustPrivilege(SE_DEBUG_PRIVILEGE, true, false, &result);
	g_serverClosed = false;
	g_dwmInjectionMap.clear();
	g_dwmInjectionCheckPoint = std::chrono::steady_clock::now();
	SleepEx(50ul, TRUE);

	auto WalkDwmProcesses = [](std::function<void(DWORD)>&& callback)
	{
		wil::unique_handle snapshot{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
		PROCESSENTRY32W pe{ sizeof(pe) };
		RETURN_IF_WIN32_BOOL_FALSE(Process32FirstW(snapshot.get(), &pe));

		do { if (!_wcsicmp(pe.szExeFile, L"dwm.exe")) { callback(pe.th32ProcessID); } } while (Process32NextW(snapshot.get(), &pe));
		return S_OK;
	};

	HRESULT hr{ S_OK };
	bool injectionSusepended{ false };
	while (!g_serverClosed)
	{
		hr = WalkDwmProcesses([&injectionSusepended](DWORD processId)
		{
			if (injectionSusepended)
			{
				return;
			}

			DWORD sessionId{ 0 };
			LOG_IF_WIN32_BOOL_FALSE(ProcessIdToSessionId(processId, &sessionId));
			auto currentTimeStamp{ std::chrono::steady_clock::now() };

			bool actionPerformed{ false };
			LOG_IF_FAILED(InjectDllToDwm(processId, true, &actionPerformed));

			if (actionPerformed)
			{
				// DWM crashes constantly
				auto it{ g_dwmInjectionMap.find(sessionId) };
				if (it != g_dwmInjectionMap.end())
				{
					if (currentTimeStamp - it->second <= std::chrono::seconds{ 15 })
					{
						auto title{ Utils::GetResWStringView<IDS_STRING101>() };
						auto content{ Utils::GetResWStringView<IDS_STRING105>() };
						DWORD response{ IDTIMEOUT };
						LOG_IF_WIN32_BOOL_FALSE(
							WTSSendMessageW(
								WTS_CURRENT_SERVER_HANDLE,
								sessionId,
								const_cast<LPWSTR>(title.data()),
								static_cast<DWORD>(title.size() * sizeof(WCHAR)),
								const_cast<LPWSTR>(content.data()),
								static_cast<DWORD>(content.size() * sizeof(WCHAR)),
								MB_ICONERROR,
								0,
								&response,
								FALSE
							)
						);
						injectionSusepended = true;
						return;
					}
				}

				g_dwmInjectionMap.insert_or_assign(sessionId, currentTimeStamp);
				g_dwmInjectionCheckPoint = currentTimeStamp;
			}
			else if (currentTimeStamp - g_dwmInjectionCheckPoint >= std::chrono::minutes{ 2 }) // GC
			{
				g_dwmInjectionMap.clear();
				g_dwmInjectionCheckPoint = currentTimeStamp;
			}
		});
		LOG_IF_FAILED(hr);

		SleepEx(injectionSusepended ? INFINITE : 3000ul, TRUE);
	}
	hr = WalkDwmProcesses([](DWORD processId)
	{
		LOG_IF_FAILED(InjectDllToDwm(processId, false));
	});
	LOG_IF_FAILED(hr);

	return S_OK;
}

HRESULT Server::Run()
{
	wil::unique_sid sid{ nullptr };
	SID_IDENTIFIER_AUTHORITY authority{ SECURITY_WORLD_SID_AUTHORITY };
	RETURN_IF_WIN32_BOOL_FALSE(
		AllocateAndInitializeSid(
			&authority,
			1,
			SECURITY_WORLD_RID,
			0,
			0,
			0,
			0,
			0,
			0,
			0,
			&sid
		)
	);

	EXPLICIT_ACCESS_W explicitAccess{ GENERIC_ALL, SET_ACCESS, NO_INHERITANCE, {.TrusteeForm{TRUSTEE_IS_SID}, .TrusteeType{TRUSTEE_IS_WELL_KNOWN_GROUP}, .ptstrName{reinterpret_cast<LPWCH>(sid.get())}} };
	PACL acl{ nullptr };
	RETURN_IF_WIN32_ERROR(
		SetEntriesInAclW(1ul, &explicitAccess, nullptr, &acl)
	);
	auto cleanUp = wil::scope_exit([&acl] { if (acl) { LocalFree(reinterpret_cast<HLOCAL>(acl)); acl = nullptr; } });

	SECURITY_DESCRIPTOR descriptor{};
	RETURN_IF_WIN32_BOOL_FALSE(
		InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION)
	);
	RETURN_IF_WIN32_BOOL_FALSE(
		SetSecurityDescriptorDacl(&descriptor, TRUE, acl, FALSE)
	);

	SECURITY_ATTRIBUTES attributes{ sizeof(SECURITY_ATTRIBUTES), &descriptor, FALSE };
	wil::unique_handle pipe
	{
		CreateNamedPipeW(
			pipe_name.data(),
			PIPE_ACCESS_DUPLEX,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,
			1024ul,
			1024ul,
			0ul,
			&attributes
		)
	};
	wil::unique_handle injectionThread{ CreateThread(nullptr, 0, InjectionThreadProc, nullptr, 0, nullptr) };

	bool stop{ false };
	while (!stop)
	{
		DWORD error{ 0ul };
		if (ConnectNamedPipe(pipe.get(), nullptr) || (error = GetLastError()) == ERROR_PIPE_CONNECTED)
		{
			OutputDebugStringW(L"pipe connected, waiting for the input from dwm...\n");
			PipeContent content{};
			if (ReadFile(pipe.get(), &content, sizeof(content), nullptr, nullptr) && !(stop = (content.processId == -1)))
			{
				OutputDebugStringW(std::format(L"handling request for dwm (PID: {})...\n", content.processId).c_str());
				LOG_IF_FAILED(DuplicateUserRegistryKeyToDwm(content));
				LOG_IF_WIN32_BOOL_FALSE(WriteFile(pipe.get(), &content, sizeof(content), nullptr, nullptr));
				LOG_IF_WIN32_BOOL_FALSE(FlushFileBuffers(pipe.get()));
			}
			OutputDebugStringW(std::format(L"pipe disconnected for dwm (PID: {}).\n", content.processId).c_str());
			LOG_IF_WIN32_BOOL_FALSE(DisconnectNamedPipe(pipe.get()));
		}
		Sleep(0);
	}
	
	QueueUserAPC([](ULONG_PTR){g_serverClosed = true; }, injectionThread.get(), 0);
	WaitForSingleObject(injectionThread.get(), INFINITE);

	return S_OK;
}

HRESULT Client::RequestUserRegistryKey(PipeContent& content)
{
	wil::unique_hfile pipe
	{
		CreateFile2(
			pipe_name.data(),
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

		if (!WaitNamedPipeW(pipe_name.data(), 3000ul))
		{
			error = GetLastError();
			RETURN_WIN32(error == ERROR_SEM_TIMEOUT ? ERROR_SERVICE_REQUEST_TIMEOUT : error);
		}
		pipe.reset(
			CreateFile2(
				pipe_name.data(),
				GENERIC_READ | GENERIC_WRITE,
				0,
				OPEN_EXISTING,
				nullptr
			)
		);
	}

	RETURN_IF_WIN32_BOOL_FALSE(WriteFile(pipe.get(), &content, sizeof(content), nullptr, nullptr));
	OutputDebugStringW(L"request sent, waiting for the response from host process...\n");
	if (content.processId != -1)
	{
		OutputDebugStringW(L"reading the data sent by the host process...\n");
		RETURN_IF_WIN32_BOOL_FALSE(ReadFile(pipe.get(), &content, sizeof(content), nullptr, nullptr));
	}

	return S_OK;
}