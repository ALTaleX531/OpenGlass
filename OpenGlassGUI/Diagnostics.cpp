#include "pch.h"
#include "ApplicationPaths.hpp"
#include "Diagnostics.hpp"

#include <filesystem>
#include <atomic>

#pragma comment(lib, "powrprof.lib")

namespace OpenGlass
{
	namespace
	{
		constexpr PCWSTR DwmCrashDumpKey = LR"(SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\dwm.exe)";
		constexpr PCWSTR DwmConfigurationKey = LR"(SOFTWARE\Microsoft\Windows\DWM)";
		constexpr PCWSTR PersonalizeKey = LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize)";
		constexpr DWORD Windows11_24H2Build = 26100;

		struct PowerModeCapture
		{
			std::atomic<EFFECTIVE_POWER_MODE> mode{ EffectivePowerModeBalanced };
			HANDLE receivedEvent{ nullptr };
		};

		_Function_class_(EFFECTIVE_POWER_MODE_CALLBACK)
		VOID CALLBACK CaptureEffectivePowerMode(EFFECTIVE_POWER_MODE mode, PVOID context)
		{
			auto* capture = static_cast<PowerModeCapture*>(context);
			capture->mode.store(mode, std::memory_order_release);
			SetEvent(capture->receivedEvent);
		}

		DWORD GetWindowsBuildNumber() noexcept
		{
			using RtlGetVersionFn = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);
			const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
				GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")
			);
			if (!rtlGetVersion)
			{
				return 0;
			}

			RTL_OSVERSIONINFOW version{ sizeof(version) };
			return rtlGetVersion(&version) >= 0 ? version.dwBuildNumber : 0;
		}

		HRESULT ResultFromStatus(LSTATUS status) noexcept
		{
			return status == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(status);
		}

		std::filesystem::path GetExecutableDirectory()
		{
			std::vector<WCHAR> modulePath(32768);
			const DWORD length = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
			if (length == 0 || length >= modulePath.size())
			{
				return {};
			}

			return std::filesystem::path{ modulePath.data() }.parent_path();
		}

		HRESULT QueryDword(HKEY key, PCWSTR valueName, DWORD defaultValue, DWORD& value) noexcept
		{
			DWORD type{};
			DWORD size = sizeof(value);
			const LSTATUS status = RegQueryValueExW(
				key,
				valueName,
				nullptr,
				&type,
				reinterpret_cast<BYTE*>(&value),
				&size
			);
			if (status == ERROR_FILE_NOT_FOUND)
			{
				value = defaultValue;
				return S_OK;
			}
			if (status != ERROR_SUCCESS)
			{
				return ResultFromStatus(status);
			}
			if (type != REG_DWORD || size != sizeof(value))
			{
				return HRESULT_FROM_WIN32(ERROR_DATATYPE_MISMATCH);
			}
			return S_OK;
		}

		HRESULT QueryString(HKEY key, PCWSTR valueName, std::wstring& value) noexcept
		{
			DWORD type{};
			DWORD size{};
			LSTATUS status = RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &size);
			if (status == ERROR_FILE_NOT_FOUND)
			{
				value.clear();
				return S_OK;
			}
			if (status != ERROR_SUCCESS)
			{
				return ResultFromStatus(status);
			}
			if ((type != REG_SZ && type != REG_EXPAND_SZ) || size % sizeof(WCHAR) != 0)
			{
				return HRESULT_FROM_WIN32(ERROR_DATATYPE_MISMATCH);
			}

			std::wstring buffer(size / sizeof(WCHAR), L'\0');
			status = RegQueryValueExW(
				key,
				valueName,
				nullptr,
				&type,
				reinterpret_cast<BYTE*>(buffer.data()),
				&size
			);
			if (status != ERROR_SUCCESS)
			{
				return ResultFromStatus(status);
			}
			while (!buffer.empty() && buffer.back() == L'\0')
			{
				buffer.pop_back();
			}
			value = std::move(buffer);
			return S_OK;
		}

		HRESULT QueryDwordValue(HKEY root, const std::wstring& subKey, PCWSTR valueName, DWORD& value) noexcept
		{
			wil::unique_hkey key;
			const HRESULT openResult = wil::reg::open_unique_key_nothrow(root, subKey.c_str(), key, wil::reg::key_access::read);
			if (FAILED(openResult))
			{
				return openResult;
			}
			return wil::reg::get_value_dword_nothrow(key.get(), valueName, &value);
		}

		void ResolveRuntimeDword(
			std::wstring_view userSid,
			PCWSTR valueName,
			DWORD defaultValue,
			DWORD& value,
			DiagnosticRegistrySource& source
		) noexcept
		{
			const std::wstring userKey = std::wstring(userSid) + L"\\" + DwmConfigurationKey;
			const HRESULT userResult = userSid.empty()
				? HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
				: QueryDwordValue(HKEY_USERS, userKey, valueName, value);
			if (SUCCEEDED(userResult))
			{
				source = DiagnosticRegistrySource::User;
				return;
			}
			if (userResult != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
				&& userResult != HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))
			{
				value = defaultValue;
				source = DiagnosticRegistrySource::Default;
				return;
			}
			if (SUCCEEDED(QueryDwordValue(HKEY_LOCAL_MACHINE, DwmConfigurationKey, valueName, value)))
			{
				source = DiagnosticRegistrySource::Machine;
				return;
			}

			value = defaultValue;
			source = DiagnosticRegistrySource::Default;
		}

		HRESULT ResolveDumpFolder(std::wstring_view requestedFolder, std::filesystem::path& resolvedFolder) noexcept
		try
		{
			if (requestedFolder.empty())
			{
				return E_INVALIDARG;
			}

			resolvedFolder = std::filesystem::path{ requestedFolder };
			if (resolvedFolder.is_relative())
			{
				const auto executableDirectory = GetExecutableDirectory();
				if (executableDirectory.empty())
				{
					return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
				}
				resolvedFolder = executableDirectory / resolvedFolder;
			}
			resolvedFolder = resolvedFolder.lexically_normal();

			std::error_code error;
			std::filesystem::create_directories(resolvedFolder, error);
			if (error)
			{
				return HRESULT_FROM_WIN32(error.value());
			}
			if (!std::filesystem::is_directory(resolvedFolder, error))
			{
				return error ? HRESULT_FROM_WIN32(error.value()) : HRESULT_FROM_WIN32(ERROR_DIRECTORY);
			}
			return S_OK;
		}
		catch (...)
		{
			return wil::ResultFromCaughtException();
		}
	}

	std::wstring GetDefaultDwmCrashDumpFolder()
	{
		return ApplicationPaths::GetProgramDataSubdirectory(L"dumps").wstring();
	}

	HRESULT QueryTransparencyDiagnostics(std::wstring_view userSid, TransparencyDiagnostics& diagnostics) noexcept
	try
	{
		diagnostics = {};

		DWORD transparencyEnabled{};
		const std::wstring personalizeKey = std::wstring(userSid) + L"\\" + PersonalizeKey;
		wil::unique_hkey personalize;
		const HRESULT personalizeResult = wil::reg::open_unique_key_nothrow(
			userSid.empty() ? HKEY_CURRENT_USER : HKEY_USERS,
			userSid.empty() ? PersonalizeKey : personalizeKey.c_str(),
			personalize,
			wil::reg::key_access::read
		);
		if (SUCCEEDED(personalizeResult))
		{
			wil::reg::get_value_dword_nothrow(personalize.get(), L"EnableTransparency", &transparencyEnabled);
		}
		diagnostics.windowsTransparencyEnabled = FAILED(personalizeResult) || transparencyEnabled != 0;

		ResolveRuntimeDword(
			userSid,
			L"ColorizationOpaqueBlend",
			0,
			diagnostics.colorizationOpaqueBlend,
			diagnostics.colorizationOpaqueBlendSource
		);
		DWORD disableGlassOnBattery{};
		ResolveRuntimeDword(
			userSid,
			L"DisableGlassOnBattery",
			1,
			disableGlassOnBattery,
			diagnostics.disableGlassOnBatterySource
		);
		diagnostics.disableGlassOnBattery = disableGlassOnBattery != 0;

		wil::unique_handle receivedEvent{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
		RETURN_LAST_ERROR_IF_NULL(receivedEvent);
		PowerModeCapture capture;
		capture.receivedEvent = receivedEvent.get();
		PVOID registration{};
		RETURN_IF_FAILED(PowerRegisterForEffectivePowerModeNotifications(
			EFFECTIVE_POWER_MODE_V1,
			CaptureEffectivePowerMode,
			&capture,
			&registration
		));
		const DWORD waitResult = WaitForSingleObject(receivedEvent.get(), 1000);
		const HRESULT unregisterResult = PowerUnregisterFromEffectivePowerModeNotifications(registration);
		if (waitResult == WAIT_FAILED)
		{
			return HRESULT_FROM_WIN32(GetLastError());
		}
		if (waitResult == WAIT_TIMEOUT)
		{
			return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
		}
		RETURN_IF_FAILED(unregisterResult);
		diagnostics.effectivePowerMode = capture.mode.load(std::memory_order_acquire);
		const EFFECTIVE_POWER_MODE saverThreshold = GetWindowsBuildNumber() < Windows11_24H2Build
			? EffectivePowerModeBetterBattery
			: EffectivePowerModeBalanced;
		diagnostics.powerSaverActive = diagnostics.effectivePowerMode < saverThreshold;
		return S_OK;
	}
	catch (...)
	{
		return wil::ResultFromCaughtException();
	}

	HRESULT QueryDwmCrashDumpConfiguration(DwmCrashDumpConfiguration& configuration) noexcept
	{
		configuration = {};

		wil::unique_hkey key;
		const LSTATUS status = RegOpenKeyExW(
			HKEY_LOCAL_MACHINE,
			DwmCrashDumpKey,
			0,
			KEY_QUERY_VALUE | KEY_WOW64_64KEY,
			key.put()
		);
		if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
		{
			return S_OK;
		}
		RETURN_IF_WIN32_ERROR(status);

		configuration.enabled = true;
		RETURN_IF_FAILED(QueryDword(key.get(), L"DumpType", 1, configuration.dumpType));
		RETURN_IF_FAILED(QueryDword(key.get(), L"DumpCount", 10, configuration.dumpCount));
		RETURN_IF_FAILED(QueryString(key.get(), L"DumpFolder", configuration.dumpFolder));
		return S_OK;
	}

	HRESULT EnableDwmCrashDumps(std::wstring_view requestedFolder, std::wstring& configuredFolder) noexcept
	{
		std::filesystem::path resolvedFolder;
		RETURN_IF_FAILED(ResolveDumpFolder(requestedFolder, resolvedFolder));

		wil::unique_hkey key;
		DWORD disposition{};
		RETURN_IF_WIN32_ERROR(RegCreateKeyExW(
			HKEY_LOCAL_MACHINE,
			DwmCrashDumpKey,
			0,
			nullptr,
			REG_OPTION_NON_VOLATILE,
			KEY_SET_VALUE | KEY_WOW64_64KEY,
			nullptr,
			key.put(),
			&disposition
		));

		const std::wstring folder = resolvedFolder.wstring();
		const DWORD dumpType = 2;
		const DWORD dumpCount = 1;
		RETURN_IF_WIN32_ERROR(RegSetValueExW(
			key.get(),
			L"DumpFolder",
			0,
			REG_EXPAND_SZ,
			reinterpret_cast<const BYTE*>(folder.c_str()),
			static_cast<DWORD>((folder.size() + 1) * sizeof(WCHAR))
		));
		RETURN_IF_WIN32_ERROR(RegSetValueExW(
			key.get(),
			L"DumpCount",
			0,
			REG_DWORD,
			reinterpret_cast<const BYTE*>(&dumpCount),
			sizeof(dumpCount)
		));
		RETURN_IF_WIN32_ERROR(RegSetValueExW(
			key.get(),
			L"DumpType",
			0,
			REG_DWORD,
			reinterpret_cast<const BYTE*>(&dumpType),
			sizeof(dumpType)
		));

		configuredFolder = folder;
		return S_OK;
	}

	HRESULT DisableDwmCrashDumps() noexcept
	{
		const LSTATUS status = RegDeleteTreeW(HKEY_LOCAL_MACHINE, DwmCrashDumpKey);
		if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND)
		{
			return S_OK;
		}
		return ResultFromStatus(status);
	}
}
