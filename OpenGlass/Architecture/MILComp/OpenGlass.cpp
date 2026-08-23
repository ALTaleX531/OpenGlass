#include "pch.h"
#include "ApplicationPaths.hpp"
#include "resource.h"
#include "Util.hpp"
#include "Shared.hpp"
#include "OpenGlass.hpp"
#include "HookHelper.hpp"
#include "SymbolParser.hpp"
#include "SymbolDownloader.hpp"
#include "SymbolCatalog.hpp"
#include "OSHelper.hpp"
#include "uDWMProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "GlassEngine.hpp"

namespace OpenGlass
{
	void Startup();
	void Shutdown();
	enum class LifecycleState
	{
		Inert,
		Preparing,
		HooksActive,
		Active,
		Stopping
	};
	std::atomic<LifecycleState> g_lifecycleState{ LifecycleState::Inert };

	_Function_class_(EFFECTIVE_POWER_MODE_CALLBACK)
	VOID CALLBACK EffectivePowerModeCallback(
		EFFECTIVE_POWER_MODE mode,
		PVOID context
	);
	LRESULT CALLBACK DwmNotificationWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	PVOID g_powerNotify{ nullptr };
	WNDPROC g_oldWndProc{ nullptr };

	DWORD SymbolLoaderUIThreadEntryPoint(PVOID);
	bool g_asyncTaskDialogCreated{ false };
	HWND g_symbolDownloaderHwnd{ nullptr };
	bool g_progressIndetermine{ false };
	ULONGLONG g_progressCompleted{ 0 };
	ULONGLONG g_progressPreviouslyCompleted{ 0 };
	const ULONGLONG g_progressTotal{ 200 };
	enum class DownloaderStatus : UCHAR
	{
		None,
		Indeterminate,
		Normal,
		Error,
		Paused,
	};
	auto g_status = DownloaderStatus::None;
	std::wstring g_currentlyDownloadingSymbol{};
	CDownloadContext* g_downloaderContext{};

	bool InitializeProjectionBySymbols();
	void SymbolDownloaderCallback(const CDownloadProgress& progress);
	bool g_symbolIsDownloading{ false };
	bool g_symbolRequiresDownloading{ false };
	bool g_symbolDownloadCompleted{ false };
	std::wstring g_detailsInfo{};
}

_Function_class_(EFFECTIVE_POWER_MODE_CALLBACK)
VOID CALLBACK OpenGlass::EffectivePowerModeCallback(
	EFFECTIVE_POWER_MODE mode,
	[[maybe_unused]] PVOID context
)
{
	if (g_lifecycleState.load(std::memory_order_acquire) != LifecycleState::Active)
	{
		return;
	}
	if (mode < (os::buildNumber < os::build_w11_24h2 ? EffectivePowerModeBetterBattery : EffectivePowerModeBalanced))
	{
		Shared::g_xxSaver = true;
	}
	else
	{
		Shared::g_xxSaver = false;
	}
	GlassEngine::Update(GlassEngine::UpdateType::Framework);
}

LRESULT CALLBACK OpenGlass::DwmNotificationWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (g_lifecycleState.load(std::memory_order_acquire) != LifecycleState::Active)
	{
		return g_oldWndProc(hWnd, uMsg, wParam, lParam);
	}
	switch (uMsg)
	{
		case WM_CLOSE:
		{
			GlassEngine::SetDwmNotificationWindow(nullptr);
			break;
		}
		case WM_WININICHANGE:
		case WM_DWMCOLORIZATIONCOLORCHANGED: // accent color changed
		{
			Util::ClearMessageQueue(hWnd, uMsg);
			GlassEngine::Update(GlassEngine::UpdateType::Backdrop);
			break;
		}
		case WM_THEMECHANGED: // theme switched, we can handle this to load our custom theme atlas
		{
			Util::ClearMessageQueue(hWnd, uMsg);
			GlassEngine::Update(GlassEngine::UpdateType::Theme);
			break;
		}
		case WM_WTSSESSION_CHANGE: // session changed, user has just logged in/off
		{
			if (wParam == WTS_SESSION_LOGON)
			{
				GlassEngine::LoadRegistry();
			}
			else if (wParam == WTS_SESSION_LOGOFF)
			{
				GlassEngine::UnloadRegistry();
			}
			break;
		}
		default:
			break;
	}

	return g_oldWndProc(hWnd, uMsg, wParam, lParam);
}

DWORD OpenGlass::SymbolLoaderUIThreadEntryPoint(PVOID)
{
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	THROW_IF_FAILED(SetThreadDescription(GetCurrentThread(), L"OpenGlass Symbol Loader UI Thread"));

	THROW_IF_FAILED(RoInitialize(RO_INIT_MULTITHREADED));
	wil::unique_rouninitialize_call wrtScope{};

	INITCOMMONCONTROLSEX icex{ .dwSize{sizeof(icex)}, .dwICC{ICC_PROGRESS_CLASS} };
	THROW_IF_WIN32_BOOL_FALSE(InitCommonControlsEx(&icex));

	winrt::com_ptr<ITaskbarList4> m_taskbarList{ nullptr };

	THROW_IF_FAILED(
		CoCreateInstance(
			CLSID_TaskbarList,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(m_taskbarList.put())
		)
	);

	wil::unique_hicon icon{ LoadIconW(GetModuleHandleW(L"shell32.dll"), MAKEINTRESOURCEW(18)) };
	auto callback = [](HWND hwnd, UINT uNotification, [[maybe_unused]] WPARAM wParam, [[maybe_unused]] LPARAM lParam, LONG_PTR lpRefData) static -> HRESULT
	{
		const auto taskbarList = reinterpret_cast<ITaskbarList4*>(lpRefData);
		switch (uNotification)
		{
		case TDN_CREATED:
		{
			const auto instruction = Util::GetResourceStringView<IDS_STRING111>();
			SendMessageW(hwnd, TDM_SET_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION, reinterpret_cast<LPARAM>(instruction.data()));

			BOOL value{ TRUE };
			THROW_IF_FAILED(DwmSetWindowAttribute(hwnd, DWMWA_EXCLUDED_FROM_PEEK, &value, sizeof(value)));

			g_symbolDownloaderHwnd = hwnd;
			break;
		}
		case TDN_DESTROYED:
		{
			g_symbolDownloaderHwnd = nullptr;
			break;
		}
		case TDN_TIMER:
		{
			static auto s_prevStatus = DownloaderStatus::None;
			if (s_prevStatus != g_status)
			{
				static bool s_marquee{ false };
				if (const auto marquee{ g_status == DownloaderStatus::Indeterminate }; s_marquee != marquee)
				{
					s_marquee = marquee;
					SendMessageW(hwnd, TDM_SET_MARQUEE_PROGRESS_BAR, static_cast<WPARAM>(marquee), 0);
					SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_MARQUEE, static_cast<WPARAM>(marquee), 0);
				}

				s_prevStatus = g_status;
				switch (g_status)
				{
				case DownloaderStatus::None:
				{
					THROW_IF_FAILED(taskbarList->SetProgressState(hwnd, TBPF_NOPROGRESS));
					SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_STATE, PBST_NORMAL, 0);
					break;
				}
				case DownloaderStatus::Indeterminate:
				{
					THROW_IF_FAILED(taskbarList->SetProgressState(hwnd, TBPF_INDETERMINATE));
					break;
				}
				case DownloaderStatus::Normal:
				{
					THROW_IF_FAILED(taskbarList->SetProgressState(hwnd, TBPF_NORMAL));
					SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_STATE, PBST_NORMAL, 0);
					break;
				}
				case DownloaderStatus::Error:
				{
					THROW_IF_FAILED(taskbarList->SetProgressState(hwnd, TBPF_ERROR));
					SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_STATE, PBST_ERROR, 0);
					break;
				}
				case DownloaderStatus::Paused:
				{
					THROW_IF_FAILED(taskbarList->SetProgressState(hwnd, TBPF_PAUSED));
					SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_STATE, PBST_PAUSED, 0);
					break;
				}
				default:
					break;
				}
			}
			const auto actualProgress = g_progressCompleted < 100 ? g_progressCompleted : g_progressCompleted - 100;
			if (g_status != DownloaderStatus::Indeterminate)
			{
				THROW_IF_FAILED(taskbarList->SetProgressValue(hwnd, g_progressCompleted, g_progressTotal));
				SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_POS, actualProgress, 0);
			}
			auto instruction = Util::GetResourceString<IDS_STRING111>();
			instruction.pop_back();
			instruction += (g_progressPreviouslyCompleted < 100 ? L" (1/2)" : L" (2/2)");
			SendMessageW(hwnd, TDM_SET_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION, reinterpret_cast<LPARAM>(instruction.c_str()));

			const auto content = g_currentlyDownloadingSymbol + (g_status == DownloaderStatus::Indeterminate ? Util::GetResourceString<IDS_STRING112>() : (g_progressIndetermine ? L"" : (L"(" + std::to_wstring(actualProgress)) + L"%)"));
			SendMessageW(hwnd, TDM_SET_ELEMENT_TEXT, TDE_CONTENT, reinterpret_cast<LPARAM>(content.c_str()));

			break;
		}
		}
		return S_OK;
	};
	TASKDIALOGCONFIG config
	{
		sizeof(TASKDIALOGCONFIG),
		nullptr,
		nullptr,
		TDF_SIZE_TO_CONTENT | TDF_SHOW_PROGRESS_BAR | TDF_CALLBACK_TIMER | TDF_ALLOW_DIALOG_CANCELLATION | TDF_CAN_BE_MINIMIZED | TDF_USE_HICON_MAIN,
		TDCBF_CLOSE_BUTTON,
		nullptr,
		{.hMainIcon{icon.get()}},
		nullptr,
		nullptr,
		0,
		nullptr,
		0,
		0,
		nullptr,
		0,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		{},
		nullptr,
		callback,
		reinterpret_cast<LONG_PTR>(m_taskbarList.get()),
		0
	};

	int button{};
	THROW_IF_FAILED(
		TaskDialogIndirect(
			&config,
			&button,
			nullptr,
			nullptr
		)
	);

	return S_OK;
}

void OpenGlass::SymbolDownloaderCallback(const CDownloadProgress& progress)
{
	g_currentlyDownloadingSymbol = {};
	g_currentlyDownloadingSymbol.append(g_downloaderContext->pdbFileName);
	g_currentlyDownloadingSymbol.append(L" - ");

	if (progress.downloadedBytes)
	{
		g_currentlyDownloadingSymbol += std::to_wstring(progress.downloadedBytes);
		g_currentlyDownloadingSymbol.append(L" bytes ");
		g_status = DownloaderStatus::Normal;

		if (progress.percent >= 0.0)
		{
			g_progressIndetermine = false;
			g_progressCompleted = g_progressPreviouslyCompleted + static_cast<int>(progress.percent);
		}
		else
		{
			g_progressIndetermine = true;
		}
	}
}

bool OpenGlass::InitializeProjectionBySymbols()
{
	const auto mainInstruction = Util::GetResourceStringView<IDS_STRING101>();
	const auto expandText = Util::GetResourceStringView<IDS_STRING103>();
	const auto collapseText = Util::GetResourceStringView<IDS_STRING104>();
	int result{ 0 };
	TASKDIALOGCONFIG config
	{
		sizeof(TASKDIALOGCONFIG),
		nullptr,
		nullptr,
		TDF_SIZE_TO_CONTENT | TDF_EXPAND_FOOTER_AREA | TDF_ALLOW_DIALOG_CANCELLATION,
		TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON,
		nullptr,
		{.pszMainIcon{TD_ERROR_ICON}},
		mainInstruction.data(),
		nullptr,
		0,
		nullptr,
		0,
		0,
		nullptr,
		0,
		nullptr,
		nullptr,
		collapseText.data(),
		expandText.data(),
		{},
		nullptr,
		nullptr,
		0,
		0
	};

	{
		const auto asyncUICleanup = wil::scope_exit([]
		{
			if (g_symbolDownloaderHwnd)
			{
				SendMessageW(g_symbolDownloaderHwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
			}
		});

		const auto udwmModuleHandle = GetModuleHandleW(L"uDWM.dll");
		const auto dwmcoreModuleHandle = GetModuleHandleW(L"dwmcore.dll");
		if (
			!uDWM::g_registry.Freeze(
				{ uDWM::g_versionInfo.build, uDWM::g_versionInfo.revision }
			) ||
			!dwmcore::g_registry.Freeze(
				{ dwmcore::g_versionInfo.build, dwmcore::g_versionInfo.revision }
			)
		)
		{
			return false;
		}
		std::filesystem::path symbolPath;
		std::unique_ptr<CSymbolParser> parser;
		std::unique_ptr<CSymbolDownloader> downloader;
		const auto ensureSymbolParser = [&]()
		{
			if (parser)
			{
				return;
			}
			// The installer grants Window Manager (S-1-5-90-0) write permissions to this shared cache.
			symbolPath = ApplicationPaths::GetProgramDataSubdirectory(L"symbols");
			if (!PathFileExistsW(symbolPath.c_str()))
			{
				wil::CreateDirectoryDeep(symbolPath.c_str());
			}
			parser = std::make_unique<CSymbolParser>(symbolPath.c_str());
		};
		const auto ensureDownloader = [&]()
		{
			if (!downloader)
			{
				downloader = std::make_unique<CSymbolDownloader>();
			}
		};
		const auto symbolServerBase = L"https://msdl.microsoft.com/download/symbols/";
		HRESULT hr{ S_OK };

		const auto showSymbolDownloaderUI = []()
		{
			g_symbolIsDownloading = true;
			g_symbolRequiresDownloading = true;
			g_progressCompleted = g_progressPreviouslyCompleted;
			g_status = DownloaderStatus::Indeterminate;

			if (!g_asyncTaskDialogCreated)
			{
				g_asyncTaskDialogCreated = true;
				[[maybe_unused]] wil::unique_handle uiThread
				{
					CreateThread(
						nullptr,
						0,
						SymbolLoaderUIThreadEntryPoint,
						nullptr,
						0,
						nullptr
					)
				};
			}
		};

		for(;;)
		{
			g_progressPreviouslyCompleted = 0;
			g_symbolDownloadCompleted = false;
			g_symbolRequiresDownloading = false;
			g_detailsInfo.clear();
			const auto udwmCatalogResult = Projection::CollectBuiltInSymbols(
				udwmModuleHandle,
				Projection::ModuleId::uDWM,
				uDWM::g_registry
			);
			LOG_HR_IF_MSG(
				HRESULT_FROM_WIN32(ERROR_INVALID_DATA),
				udwmCatalogResult == Projection::SymbolCatalogResult::Rejected,
				"Rejected built-in uDWM symbol catalog record; falling back to PDB resolution"
			);
			if (udwmCatalogResult == Projection::SymbolCatalogResult::Collected)
			{
				hr = S_OK;
			}
			else
			{
				ensureSymbolParser();
				hr = parser->ParsePdb(udwmModuleHandle, uDWM::g_registry);
			}
			if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
			{
				showSymbolDownloaderUI();

				ensureDownloader();
				hr = DownloadSymbolForModuleAsync(
					*downloader,
					udwmModuleHandle,
					symbolServerBase,
					symbolPath.c_str(),
					&g_downloaderContext,
					SymbolDownloaderCallback
				).get();
				g_symbolIsDownloading = false;
				g_status = DownloaderStatus::Indeterminate;

				if (SUCCEEDED(hr))
				{
					g_progressCompleted = g_progressPreviouslyCompleted + 100;
					g_currentlyDownloadingSymbol = {};
					g_symbolDownloadCompleted = true;
					hr = parser->ParsePdb(udwmModuleHandle, uDWM::g_registry);
				}
			}
			if (FAILED(hr))
			{
				const auto contentTemplate = g_symbolRequiresDownloading && !g_symbolDownloadCompleted ? Util::GetResourceStringView<IDS_STRING106>() : Util::GetResourceStringView<IDS_STRING107>();
				const auto buffSize = contentTemplate.size() + 8ull;
				const auto content = std::make_unique_for_overwrite<WCHAR[]>(buffSize);
				swprintf_s(
					content.get(),
					buffSize,
					contentTemplate.data(),
					hr
				);

				g_status = DownloaderStatus::Error;
				config.hwndParent = g_symbolDownloaderHwnd;
				config.pszContent = content.get();
				config.pszExpandedInformation = g_detailsInfo.c_str();
				THROW_IF_FAILED(
					TaskDialogIndirect(
						&config,
						&result,
						nullptr,
						nullptr
					)
				);
				if (result != IDRETRY)
				{
					return false;
				}
			}
			else
			{
				break;
			}
		}

		for (;;)
		{
			g_progressPreviouslyCompleted = 100;
			g_symbolDownloadCompleted = false;
			g_symbolRequiresDownloading = false;
			g_detailsInfo.clear();
			const auto dwmcoreCatalogResult = Projection::CollectBuiltInSymbols(
				dwmcoreModuleHandle,
				Projection::ModuleId::DwmCore,
				dwmcore::g_registry
			);
			LOG_HR_IF_MSG(
				HRESULT_FROM_WIN32(ERROR_INVALID_DATA),
				dwmcoreCatalogResult == Projection::SymbolCatalogResult::Rejected,
				"Rejected built-in dwmcore symbol catalog record; falling back to PDB resolution"
			);
			if (dwmcoreCatalogResult == Projection::SymbolCatalogResult::Collected)
			{
				hr = S_OK;
			}
			else
			{
				ensureSymbolParser();
				hr = parser->ParsePdb(dwmcoreModuleHandle, dwmcore::g_registry);
			}
			if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
			{
				showSymbolDownloaderUI();

				ensureDownloader();
				hr = DownloadSymbolForModuleAsync(
					*downloader,
					dwmcoreModuleHandle,
					symbolServerBase,
					symbolPath.c_str(),
					&g_downloaderContext,
					SymbolDownloaderCallback
				).get();
				g_symbolIsDownloading = false;
				g_status = DownloaderStatus::Indeterminate;

				if (SUCCEEDED(hr))
				{
					g_progressCompleted = g_progressPreviouslyCompleted + 100;
					g_currentlyDownloadingSymbol = {};
					g_symbolDownloadCompleted = true;
					hr = parser->ParsePdb(dwmcoreModuleHandle, dwmcore::g_registry);
				}
			}
			if (FAILED(hr))
			{
				const auto contentTemplate = g_symbolRequiresDownloading && !g_symbolDownloadCompleted ? Util::GetResourceStringView<IDS_STRING106>() : Util::GetResourceStringView<IDS_STRING107>();
				const auto buffSize = contentTemplate.size() + 8ull;
				const auto content = std::make_unique_for_overwrite<WCHAR[]>(buffSize);
				swprintf_s(
					content.get(),
					buffSize,
					contentTemplate.data(),
					hr
				);

				g_status = DownloaderStatus::Error;
				config.hwndParent = g_symbolDownloaderHwnd;
				config.pszContent = content.get();
				config.pszExpandedInformation = g_detailsInfo.c_str();
				THROW_IF_FAILED(
					TaskDialogIndirect(
						&config,
						&result,
						nullptr,
						nullptr
					)
				);
				if (result != IDRETRY)
				{
					return false;
				}
			}
			else
			{
				break;
			}
		}
	}

	std::string missingFunctionsOrVariables{};
	uDWM::g_registry.ReportUnresolved(missingFunctionsOrVariables, "uDWM.dll!");
	dwmcore::g_registry.ReportUnresolved(missingFunctionsOrVariables, "dwmcore.dll!");
	const auto projectionsReady = Projection::CommitModules(uDWM::g_registry, dwmcore::g_registry);
	if (!projectionsReady || !missingFunctionsOrVariables.empty())
	{
		if (missingFunctionsOrVariables.empty())
		{
			missingFunctionsOrVariables = "Projection metadata validation failed.\n";
		}
		missingFunctionsOrVariables.pop_back();
		missingFunctionsOrVariables.append(std::string_view{ "\0", 1 });
		const auto content = Util::GetResourceStringView<IDS_STRING108>();

		std::unique_ptr<WCHAR[]> convertedMissingInfo{};
		THROW_IF_FAILED(Util::MB2WC(convertedMissingInfo, missingFunctionsOrVariables.c_str(), static_cast<int>(missingFunctionsOrVariables.size())));

		config.hwndParent = nullptr;
		config.pszMainInstruction = mainInstruction.data();
		config.pszContent = content.data();
		config.pszExpandedInformation = convertedMissingInfo.get();
		config.dwCommonButtons = TDCBF_CLOSE_BUTTON;
		THROW_IF_FAILED(
			TaskDialogIndirect(
				&config,
				&result,
				nullptr,
				nullptr
			)
		);

		return false;
	}

	return true;
}

DWORD WINAPI OpenGlass::InitializationThreadEntryPoint(PVOID)
{
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	THROW_IF_FAILED(SetThreadDescription(GetCurrentThread(), L"OpenGlass Initialization Thread"));

	Startup();

	return S_OK;
}

DWORD WINAPI OpenGlass::UnInitializationThreadEntryPoint(PVOID)
{
	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	THROW_IF_FAILED(SetThreadDescription(GetCurrentThread(), L"OpenGlass UnInitialization Thread"));

	Shutdown();

	FreeLibraryAndExitThread(wil::GetModuleInstanceHandle(), S_OK);
}

void OpenGlass::Startup()
{
	{
		HWND hWnd{ nullptr };
		// just wait patiently, in case the dwm notification window is not ready...
		while (!(hWnd = FindWindowW(L"DWM", nullptr))) { Sleep(50); }
		GlassEngine::SetDwmNotificationWindow(hWnd);
	}

	wil::SetResultLoggingCallback([](const wil::FailureInfo& failure) static noexcept
	{
		WCHAR logMessage[MAX_PATH]{};
		if (FAILED(wil::GetFailureLogString(logMessage, MAX_PATH, failure)))
		{
			return;
		}

		OutputDebugStringW(logMessage);
	});

	THROW_IF_FAILED(RoInitialize(RO_INIT_MULTITHREADED));
	wil::unique_rouninitialize_call wrtScope{};

	if (
		!uDWM::g_registry.RecognizesBuild(uDWM::g_versionInfo.build) ||
		!dwmcore::g_registry.RecognizesBuild(dwmcore::g_versionInfo.build)
	)
	{
		auto result = 0;
		THROW_IF_FAILED(
			Util::TaskDialog(
				nullptr,
				nullptr,
				nullptr,
				Util::GetResourceStringView<IDS_STRING101>().data(),
				Util::GetResourceStringView<IDS_STRING105>().data(),
				TDCBF_YES_BUTTON | TDCBF_NO_BUTTON,
				TD_WARNING_ICON,
				&result
			)
		);
		if (result != IDYES)
		{
			return;
		}
	}

	//SYSTEM_POWER_CAPABILITIES powerCaps{};
	//if (GetPwrCapabilities(&powerCaps) && powerCaps.ThermalControl)
	//{
	//	// we are not running in a virtual machine and thermal control is supported
	//}

	if (!InitializeProjectionBySymbols())
	{
		return;
	}

	GlassEngine::LoadRegistry(false);
	g_lifecycleState.store(LifecycleState::Preparing, std::memory_order_release);
	GlassEngine::Startup();
	g_lifecycleState.store(LifecycleState::HooksActive, std::memory_order_release);

	// make sure guis can send message to dwm
	FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(ChangeWindowMessageFilterEx(GlassEngine::GetDwmNotificationWindow(), WM_DWMCOLORIZATIONCOLORCHANGED, MSGFLT_ALLOW, nullptr), "Unable to allow the DWM colorization notification");

	FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(WTSRegisterSessionNotification(GlassEngine::GetDwmNotificationWindow(), NOTIFY_FOR_THIS_SESSION), "Unable to register DWM session notifications");
	FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(ChangeWindowMessageFilterEx(GlassEngine::GetDwmNotificationWindow(), WM_WTSSESSION_CHANGE, MSGFLT_ALLOW, nullptr), "Unable to allow the DWM session notification");

	g_oldWndProc = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrW(
			GlassEngine::GetDwmNotificationWindow(),
			GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(DwmNotificationWndProc)
		)
	);
	FAIL_FAST_LAST_ERROR_IF_MSG(g_oldWndProc == 0, "Unable to subclass the DWM notification window");

	FAIL_FAST_IF_FAILED_MSG(
		PowerRegisterForEffectivePowerModeNotifications(
			EFFECTIVE_POWER_MODE_V1,
			EffectivePowerModeCallback,
			nullptr,
			&g_powerNotify
		),
		"Unable to register effective power mode notifications"
	);

	g_lifecycleState.store(LifecycleState::Active, std::memory_order_release);
	GlassEngine::Activate();

#if defined(_DEBUG)
	winrt::com_ptr<IDCompositionDeviceDebug> debugDevice{ nullptr };
	uDWM::CDesktopManager::GetInstance()->GetInteropCompositorDCompDevicePartner()->QueryInterface(
		debugDevice.put()
	);
	debugDevice->EnableDebugCounters();
#endif // _DEBUG

	/*DWORD broadcastInfo{ BSM_APPLICATIONS };
	BroadcastSystemMessageW(BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG | BSF_ALLOWSFW, &broadcastInfo, WM_THEMECHANGED, 0, 0);
	RedrawWindow(GetShellWindow(), nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE);*/

	return;
}

void OpenGlass::Shutdown()
{
	if (!GlassEngine::GetDwmNotificationWindow())
	{
		return;
	}

	const auto previousState = g_lifecycleState.exchange(LifecycleState::Stopping, std::memory_order_acq_rel);
	if (previousState == LifecycleState::Active)
	{
		FAIL_FAST_LAST_ERROR_IF_MSG(SetWindowLongPtrW(GlassEngine::GetDwmNotificationWindow(), GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_oldWndProc)) == 0, "Unable to restore the DWM notification window procedure");
		g_oldWndProc = nullptr;

		FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(ChangeWindowMessageFilterEx(GlassEngine::GetDwmNotificationWindow(), WM_WTSSESSION_CHANGE, MSGFLT_DISALLOW, nullptr), "Unable to remove the DWM session message filter");
		FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(WTSUnRegisterSessionNotification(GlassEngine::GetDwmNotificationWindow()), "Unable to unregister DWM session notifications");

		FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(ChangeWindowMessageFilterEx(GlassEngine::GetDwmNotificationWindow(), WM_DWMCOLORIZATIONCOLORCHANGED, MSGFLT_DISALLOW, nullptr), "Unable to remove the DWM colorization message filter");

		FAIL_FAST_IF_FAILED_MSG(
			PowerUnregisterFromEffectivePowerModeNotifications(g_powerNotify),
			"Unable to unregister effective power mode notifications"
		);
		g_powerNotify = nullptr;

		GlassEngine::Shutdown();
		GlassEngine::UnloadRegistry();

#if defined(_DEBUG)
		winrt::com_ptr<IDCompositionDeviceDebug> debugDevice{ nullptr };
		uDWM::CDesktopManager::GetInstance()->GetInteropCompositorDCompDevicePartner()->QueryInterface(
			debugDevice.put()
		);
		debugDevice->DisableDebugCounters();
#endif // _DEBUG

		/*DWORD broadcastInfo{ BSM_APPLICATIONS };
		BroadcastSystemMessageW(BSF_IGNORECURRENTTASK | BSF_FORCEIFHUNG | BSF_ALLOWSFW, &broadcastInfo, WM_THEMECHANGED, 0, 0);
		RedrawWindow(GetShellWindow(), nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE);*/

	}
	else
	{
		FAIL_FAST_IF_FAILED_MSG(previousState == LifecycleState::Inert ? S_OK : E_UNEXPECTED, "Shutdown entered from an invalid lifecycle state");
	}
	g_lifecycleState.store(LifecycleState::Inert, std::memory_order_release);

	GlassEngine::SetDwmNotificationWindow(nullptr);
}
