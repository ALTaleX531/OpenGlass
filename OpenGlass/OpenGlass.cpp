#include "pch.h"
#include "resource.h"
#include "Utils.hpp"
#include "OpenGlass.hpp"
#include "HookHelper.hpp"
#include "SymbolParser.hpp"
#include "OSHelper.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "GeometryRecorder.hpp"
#include "CaptionTextHandler.hpp"
#include "GlassFramework.hpp"
#include "OcclusionCulling.hpp"
#include "ConfigurationFramework.hpp"

namespace OpenGlass
{
	DWORD WINAPI Initialize(PVOID);
	bool g_outOfLoaderLock{ false };

	LONG NTAPI TopLevelExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
	LRESULT CALLBACK DwmNotificationWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LPTOP_LEVEL_EXCEPTION_FILTER g_old{ nullptr };
	HWND g_notificationWindow{ nullptr };
	HPOWERNOTIFY g_powerNotify{ nullptr };
	WNDPROC g_oldWndProc{ nullptr };
	bool g_startup{ false };

	void OnSymbolDownloading(SymbolDownloaderStatus status, std::wstring_view text);
	bool OnSymbolParsing_uDwm(
		std::string_view functionName, 
		std::string_view fullyUnDecoratedFunctionName, 
		const HookHelper::OffsetStorage& offset, 
		const PSYMBOL_INFO originalSymInfo
	);
	bool OnSymbolParsing_Dwmcore(
		std::string_view functionName,
		std::string_view fullyUnDecoratedFunctionName,
		const HookHelper::OffsetStorage& offset,
		const PSYMBOL_INFO originalSymInfo
	);
}

LONG NTAPI OpenGlass::TopLevelExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
	LONG result{ g_old ? g_old(exceptionInfo) : EXCEPTION_CONTINUE_SEARCH };
	[exceptionInfo]()
	{
		CreateDirectoryW(Utils::make_current_folder_file_wstring(L"dumps").c_str(), nullptr);

		std::time_t tt{ std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) };
		tm _tm{};
		WCHAR time[MAX_PATH + 1];
		localtime_s(&_tm, &tt);
		std::wcsftime(time, MAX_PATH, L"%Y-%m-%d-%H-%M-%S", &_tm);

		wil::unique_hfile fileHandle
		{
			CreateFile2(
				Utils::make_current_folder_file_wstring(
					std::format(
						L"dumps\\minidump-{}.dmp",
						time
					)
				).c_str(),
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ,
				CREATE_ALWAYS,
				nullptr
			)
		};
		RETURN_LAST_ERROR_IF(!fileHandle.is_valid());

		MINIDUMP_EXCEPTION_INFORMATION minidumpExceptionInfo{ GetCurrentThreadId(), exceptionInfo, FALSE };
		RETURN_IF_WIN32_BOOL_FALSE(
			MiniDumpWriteDump(
				GetCurrentProcess(),
				GetCurrentProcessId(),
				fileHandle.get(),
				static_cast<MINIDUMP_TYPE>(
					MINIDUMP_TYPE::MiniDumpWithThreadInfo |
					MINIDUMP_TYPE::MiniDumpWithFullMemory |
					MINIDUMP_TYPE::MiniDumpWithUnloadedModules
					),
				&minidumpExceptionInfo,
				nullptr,
				nullptr
			)
		);

		return S_OK;
	} ();

	return result;
}

LRESULT CALLBACK OpenGlass::DwmNotificationWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_WININICHANGE:
		case WM_DWMCOLORIZATIONCOLORCHANGED: // accent color changed
		{
			ConfigurationFramework::Update(ConfigurationFramework::UpdateType::Backdrop);
			break;
		}
		case WM_THEMECHANGED: // theme switched, we can handle this to load our custom theme atlas
		{
			ConfigurationFramework::Update(ConfigurationFramework::UpdateType::Theme);
			break;
		}
		case WM_POWERBROADCAST: // entering or leaving battery mode?
		{
			if (wParam == PBT_POWERSETTINGCHANGE)
			{
				ConfigurationFramework::Update(ConfigurationFramework::UpdateType::Framework);
			}
			break;
		}
		case WM_WTSSESSION_CHANGE: // session changed, user has justed login in/off
		{
			if (wParam == WTS_SESSION_LOGON)
			{
				ConfigurationFramework::Load();
			}
			else if (wParam == WTS_SESSION_LOGOFF)
			{
				ConfigurationFramework::Unload();
			}
			break;
		}
		default:
			break;
	}

	return g_oldWndProc(hWnd, uMsg, wParam, lParam);
}

void OpenGlass::OnSymbolDownloading(SymbolDownloaderStatus status, std::wstring_view text)
{
	switch (status)
	{
		case SymbolDownloaderStatus::Start:
		{
			if (GetConsoleWindow())
			{
				if (AllocConsole())
				{
					FILE* fpstdin{ nullptr }, * fpstdout{ nullptr };
					_wfreopen_s(&fpstdin, L"CONIN$", L"r", stdin);
					_wfreopen_s(&fpstdout, L"CONOUT$", L"w+t", stdout);
					_wsetlocale(LC_ALL, L"");
				}
			}
			break;
		}
		case SymbolDownloaderStatus::Downloading:
		{
			if (GetConsoleWindow())
			{
				wprintf_s(text.data());
			}
			break;
		}
		case SymbolDownloaderStatus::OK:
		{
			break;
		}
	}
}
bool OpenGlass::OnSymbolParsing_uDwm(
	std::string_view functionName,
	std::string_view fullyUnDecoratedFunctionName,
	const HookHelper::OffsetStorage& offset,
	const PSYMBOL_INFO originalSymInfo
)
{
	uDwm::InitializeFromSymbol(fullyUnDecoratedFunctionName, offset);
	GlassFramework::InitializeFromSymbol(fullyUnDecoratedFunctionName, offset);
	CaptionTextHandler::InitializeFromSymbol(fullyUnDecoratedFunctionName, offset);
	GeometryRecorder::InitializeFromSymbol(functionName, fullyUnDecoratedFunctionName, offset);
	OcclusionCulling::InitializeFromSymbol(fullyUnDecoratedFunctionName, offset);

	return true;
}
bool OpenGlass::OnSymbolParsing_Dwmcore(
	std::string_view functionName,
	std::string_view fullyUnDecoratedFunctionName,
	const HookHelper::OffsetStorage& offset,
	const PSYMBOL_INFO originalSymInfo
)
{
	dwmcore::InitializeFromSymbol(functionName, fullyUnDecoratedFunctionName, offset);

	return true;
}

DWORD WINAPI OpenGlass::Initialize(PVOID)
{
	auto moduleReference{ wil::get_module_reference_for_thread() };
	auto coCleanUp{ wil::CoInitializeEx() };
	while (!g_outOfLoaderLock)
	{
		Sleep(50);
	}
	Sleep(100);

	SymbolParser parser{};
	HRESULT hr{ parser.Walk(L"uDwm.dll", OnSymbolDownloading, OnSymbolParsing_uDwm) };
	if (FAILED(hr))
	{
		// TO-DO: Replace ShellMessageBoxW
		ShellMessageBoxW(
			wil::GetModuleInstanceHandle(),
			nullptr,
			MAKEINTRESOURCEW(IDS_STRING103),
			MAKEINTRESOURCEW(IDS_STRING101),
			MB_ICONERROR,
			std::format(L"{:#x}", static_cast<ULONG>(hr)).c_str()
		);
		return hr;
	}
	hr = parser.Walk(L"dwmcore.dll", OnSymbolDownloading, OnSymbolParsing_Dwmcore);
	if (FAILED(hr))
	{
		// TO-DO: Replace ShellMessageBoxW
		ShellMessageBoxW(
			wil::GetModuleInstanceHandle(),
			nullptr,
			MAKEINTRESOURCEW(IDS_STRING103),
			MAKEINTRESOURCEW(IDS_STRING101),
			MB_ICONERROR,
			std::format(L"{:#x}", static_cast<ULONG>(hr)).c_str()
		);
		return hr;
	}

	if (GetConsoleWindow())
	{
		system("pause");
		fclose(stdin);
		fclose(stdout);
		FreeConsole();
	}

	ConfigurationFramework::Unload();
	GlassFramework::Startup();
	CaptionTextHandler::Startup();
	GeometryRecorder::Startup();
	OcclusionCulling::Startup();
	ConfigurationFramework::Load();

#ifdef _DEBUG
	winrt::com_ptr<IDCompositionDeviceDebug> debugDevice{ nullptr };
	uDwm::CDesktopManager::s_pDesktopManagerInstance->GetDCompDevice()->QueryInterface(
		debugDevice.put()
	);
	debugDevice->EnableDebugCounters();
#endif // _DEBUG

	// just wait patiently, the dwm notification window maybe is not ready...
	while (!(g_notificationWindow = FindWindowW(L"Dwm", nullptr))) { DwmFlush(); }
	g_oldWndProc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrW(
			g_notificationWindow,
			GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(DwmNotificationWndProc)
		)
		);
	ChangeWindowMessageFilterEx(g_notificationWindow, WM_DWMCOLORIZATIONCOLORCHANGED, MSGFLT_ALLOW, nullptr);
	ChangeWindowMessageFilterEx(g_notificationWindow, WM_THEMECHANGED, MSGFLT_ALLOW, nullptr);
	if (g_powerNotify = RegisterPowerSettingNotification(g_notificationWindow, &GUID_POWER_SAVING_STATUS, DEVICE_NOTIFY_WINDOW_HANDLE))
	{
		ChangeWindowMessageFilterEx(g_notificationWindow, WM_POWERBROADCAST, MSGFLT_ALLOW, nullptr);
	}
	if (WTSRegisterSessionNotification(g_notificationWindow, NOTIFY_FOR_THIS_SESSION))
	{
		ChangeWindowMessageFilterEx(g_notificationWindow, WM_WTSSESSION_CHANGE, MSGFLT_ALLOW, nullptr);
	}

	// refresh the whole desktop to apply our glass effect
	DWORD info{ BSM_APPLICATIONS };
	BroadcastSystemMessageW(BSF_IGNORECURRENTTASK | BSF_ALLOWSFW | BSF_FORCEIFHUNG, &info, WM_THEMECHANGED, 0, 0);
	InvalidateRect(nullptr, nullptr, FALSE);

	return S_OK;
}

void OpenGlass::Startup()
{
	if (g_notificationWindow)
	{
		return;
	}
	if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000))
	{
		return;
	}
	if (!GetModuleHandleW(L"dwm.exe"))
	{
		return;
	}
	if (os::IsOpenGlassUnsupported())
	{
		ShellMessageBoxW(
			wil::GetModuleInstanceHandle(),
			nullptr,
			MAKEINTRESOURCEW(IDS_STRING102),
			MAKEINTRESOURCEW(IDS_STRING101),
			MB_ICONERROR
		);
		return;
	}

	wil::SetResultLoggingCallback([](const wil::FailureInfo& failure) noexcept
	{
		OutputDebugStringW(failure.pszMessage);
	});
	g_old = SetUnhandledExceptionFilter(TopLevelExceptionFilter);
	#pragma warning (suppress : 26444)
	wil::unique_handle{ CreateThread(nullptr, 0, Initialize, nullptr, 0, nullptr) };
	g_outOfLoaderLock = true;

	return;
}

void OpenGlass::Shutdown()
{
	if (!g_notificationWindow)
	{
		return;
	}

	if (g_oldWndProc)
	{
		SetWindowLongPtrW(g_notificationWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_oldWndProc));
		g_oldWndProc = nullptr;
	}
	if (g_powerNotify)
	{
		UnregisterPowerSettingNotification(g_powerNotify);
		g_powerNotify = nullptr;
	}
	WTSUnRegisterSessionNotification(g_notificationWindow);
#ifdef _DEBUG
	winrt::com_ptr<IDCompositionDeviceDebug> debugDevice{ nullptr };
	uDwm::CDesktopManager::s_pDesktopManagerInstance->GetDCompDevice()->QueryInterface(
		debugDevice.put()
	);
	debugDevice->DisableDebugCounters();
#endif // _DEBUG

	OcclusionCulling::Shutdown();
	GeometryRecorder::Shutdown();
	CaptionTextHandler::Shutdown();
	GlassFramework::Shutdown();

	PostMessageW(g_notificationWindow, WM_THEMECHANGED, 0, 0);
	InvalidateRect(nullptr, nullptr, FALSE);
	g_notificationWindow = nullptr;
	g_outOfLoaderLock = false;

	if (g_old)
	{
		SetUnhandledExceptionFilter(g_old);
		g_old = nullptr;
	}
}