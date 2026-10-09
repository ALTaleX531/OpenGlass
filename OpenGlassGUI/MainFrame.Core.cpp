#include "pch.h"
#include "MainFrame.hpp"
#include "WrappingLayout.hpp"
#include "ColorPresetList.hpp"
#include "Symbols.hpp"
#include "BlurSettings.hpp"
#include "ConfigurationMigration.hpp"
#include "EffectiveConfiguration.hpp"
#include "Elevation.hpp"
#include "resource.h"
#include <wx/iconbndl.h>

namespace OpenGlass
{
	namespace
	{
		class MoreButton final : public wxButton
		{
		public:
			using wxButton::wxButton;

		private:
			bool DoPopupMenu(wxMenu* menu, int x, int y) override
			{
				const auto position = ClientToScreen(wxPoint(x, y));
				// Keep wx's popup wrapper and status-bar help, changing only alignment.
				const auto selected = TrackPopupMenuEx(reinterpret_cast<HMENU>(menu->GetHMenu()),
					TPM_RIGHTBUTTON | TPM_RECURSE | TPM_BOTTOMALIGN | TPM_RETURNCMD,
					position.x, position.y, reinterpret_cast<HWND>(GetHandle()), nullptr);
				if (selected) menu->MSWCommand(0, static_cast<WXWORD>(selected));
				return true;
			}
		};
	}

	std::wstring MainFrame::ResolveAccountName(const std::wstring& sidText, bool includeDomain)
	{
		PSID sid{};
		if (!ConvertStringSidToSidW(sidText.c_str(), &sid)) return {};
		wil::unique_hlocal sidStorage{ sid };
		DWORD nameLength{}, domainLength{};
		SID_NAME_USE use{};
		LookupAccountSidW(nullptr, sid, nullptr, &nameLength, nullptr, &domainLength, &use);
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return {};
		std::wstring name(nameLength, L'\0');
		std::wstring domain(domainLength, L'\0');
		if (!LookupAccountSidW(nullptr, sid, name.data(), &nameLength, domain.data(), &domainLength, &use)) return {};
		if (!name.empty() && name.back() == L'\0') name.pop_back();
		if (!domain.empty() && domain.back() == L'\0') domain.pop_back();
		return !includeDomain || domain.empty() ? name : domain + L"\\" + name;
	}

	MainFrame::MainFrame(std::wstring userSid, Settings::Scope scope)
		: wxFrame(nullptr, wxID_ANY, L"Aero Glass for Win10+", wxDefaultPosition, wxSize(900, 750))
	{
		SetIcons(wxIconBundle(wxString::Format(L"#%d", IDI_OPENGLASS), reinterpret_cast<WXHINSTANCE>(GetModuleHandleW(nullptr))));
		m_isAdmin = Elevation::IsProcessElevated();
		m_editScope = scope;
		if (scope == Settings::Scope::User)
		{
			const auto accountName = ResolveAccountName(userSid, true);
			SetTitle(GetTitle() + L" (HKCU: " + (accountName.empty() ? userSid : accountName) + L")");
		}
		else
		{
			SetTitle(GetTitle() + L" (HKLM)");
		}
		m_baseTitle = GetTitle();
		m_config = std::make_unique<RegistryConfig>(scope == Settings::Scope::User ? RegistryConfig::Mode::User : RegistryConfig::Mode::Machine, userSid);
		m_userConfig = std::make_unique<RegistryConfig>(RegistryConfig::Mode::User, userSid);
		m_resources.Initialize(PresetPackages::GetPresetRoot().parent_path() / L"Configuration", userSid);
		m_systemConfig = std::make_unique<RegistryConfig>(RegistryConfig::Mode::Machine, userSid);
		m_targetUserSid = userSid;

		SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK));

		wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
		SetSizer(mainSizer);

		CreateControls();
		CreateBottomControls(mainSizer);
		BindEvents();
		LoadSettings();
		SetDirty(false);

		Centre();
	}

	void MainFrame::CreateControls()
	{
		m_notebook = new wxNotebook(this, wxID_ANY);

		CreateSystemTab();
		CreateThemeTab();
		CreateAppearanceTab();
		CreateGlassColorsTab();
		CreatePresetsTab();
		CreateDiagnosticsTab();

		m_notebook->SetSelection(2);

		GetSizer()->Add(m_notebook, 1, wxEXPAND | wxALL, 5);
		CreateStatusBar();
	}

	void MainFrame::CreateBottomControls(wxSizer* parentSizer)
	{
		wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);

		auto* more = new MoreButton(this, wxID_ANY, L"More \u25B4");
		more->SetToolTip(L"Merge configuration or restore defaults in the editing scope.");
		more->Bind(wxEVT_BUTTON, [this, more](wxCommandEvent&)
		{
			const bool allowed = m_editScope == Settings::Scope::User || m_isAdmin;
			wxMenu menu;
			auto* merge = menu.Append(wxID_ANY,
				m_editScope == Settings::Scope::User ? L"Merge into HKCU..." : L"Merge into HKLM...",
				!allowed ? L"Requires administrator privileges to modify HKLM."
					: m_isDirty ? L"Save or Revert pending changes before merging."
					: L"Merge the saved effective configuration into the editing scope. Revert can undo this preview.");
			merge->Enable(allowed && !m_isDirty);
			auto* restore = menu.Append(wxID_ANY, L"Restore defaults...",
				!allowed ? L"Requires administrator privileges to modify HKLM."
					: L"Remove this scope's custom settings. Values from the other scope may still apply. Revert can undo this preview.");
			restore->Enable(allowed);
			wxMenuItem* switchView{};
			wxMenuItem* sync{};
			if (wxGetKeyState(WXK_SHIFT))
			{
				menu.AppendSeparator();
				sync = menu.Append(wxID_ANY, L"Sync",
					L"Save or Revert pending changes before reloading external changes into the GUI.");
				sync->Enable(CanSyncExternalChanges());
				switchView = menu.Append(wxID_ANY,
					m_editScope == Settings::Scope::User ? L"Switch to HKLM view" : L"Switch to HKCU view",
					L"Switch editing scope after saving or reverting pending changes and finishing symbol downloads.");
				switchView->Enable(CanSwitchEditingScope());
			}
			const auto selected = more->GetPopupMenuSelectionFromUser(menu, wxPoint(0, 0));
			if (selected == merge->GetId()) MergeConfiguration();
			else if (selected == restore->GetId()) RestoreDefaults();
			else if (sync && selected == sync->GetId()) SyncExternalChanges();
			else if (switchView && selected == switchView->GetId()) SwitchEditingScope();
		});

		m_btnSave = new wxButton(this, wxID_ANY, L"Save");
		m_btnRevert = new wxButton(this, wxID_ANY, L"Revert");
		m_btnSave->Enable(false);
		m_btnRevert->Enable(false);
		m_btnSave->SetToolTip(L"Save changes (Ctrl+S)");
		m_btnRevert->SetToolTip(L"Revert pending changes");

		btnSizer->Add(more, 0);
		btnSizer->AddStretchSpacer();
		btnSizer->Add(m_btnSave, 0, wxRIGHT, 5);
		btnSizer->Add(m_btnRevert, 0);

		parentSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 5);
	}

	bool MainFrame::CanSwitchEditingScope() const
	{
		return !m_isDirty && !m_colorPreference.IsDirty() && !m_preview.IsAttemptActive()
			&& !m_symbolDownloadRunning && !m_closeWhenSymbolDownloadStops && !IsBeingDeleted();
	}

	void MainFrame::SwitchEditingScope()
	{
		if (!CanSwitchEditingScope()) return;
		Enable(false);
		auto enableOnFailure = wil::scope_exit([this] { Enable(); });
		MainFrame* replacement{};
		try
		{
			// Keep the verified original user and the app's single-instance lock.
			const auto scope = m_editScope == Settings::Scope::User ? Settings::Scope::Machine : Settings::Scope::User;
			WINDOWPLACEMENT placement{ sizeof(WINDOWPLACEMENT) };
			THROW_IF_WIN32_BOOL_FALSE(GetWindowPlacement(reinterpret_cast<HWND>(GetHandle()), &placement));
			replacement = new MainFrame(m_targetUserSid.ToStdWstring(), scope);
			// Copy the restore rectangle too, while keeping the new window hidden.
			placement.showCmd = SW_HIDE;
			THROW_IF_WIN32_BOOL_FALSE(SetWindowPlacement(reinterpret_cast<HWND>(replacement->GetHandle()), &placement));
			replacement->m_notebook->ChangeSelection(m_notebook->GetSelection());
			if (IsMaximized()) replacement->Maximize();
		}
		catch (...)
		{
			if (replacement) replacement->Destroy();
			wxMessageBox(wxString::Format(L"The other editing view could not be opened (HRESULT 0x%08lX).",
				static_cast<unsigned long>(wil::ResultFromCaughtException())), L"Switch editing scope", wxOK | wxICON_ERROR, this);
			return;
		}
		wxTheApp->SetTopWindow(replacement);
		if (!CanSwitchEditingScope() || !Close())
		{
			wxTheApp->SetTopWindow(this);
			replacement->Destroy();
			return;
		}
		enableOnFailure.release();
		replacement->Show();
	}

	void MainFrame::NotifySettingsChange(Settings::UpdateImpact impact)
	{
		const auto flags = static_cast<unsigned>(impact);
		if (!(flags & static_cast<unsigned>(Settings::UpdateImpact::Colorization | Settings::UpdateImpact::Theme))) return;
		if (!m_dwmWindow || !IsWindow(m_dwmWindow))
		{
			m_dwmWindow = FindWindowW(L"Dwm", nullptr);
		}
		HWND notificationWindow = m_dwmWindow;
		if (!notificationWindow) return;
		// Configuration editing and recovery also work without OpenGlass loaded;
		// notification delivery is best-effort.
		if (flags & static_cast<unsigned>(Settings::UpdateImpact::Colorization))
		{
			// DWM reloads its Accent cache only for the ImmersiveColorSet category.
			SetLastError(ERROR_SUCCESS);
			LOG_IF_WIN32_BOOL_FALSE(static_cast<BOOL>(SendMessageTimeoutW(
				notificationWindow, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"ImmersiveColorSet"),
				SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 1000, nullptr
			) != 0));
		}

		if (flags & static_cast<unsigned>(Settings::UpdateImpact::Theme))
			LOG_IF_WIN32_BOOL_FALSE(SendNotifyMessageW(notificationWindow, WM_THEMECHANGED, 0, 0));
	}

	void MainFrame::SetDirty(bool dirty)
	{
		auto syncButtons = [this]() {
			if (m_btnSave)
			{
				m_btnSave->Enable(m_isDirty);
			}
			if (m_btnRevert)
			{
				m_btnRevert->Enable(m_isDirty);
			}
		};
		if (m_isDirty == dirty)
		{
			syncButtons();
			return;
		}
		m_isDirty = dirty;
		UpdateWindowTitle();
		syncButtons();
	}

	void MainFrame::UpdateWindowTitle()
	{
		if (m_isDirty)
		{
			SetTitle(L"*" + m_baseTitle);
		}
		else
		{
			SetTitle(m_baseTitle);
		}
	}

	void MainFrame::StartSymbolDownload()
	{
		if (m_symbolDownloadRunning || !m_isAdmin)
		{
			return;
		}

		m_closeWhenSymbolDownloadStops = false;
		m_symbolDownloadRunning = true;
		m_btnDownloadSymbols->Enable(false);
		m_btnCancelSymbolDownload->Enable(true);
		m_dpSymbolCacheDirectory->Enable(false);
		std::wstring symbolDirectory = m_dpSymbolCacheDirectory->GetPath().ToStdWstring();
		if (symbolDirectory.empty())
		{
			symbolDirectory = GetSymbolCacheDirectory();
			m_dpSymbolCacheDirectory->SetPath(symbolDirectory);
		}
		UpdateSymbolDownloadResult(wxART_INFORMATION, wxEmptyString);
		UpdateSymbolDownloadProgress(SymbolDownloadProgress{
			0,
			true,
			L"Connecting to Microsoft Symbol Server...",
			L"Preparing symbol download."
		});

		m_symbolDownloadThread = std::jthread([this, symbolDirectory = std::move(symbolDirectory)](std::stop_token stopToken)
		{
			const auto progressCallback = [this](const SymbolDownloadProgress& progress)
			{
				CallAfter([this, progress]
				{
					UpdateSymbolDownloadProgress(progress);
				});
			};

			const SymbolDownloadOutcome outcome = DownloadSymbols(symbolDirectory, stopToken, progressCallback);
			CallAfter([this, outcome]
			{
				FinishSymbolDownload(outcome);
			});
		});
	}

	void MainFrame::RefreshDiagnosticsLayout()
	{
		wxWindow* parent = nullptr;
		if (m_pnlSymbolDownloadResult)
		{
			parent = m_pnlSymbolDownloadResult->GetParent();
		}
		else if (m_lblSymbolDownloadDetail)
		{
			parent = m_lblSymbolDownloadDetail->GetParent();
		}

		if (parent)
		{
			// Assign widths first, including a result panel that has just become visible.
			parent->Layout();
			WrapStaticTextToParentWidth(m_lblSymbolDownloadDetail, m_symbolDownloadDetailText);
			WrapStaticTextToParentWidth(m_lblSymbolDownloadResult, m_symbolDownloadResultText);
			WrapStaticTextToParentWidth(m_lblDwmCrashDumpStatus, m_dwmCrashDumpStatusText);
			parent->Layout();
			if (wxScrolledWindow* scrolled = wxDynamicCast(parent, wxScrolledWindow))
			{
				scrolled->FitInside();
			}
		}

		Layout();
		Refresh();
		Update();
	}

	void MainFrame::RefreshTransparencyDiagnostics()
	{
		if (!m_lblWindowsTransparencyStatus || !m_lblOpaqueBlendStatus || !m_lblPowerModeStatus || !m_lblDisableOnBatteryStatus || !m_bmpEffectiveTransparencyWarning || !m_lblEffectiveTransparencyStatus)
		{
			return;
		}

		TransparencyDiagnostics diagnostics;
		const HRESULT result = QueryTransparencyDiagnostics(m_targetUserSid.ToStdWstring(), diagnostics);
		if (FAILED(result))
		{
			const wxString error = wxString::Format(L"Unavailable (HRESULT 0x%08lX)", static_cast<unsigned long>(result));
			m_lblWindowsTransparencyStatus->SetLabel(error);
			m_lblOpaqueBlendStatus->SetLabel(error);
			m_lblPowerModeStatus->SetLabel(error);
			m_lblDisableOnBatteryStatus->SetLabel(error);
			m_bmpEffectiveTransparencyWarning->Hide();
			m_lblEffectiveTransparencyStatus->SetLabel(error);
			RefreshDiagnosticsLayout();
			return;
		}

		auto sourceName = [](DiagnosticRegistrySource source)
		{
			switch (source)
			{
			case DiagnosticRegistrySource::User: return L"HKCU";
			case DiagnosticRegistrySource::Machine: return L"HKLM";
			default: return L"default";
			}
		};
		auto powerModeName = [](EFFECTIVE_POWER_MODE mode)
		{
			switch (mode)
			{
			case EffectivePowerModeBatterySaver: return L"Battery saver / high savings";
			case EffectivePowerModeBetterBattery: return L"Better battery / standard savings";
			case EffectivePowerModeBalanced: return L"Balanced";
			case EffectivePowerModeHighPerformance: return L"High performance";
			case EffectivePowerModeMaxPerformance: return L"Maximum performance";
			case EffectivePowerModeGameMode: return L"Game mode";
			case EffectivePowerModeMixedReality: return L"Mixed reality";
			default: return L"Unknown";
			}
		};

		m_lblWindowsTransparencyStatus->SetLabel(diagnostics.windowsTransparencyEnabled ? L"On" : L"Off (opaque)");
		m_lblOpaqueBlendStatus->SetLabel(wxString::Format(
			L"%lu (%ls, %ls)",
			static_cast<unsigned long>(diagnostics.colorizationOpaqueBlend),
			sourceName(diagnostics.colorizationOpaqueBlendSource),
			diagnostics.colorizationOpaqueBlend ? L"opaque" : L"transparent"
		));
		m_lblPowerModeStatus->SetLabel(wxString::Format(
			L"%ls (%ls)",
			powerModeName(diagnostics.effectivePowerMode),
			diagnostics.powerSaverActive ? L"saver" : L"normal"
		));
		m_lblDisableOnBatteryStatus->SetLabel(wxString::Format(
			L"%ls (%ls)",
			diagnostics.disableGlassOnBattery ? L"On" : L"Off",
			sourceName(diagnostics.disableGlassOnBatterySource)
		));

		const bool isOpaque = diagnostics.colorizationOpaqueBlend
			|| (diagnostics.powerSaverActive && diagnostics.disableGlassOnBattery)
			|| !diagnostics.windowsTransparencyEnabled;
		wxString resultText = L"Transparent";
		if (diagnostics.colorizationOpaqueBlend)
		{
			resultText = L"Opaque: Opaque blend";
		}
		else if (diagnostics.powerSaverActive && diagnostics.disableGlassOnBattery)
		{
			resultText = L"Opaque: Power saver";
		}
		else if (!diagnostics.windowsTransparencyEnabled)
		{
			resultText = L"Opaque: Windows setting";
		}
		m_bmpEffectiveTransparencyWarning->Show(isOpaque);
		m_lblEffectiveTransparencyStatus->SetLabel(resultText);
		RefreshDiagnosticsLayout();
	}

	void MainFrame::UpdateSymbolDownloadProgress(const SymbolDownloadProgress& progress)
	{
		if (m_gaugeSymbolDownload)
		{
			if (progress.indeterminate)
			{
				m_gaugeSymbolDownload->Pulse();
			}
			else
			{
				m_gaugeSymbolDownload->SetValue(std::clamp(progress.percent, 0, 100));
			}
		}

		bool textChanged{};
		if (m_lblSymbolDownloadPhase)
		{
			if (m_lblSymbolDownloadPhase->GetLabelText() != progress.phase)
			{
				m_lblSymbolDownloadPhase->SetLabel(progress.phase);
				textChanged = true;
			}
		}
		if (m_lblSymbolDownloadDetail)
		{
			if (m_symbolDownloadDetailText != progress.detail)
			{
				m_symbolDownloadDetailText = progress.detail;
				textChanged = true;
			}
		}
		if (textChanged) RefreshDiagnosticsLayout();
	}

	void MainFrame::UpdateSymbolDownloadResult(wxArtID iconId, const wxString& details)
	{
		if (!m_pnlSymbolDownloadResult || !m_bmpSymbolDownloadResult || !m_lblSymbolDownloadResult)
		{
			return;
		}

		if (details.empty())
		{
			m_symbolDownloadResultText.clear();
			m_lblSymbolDownloadResult->SetLabel(wxEmptyString);
			m_pnlSymbolDownloadResult->Hide();
			RefreshDiagnosticsLayout();
			return;
		}

		m_bmpSymbolDownloadResult->SetBitmap(
			wxArtProvider::GetBitmap(iconId, wxART_MESSAGE_BOX, wxSize(16, 16))
		);

		m_symbolDownloadResultText = details;
		WrapStaticTextToParentWidth(m_lblSymbolDownloadResult, m_symbolDownloadResultText);
		m_pnlSymbolDownloadResult->Show();
		RefreshDiagnosticsLayout();
	}

	void MainFrame::FinishSymbolDownload(const SymbolDownloadOutcome& outcome)
	{
		m_symbolDownloadRunning = false;
		m_btnDownloadSymbols->Enable(m_isAdmin);
		m_btnCancelSymbolDownload->Enable(false);
		m_dpSymbolCacheDirectory->Enable(m_isAdmin);

		switch (outcome.result)
		{
		case SymbolDownloadResult::Success:
			UpdateSymbolDownloadProgress(SymbolDownloadProgress{
				100,
				false,
				L"Symbols downloaded successfully.",
				std::format(L"The symbol cache has been updated:\n{}", outcome.symbolDirectory)
			});
			UpdateSymbolDownloadResult(wxART_INFORMATION, wxEmptyString);
			break;
		case SymbolDownloadResult::Cancelled:
			UpdateSymbolDownloadProgress(SymbolDownloadProgress{
				m_gaugeSymbolDownload ? m_gaugeSymbolDownload->GetValue() : 0,
				false,
				L"Symbol download cancelled.",
				L"No further network requests will be started."
			});
			UpdateSymbolDownloadResult(wxART_WARNING, outcome.details);
			break;
		case SymbolDownloadResult::Failed:
		default:
			UpdateSymbolDownloadProgress(SymbolDownloadProgress{
				m_gaugeSymbolDownload ? m_gaugeSymbolDownload->GetValue() : 0,
				false,
				L"Symbol download failed.",
				outcome.summary
			});
			UpdateSymbolDownloadResult(wxART_ERROR, outcome.details);
			break;
		}

		const bool closePending = m_closeWhenSymbolDownloadStops;
		m_closeWhenSymbolDownloadStops = false;
		if (closePending)
		{
			Close(true);
			return;
		}
	}

	void MainFrame::RefreshDwmCrashDumpConfiguration()
	{
		DwmCrashDumpConfiguration configuration;
		const HRESULT result = QueryDwmCrashDumpConfiguration(configuration);
		if (FAILED(result))
		{
			m_dwmCrashDumpStatusText = wxString::Format(
				L"Unable to read the dwm.exe WER configuration (HRESULT 0x%08lX).",
				static_cast<unsigned long>(result)
			);
			m_btnEnableDwmCrashDumps->Enable(m_isAdmin);
			m_btnDisableDwmCrashDumps->Enable(m_isAdmin && configuration.enabled);
			RefreshDiagnosticsLayout();
			return;
		}

		m_btnEnableDwmCrashDumps->Enable(m_isAdmin);
		m_btnDisableDwmCrashDumps->Enable(m_isAdmin && configuration.enabled);
		if (!configuration.enabled)
		{
			m_dwmCrashDumpStatusText = L"Disabled. No per-application WER LocalDumps configuration exists for dwm.exe. System-wide WER settings, if present, may still apply.";
			RefreshDiagnosticsLayout();
			return;
		}

		if (!configuration.dumpFolder.empty())
		{
			m_dpDwmCrashDumpFolder->SetPath(configuration.dumpFolder);
		}

		const wxString folder = configuration.dumpFolder.empty()
			? wxString{ L"Windows default" }
			: wxString{ configuration.dumpFolder };
		if (configuration.dumpType == 2 && configuration.dumpCount == 1 && !configuration.dumpFolder.empty())
		{
			m_dwmCrashDumpStatusText = wxString::Format(
				L"Enabled for dwm.exe: full dump, keep 1, folder: %s",
				folder
			);
		}
		else
		{
			m_dwmCrashDumpStatusText = wxString::Format(
				L"Enabled with custom settings for dwm.exe: DumpType=%lu, DumpCount=%lu, folder: %s. Click Enable full dumps to apply the recommended OpenGlass settings.",
				configuration.dumpType,
				configuration.dumpCount,
				folder
			);
		}
		RefreshDiagnosticsLayout();
	}

	void MainFrame::SetDwmCrashDumpsEnabled(bool enabled)
	{
		if (!m_isAdmin)
		{
			return;
		}

		HRESULT result{};
		if (enabled)
		{
			std::wstring requestedFolder = m_dpDwmCrashDumpFolder->GetPath().ToStdWstring();
			if (requestedFolder.empty())
			{
				requestedFolder = GetDefaultDwmCrashDumpFolder();
			}

			std::wstring configuredFolder;
			result = EnableDwmCrashDumps(requestedFolder, configuredFolder);
			if (SUCCEEDED(result))
			{
				m_dpDwmCrashDumpFolder->SetPath(configuredFolder);
			}
		}
		else
		{
			result = DisableDwmCrashDumps();
		}

		if (FAILED(result))
		{
			wxMessageBox(
				wxString::Format(
					enabled
						? L"Failed to enable WER crash dumps (HRESULT 0x%08lX)."
						: L"Failed to disable WER crash dumps (HRESULT 0x%08lX).",
					static_cast<unsigned long>(result)
				),
				L"WER crash dumps",
				wxOK | wxICON_ERROR,
				this
			);
		}
		RefreshDwmCrashDumpConfiguration();
	}

	RegistryConfig* MainFrame::GetConfigForScope(Settings::Scope scope) const
	{
		return scope == Settings::Scope::User ? m_userConfig.get() : m_systemConfig.get();
	}

	void MainFrame::EnsurePreviewWriter()
	{
		if (m_previewWriter) return;
		m_previewWriter = m_resources.AcquireWriter();
		auto releaseOnFailure = wil::scope_exit([this] { m_previewWriter.reset(); });
		if (m_resources.HasForeignRecovery())
		{
			m_previewWriter.reset();
			wxMessageBox(L"An interrupted operation belongs to another user. Open the editor as that user to recover it before changing configuration.", L"Configuration recovery", wxOK | wxICON_WARNING, this);
			THROW_HR(HRESULT_FROM_WIN32(ERROR_RECOVERY_FAILURE));
		}
		if (m_resources.HasRecovery())
		{
			if (wxMessageBox(L"An interrupted configuration operation needs recovery before editing. Restore its previous state now?", L"Recover configuration", wxYES_NO | wxICON_WARNING, this) != wxYES)
			{
				m_previewWriter.reset(); THROW_HR(HRESULT_FROM_WIN32(ERROR_RECOVERY_FAILURE));
			}
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RECOVERY_FAILURE), !m_resources.Recover(
				[this](auto scope, auto id, const auto& value) { return SUCCEEDED(GetConfigForScope(scope)->WriteRaw(std::wstring(Settings::Get(id).name), value)); },
				[this](const auto& color)
				{
					ColorPreference::Snapshot current;
					return SUCCEEDED(m_colorPreference.Capture(m_targetUserSid.ToStdWstring(), current))
						&& SUCCEEDED(m_colorPreference.RecoverSnapshot(color));
				}));
			m_colorPreference.Accept(); NotifySettingsChange(); LoadSettings();
		}
		releaseOnFailure.release();
	}

	void MainFrame::TrackSettingChange(Settings::Id id)
	{
		TrackSettingChange(m_editScope, id);
	}

	void MainFrame::TrackSettingChange(Settings::Scope scope, Settings::Id id)
	{
		EnsurePreviewWriter();
		m_resources.TrackRegistry(scope, id, GetConfigForScope(scope)->ReadRaw(std::wstring(Settings::Get(id).name)));
		const auto read = [this](const TrackedSetting& key)
		{
			return GetConfigForScope(key.scope)->ReadRaw(key.Name());
		};
		m_preview.Touch({ scope, id }, read);
	}

	void MainFrame::ReconcilePreview()
	{
		m_resources.Reconcile();
		m_preview.Reconcile([this](const TrackedSetting& key) { return GetConfigForScope(key.scope)->ReadRaw(key.Name()); });
		SetDirty(m_preview.IsDirty() || m_colorPreference.IsDirty() || m_resources.IsDirty());
	}

	void MainFrame::ReportRegistryError(HRESULT result, const std::wstring& name)
	{
		if (SUCCEEDED(result)) return;
		if (m_preview.IsAttemptActive()) THROW_IF_FAILED(result);
		SetDirty(m_preview.IsDirty() || m_colorPreference.IsDirty() || m_resources.IsDirty());
		NotifySettingsChange();
		LoadSettings();
		wxMessageBox(
			wxString::Format(L"The setting '%s' could not be updated (HRESULT 0x%08lX).", name.c_str(), static_cast<unsigned long>(result)),
			L"OpenGlass configuration",
			wxOK | wxICON_ERROR,
			this
		);
	}

	void MainFrame::RestoreDefaults()
	{
		if (!m_config || (m_editScope == Settings::Scope::Machine && !m_isAdmin)) return;
		if (wxMessageBox(wxString::Format(
			L"Restore all OpenGlass settings in %s to defaults? This removes custom values in this scope; values from the other scope may still apply. Windows accent color and diagnostics settings will be preserved. Use Revert to undo this preview.",
			m_editScope == Settings::Scope::User ? L"HKCU" : L"HKLM"), L"Restore defaults", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES) return;
		bool restartRequired{};
		const bool applied = RunPreview([this, &restartRequired]
		{
			const auto plan = EffectiveConfiguration::PlanReset(EffectiveConfiguration::Read(*m_config), m_editScope);
			for (const auto& change : plan)
			{
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), m_config->ReadRaw(change.Name()) != change.before);
				restartRequired |= Settings::Get(change.id).impact == Settings::UpdateImpact::RestartRequired;
				TrackSettingChange(change.id);
				THROW_IF_FAILED(m_config->WriteRaw(change.Name(), change.after));
			}
		});
		if (applied) LoadSettings();
		if (applied && restartRequired) wxMessageBox(L"Some settings take effect after restarting DWM or signing out. DWM has not been restarted.",
			L"Restore defaults", wxOK | wxICON_INFORMATION, this);
	}

	void MainFrame::MergeConfiguration()
	{
		if (m_editScope == Settings::Scope::Machine && !m_isAdmin) return;
		const wxString title = m_editScope == Settings::Scope::User ? L"Merge into HKCU" : L"Merge into HKLM";
		if (m_isDirty)
		{
			wxMessageBox(L"Save pending changes before merging configuration.", title, wxOK | wxICON_INFORMATION, this);
			return;
		}
		try
		{
			auto prepareChanges = [&]
			{
				auto changes = ConfigurationMigration::Prepare(*m_userConfig, *m_systemConfig, m_editScope);
				const auto effective = EffectiveConfiguration::Capture(EffectiveConfiguration::Read(*m_userConfig), EffectiveConfiguration::Read(*m_systemConfig));
				for (const auto& [id, value] : effective)
				{
					if (Settings::Get(id).assetRole == Settings::AssetRole::None) continue;
					const auto source = std::get_if<std::wstring>(&value);
					if (!source || source->empty() || CompareStringOrdinal(source->c_str(), -1, m_resources.Path(m_editScope, id).c_str(), -1, TRUE) == CSTR_EQUAL) continue;
					if (std::ranges::any_of(changes, [&](const auto& change) { return change.scope == m_editScope && change.id == id; })) continue;
					const std::wstring name(Settings::Get(id).name);
					changes.push_back({ m_editScope, id, m_config->ReadRaw(name), EffectiveConfiguration::Encode(value), L"Copy " + name + L" to this layer's fixed resource location." });
				}
				std::stable_partition(changes.begin(), changes.end(), [&](const auto& change) { return change.scope == m_editScope; });
				return changes;
			};
			const auto changes = prepareChanges();
			if (changes.empty())
			{
				wxMessageBox(L"No configuration values need merging.", L"Merge configuration", wxOK, this);
				return;
			}
			wxString description = m_editScope == Settings::Scope::User
				? L"Merge the effective OpenGlass configuration into the original interactive user's HKCU. HKLM will be preserved."
				: L"Merge the effective OpenGlass configuration into HKLM. Copied OpenGlass values will be removed from the original interactive user's HKCU.";
			description += L" The five Windows base color values will be preserved in both layers. Other users are not inspected. This is a preview: Save accepts it; Revert restores the previous configuration.\n\n";
			for (const auto& change : changes) description += change.description + L"\n";
			wxDialog review(this, wxID_ANY, title, wxDefaultPosition, wxSize(760, 560), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
			auto* layout = new wxBoxSizer(wxVERTICAL);
			layout->Add(new wxTextCtrl(&review, wxID_ANY, description, wxDefaultPosition, wxDefaultSize,
				wxTE_MULTILINE | wxTE_READONLY), 1, wxEXPAND | wxALL, 12);
			layout->Add(review.CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);
			review.SetSizer(layout);
			if (review.ShowModal() != wxID_OK) return;
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), prepareChanges() != changes);
			std::map<std::pair<Settings::Scope, Settings::Id>, std::shared_ptr<ConfigurationResources::Preparation>> prepared;
			for (const auto& change : changes)
				if (Settings::Get(change.id).assetRole != Settings::AssetRole::None)
				{
					const auto value = EffectiveConfiguration::Decode(change.after, Settings::Get(change.id));
					if (const auto source = std::get_if<std::wstring>(&value); source && !source->empty())
						prepared[{ change.scope, change.id }] = m_resources.PrepareFile(change.scope, change.id, *source);
				}
			if (RunPreview([&]
			{
				for (const auto& change : changes)
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), GetConfigForScope(change.scope)->ReadRaw(change.Name()) != change.before);
				for (const auto& [key, files] : prepared) m_resources.Install(*files);
				for (const auto& change : changes)
				{
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), GetConfigForScope(change.scope)->ReadRaw(change.Name()) != change.before);
					TrackSettingChange(change.scope, change.id);
					if (prepared.contains({ change.scope, change.id })) THROW_IF_FAILED(GetConfigForScope(change.scope)->SetString(change.Name(), m_resources.Path(change.scope, change.id).wstring()));
					else THROW_IF_FAILED(GetConfigForScope(change.scope)->WriteRaw(change.Name(), change.after));
				}
			})) LoadSettings();
		}
		catch (...)
		{
			const auto failure = wil::ResultFromCaughtException();
			const bool restored = RevertSettings();
			wxMessageBox(wxString::Format(L"Merge failed (0x%08lX). %s", failure,
				restored ? L"The previous state was restored." : L"Recovery is incomplete. Use Revert to retry."),
				L"Merge configuration", wxOK | wxICON_ERROR, this);
		}
	}

	bool MainFrame::RunPreview(const std::function<void()>& operation, Settings::UpdateImpact impact)
	{
		const bool wasDirty = m_isDirty;
		const bool colorWasDirty = m_colorPreference.IsDirty();
		const auto needsRefresh = [this]
		{
			// Explicit color operations can refresh Windows-derived values without
			// changing the selected mode or RGB (for example, reselecting Automatic).
			return m_preview.HasAttemptChanges() || m_colorPreference.IsAttemptActive() || m_resources.HasAttemptChanges();
		};
		bool refreshRequired{};
		try
		{
			EnsurePreviewWriter();
			m_resources.Begin();
			m_preview.Begin();
			operation();
			m_preview.VisitRestore(true, [this](const TrackedSetting& key, const auto&)
			{
				if (Settings::Get(key.id).assetRole != Settings::AssetRole::None)
					m_resources.RemoveUnused(key.scope, key.id, GetConfigForScope(key.scope)->GetString(key.Name(), L""));
			});
			ReconcilePreview();
			// An empty reset or unchanged write needs no DWM delivery. Returning to
			// the session baseline still does, even though the preview is now clean.
			refreshRequired = needsRefresh();
			// A no-op cannot accept an earlier failed recovery.
			if (!refreshRequired && wasDirty) SetDirty(true);
			if (refreshRequired) NotifySettingsChange(impact);
			// Preserve the control being edited (for example an empty file picker or
			// a newly selected Custom mode). Whole-configuration operations reload explicitly.
			UpdateOptionStatusIcons();
			UpdatePathWarningIcons();
			UpdateColorizationPresetSelection();
			if (!m_isDirty) m_presetProvenance.Revert();
			m_resources.CommitAttempt();
			if (m_resources.TakeCleanupWarning()) wxMessageBox(L"Configuration was updated, but temporary-file cleanup is pending. It will be retried on the next operation.", L"Configuration cleanup", wxOK | wxICON_INFORMATION, this);
			m_preview.CommitAttempt();
			m_colorPreference.CommitAttempt();
			if (!m_isDirty) m_previewWriter.reset();
			return true;
		}
		catch (...)
		{
			const auto failure = wil::ResultFromCaughtException();
			try { m_preview.Reconcile([this](const auto& key) { return GetConfigForScope(key.scope)->ReadRaw(key.Name()); }); }
			catch (...) { LOG_CAUGHT_EXCEPTION(); }
			refreshRequired |= needsRefresh();
			const auto colorBefore = m_colorPreference.Baseline(true);
			bool restored = SUCCEEDED(m_colorPreference.RollbackAttempt());
			restored = m_resources.RevertAttemptFiles() && restored;
			if (m_preview.IsAttemptActive()) restored = m_preview.RollbackAttempt(
				[this, &colorBefore](const TrackedSetting& key, const RegistryConfig::RawValue& value)
				{
					if (colorBefore && colorBefore->RestoresAutomatic() && key.scope == Settings::Scope::User
						&& (key.id == Settings::Id::ColorizationColor || key.id == Settings::Id::ColorizationAfterglow)) return true;
					return SUCCEEDED(GetConfigForScope(key.scope)->WriteRaw(key.Name(), value));
				}, restored);
			try { restored = m_resources.RollbackAttempt(restored) && restored; }
			catch (...) { LOG_CAUGHT_EXCEPTION(); restored = false; }
			if (refreshRequired) NotifySettingsChange();
			if (restored && !colorWasDirty) m_colorPreference.Accept();
			SetDirty(wasDirty || !restored || m_preview.IsDirty() || m_colorPreference.IsDirty() || m_resources.IsDirty());
			if (!m_isDirty && !m_resources.HasRecovery()) m_previewWriter.reset();
			LoadSettings();
			wxMessageBox(wxString::Format(L"Preview failed (0x%08lX). %s", failure,
				restored ? L"This attempt was restored; earlier previews are preserved." : L"Recovery is incomplete. Use Revert to retry."),
				L"Configuration preview", wxOK | wxICON_ERROR, this);
			return false;
		}
	}

	void MainFrame::SaveSettings()
	{
		if (!m_config)
		{
			return;
		}
		try { m_resources.Accept(); }
		catch (...) { wxMessageBox(L"The recovery record could not be completed. Retry Save or Revert.", L"Configuration recovery", wxOK | wxICON_ERROR, this); return; }
		if (m_resources.TakeCleanupWarning()) wxMessageBox(L"The configuration checkpoint is complete. Temporary-file cleanup will be retried on the next operation.", L"Configuration cleanup", wxOK | wxICON_INFORMATION, this);
		m_previewWriter.reset();
		m_colorPreference.Accept();
		m_preview.Accept();
		m_presetProvenance.Accept();
		SetDirty(false);
		UpdateOptionStatusIcons();
		UpdatePathWarningIcons();
	}

	bool MainFrame::RevertSettings()
	{
		if (!m_config)
		{
			return false;
		}
		if (!m_isDirty && !m_colorPreference.IsDirty() && !m_resources.IsDirty()) return true;
		try
		{
			std::vector<ConfigurationResources::RegistryBefore> registry;
			m_preview.VisitRestore(false, [&](const TrackedSetting& key, const RegistryConfig::RawValue& value) { registry.push_back({ key.scope, key.id, value }); });
			m_resources.PrepareRevert(std::move(registry), m_colorPreference.Baseline());
		}
		catch (...) { wxMessageBox(L"The recovery record could not be prepared. No Revert changes were made; retry Revert.", L"Configuration recovery", wxOK | wxICON_ERROR, this); return false; }
		const auto colorBefore = m_colorPreference.Baseline();
		const auto colorResult = m_colorPreference.Revert();
		const bool filesRestored = m_resources.Revert();
		const bool restored = m_preview.Revert([this, &colorBefore](const TrackedSetting& key, const RegistryConfig::RawValue& value)
		{
			// Automatic recovery recomputes wallpaper RGB; do not replace it with an old derived color.
			if (colorBefore && colorBefore->RestoresAutomatic() && key.scope == Settings::Scope::User
				&& (key.id == Settings::Id::ColorizationColor || key.id == Settings::Id::ColorizationAfterglow)) return true;
			return SUCCEEDED(GetConfigForScope(key.scope)->WriteRaw(key.Name(), value));
		}, SUCCEEDED(colorResult) && filesRestored, false);
		if (!restored)
		{
			SetDirty(true);
			wxMessageBox(L"Recovery is incomplete. The preview remains pending; use Revert to retry.", L"Revert", wxOK | wxICON_ERROR, this);
			return false;
		}
		NotifySettingsChange();
		LoadSettings();
		try { m_resources.Accept(); }
		catch (...) { wxMessageBox(L"The recovery record could not be completed. Retry Save or Revert.", L"Configuration recovery", wxOK | wxICON_ERROR, this); return false; }
		if (m_resources.TakeCleanupWarning()) wxMessageBox(L"The configuration checkpoint is complete. Temporary-file cleanup will be retried on the next operation.", L"Configuration cleanup", wxOK | wxICON_INFORMATION, this);
		m_previewWriter.reset();
		m_colorPreference.Accept();
		m_preview.Accept();
		SetDirty(false);
		m_presetProvenance.Revert();
		return true;
	}

	void MainFrame::AddProperty(
		wxWindow* parent,
		wxSizer* sizer,
		const wxString& label,
		wxWindow* control,
		Settings::Id setting
	)
	{
		if (wxSlider* slider = dynamic_cast<wxSlider*>(control))
		{
			slider->SetToolTip(wxString::Format(L"%d", slider->GetValue()));

			// Update tooltip on slider release
			slider->Bind(wxEVT_SLIDER, [slider]([[maybe_unused]] wxCommandEvent& e) {
				slider->SetToolTip(wxString::Format(L"%d", slider->GetValue()));
			});

			// Update tooltip while dragging
			slider->Bind(wxEVT_SCROLL_THUMBTRACK, [slider]([[maybe_unused]] wxScrollEvent& e) {
				slider->SetToolTip(wxString::Format(L"%d", slider->GetValue()));
			});
		}

		wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
		wxStaticText* text = new wxStaticText(parent, wxID_ANY, label, wxDefaultPosition, wxSize(300, -1));
		row->Add(text, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
		row->Add(control, 1, wxALIGN_CENTER_VERTICAL);
		AddOptionStatus(parent, row, setting);
		sizer->Add(row, 0, wxEXPAND | wxALL, 2);
	}

	void MainFrame::AddOptionStatus(
		wxWindow* parent,
		wxBoxSizer* row,
		Settings::Id setting
	)
	{
		if (!parent || !row)
		{
			return;
		}

		const wxSize iconSize(16, 16);
		const wxBitmap infoBmp = wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_MESSAGE_BOX, iconSize);
		auto* info = new wxStaticBitmap(parent, wxID_ANY, infoBmp);
		info->SetMinSize(iconSize);
		info->Hide();
		row->Add(info, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxLEFT, 2);

		m_optionStatus.push_back({ info, setting });
	}

	void MainFrame::UpdateOptionStatusIcons()
	{
		if (!m_config)
		{
			return;
		}

		bool needLayout = false;

		const auto information = wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_MESSAGE_BOX, wxSize(16, 16));
		const auto warning = wxArtProvider::GetBitmap(wxART_WARNING, wxART_MESSAGE_BOX, wxSize(16, 16));
		for (auto& item : m_optionStatus)
		{
			const auto& spec = Settings::Get(item.setting);
			const std::wstring name(spec.name);
			auto available = [&](const RegistryConfig& config)
			{
				DWORD number{}; std::wstring text;
				return spec.type == Settings::ValueType::Dword
					? config.TryGetDword(name, number) : config.TryGetString(name, text);
			};
			const auto notice = GetEditorRegistryNotice(m_editScope == Settings::Scope::User,
				available(*m_userConfig), available(*m_systemConfig));
			const bool show = notice != EditorRegistryNotice::None;
			if (show)
			{
				const bool overridden = notice == EditorRegistryNotice::Overridden;
				item.icon->SetBitmap(overridden ? warning : information);
				item.icon->SetToolTip(overridden
					? L"The current user's HKCU setting takes precedence. This control shows and edits HKLM only."
					: L"Not configured in HKCU; HKLM is inherited at runtime. This control shows the local default and edits HKCU only.");
			}
			if (item.icon->IsShown() != show)
			{
				item.icon->Show(show);
				item.icon->GetParent()->Layout();
				needLayout = true;
			}
		}

		if (needLayout)
		{
			if (m_glassColorsPanel)
			{
				LayoutWrappingPage(m_glassColorsPanel);
			}
			Layout();
			Refresh();
		}
	}

	void MainFrame::AddPathWarningIcon(wxWindow* parent, wxBoxSizer* row, wxFilePickerCtrl* picker, wxCheckBox* checkbox, const wxString& title)
	{
		if (!parent || !row || !picker)
		{
			return;
		}

		const wxSize iconSize(16, 16);
		const wxBitmap warnBmp = wxArtProvider::GetBitmap(wxART_WARNING, wxART_MESSAGE_BOX, iconSize);
		auto* warn = new wxStaticBitmap(parent, wxID_ANY, warnBmp);
		warn->SetMinSize(iconSize);
		warn->SetToolTip(title + L" path does not exist. The value will still be saved.");
		warn->Hide();

		row->Add(warn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxLEFT, 2);

		m_pathWarnings.push_back({ picker, checkbox, warn });
	}

	void MainFrame::UpdatePathWarningIcons()
	{
		bool needLayout = false;
		std::vector<wxWindow*> parents;
		auto addParent = [&parents](wxWindow* parent) {
			if (!parent)
			{
				return;
			}
			if (std::find(parents.begin(), parents.end(), parent) == parents.end())
			{
				parents.push_back(parent);
			}
		};
		for (auto& item : m_pathWarnings)
		{
			if (!item.icon || !item.picker)
			{
				continue;
			}
			const bool enabled = !item.checkbox || item.checkbox->IsChecked();
			const wxString path = item.picker->GetPath();
			if (!item.initialized || enabled != item.lastEnabled || path != item.lastPath)
			{
				item.lastEnabled = enabled;
				item.lastPath = path;
				item.lastExists = enabled && !path.empty() && wxFileExists(path);
				item.initialized = true;
			}
			const bool showWarn = enabled && !path.empty() && !item.lastExists;
			if (item.icon->IsShown() != showWarn)
			{
				item.icon->Show(showWarn);
				needLayout = true;
				addParent(item.icon->GetParent());
			}
		}
		if (needLayout)
		{
			for (wxWindow* parent : parents)
			{
				if (!parent)
				{
					continue;
				}
				parent->Layout();
				if (wxScrolledWindow* scrolled = wxDynamicCast(parent, wxScrolledWindow))
				{
					scrolled->FitInside();
				}
			}
			Layout();
			Refresh();
		}
	}

	void MainFrame::BindEvents()
	{
		Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
		Bind(wxEVT_ACTIVATE, [this](wxActivateEvent& event)
		{
			if (event.GetActive()) QueueColorizationRefresh();
			event.Skip();
		});
		Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
			const int keyCode = e.GetKeyCode();
			if ((e.ControlDown() && (keyCode == 'S' || keyCode == 's')) || keyCode == WXK_F2)
			{
				if (m_isDirty)
				{
					SaveSettings();
					return;
				}
				e.Skip();
				return;
			}
			e.Skip();
		});

		auto updateDword = [this](Settings::Id id, DWORD val) {
			const auto& spec = Settings::Get(id);
			const std::wstring name(spec.name);
			if (spec.type != Settings::ValueType::Dword)
			{
				ReportRegistryError(E_INVALIDARG, name);
				return;
			}
			RegistryConfig* config = m_config.get();
			if (!config)
			{
				return;
			}
			RunPreview([&]
			{
				TrackSettingChange(id);
				THROW_IF_FAILED(config->SetDword(name, val));
			}, spec.impact);
		};
		auto colorToDwordBgr = [](const wxColour& c) -> DWORD {
			return (c.Red()) | (c.Green() << 8) | (c.Blue() << 16);
		};
		auto hasDword = [this](Settings::Id id) {
			const std::wstring key(Settings::Get(id).name);
			RegistryConfig* config = m_config.get();
			DWORD value{};
			return config && config->TryGetDword(key, value);
		};

		auto syncSliderTooltip = [](wxSlider* slider) {
			if (slider)
			{
				slider->SetToolTip(wxString::Format(L"%d", slider->GetValue()));
			}
		};
		auto setSliderTooltipValue = [](wxSlider* slider, int value) {
			if (slider)
			{
				slider->SetToolTip(wxString::Format(L"%d", value));
			}
		};

		auto updateInheritance = [this, hasDword, syncSliderTooltip]() {
			if (!m_config)
			{
				return;
			}
			DWORD captionActive = m_config->GetDword(L"ColorizationColorCaption", 0xFFFFFFFD);
			DWORD captionInactive = m_config->GetDword(L"ColorizationColorCaptionInactive", captionActive);
			DWORD captionMaximized = m_config->GetDword(L"ColorizationColorCaptionMaximized", captionActive);
			DWORD captionInactiveMaximized = m_config->GetDword(L"ColorizationColorCaptionInactiveMaximized", captionInactive);

			if (!hasDword(Settings::Id::ColorizationColorCaptionInactive))
			{
				ApplyChoiceColorEx(m_chModeColorCaptionInactive, m_cpColorCaptionInactive, captionInactive, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
			}
			if (!hasDword(Settings::Id::ColorizationColorCaptionMaximized))
			{
				ApplyChoiceColorEx(m_chModeColorCaptionMaximized, m_cpColorCaptionMaximized, captionMaximized, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
			}
			if (!hasDword(Settings::Id::ColorizationColorCaptionInactiveMaximized))
			{
				ApplyChoiceColorEx(m_chModeColorCaptionInactiveMaximized, m_cpColorCaptionInactiveMaximized, captionInactiveMaximized, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
			}

			DWORD refOpacityActive = m_config->GetDword(L"ColorizationGlassReflectionOpacity", 0xFFFFFFFE);
			DWORD refOpacityInactive = m_config->GetDword(L"ColorizationGlassReflectionOpacityInactive", refOpacityActive);
			DWORD refOpacityMaximized = m_config->GetDword(L"ColorizationGlassReflectionOpacityMaximized", refOpacityActive);
			DWORD refOpacityInactiveMaximized = m_config->GetDword(L"ColorizationGlassReflectionOpacityInactiveMaximized", refOpacityInactive);

			if (!hasDword(Settings::Id::ColorizationGlassReflectionOpacityInactive))
			{
				ApplyChoiceSlider(m_chModeReflectionOpacityInactive, m_slReflectionOpacityInactive, refOpacityInactive, 0xFFFFFFFF, 0xFFFFFFFE, 50);
				syncSliderTooltip(m_slReflectionOpacityInactive);
			}
			if (!hasDword(Settings::Id::ColorizationGlassReflectionOpacityMaximized))
			{
				ApplyChoiceSlider(m_chModeReflectionOpacityMaximized, m_slReflectionOpacityMaximized, refOpacityMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 50);
				syncSliderTooltip(m_slReflectionOpacityMaximized);
			}
			if (!hasDword(Settings::Id::ColorizationGlassReflectionOpacityInactiveMaximized))
			{
				ApplyChoiceSlider(m_chModeReflectionOpacityInactiveMaximized, m_slReflectionOpacityInactiveMaximized, refOpacityInactiveMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 50);
				syncSliderTooltip(m_slReflectionOpacityInactiveMaximized);
			}

			DWORD colorOpacityActive = m_config->GetDword(L"ColorizationOpacity", 0xFFFFFFFE);
			DWORD colorOpacityInactive = m_config->GetDword(L"ColorizationOpacityInactive", colorOpacityActive);
			DWORD colorOpacityMaximized = m_config->GetDword(L"ColorizationOpacityMaximized", colorOpacityActive);
			DWORD colorOpacityInactiveMaximized = m_config->GetDword(L"ColorizationOpacityInactiveMaximized", colorOpacityInactive);

			if (!hasDword(Settings::Id::ColorizationOpacityInactive))
			{
				ApplyChoiceSlider(m_chModeColorizationOpacityInactive, m_slColorizationOpacityInactive, colorOpacityInactive, 0xFFFFFFFF, 0xFFFFFFFE, 100);
				syncSliderTooltip(m_slColorizationOpacityInactive);
			}
			if (!hasDword(Settings::Id::ColorizationOpacityMaximized))
			{
				ApplyChoiceSlider(m_chModeColorizationOpacityMaximized, m_slColorizationOpacityMaximized, colorOpacityMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 100);
				syncSliderTooltip(m_slColorizationOpacityMaximized);
			}
			if (!hasDword(Settings::Id::ColorizationOpacityInactiveMaximized))
			{
				ApplyChoiceSlider(m_chModeColorizationOpacityInactiveMaximized, m_slColorizationOpacityInactiveMaximized, colorOpacityInactiveMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 100);
				syncSliderTooltip(m_slColorizationOpacityInactiveMaximized);
			}
		};

		auto updateString = [this](Settings::Id id, const std::wstring& val) {
			const auto& spec = Settings::Get(id);
			const std::wstring name(spec.name);
			if (spec.type != Settings::ValueType::String)
			{
				ReportRegistryError(E_INVALIDARG, name);
				return;
			}
			RegistryConfig* config = m_config.get();
			if (!config)
			{
				return;
			}
			try
			{
				const auto prepared = !val.empty() && m_resources.NeedsImport(m_editScope, id, val) ? m_resources.PrepareFile(m_editScope, id, val) : nullptr;
				if (RunPreview([&]
				{
					if (prepared) m_resources.Install(*prepared);
					TrackSettingChange(id);
					THROW_IF_FAILED(config->SetString(name, prepared ? m_resources.Path(m_editScope, id).wstring() : val));
				}, spec.impact)) LoadSettings();
			}
			catch (...) { ReportRegistryError(wil::ResultFromCaughtException(), name); }
		};

		auto deleteValue = [this](Settings::Id id) {
			const std::wstring name(Settings::Get(id).name);
			RegistryConfig* config = m_config.get();
			if (!config)
			{
				return;
			}
			RunPreview([&]
			{
				TrackSettingChange(id);
				THROW_IF_FAILED(config->DeleteValue(name));
			}, Settings::Get(id).impact);
		};


		m_chkDisableGlassOnBattery->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			if (e.IsChecked())
			{
				deleteValue(Settings::Id::DisableGlassOnBattery);
			}
			else
			{
				updateDword(Settings::Id::DisableGlassOnBattery, 0);
			}
		});

		m_clDisabledHooks->Bind(wxEVT_CHECKLISTBOX, [this, updateDword, deleteValue](wxCommandEvent&) {
			DWORD mask = 0;
			if (m_clDisabledHooks->IsChecked(0)) mask |= 0x1;
			if (m_clDisabledHooks->IsChecked(1)) mask |= 0x2;
			if (m_clDisabledHooks->IsChecked(2)) mask |= 0x4;
			if (m_clDisabledHooks->IsChecked(3)) mask |= 0x8;
			if (m_clDisabledHooks->IsChecked(4)) mask |= 0x10;

			if (mask == 0)
			{
				deleteValue(Settings::Id::DisabledHooks);
			}
			else
			{
				updateDword(Settings::Id::DisabledHooks, mask);
			}
		});

		m_fpCustomThemeAtlas->Bind(wxEVT_FILEPICKER_CHANGED, [this, updateString](wxFileDirPickerEvent& e) {
			if (m_chkCustomThemeAtlas->IsChecked())
			{
				updateString(Settings::Id::CustomThemeAtlas, e.GetPath().ToStdWstring());
			}
			UpdatePathWarningIcons();
		});
		m_chkCustomThemeAtlas->Bind(wxEVT_CHECKBOX, [this, updateString, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			m_fpCustomThemeAtlas->Enable(checked);
			if (!checked)
			{
				m_fpCustomThemeAtlas->SetPath(wxEmptyString);
				deleteValue(Settings::Id::CustomThemeAtlas);
			}
			else
			{
				if (m_fpCustomThemeAtlas->GetPath().empty())
				{
					deleteValue(Settings::Id::CustomThemeAtlas);
					UpdatePathWarningIcons();
					return;
				}
				updateString(Settings::Id::CustomThemeAtlas, m_fpCustomThemeAtlas->GetPath().ToStdWstring());
			}
			UpdatePathWarningIcons();
		});

		m_fpCustomThemeReflection->Bind(wxEVT_FILEPICKER_CHANGED, [this, updateString](wxFileDirPickerEvent& e) {
			if (m_chkCustomThemeReflection->IsChecked())
			{
				updateString(Settings::Id::CustomThemeReflection, e.GetPath().ToStdWstring());
			}
			UpdatePathWarningIcons();
		});
		m_chkCustomThemeReflection->Bind(wxEVT_CHECKBOX, [this, updateString, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			m_fpCustomThemeReflection->Enable(checked);
			if (!checked)
			{
				m_fpCustomThemeReflection->SetPath(wxEmptyString);
				deleteValue(Settings::Id::CustomThemeReflection);
			}
			else
			{
				if (m_fpCustomThemeReflection->GetPath().empty())
				{
					deleteValue(Settings::Id::CustomThemeReflection);
					UpdatePathWarningIcons();
					return;
				}
				updateString(Settings::Id::CustomThemeReflection, m_fpCustomThemeReflection->GetPath().ToStdWstring());
			}
			UpdatePathWarningIcons();
		});

		m_slReflectionIntensity->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			int val = e.GetInt();
			if (val == 0)
			{
				deleteValue(Settings::Id::ColorizationGlassReflectionIntensity);
			}
			else
			{
				updateDword(Settings::Id::ColorizationGlassReflectionIntensity, val);
			}
			setSliderTooltipValue(m_slReflectionIntensity, val);
		});

		auto bindRefOpacity = [&](wxChoice* ch, wxSlider* sl, Settings::Id id, DWORD themeSentinel) {
			auto update = [ch, sl, id, themeSentinel, updateDword, deleteValue, updateInheritance]() {
				int sel = ch->GetSelection();
				sl->Enable(sel == 2);
				if (sel == 0) deleteValue(id);
				else if (sel == 1) updateDword(id, themeSentinel);
				else updateDword(id, sl->GetValue());
				sl->SetToolTip(wxString::Format(L"%d", sl->GetValue()));
				updateInheritance();
			};
			ch->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			sl->Bind(wxEVT_SLIDER, [update](wxCommandEvent&) { update(); });
		};

		bindRefOpacity(m_chModeReflectionOpacity, m_slReflectionOpacity, Settings::Id::ColorizationGlassReflectionOpacity, 0xFFFFFFFF);
		bindRefOpacity(m_chModeReflectionOpacityInactive, m_slReflectionOpacityInactive, Settings::Id::ColorizationGlassReflectionOpacityInactive, 0xFFFFFFFF);
		bindRefOpacity(m_chModeReflectionOpacityMaximized, m_slReflectionOpacityMaximized, Settings::Id::ColorizationGlassReflectionOpacityMaximized, 0xFFFFFFFF);
		bindRefOpacity(m_chModeReflectionOpacityInactiveMaximized, m_slReflectionOpacityInactiveMaximized, Settings::Id::ColorizationGlassReflectionOpacityInactiveMaximized, 0xFFFFFFFF);

		m_slReflectionParallax->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			int val = e.GetInt();
			if (val == 13)
			{
				deleteValue(Settings::Id::ColorizationGlassReflectionParallaxIntensity);
			}
			else
			{
				updateDword(Settings::Id::ColorizationGlassReflectionParallaxIntensity, val);
			}
			setSliderTooltipValue(m_slReflectionParallax, val);
		});

		auto updateReflectionPolicy = [this, updateDword, deleteValue]() {
			DWORD mask = 0;
			if (m_chkReflectionPolicyTitlebar && m_chkReflectionPolicyTitlebar->IsChecked()) mask |= (1 << 0);
			if (m_chkReflectionPolicyPeek && m_chkReflectionPolicyPeek->IsChecked()) mask |= (1 << 2);
			if (m_chkReflectionPolicySnap && m_chkReflectionPolicySnap->IsChecked()) mask |= (1 << 3);
			if ((mask & 0xD) == 0xD)
			{
				deleteValue(Settings::Id::ColorizationGlassReflectionPolicy);
			}
			else
			{
				updateDword(Settings::Id::ColorizationGlassReflectionPolicy, mask);
			}
		};
		if (m_chkReflectionPolicyTitlebar)
		{
			m_chkReflectionPolicyTitlebar->Bind(wxEVT_CHECKBOX, [updateReflectionPolicy](wxCommandEvent&) { updateReflectionPolicy(); });
		}
		if (m_chkReflectionPolicyPeek)
		{
			m_chkReflectionPolicyPeek->Bind(wxEVT_CHECKBOX, [updateReflectionPolicy](wxCommandEvent&) { updateReflectionPolicy(); });
		}
		if (m_chkReflectionPolicySnap)
		{
			m_chkReflectionPolicySnap->Bind(wxEVT_CHECKBOX, [updateReflectionPolicy](wxCommandEvent&) { updateReflectionPolicy(); });
		}

		m_fpCustomThemeMaterial->Bind(wxEVT_FILEPICKER_CHANGED, [this, updateString](wxFileDirPickerEvent& e) {
			if (m_chkCustomThemeMaterial->IsChecked())
			{
				updateString(Settings::Id::CustomThemeMaterial, e.GetPath().ToStdWstring());
			}
			UpdatePathWarningIcons();
		});
		m_chkCustomThemeMaterial->Bind(wxEVT_CHECKBOX, [this, updateString, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			m_fpCustomThemeMaterial->Enable(checked);
			if (!checked)
			{
				m_fpCustomThemeMaterial->SetPath(wxEmptyString);
				deleteValue(Settings::Id::CustomThemeMaterial);
			}
			else
			{
				if (m_fpCustomThemeMaterial->GetPath().empty())
				{
					deleteValue(Settings::Id::CustomThemeMaterial);
					UpdatePathWarningIcons();
					return;
				}
				updateString(Settings::Id::CustomThemeMaterial, m_fpCustomThemeMaterial->GetPath().ToStdWstring());
			}
			UpdatePathWarningIcons();
		});

		m_slMaterialOpacity->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			int val = e.GetInt();
			if (val == 0)
			{
				deleteValue(Settings::Id::MaterialOpacity);
			}
			else
			{
				updateDword(Settings::Id::MaterialOpacity, val);
			}
			setSliderTooltipValue(m_slMaterialOpacity, val);
		});
		m_slBlurAmount->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			const DWORD encodedDeviation = BlurSettings::EncodeGuiBlurAmount(e.GetInt());
			if (encodedDeviation == BlurSettings::DefaultEncodedDeviation)
			{
				deleteValue(Settings::Id::BlurDeviation);
			}
			else
			{
				updateDword(Settings::Id::BlurDeviation, encodedDeviation);
			}
			setSliderTooltipValue(m_slBlurAmount, e.GetInt());
		});
		m_chBlurOptimization->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				deleteValue(Settings::Id::BlurOptimization);
			}
			else
			{
				updateDword(Settings::Id::BlurOptimization, sel);
			}
		});
		auto updateD3DControls = [this]() {
			const bool enabled = !m_chkUseD3D->IsChecked();
			const DWORD encodedDeviation = m_config->GetDword(L"BlurDeviation", BlurSettings::DefaultEncodedDeviation);
			m_slBlurAmount->SetValue(BlurSettings::DecodeGuiBlurAmount(encodedDeviation));
			m_slBlurAmount->Enable(enabled);
			m_slBlurAmount->SetToolTip(wxString::Format(L"%d", m_slBlurAmount->GetValue()));

			const int blurOptimization = std::clamp<int>(m_config->GetDword(L"BlurOptimization", 0), 0, 2);
			m_chBlurOptimization->SetSelection(blurOptimization);
			m_chBlurOptimization->Enable(enabled);
		};

		m_chkUseD3D->Bind(wxEVT_CHECKBOX, [this, updateDword, updateD3DControls, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			if (!checked)
			{
				deleteValue(Settings::Id::UseDirect3DRendering);
			}
			else
			{
				updateDword(Settings::Id::UseDirect3DRendering, 1);
			}
			updateD3DControls();
		});
		m_chkGlassSafetyZone->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			if (checked)
			{
				updateDword(Settings::Id::GlassSafetyZoneMode, 0);
			}
			else
			{
				deleteValue(Settings::Id::GlassSafetyZoneMode);
			}
		});

		m_chRoundRectProfile->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				m_scRoundRectRadius->SetValue(0);
				m_scRoundRectRadius->Disable();
				deleteValue(Settings::Id::RoundRectRadius);
			}
			else if (sel == 1)
			{
				m_scRoundRectRadius->SetValue(6);
				m_scRoundRectRadius->Disable();
				updateDword(Settings::Id::RoundRectRadius, 6);
			}
			else
			{
				m_scRoundRectRadius->Enable();
				DWORD r = m_scRoundRectRadius->GetValue();
				if (r == 0)
				{
					deleteValue(Settings::Id::RoundRectRadius);
				}
				else
				{
					updateDword(Settings::Id::RoundRectRadius, r);
				}
			}
		});
		m_scRoundRectRadius->Bind(wxEVT_SPINCTRL, [this, updateDword, deleteValue](wxSpinEvent& e) {
			DWORD val = e.GetPosition();
			if (val == 0)
			{
				deleteValue(Settings::Id::RoundRectRadius);
			}
			else
			{
				updateDword(Settings::Id::RoundRectRadius, val);
			}
		});

		auto updateGlow = [this, updateDword, deleteValue]() {
			int mode = m_chTextGlowMode->GetSelection();
			int size = m_scTextGlowSize->GetValue();
			DWORD val = (DWORD)mode | ((DWORD)size << 16);
			if (val == 1)
			{
				deleteValue(Settings::Id::TextGlowMode);
			}
			else
			{
				updateDword(Settings::Id::TextGlowMode, val);
			}
		};

		m_chTextGlowMode->Bind(wxEVT_CHOICE, [this, updateGlow]([[maybe_unused]] wxCommandEvent& e) {
			m_scTextGlowSize->Enable(e.GetSelection() == 3);
			updateGlow();
		});
		m_scTextGlowSize->Bind(wxEVT_SPINCTRL, [this, updateGlow](wxSpinEvent&) {
			updateGlow();
		});

		m_chCaptionButtons->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				deleteValue(Settings::Id::CaptionButtons);
			}
			else
			{
				updateDword(Settings::Id::CaptionButtons, sel);
			}
		});
		m_chCenterCaption->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				deleteValue(Settings::Id::CenterCaption);
			}
			else
			{
				updateDword(Settings::Id::CenterCaption, sel);
			}
		});
		m_chkDisableModernBorders->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			if (!e.IsChecked())
			{
				deleteValue(Settings::Id::DisableModernBorders);
			}
			else
			{
				updateDword(Settings::Id::DisableModernBorders, 1);
			}
		});

		m_rbGlassType->Bind(wxEVT_RADIOBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				deleteValue(Settings::Id::GlassType);
			}
			else
			{
				updateDword(Settings::Id::GlassType, sel);
			}
			LoadSettings();
		});

		m_colorPresetList->Bind(wxEVT_COLOR_PRESET_INVOKED, [this](wxCommandEvent& event) {
			const auto index = event.GetInt();
			const auto family = m_rbGlassType->GetSelection() == 0
				? ColorizationPresets::Family::Vista
				: ColorizationPresets::Family::Windows7;
			if (index == 0)
			{
				ApplyColorizationColor(std::nullopt, family);
			}
			else if (const auto* preset = m_colorPresetList->GetPreset(index))
			{
				ApplyColorizationPreset(*preset);
			}
			else if (index == m_colorPresetList->GetCustomIndex())
			{
				ApplyColorizationColor(m_colorPresetList->GetCustomColor(), family);
			}
			UpdateColorizationPresetSelection();
		});

		m_colorPresetList->Bind(wxEVT_CUSTOM_COLOR_EDIT_REQUESTED, [this](wxCommandEvent&) {
			wxColourData colorData;
			colorData.SetChooseFull(true);
			colorData.SetChooseAlpha(false);
			const DWORD currentValue = m_colorPresetList->GetCustomColor();
			colorData.SetColour(wxColour(
				(currentValue >> 16) & 0xFF,
				(currentValue >> 8) & 0xFF,
				currentValue & 0xFF
			));

			wxColourDialog dialog(this, &colorData);
			if (dialog.ShowModal() == wxID_OK)
			{
				const wxColour selected = dialog.GetColourData().GetColour();
				const DWORD alpha = ColorizationPresets::CalculateIntensityAlpha(m_slColorIntensity->GetValue()) << 24;
				const DWORD argb = alpha
					| (static_cast<DWORD>(selected.Red()) << 16)
					| (static_cast<DWORD>(selected.Green()) << 8)
					| static_cast<DWORD>(selected.Blue());
				const auto family = m_rbGlassType->GetSelection() == 0
					? ColorizationPresets::Family::Vista
					: ColorizationPresets::Family::Windows7;
				m_colorPresetList->SetCustomColor(argb);
				ApplyColorizationColor(argb, family);
			}
			// Cancellation and failed operations restore the actual configuration's selection.
			UpdateColorizationPresetSelection();
		});

		m_chkEnableTransparency->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& e) {
			RunPreview([&]
			{
				TrackSettingChange(Settings::Id::ColorizationOpaqueBlend);
				if (e.IsChecked()) THROW_IF_FAILED(m_config->DeleteValue(L"ColorizationOpaqueBlend"));
				else THROW_IF_FAILED(m_config->SetDword(L"ColorizationOpaqueBlend", 1));
				if (m_rbGlassType->GetSelection() == 1) ApplyColorizationBalances(m_slColorIntensity->GetValue());
			}, Settings::UpdateImpact::Colorization);
		});

		m_slColorIntensity->Bind(wxEVT_SLIDER, [this](wxCommandEvent& e) {
			RunPreview([&]
			{
				const DWORD intensity = static_cast<DWORD>(e.GetInt());
				TrackSettingChange(Settings::Id::GlassOpacity);
				THROW_IF_FAILED(m_config->SetDword(L"GlassOpacity", intensity));
				if (m_rbGlassType->GetSelection() == 1) ApplyColorizationBalances(intensity);
			}, Settings::UpdateImpact::Colorization);
			m_slColorIntensity->SetToolTip(wxString::Format(L"%d", m_slColorIntensity->GetValue()));
		});

		auto bindChoiceColorEx = [&](wxChoice* ch, wxColourPickerCtrl* cp, Settings::Id id, DWORD themeSentinel, DWORD systemSentinel) {
			auto update = [ch, cp, id, themeSentinel, systemSentinel, updateDword, deleteValue, colorToDwordBgr, updateInheritance]() {
				int sel = ch->GetSelection();
				cp->Enable(sel == 2);
				if (sel == 0) deleteValue(id);
				else if (sel == 1) updateDword(id, themeSentinel);
				else if (sel == 3) updateDword(id, systemSentinel);
				else updateDword(id, colorToDwordBgr(cp->GetColour()));
				updateInheritance();
			};
			ch->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			cp->Bind(wxEVT_COLOURPICKER_CHANGED, [update](wxColourPickerEvent&) { update(); });
		};

		bindChoiceColorEx(m_chModeColorCaption, m_cpColorCaption, Settings::Id::ColorizationColorCaption, 0xFFFFFFFE, 0xFFFFFFFF);
		bindChoiceColorEx(m_chModeColorCaptionInactive, m_cpColorCaptionInactive, Settings::Id::ColorizationColorCaptionInactive, 0xFFFFFFFE, 0xFFFFFFFF);
		bindChoiceColorEx(m_chModeColorCaptionMaximized, m_cpColorCaptionMaximized, Settings::Id::ColorizationColorCaptionMaximized, 0xFFFFFFFE, 0xFFFFFFFF);
		bindChoiceColorEx(m_chModeColorCaptionInactiveMaximized, m_cpColorCaptionInactiveMaximized, Settings::Id::ColorizationColorCaptionInactiveMaximized, 0xFFFFFFFE, 0xFFFFFFFF);

		auto bindBaseColor = [updateDword, deleteValue](wxChoice* choice, wxColourPickerCtrl* picker, wxSpinCtrl* alphaSpin, Settings::Id id, DWORD themeVal) {
			auto update = [choice, picker, alphaSpin, id, themeVal, updateDword, deleteValue]() {
				int sel = choice->GetSelection();
				if (sel == 0)
				{
					picker->Disable();
					alphaSpin->Disable();
					deleteValue(id);
				}
				else if (sel == 1)
				{
					picker->Disable();
					alphaSpin->Disable();
					updateDword(id, themeVal);
				}
				else
				{
					picker->Enable();
					alphaSpin->Enable();
					wxColour c = picker->GetColour();
					int a = alphaSpin->GetValue();
					DWORD val = (a << 24) | (c.Red() << 16) | (c.Green() << 8) | c.Blue();
					updateDword(id, val);
				}
			};

			choice->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			picker->Bind(wxEVT_COLOURPICKER_CHANGED, [update](wxColourPickerEvent&) { update(); });
			alphaSpin->Bind(wxEVT_SPINCTRL, [update](wxSpinEvent&) { update(); });
			alphaSpin->Bind(wxEVT_TEXT, [update](wxCommandEvent&) { update(); });
		};

		bindBaseColor(m_chModeBaseTransparent, m_cpBaseTransparent, m_scBaseTransparentAlpha, Settings::Id::ColorizationBaseTransparent, 0xFFFFFFFF);
		bindBaseColor(m_chModeBaseMaximized, m_cpBaseMaximized, m_scBaseMaximizedAlpha, Settings::Id::ColorizationBaseMaximized, 0xFFFFFFFF);
		bindBaseColor(m_chModeBaseOpaque, m_cpBaseOpaque, m_scBaseOpaqueAlpha, Settings::Id::ColorizationBaseOpaque, 0xFFFFFFFF);

		m_chOpaqueBlendPriority->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue](wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 2)
			{
				deleteValue(Settings::Id::ColorizationOpaqueBlendPriority);
			}
			else
			{
				updateDword(Settings::Id::ColorizationOpaqueBlendPriority, sel);
			}
		});

		auto bindOpacity = [updateDword, deleteValue, updateInheritance](wxChoice* ch, wxSlider* sl, Settings::Id id, DWORD themeSentinel) {
			auto update = [ch, sl, id, themeSentinel, updateDword, deleteValue, updateInheritance]() {
				int sel = ch->GetSelection();
				sl->Enable(sel == 2);
				if (sel == 0) {
					deleteValue(id);
				}
				else if (sel == 1) updateDword(id, themeSentinel);
				else updateDword(id, sl->GetValue());
				sl->SetToolTip(wxString::Format(L"%d", sl->GetValue()));
				updateInheritance();
			};
			ch->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			sl->Bind(wxEVT_SLIDER, [update](wxCommandEvent&) { update(); });
		};

		bindOpacity(m_chModeColorizationOpacity, m_slColorizationOpacity, Settings::Id::ColorizationOpacity, 0xFFFFFFFF);
		bindOpacity(m_chModeColorizationOpacityInactive, m_slColorizationOpacityInactive, Settings::Id::ColorizationOpacityInactive, 0xFFFFFFFF);
		bindOpacity(m_chModeColorizationOpacityMaximized, m_slColorizationOpacityMaximized, Settings::Id::ColorizationOpacityMaximized, 0xFFFFFFFF);
		bindOpacity(m_chModeColorizationOpacityInactiveMaximized, m_slColorizationOpacityInactiveMaximized, Settings::Id::ColorizationOpacityInactiveMaximized, 0xFFFFFFFF);

		m_chkGlassOverrideAccent->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue](wxCommandEvent& e) {
			if (!e.IsChecked())
			{
				deleteValue(Settings::Id::GlassOverrideAccent);
			}
			else
			{
				updateDword(Settings::Id::GlassOverrideAccent, 1);
			}
		});

		m_btnExportAtlas->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			wxFileDialog saveDialog(this, L"Save atlas file", L"", L"theme.png", L"PNG files (*.png)|*.png", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
			if (saveDialog.ShowModal() != wxID_OK) return;

			WCHAR themeFileName[MAX_PATH]{};
			if (FAILED(GetCurrentThemeName(themeFileName, MAX_PATH, nullptr, 0, nullptr, 0)))
			{
				wxMessageBox(L"Failed to get current system theme name.", L"Export Failed", wxICON_ERROR);
				return;
			}

			wil::unique_hmodule themeResource{ LoadLibraryExW(themeFileName, nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_SEARCH_SYSTEM32) };
			if (!themeResource)
			{
				wxMessageBox(L"Failed to load msstyle.", L"Export Failed", wxICON_ERROR);
				return;
			}

			HTHEME hTheme = OpenThemeData(nullptr, L"DWMWindow");
			if (!hTheme)
			{
				wxMessageBox(L"Failed to open DWMWindow theme data.", L"Export Failed", wxICON_ERROR);
				return;
			}
			auto closeTheme = wil::scope_exit([&] { CloseThemeData(hTheme); });

			VOID* streamAddress = nullptr;
			DWORD streamSize = 0;

			if (FAILED(GetThemeStream(hTheme, 0, 0, TMT_DISKSTREAM, &streamAddress, &streamSize, themeResource.get())))
			{
				wxMessageBox(L"Failed to retrieve theme stream (Atlas).", L"Export Failed", wxICON_ERROR);
				return;
			}

			if (streamSize == 0 || !streamAddress)
			{
				wxMessageBox(L"Retrieved atlas stream is empty.", L"Export Failed", wxICON_ERROR);
				return;
			}

			{
				wil::unique_hfile file{ CreateFileW(saveDialog.GetPath().wc_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };
				if (!file)
				{
					wxMessageBox(L"Failed to create output file.", L"Export Failed", wxICON_ERROR);
					return;
				}

				DWORD bytesWritten = 0;
				if (!WriteFile(file.get(), streamAddress, streamSize, &bytesWritten, nullptr) || bytesWritten != streamSize)
				{
					wxMessageBox(L"Failed to write all data to file.", L"Export Failed", wxICON_ERROR);
					return;
				}
			}

			wxString layoutPath = saveDialog.GetPath() + L".layout";
			wil::unique_hfile layoutFile{ CreateFileW(layoutPath.wc_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };

			if (layoutFile)
			{
				std::string layoutContent;
				char buffer[256];

				layoutContent += "# Rects\n";
				layoutContent += "# 8002 = TMT_ATLASRECT\n";
				for (int iPartId = 1; iPartId < 100; ++iPartId)
				{
					RECT rc;
					if (SUCCEEDED(GetThemeRect(hTheme, iPartId, 0, TMT_ATLASRECT, &rc)))
					{
						int len = sprintf_s(buffer, "%u;0;8002=%ld,%ld,%ld,%ld\n",
							iPartId, rc.left, rc.top, rc.right, rc.bottom);
						layoutContent.append(buffer, len);
					}
				}

				layoutContent += "\n# Margins\n";
				layoutContent += "# 3601 = TMT_SIZINGMARGINS\n";
				layoutContent += "# 3602 = TMT_CONTENTMARGINS\n";
				for (int i = 1; i < 100; ++i)
				{
					MARGINS mg;
					if (SUCCEEDED(GetThemeMargins(hTheme, nullptr, i, 0, TMT_SIZINGMARGINS, nullptr, &mg)))
					{
						int len = sprintf_s(buffer, "%u;0;3601=%d,%d,%d,%d\n",
							i, mg.cxLeftWidth, mg.cxRightWidth, mg.cyTopHeight, mg.cyBottomHeight);
						layoutContent.append(buffer, len);
					}

					if (SUCCEEDED(GetThemeMargins(hTheme, nullptr, i, 0, TMT_CONTENTMARGINS, nullptr, &mg)))
					{
						int len = sprintf_s(buffer, "%u;0;3602=%d,%d,%d,%d\n",
							i, mg.cxLeftWidth, mg.cxRightWidth, mg.cyTopHeight, mg.cyBottomHeight);
						layoutContent.append(buffer, len);
					}
				}

				if (!layoutContent.empty())
				{
					DWORD bytesWritten = 0;
					WriteFile(layoutFile.get(), layoutContent.c_str(), static_cast<DWORD>(layoutContent.size()), &bytesWritten, nullptr);
				}
			}

		});

		m_btnDownloadSymbols->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			StartSymbolDownload();
		});

		m_btnRefreshTransparencyDiagnostics->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			RefreshTransparencyDiagnostics();
		});

		m_btnCancelSymbolDownload->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			if (!m_symbolDownloadRunning)
			{
				return;
			}

			m_symbolDownloadThread.request_stop();
			m_btnCancelSymbolDownload->Enable(false);
			UpdateSymbolDownloadProgress(SymbolDownloadProgress{
				m_gaugeSymbolDownload ? m_gaugeSymbolDownload->GetValue() : 0,
				true,
				L"Cancelling symbol download...",
				L"Waiting for the current network operation to stop."
			});
		});

		m_btnEnableDwmCrashDumps->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			SetDwmCrashDumpsEnabled(true);
		});

		m_btnDisableDwmCrashDumps->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			SetDwmCrashDumpsEnabled(false);
		});

		m_btnSave->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			SaveSettings();
		});

		m_btnRevert->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			RevertSettings();
		});
	}

	void MainFrame::OnClose(wxCloseEvent& event)
	{
		if (!m_config)
		{
			event.Skip();
			return;
		}
		if (m_symbolDownloadRunning)
		{
			m_closeWhenSymbolDownloadStops = true;
			m_symbolDownloadThread.request_stop();
			if (m_btnCancelSymbolDownload)
			{
				m_btnCancelSymbolDownload->Enable(false);
			}
			UpdateSymbolDownloadProgress(SymbolDownloadProgress{
				m_gaugeSymbolDownload ? m_gaugeSymbolDownload->GetValue() : 0,
				true,
				L"Cancelling symbol download before closing...",
				L"The window will close after the current network request finishes or times out."
			});
			event.Veto();
			return;
		}
		if ((m_isDirty || m_colorPreference.IsDirty()) && !RevertSettings())
		{
			event.Veto();
			return;
		}
		event.Skip();
	}
}
