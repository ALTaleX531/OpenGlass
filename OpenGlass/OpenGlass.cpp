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
#include "ButtonGlowHandler.hpp"
#include "GlassFramework.hpp"
#include "GlassRenderer.hpp"
#include "CustomMsstyleLoader.hpp"
#include "ConfigurationFramework.hpp"

namespace OpenGlass
{
	DWORD WINAPI Initialize(PVOID);
	bool g_outOfLoaderLock{ false };

	LONG NTAPI TopLevelExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
	LPTOP_LEVEL_EXCEPTION_FILTER g_old{ nullptr };

	LRESULT CALLBACK DwmNotificationWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	HWND g_notificationWindow{ nullptr };
	HPOWERNOTIFY g_powerNotify{ nullptr };
	WNDPROC g_oldWndProc{ nullptr };

	// for symbol downloading use
	void OnSymbolDownloading(SymbolDownloaderStatus status, std::wstring_view text);
	bool g_symbolRequiresDownloading{ false };
	bool g_symbolDownloadCompleted{ false };
	ULONGLONG g_previouslyCompletedProgress{ 0 };
	class CDownloadingProgressIndicator
	{
		bool m_tabAdded{ false };
		bool m_windowShowed{ false };
		bool m_titleChanged{ false };
		HWND m_hwnd{ nullptr };
		ULONGLONG m_total{ 100 };
		wil::com_ptr_nothrow<ITaskbarList4> m_taskbar{ nullptr };
		WCHAR m_originalTitle[MAX_PATH + 1]{};

		void EnureTaskbarIndicatorInitialized()
		{
			if (!m_windowShowed)
			{
				ShowWindowAsync(m_hwnd, SW_SHOWNOACTIVATE);
				BOOL value{ TRUE };
				DwmSetWindowAttribute(m_hwnd, DWMWA_DISALLOW_PEEK, &value, sizeof(value));
				DwmSetWindowAttribute(m_hwnd, DWMWA_EXCLUDED_FROM_PEEK, &value, sizeof(value));
				DwmSetWindowAttribute(m_hwnd, DWMWA_CLOAK, &value, sizeof(value));
				wil::unique_hicon icon{ LoadIconW(GetModuleHandleW(L"shell32.dll"), MAKEINTRESOURCEW(18)) };
				SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon.get()));
				SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon.get()));
				m_windowShowed = true;
			}
			if (!m_taskbar)
			{
				m_taskbar = wil::CoCreateInstanceNoThrow<ITaskbarList4>(CLSID_TaskbarList);
				m_taskbar->HrInit();
			}
			if (!m_tabAdded)
			{
				m_taskbar->AddTab(m_hwnd);
				m_tabAdded = true;
			}
		}
	public:
		CDownloadingProgressIndicator()
		{
			// just wait patiently, the dwm notification window maybe is not ready...
			while (!(m_hwnd = FindWindowW(L"Dwm", nullptr))) { DwmFlush(); }
			InternalGetWindowText(m_hwnd, m_originalTitle, MAX_PATH);
		}
		~CDownloadingProgressIndicator()
		{
			if (m_windowShowed)
			{
				BOOL value{ FALSE };
				SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, 0);
				SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, 0);
				DwmSetWindowAttribute(m_hwnd, DWMWA_CLOAK, &value, sizeof(value));
				DwmSetWindowAttribute(m_hwnd, DWMWA_DISALLOW_PEEK, &value, sizeof(value));
				DwmSetWindowAttribute(m_hwnd, DWMWA_EXCLUDED_FROM_PEEK, &value, sizeof(value));
				ShowWindowAsync(m_hwnd, SW_HIDE);
				m_windowShowed = false;
			}
			if (m_tabAdded)
			{
				m_taskbar->SetProgressState(m_hwnd, TBPF_NOPROGRESS);
				m_taskbar->DeleteTab(m_hwnd);
				m_tabAdded = false;
			}
			if (m_titleChanged)
			{
				SetWindowTextW(m_hwnd, m_originalTitle);
				m_titleChanged = false;
			}
		}
		void SetProgressState(TBPFLAG flags, bool ensureInitialized = true)
		{	
			if (ensureInitialized)
			{
				EnureTaskbarIndicatorInitialized();
			}
			if (m_tabAdded)
			{
				m_taskbar->SetProgressState(m_hwnd, flags);
			}
		}
		void SetProgressTotalValue(ULONGLONG total, bool ensureInitialized = true)
		{
			if (ensureInitialized)
			{
				EnureTaskbarIndicatorInitialized();
			}
			m_total = total;
		}
		void SetProgressValue(ULONGLONG completed, bool ensureInitialized = true)
		{
			if (ensureInitialized)
			{
				EnureTaskbarIndicatorInitialized();
			}
			if (m_tabAdded)
			{
				m_taskbar->SetProgressValue(m_hwnd, completed, m_total);
			}
		}
		void SetProgressTitle(std::wstring_view titleView, bool ensureInitialized = true)
		{
			if (ensureInitialized)
			{
				EnureTaskbarIndicatorInitialized();
			}
			if (m_windowShowed)
			{
				m_titleChanged = true;
				SetWindowTextW(m_hwnd, titleView.data());
			}
		}
		HWND GetHwnd() const
		{
			return m_hwnd;
		}
	};
	CDownloadingProgressIndicator* g_indicator{ nullptr };

	bool g_startup{ false };
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
					std::wstring{L"dumps\\minidump-"} + time + L".dmp"
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
		case WM_WTSSESSION_CHANGE: // session changed, user has just logged in/off
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
			g_symbolRequiresDownloading = true;
			g_indicator->SetProgressValue(g_previouslyCompletedProgress);
			g_indicator->SetProgressState(TBPF_NORMAL | TBPF_INDETERMINATE);
			break;
		}
		case SymbolDownloaderStatus::Downloading:
		{
			auto percentIndex = text.find(L" percent");
			if (percentIndex != text.npos)
			{
				std::wstring_view progressView{ &text[percentIndex] - 3, 4 };
				OutputDebugStringW(std::wstring(progressView).c_str());
				g_indicator->SetProgressValue(g_previouslyCompletedProgress + _wtoll(std::wstring(progressView).c_str()));
				g_indicator->SetProgressState(TBPF_NORMAL);
				g_indicator->SetProgressTitle(Utils::GetResWString<IDS_STRING114>());
			}
			else
			{
				g_indicator->SetProgressState(TBPF_NORMAL | TBPF_INDETERMINATE);
			}

			OutputDebugStringW(text.data());
			break;
		}
		case SymbolDownloaderStatus::OK:
		{
			g_indicator->SetProgressValue(g_previouslyCompletedProgress + 100);
			g_indicator->SetProgressState(TBPF_NORMAL);
			g_symbolDownloadCompleted = true;

			OutputDebugStringW(text.data());
			break;
		}
	}
}

DWORD WINAPI OpenGlass::Initialize(PVOID)
{
	auto moduleReference = wil::get_module_reference_for_thread();
	auto coCleanUp = wil::CoInitializeEx();

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	RETURN_IF_FAILED(SetThreadDescription(GetCurrentThread(), L"OpenGlass Initialization Thread"));

	while (!g_outOfLoaderLock)
	{
		Sleep(50);
	}
	Sleep(100);

	if (os::IsOpenGlassUnsupported())
	{
		auto result = 0;
		LOG_IF_FAILED(
			TaskDialog(
				nullptr,
				nullptr,
				nullptr,
				Utils::GetResWString<IDS_STRING101>().c_str(),
				Utils::GetResWString<IDS_STRING102>().c_str(),
				TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
				TD_WARNING_ICON,
				&result
			)
		);
		if (result == IDNO)
		{
			return static_cast<DWORD>(E_ABORT);
		}
	}

	{
		auto indicator = std::make_unique<CDownloadingProgressIndicator>();
		indicator->SetProgressTotalValue(200, false);
		g_indicator = indicator.get();

		std::wstring expandText{ Utils::GetResWString<IDS_STRING108>() };
		std::wstring collapseText{ Utils::GetResWString<IDS_STRING109>() };
		int result{ 0 };
		TASKDIALOGCONFIG config{ sizeof(TASKDIALOGCONFIG), nullptr, nullptr, TDF_SIZE_TO_CONTENT, TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON, nullptr, {.pszMainIcon{TD_ERROR_ICON}}, nullptr, nullptr, 0, nullptr, IDRETRY, 0, nullptr, 0, nullptr, nullptr, collapseText.c_str(), expandText.c_str(), {}, nullptr, nullptr, 0, 0 };

		SymbolParser parser{};
		HRESULT hr{ S_OK };

do_udwm_symbol_parsing:
		g_previouslyCompletedProgress = 0;
		g_symbolDownloadCompleted = false;
		g_symbolRequiresDownloading = false;
		hr = parser.Walk(L"uDwm.dll", OnSymbolDownloading, uDwm::OnSymbolParsing);
		if (FAILED(hr))
		{
			std::wstring mainInstruction{ !g_symbolRequiresDownloading || g_symbolDownloadCompleted ? Utils::GetResWStringView<IDS_STRING103>() : Utils::GetResWStringView<IDS_STRING110>() };
			std::wstring content{ Utils::to_error_wstring(hr) + L"\n\n" + (!g_symbolRequiresDownloading || g_symbolDownloadCompleted ? Utils::GetResWStringView<IDS_STRING107>() : Utils::GetResWStringView<IDS_STRING114>()) };
			WCHAR errorText[MAX_PATH + 1]{};
			swprintf_s(
				errorText, 
				L"hr: 0x%lx (uDwm.dll)\n%s", 
				hr,
				Utils::GetResWString<IDS_STRING112>().c_str()
			);

			indicator->SetProgressTitle(Utils::GetResWString<IDS_STRING115>());
			indicator->SetProgressState(TBPF_INDETERMINATE);
			config.pszMainInstruction = mainInstruction.c_str();
			config.pszContent = content.c_str();
			config.pszExpandedInformation = errorText;
			LOG_IF_FAILED(
				TaskDialogIndirect(
					&config,
					&result,
					nullptr,
					nullptr
				)
			);
			if (result == IDCANCEL)
			{
				return static_cast<DWORD>(hr);
			}
			else
			{
				goto do_udwm_symbol_parsing;
			}
		}

do_dwmcore_symbol_parsing:
		g_previouslyCompletedProgress = 100;
		g_symbolDownloadCompleted = false;
		g_symbolRequiresDownloading = false;
		hr = parser.Walk(L"dwmcore.dll", OnSymbolDownloading, dwmcore::OnSymbolParsing);
		if (FAILED(hr))
		{
			std::wstring mainInstruction{ !g_symbolRequiresDownloading || g_symbolDownloadCompleted ? Utils::GetResWStringView<IDS_STRING103>() : Utils::GetResWStringView<IDS_STRING110>() };
			std::wstring content{ Utils::to_error_wstring(hr) + L"\n\n" + (!g_symbolRequiresDownloading || g_symbolDownloadCompleted ? Utils::GetResWStringView<IDS_STRING107>() : Utils::GetResWStringView<IDS_STRING114>()) };
			WCHAR errorText[MAX_PATH + 1]{};
			swprintf_s(
				errorText,
				L"hr: 0x%lx (dwmcore.dll)\n%s",
				hr,
				Utils::GetResWString<IDS_STRING112>().c_str()
			);

			indicator->SetProgressTitle(Utils::GetResWString<IDS_STRING115>());
			indicator->SetProgressState(TBPF_INDETERMINATE);
			config.pszMainInstruction = mainInstruction.c_str();
			config.pszContent = content.c_str();
			config.pszExpandedInformation = errorText;
			LOG_IF_FAILED(
				TaskDialogIndirect(
					&config,
					&result,
					nullptr,
					nullptr
				)
			);
			if (result == IDCANCEL)
			{
				return static_cast<DWORD>(hr);
			}
			else
			{
				goto do_dwmcore_symbol_parsing;
			}
		}

		g_previouslyCompletedProgress = 200;
		g_notificationWindow = indicator->GetHwnd();
		g_indicator = nullptr;
	}
	
	ConfigurationFramework::Unload();
	GlassFramework::Startup();
	GlassRenderer::Startup();
	CaptionTextHandler::Startup();
	ButtonGlowHandler::Startup();
	GeometryRecorder::Startup();
	CustomMsstyleLoader::Startup();
	ConfigurationFramework::Load();

#ifdef _DEBUG
	winrt::com_ptr<IDCompositionDeviceDebug> debugDevice{ nullptr };
	uDwm::CDesktopManager::s_pDesktopManagerInstance->GetDCompDevice()->QueryInterface(
		debugDevice.put()
	);
	debugDevice->EnableDebugCounters();
#endif // _DEBUG

	g_oldWndProc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrW(
			g_notificationWindow,
			GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(DwmNotificationWndProc)
		)
	);
	// make sure our third-party ui creators can send message to dwm
	ChangeWindowMessageFilterEx(g_notificationWindow, WM_DWMCOLORIZATIONCOLORCHANGED, MSGFLT_ALLOW, nullptr);
	ChangeWindowMessageFilterEx(g_notificationWindow, WM_THEMECHANGED, MSGFLT_ALLOW, nullptr);
	if (g_powerNotify = RegisterPowerSettingNotification(g_notificationWindow, &GUID_POWER_SAVING_STATUS, DEVICE_NOTIFY_WINDOW_HANDLE); g_powerNotify)
	{
		ChangeWindowMessageFilterEx(g_notificationWindow, WM_POWERBROADCAST, MSGFLT_ALLOW, nullptr);
	}
	if (WTSRegisterSessionNotification(g_notificationWindow, NOTIFY_FOR_THIS_SESSION))
	{
		ChangeWindowMessageFilterEx(g_notificationWindow, WM_WTSSESSION_CHANGE, MSGFLT_ALLOW, nullptr);
	}

	// refresh the whole desktop to apply our glass effect
	SendMessageW(g_notificationWindow, WM_THEMECHANGED, 0, 0);
	InvalidateRect(nullptr, nullptr, FALSE);

	return static_cast<DWORD>(S_OK);
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

	wil::SetResultLoggingCallback([](const wil::FailureInfo& failure) noexcept
	{
		OutputDebugStringW(failure.pszMessage);
	});
	g_old = SetUnhandledExceptionFilter(TopLevelExceptionFilter);
	// the injection thread cannot show TaskDialog but MessageBox, i don't known why...
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

	GeometryRecorder::Shutdown();
	CaptionTextHandler::Shutdown();
	ButtonGlowHandler::Shutdown();
	GlassRenderer::Shutdown();
	GlassFramework::Shutdown();
	CustomMsstyleLoader::Shutdown();

	SendMessageW(g_notificationWindow, WM_THEMECHANGED, 0, 0);
	InvalidateRect(nullptr, nullptr, FALSE);
	g_notificationWindow = nullptr;
	g_outOfLoaderLock = false;

	if (g_old)
	{
		SetUnhandledExceptionFilter(g_old);
		g_old = nullptr;
	}
}