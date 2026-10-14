#include "HookHelper.hpp"
#include "Core.hpp"
#include "ColorHooks.hpp"
#include "DuiHooks.hpp"
#include <objbase.h>
#include <ktmw32.h>
#include <tlhelp32.h>
#include <array>
#include <string>
#include <wil/resource.h>
#include <wil/result.h>
#include <wil/win32_helpers.h>
#include <wil/registry.h>
#include <wil/token_helpers.h>
#include <winrt/base.h>

using namespace OpenGlass;
namespace OpenGlass
{
	constexpr CLSID CLSID_ThemeManager2{ 0x9324DA94, 0x50EC, 0x4A14, { 0xA7, 0x70, 0xE9, 0x0C, 0xA0, 0x3E, 0x7C, 0x8F } };
	constexpr CLSID CLSID_ThemeManagerShared{ 0xC04B329E, 0x5823, 0x4415, { 0x9C, 0x93, 0xBA, 0x44, 0x68, 0x89, 0x47, 0xB0 } };
	constexpr CLSID CLSID_ThemeCplElementProvider{ 0x84CCE1D2, 0x97AD, 0x4448, { 0xAF, 0x0F, 0xA7, 0x91, 0x64, 0x07, 0x3A, 0x60 } };
	constexpr CLSID CLSID_OpenGlassThemeManager2{ 0x61CF883F, 0x296D, 0x4EFA, { 0xBF, 0x20, 0x73, 0xA9, 0xC8, 0xE4, 0x26, 0xA1 } };
	constexpr CLSID CLSID_OpenGlassThemeManagerShared{ 0x9B34D870, 0xC87D, 0x435E, { 0xA6, 0x7C, 0x58, 0xD0, 0x1C, 0x26, 0x37, 0x64 } };
	constexpr CLSID CLSID_OpenGlassThemeCplElementProvider{ 0x48A77CA6, 0x2BF8, 0x4BC0, { 0x97, 0xB6, 0xF9, 0xF1, 0x25, 0x68, 0xE4, 0x62 } };

	struct ClassRoute
	{
		CLSID original;
		CLSID proxy;
		HookType type;
	};
	constexpr std::array g_routes
	{
		ClassRoute{ CLSID_ThemeManager2, CLSID_OpenGlassThemeManager2, HookType::UI },
		ClassRoute{ CLSID_ThemeManagerShared, CLSID_OpenGlassThemeManagerShared, HookType::UI },
		ClassRoute{ CLSID_ThemeCplElementProvider, CLSID_OpenGlassThemeCplElementProvider, HookType::CPL }
	};

	void UpdateRegistration(PCWSTR modulePath, bool install)
	{
		// Closing an uncommitted transaction rolls back all three routes, including key creation/deletion.
		wil::unique_hfile transaction{ CreateTransaction(nullptr, nullptr, 0, 0, 0, 0, nullptr) };
		THROW_LAST_ERROR_IF(!transaction);
		wil::unique_hkey classes{};
		THROW_IF_WIN32_ERROR(
			RegOpenKeyTransactedW(
				HKEY_LOCAL_MACHINE,
				L"SOFTWARE\\Classes\\CLSID",
				0,
				KEY_READ | KEY_WRITE | DELETE,
				classes.put(),
				transaction.get(),
				nullptr
			)
		);

		for (const auto& route : g_routes)
		{
			const auto originalId = winrt::to_hstring(route.original);
			const auto proxyId = winrt::to_hstring(route.proxy);
			const auto serverPath = std::wstring(proxyId) + L"\\InprocServer32";
			const auto treatAsPath = std::wstring(originalId) + L"\\TreatAs";

			wil::unique_hkey treatAs{};
			auto hr = HRESULT_FROM_WIN32(
				RegOpenKeyTransactedW(
					classes.get(),
					treatAsPath.c_str(),
					0,
					KEY_READ | KEY_WRITE,
					treatAs.put(),
					transaction.get(),
					nullptr
				)
			);
			THROW_HR_IF(
				hr,
				FAILED(hr) && !wil::reg::is_registry_not_found(hr)
			);

			if (install)
			{
				if (treatAs)
				{
					wil::unique_cotaskmem_string redirect{};
					const auto result = wil::reg::get_value_string_nothrow(treatAs.get(), nullptr, redirect);
					if (!wil::reg::is_registry_not_found(result))
					{
						THROW_IF_FAILED(result);
						THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), _wcsicmp(redirect.get(), proxyId.c_str()));
					}
				}
				wil::unique_hkey server{};
				THROW_IF_WIN32_ERROR(
					RegOpenKeyTransactedW(
						classes.get(),
						(std::wstring(originalId) + L"\\InprocServer32").c_str(),
						0,
						KEY_READ,
						server.put(),
						transaction.get(),
						nullptr
					)
				);
				const auto threadingModel = wil::reg::get_value_string<wil::unique_cotaskmem_string>(server.get(), L"ThreadingModel");
				THROW_IF_WIN32_ERROR(
					RegCreateKeyTransactedW(
						classes.get(),
						serverPath.c_str(),
						0,
						nullptr,
						REG_OPTION_NON_VOLATILE,
						KEY_READ | KEY_WRITE,
						nullptr,
						server.put(),
						nullptr,
						transaction.get(),
						nullptr
					)
				);
				wil::reg::set_value_string(server.get(), nullptr, modulePath);
				wil::reg::set_value_string(server.get(), L"ThreadingModel", threadingModel.get());
				THROW_IF_WIN32_ERROR(
					RegCreateKeyTransactedW(
						classes.get(),
						treatAsPath.c_str(),
						0,
						nullptr,
						REG_OPTION_NON_VOLATILE,
						KEY_READ | KEY_WRITE,
						nullptr,
						treatAs.put(),
						nullptr,
						transaction.get(),
						nullptr
					)
				);
				wil::reg::set_value_string(treatAs.get(), nullptr, proxyId.c_str());
			}
			else
			{
				const auto Equals = [](HKEY key, PCWSTR name, PCWSTR expected)
				{
					wil::unique_cotaskmem_string value{};
					const auto result = wil::reg::get_value_string_nothrow(key, name, value);
					if (wil::reg::is_registry_not_found(result) || result == HRESULT_FROM_WIN32(ERROR_UNSUPPORTED_TYPE)) return false;
					THROW_IF_FAILED(result);
					return !_wcsicmp(value.get(), expected);
				};

				if (treatAs && Equals(treatAs.get(), nullptr, proxyId.c_str()))
				{
					THROW_IF_WIN32_ERROR(RegDeleteValueW(treatAs.get(), nullptr));
					if (!wil::reg::get_child_key_count(treatAs.get()) && !wil::reg::get_child_value_count(treatAs.get()))
					{
						THROW_IF_WIN32_ERROR(RegDeleteKeyTransactedW(classes.get(), treatAsPath.c_str(), 0, 0, transaction.get(), nullptr));
					}
				}
				hr = HRESULT_FROM_WIN32(RegDeleteTreeW(classes.get(), proxyId.c_str()));
				THROW_HR_IF(
					hr,
					FAILED(hr) && !wil::reg::is_registry_not_found(hr)
				);
			}
		}
		THROW_IF_WIN32_BOOL_FALSE(CommitTransaction(transaction.get()));
	}

	wil::unique_handle DuplicateSystemProcessToken(DWORD pid)
	{
		wil::unique_handle process{ OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid) };
		THROW_LAST_ERROR_IF(!process);
		wil::unique_handle token{}, copy{};
		THROW_IF_WIN32_BOOL_FALSE(OpenProcessToken(process.get(), TOKEN_DUPLICATE | TOKEN_QUERY, token.put()));
		const auto user = wil::get_token_information<TOKEN_USER>(token.get());
		THROW_HR_IF(E_ACCESSDENIED, !IsWellKnownSid(static_cast<PSID>(user->User.Sid), WinLocalSystemSid));
		THROW_IF_WIN32_BOOL_FALSE(DuplicateTokenEx(token.get(), TOKEN_IMPERSONATE | TOKEN_QUERY, nullptr, SecurityImpersonation, TokenImpersonation, copy.put()));
		return copy;
	}

	void RunRegistrationAsTrustedInstaller(bool install)
	{
		const auto revert = wil::run_as_self();
		const auto token = wil::open_current_access_token(TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES);
		DWORD bytes{};
		TOKEN_PRIVILEGES privileges{ 1 }, previous{};
		privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		THROW_IF_WIN32_BOOL_FALSE(LookupPrivilegeValueW(nullptr, SE_DEBUG_NAME, &privileges.Privileges[0].Luid));
		THROW_IF_WIN32_BOOL_FALSE(AdjustTokenPrivileges(token.get(), FALSE, &privileges, sizeof(previous), &previous, &bytes));
		THROW_IF_WIN32_ERROR(GetLastError());
		const auto restorePrivileges = wil::scope_exit([&]
		{
			FAIL_FAST_IF_WIN32_BOOL_FALSE(AdjustTokenPrivileges(token.get(), FALSE, &previous, 0, nullptr, nullptr));
		});

		wil::unique_hfile snapshot{ CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0) };
		THROW_LAST_ERROR_IF(!snapshot);
		PROCESSENTRY32W entry{ sizeof(entry) };
		THROW_IF_WIN32_BOOL_FALSE(Process32FirstW(snapshot.get(), &entry));
		wil::unique_handle systemToken{};
		do
		{
			if (!_wcsicmp(entry.szExeFile, L"winlogon.exe"))
			{
				systemToken = DuplicateSystemProcessToken(entry.th32ProcessID);
				break;
			}
		} while (Process32NextW(snapshot.get(), &entry));
		THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_NOT_FOUND), !systemToken);
		// SYSTEM permits opening the TI token; TI's service SID permits writes to protected COM keys.
		THROW_IF_WIN32_BOOL_FALSE(SetThreadToken(nullptr, systemToken.get()));

		wil::unique_schandle manager{ OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT) };
		THROW_LAST_ERROR_IF(!manager);
		wil::unique_schandle service{ OpenServiceW(manager.get(), L"TrustedInstaller", SERVICE_QUERY_STATUS | SERVICE_START) };
		THROW_LAST_ERROR_IF(!service);
		SERVICE_STATUS_PROCESS status{};
		const auto deadline = GetTickCount64() + 30000;
		for (;;)
		{
			THROW_IF_WIN32_BOOL_FALSE(
				QueryServiceStatusEx(
					service.get(),
					SC_STATUS_PROCESS_INFO,
					reinterpret_cast<BYTE*>(&status),
					sizeof(status),
					&bytes
				)
			);
			if (status.dwCurrentState == SERVICE_RUNNING) break;
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_TIMEOUT), GetTickCount64() >= deadline);
			if (status.dwCurrentState == SERVICE_STOPPED && !StartServiceW(service.get(), 0, nullptr))
			{
				const auto error = GetLastError();
				THROW_HR_IF(HRESULT_FROM_WIN32(error), error != ERROR_SERVICE_ALREADY_RUNNING);
			}
			Sleep(100);
		}
		const auto impersonationToken = DuplicateSystemProcessToken(status.dwProcessId);
		auto installerSid = wil::make_static_sid(
			SECURITY_NT_AUTHORITY,
			SECURITY_SERVICE_ID_BASE_RID,
			SECURITY_TRUSTED_INSTALLER_RID1,
			SECURITY_TRUSTED_INSTALLER_RID2,
			SECURITY_TRUSTED_INSTALLER_RID3,
			SECURITY_TRUSTED_INSTALLER_RID4,
			SECURITY_TRUSTED_INSTALLER_RID5
		);
		BOOL isInstaller{};
		THROW_IF_WIN32_BOOL_FALSE(CheckTokenMembership(impersonationToken.get(), reinterpret_cast<PSID>(&installerSid), &isInstaller));
		THROW_HR_IF(E_ACCESSDENIED, !isInstaller);

		THROW_IF_WIN32_BOOL_FALSE(SetThreadToken(nullptr, impersonationToken.get()));
		UpdateRegistration(wil::GetModuleFileNameW<wil::unique_cotaskmem_string>(wil::GetModuleInstanceHandle()).get(), install);
	}
}

extern "C" HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** object)
{
	if (!object) return E_POINTER;
	*object = nullptr;

	try
	{
		const ClassRoute* matchedRoute{ nullptr };
		for (const auto& route : g_routes)
		{
			if (clsid == route.proxy)
			{
				matchedRoute = &route;
				break;
			}
		}
		if (!matchedRoute)
		{
			return CLASS_E_CLASSNOTAVAILABLE;
		}

		const auto classId = winrt::to_hstring(matchedRoute->original);
		const auto serverPath = std::wstring(L"SOFTWARE\\Classes\\CLSID\\") + classId + L"\\InprocServer32";
		const auto key = wil::reg::open_unique_key(HKEY_LOCAL_MACHINE, serverPath.c_str(), wil::reg::key_access::read);
		const auto server = wil::reg::get_value_expanded_string<wil::unique_cotaskmem_string>(key.get(), nullptr);
		wil::unique_hmodule moduleHandle
		{
			LoadLibraryExW(
				server.get(), nullptr,
				LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32
			)
		};
		THROW_LAST_ERROR_IF(!moduleHandle);

		THROW_HR_IF(E_UNEXPECTED, moduleHandle.get() == wil::GetModuleInstanceHandle());
		const auto getClassObject = GetProcAddressByFunctionDeclaration(moduleHandle.get(), DllGetClassObject);
		THROW_LAST_ERROR_IF(!getClassObject);

		HMODULE pinned{};
		THROW_IF_WIN32_BOOL_FALSE(
			GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
				reinterpret_cast<PCWSTR>(moduleHandle.get()),
				&pinned
			)
		);

		THROW_IF_WIN32_BOOL_FALSE(
			GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
				reinterpret_cast<PCWSTR>(wil::GetModuleInstanceHandle()),
				&pinned
			)
		);

		const auto result = getClassObject(matchedRoute->original, iid, object);
		RETURN_IF_FAILED(result);
		// Unsupported hooks must not invalidate a successful native activation.
		try
		{
			HookHelper::HookTransaction transaction{ HookHelper::HookMode::Install };
			// Resolve every required entry before queuing any hooks.
			const auto duiHook = DuiHooks::Prepare(matchedRoute->type);
			const auto colorHook = ColorHooks::Prepare(matchedRoute->type);
			if (duiHook) transaction.Apply(*duiHook);
			if (colorHook) transaction.Apply(*colorHook);
			transaction.Commit();
		}
		CATCH_LOG();
		return result;
	}
	CATCH_RETURN();
}

extern "C" HRESULT WINAPI DllCanUnloadNow()
{
	return S_FALSE;
}

extern "C" HRESULT WINAPI DllRegisterServer() noexcept
{
	try
	{
		RunRegistrationAsTrustedInstaller(true);
		return S_OK;
	}
	CATCH_RETURN();
}

extern "C" HRESULT WINAPI DllUnregisterServer() noexcept
{
	try
	{
		RunRegistrationAsTrustedInstaller(false);
		return S_OK;
	}
	CATCH_RETURN();
}
