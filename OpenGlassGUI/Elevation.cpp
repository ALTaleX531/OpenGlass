#include "pch.h"
#include "Elevation.hpp"

#include <sddl.h>

namespace OpenGlass::Elevation
{
	namespace
	{
		std::wstring GetProcessUserSid(HANDLE process)
		{
			wil::unique_handle token;
			THROW_IF_WIN32_BOOL_FALSE(OpenProcessToken(process, TOKEN_QUERY, token.put()));
			DWORD size{};
			GetTokenInformation(token.get(), TokenUser, nullptr, 0, &size);
			THROW_LAST_ERROR_IF(GetLastError() != ERROR_INSUFFICIENT_BUFFER);
			std::vector<std::byte> buffer(size);
			THROW_IF_WIN32_BOOL_FALSE(GetTokenInformation(token.get(), TokenUser, buffer.data(), size, &size));
			const auto user = reinterpret_cast<const TOKEN_USER*>(buffer.data());
			wil::unique_hlocal_string sid;
			THROW_IF_WIN32_BOOL_FALSE(ConvertSidToStringSidW(user->User.Sid, sid.put()));
			return sid.get();
		}

		std::wstring GetCurrentImagePath()
		{
			std::wstring path(32'768, L'\0');
			DWORD length = static_cast<DWORD>(path.size());
			THROW_IF_WIN32_BOOL_FALSE(QueryFullProcessImageNameW(GetCurrentProcess(), 0, path.data(), &length));
			path.resize(length);
			return path;
		}

		std::wstring GetProcessImagePath(HANDLE process)
		{
			std::wstring path(32'768, L'\0');
			DWORD length = static_cast<DWORD>(path.size());
			THROW_IF_WIN32_BOOL_FALSE(QueryFullProcessImageNameW(process, 0, path.data(), &length));
			path.resize(length);
			return path;
		}

		std::wstring CreatePipeName()
		{
			GUID id{};
			THROW_IF_FAILED(CoCreateGuid(&id));
			WCHAR text[64]{};
			THROW_HR_IF(E_FAIL, StringFromGUID2(id, text, ARRAYSIZE(text)) == 0);
			return std::wstring(L"\\\\.\\pipe\\OpenGlassGUI.Elevation.") + text;
		}

		std::wstring QuoteArgument(std::wstring_view value)
		{
			std::wstring result{ L"\"" };
			for (const auto character : value)
			{
				if (character == L'\"')
				{
					result += L'\\';
				}
				result += character;
			}
			result += L'\"';
			return result;
		}

		std::optional<std::wstring> GetArgument(std::wstring_view name)
		{
			int argumentCount{};
			wil::unique_hlocal_ptr<PWSTR[]> arguments{ CommandLineToArgvW(GetCommandLineW(), &argumentCount) };
			if (!arguments)
			{
				return std::nullopt;
			}
			for (int index = 1; index + 1 < argumentCount; ++index)
			{
				if (arguments[index] == name)
				{
					return arguments[index + 1];
				}
			}
			return std::nullopt;
		}

		std::optional<std::wstring> GetValidatedPipeServerSid(HANDLE pipe)
		{
			ULONG serverPid{};
			if (!GetNamedPipeServerProcessId(pipe, &serverPid))
			{
				return std::nullopt;
			}
			wil::unique_handle process{ OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, serverPid) };
			if (!process)
			{
				return std::nullopt;
			}
			DWORD serverSession{}, currentSession{};
			if (!ProcessIdToSessionId(serverPid, &serverSession)
				|| !ProcessIdToSessionId(GetCurrentProcessId(), &currentSession)
				|| serverSession != currentSession)
			{
				return std::nullopt;
			}
			try
			{
				const auto serverPath = GetProcessImagePath(process.get());
				if (_wcsicmp(serverPath.c_str(), GetCurrentImagePath().c_str()) != 0) return std::nullopt;
				return GetProcessUserSid(process.get());
			}
			catch (...)
			{
				return std::nullopt;
			}
		}

		StartupResult ReceiveStartupIdentity(const std::wstring& pipeName)
		{
			if (!WaitNamedPipeW(pipeName.c_str(), 30'000))
			{
				return {};
			}
			wil::unique_hfile pipe{ CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr) };
			if (!pipe)
			{
				return {};
			}
			const auto serverSid = GetValidatedPipeServerSid(pipe.get());
			if (!serverSid) return {};
			DWORD length{}, read{};
			if (!ReadFile(pipe.get(), &length, sizeof(length), &read, nullptr) || read != sizeof(length) || length == 0 || length > SECURITY_MAX_SID_SIZE * 2)
			{
				return {};
			}
			std::wstring sid(length, L'\0');
			if (!ReadFile(pipe.get(), sid.data(), length * sizeof(WCHAR), &read, nullptr) || read != length * sizeof(WCHAR))
			{
				return {};
			}
			PSID parsedSid{};
			if (!ConvertStringSidToSidW(sid.c_str(), &parsedSid))
			{
				return {};
			}
			LocalFree(parsedSid);
			if (_wcsicmp(sid.c_str(), serverSid->c_str()) != 0) return {};
			const BYTE acknowledgement{ 1 };
			DWORD written{};
			WriteFile(pipe.get(), &acknowledgement, sizeof(acknowledgement), &written, nullptr);
			return { true, std::move(sid) };
		}

		bool LaunchElevated(const std::wstring& userSid)
		{
			const auto pipeName = CreatePipeName();
			const auto sddl = std::wstring(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;") + userSid + L")";
			PSECURITY_DESCRIPTOR descriptor{};
			THROW_IF_WIN32_BOOL_FALSE(ConvertStringSecurityDescriptorToSecurityDescriptorW(
				sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr
			));
			wil::unique_hlocal securityDescriptor{ descriptor };
			SECURITY_ATTRIBUTES attributes{ sizeof(attributes), securityDescriptor.get(), FALSE };
			wil::unique_hfile pipe{ CreateNamedPipeW(
				pipeName.c_str(),
				PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
				PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
				1,
				4096,
				4096,
				30'000,
				&attributes
			) };
			THROW_LAST_ERROR_IF(!pipe);

			const auto parameters = std::wstring(L"--elevated-pipe ") + QuoteArgument(pipeName);
			SHELLEXECUTEINFOW info{ sizeof(info) };
			info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
			info.lpVerb = L"runas";
			const auto imagePath = GetCurrentImagePath();
			info.lpFile = imagePath.c_str();
			info.lpParameters = parameters.c_str();
			info.nShow = SW_SHOWNORMAL;
			if (!ShellExecuteExW(&info))
			{
				return false;
			}
			wil::unique_handle child{ info.hProcess };
			wil::unique_event connectionEvent{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
			THROW_LAST_ERROR_IF(!connectionEvent);
			OVERLAPPED overlapped{};
			overlapped.hEvent = connectionEvent.get();
			bool connected = ConnectNamedPipe(pipe.get(), &overlapped) != FALSE;
			if (!connected)
			{
				const auto error = GetLastError();
				if (error == ERROR_PIPE_CONNECTED)
				{
					connected = true;
				}
				else if (error == ERROR_IO_PENDING)
				{
					const HANDLE waits[]{ connectionEvent.get(), child.get() };
					const auto wait = WaitForMultipleObjects(ARRAYSIZE(waits), waits, FALSE, 30'000);
					if (wait == WAIT_OBJECT_0)
					{
						DWORD transferred{};
						connected = GetOverlappedResult(pipe.get(), &overlapped, &transferred, FALSE) != FALSE;
					}
					else
					{
						CancelIoEx(pipe.get(), &overlapped);
						DWORD transferred{};
						GetOverlappedResult(pipe.get(), &overlapped, &transferred, TRUE);
					}
				}
			}
			if (!connected)
			{
				return false;
			}
			ULONG clientPid{};
			if (!GetNamedPipeClientProcessId(pipe.get(), &clientPid) || clientPid != GetProcessId(child.get()))
			{
				return false;
			}
			auto writePipe = [&](const void* data, DWORD size)
			{
				wil::unique_event event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
				if (!event) return false;
				OVERLAPPED operation{};
				operation.hEvent = event.get();
				DWORD transferred{};
				if (!WriteFile(pipe.get(), data, size, &transferred, &operation))
				{
					if (GetLastError() != ERROR_IO_PENDING) return false;
					if (WaitForSingleObject(event.get(), 30'000) != WAIT_OBJECT_0)
					{
						CancelIoEx(pipe.get(), &operation);
						GetOverlappedResult(pipe.get(), &operation, &transferred, TRUE);
						return false;
					}
					if (!GetOverlappedResult(pipe.get(), &operation, &transferred, FALSE)) return false;
				}
				return transferred == size;
			};
			auto readPipe = [&](void* data, DWORD size)
			{
				wil::unique_event event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
				if (!event) return false;
				OVERLAPPED operation{};
				operation.hEvent = event.get();
				DWORD transferred{};
				if (!ReadFile(pipe.get(), data, size, &transferred, &operation))
				{
					if (GetLastError() != ERROR_IO_PENDING) return false;
					if (WaitForSingleObject(event.get(), 30'000) != WAIT_OBJECT_0)
					{
						CancelIoEx(pipe.get(), &operation);
						GetOverlappedResult(pipe.get(), &operation, &transferred, TRUE);
						return false;
					}
					if (!GetOverlappedResult(pipe.get(), &operation, &transferred, FALSE)) return false;
				}
				return transferred == size;
			};
			const auto length = static_cast<DWORD>(userSid.size());
			if (!writePipe(&length, sizeof(length)) || !writePipe(userSid.data(), length * sizeof(WCHAR)))
			{
				return false;
			}
			BYTE acknowledgement{};
			return readPipe(&acknowledgement, sizeof(acknowledgement)) && acknowledgement == 1;
		}
	}

	bool IsProcessElevated() noexcept
	{
		wil::unique_handle token;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.put()))
		{
			return false;
		}
		TOKEN_ELEVATION elevation{};
		DWORD size{};
		return GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof(elevation), &size)
			&& elevation.TokenIsElevated;
	}

	StartupResult PrepareElevatedStartup()
	{
		try
		{
			if (const auto pipeName = GetArgument(L"--elevated-pipe"))
			{
				if (!IsProcessElevated())
				{
					return {};
				}
				return ReceiveStartupIdentity(*pipeName);
			}
			const auto userSid = GetProcessUserSid(GetCurrentProcess());
			if (IsProcessElevated())
			{
				return { true, userSid };
			}
			LaunchElevated(userSid);
			return {};
		}
		catch (...)
		{
			return {};
		}
	}
}
