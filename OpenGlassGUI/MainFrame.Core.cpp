#include "pch.h"
#include "MainFrame.hpp"
#include "Symbols.hpp"

namespace OpenGlass
{
	namespace
	{
		void WrapStaticTextToParentWidth(wxStaticText* label, const wxString& sourceText, int rightPadding = 8)
		{
			if (!label)
			{
				return;
			}

			wxWindow* parent = label->GetParent();
			if (!parent)
			{
				return;
			}

			const int availableWidth = std::max(1, parent->GetClientSize().GetWidth() - label->GetPosition().x - rightPadding);
			label->SetLabel(sourceText);
			label->Wrap(availableWidth);
		}
	}

	class SelectionDialog : public wxDialog
	{
	public:
		SelectionDialog(wxWindow* parent, bool isAdmin)
			: wxDialog(parent, wxID_ANY, L"Aero Glass for Win10+", wxDefaultPosition, wxDefaultSize)
		{
			if (isAdmin)
			{
				SetTitle(GetTitle() + L" (Administrator)");
			}
			SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK));
			wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

			// Header area
			wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
			wxStaticBitmap* icon = new wxStaticBitmap(this, wxID_ANY, wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_MESSAGE_BOX));
			headerSizer->Add(icon, 0, wxALL | wxALIGN_TOP, 20);

			wxBoxSizer* textSizer = new wxBoxSizer(wxVERTICAL);
			wxStaticText* title = new wxStaticText(this, wxID_ANY, L"Select Configuration Scope");
			wxFont font = title->GetFont();
			font.SetWeight(wxFONTWEIGHT_BOLD);
			font.SetPointSize(font.GetPointSize() + 2);
			title->SetFont(font);

			wxStaticText* subtitle = new wxStaticText(this, wxID_ANY, L"Whose settings would you like to modify?");

			textSizer->Add(title, 0, wxBOTTOM, 5);
			textSizer->Add(subtitle, 0, wxBOTTOM, 0);
			headerSizer->Add(textSizer, 1, wxALL | wxALIGN_CENTER_VERTICAL, 20);

			mainSizer->Add(headerSizer, 0, wxEXPAND | wxALL, 0);
			mainSizer->Add(new wxStaticLine(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 0);

			// Buttons
			wxBoxSizer* contentSizer = new wxBoxSizer(wxVERTICAL);

			auto* btnUser = new wxCommandLinkButton(this, wxID_NO, L"Current User", L"Modify settings for the current user (HKCU)");
			auto* btnSystem = new wxCommandLinkButton(this, wxID_YES, L"System-wide", L"Modify global settings (HKLM)");

			if (!isAdmin)
			{
				btnSystem->SetNote(L"Requires Administrator privileges (Not detected)");
				btnSystem->Disable();
			}

			contentSizer->Add(btnUser, 0, wxEXPAND | wxBOTTOM, 10);
			contentSizer->Add(btnSystem, 0, wxEXPAND | wxBOTTOM, 0);

			mainSizer->Add(contentSizer, 1, wxEXPAND | wxALL, 20);

			// Bottom cancel
			wxBoxSizer* dlgBtns = new wxBoxSizer(wxHORIZONTAL);
			dlgBtns->AddStretchSpacer();
			dlgBtns->Add(new wxButton(this, wxID_CANCEL, L"Cancel"), 0, wxALL, 0);

			mainSizer->Add(dlgBtns, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

			SetSizerAndFit(mainSizer);
			Centre();

			// Bindings
			btnUser->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_NO); });
			btnSystem->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_YES); });
			Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); }, wxID_CANCEL);
		}
	};

	MainFrame::MainFrame(const wxString& title)
		: wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(900, 750))
	{
		bool isAdmin = ::IsUserAnAdmin();
		m_isAdmin = isAdmin;

		if (isAdmin)
		{
			SetTitle(title + L" (Administrator)");
		}
		m_baseTitle = GetTitle();

		SelectionDialog dialog(nullptr, isAdmin);
		int result = dialog.ShowModal();

		if (result == wxID_CANCEL)
		{
			m_initCanceled = true;
			return;
		}

		bool useHklm = (result == wxID_YES);
		m_config = std::make_unique<RegistryConfig>(useHklm);
		m_systemConfig = std::make_unique<RegistryConfig>(true);
		m_scopeLabel = useHklm ? LR"(HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\DWM)" : LR"(HKEY_CURRENT_USER\Software\Microsoft\Windows\DWM)";

		SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_FRAMEBK));

		wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
		SetSizer(mainSizer);

		CreateControls();
		CreateBottomControls(mainSizer);
		BindEvents();
		LoadSettings(true);
		SetDirty(false);
		UpdateStatusBar();

		Centre();
	}

	void MainFrame::CreateControls()
	{
		m_notebook = new wxNotebook(this, wxID_ANY);

		CreateSystemTab();
		CreateThemeTab();
		CreateAppearanceTab();
		CreateGlassColorsTab();
		CreateDiagnosticsTab();

		m_notebook->SetSelection(2);

		GetSizer()->Add(m_notebook, 1, wxEXPAND | wxALL, 5);
		CreateStatusBar();
	}

	void MainFrame::CreateBottomControls(wxSizer* parentSizer)
	{
		wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);

		m_btnSave = new wxButton(this, wxID_ANY, L"Save");
		m_btnRevert = new wxButton(this, wxID_ANY, L"Revert");
		m_btnSave->Enable(false);
		m_btnRevert->Enable(false);
		m_btnSave->SetToolTip(L"Save changes (Ctrl+S)");
		m_btnRevert->SetToolTip(L"Revert changes (Esc)");

		btnSizer->AddStretchSpacer();
		btnSizer->Add(m_btnSave, 0, wxRIGHT, 5);
		btnSizer->Add(m_btnRevert, 0);

		parentSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 5);
	}

	void MainFrame::NotifySettingsChange(ChangeType type)
	{
		if (!m_dwmWindow || !IsWindow(m_dwmWindow))
		{
			m_dwmWindow = FindWindowW(L"Dwm", nullptr);
		}
		HWND notificationWindow = m_dwmWindow;
		if (notificationWindow)
		{
			if (type == ChangeType::Colorization || type == ChangeType::Both)
				SendNotifyMessage(notificationWindow, WM_DWMCOLORIZATIONCOLORCHANGED, 0, 0);

			if (type == ChangeType::Theme || type == ChangeType::Both)
				SendNotifyMessage(notificationWindow, WM_THEMECHANGED, 0, 0);
		}
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
			UpdateStatusBar();
			return;
		}
		m_isDirty = dirty;
		UpdateWindowTitle();
		UpdateStatusBar();
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

	void MainFrame::UpdateStatusBar()
	{
		if (!GetStatusBar())
		{
			return;
		}
		SetStatusText(m_scopeLabel);
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
		UpdateSymbolDownloadResult(wxART_INFORMATION, wxEmptyString, wxEmptyString);
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
		WrapStaticTextToParentWidth(m_lblSymbolDownloadDetail, m_symbolDownloadDetailText);
		WrapStaticTextToParentWidth(m_lblSymbolDownloadResult, m_symbolDownloadResultText);

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

		if (m_lblSymbolDownloadPhase)
		{
			if (m_lblSymbolDownloadPhase->GetLabelText() != progress.phase)
			{
				m_lblSymbolDownloadPhase->SetLabel(progress.phase);
			}
		}
		if (m_lblSymbolDownloadDetail)
		{
			if (m_symbolDownloadDetailText != progress.detail)
			{
				m_symbolDownloadDetailText = progress.detail;
				WrapStaticTextToParentWidth(m_lblSymbolDownloadDetail, m_symbolDownloadDetailText);
			}
		}
	}

	void MainFrame::UpdateSymbolDownloadResult(wxArtID iconId, const wxString& summary, const wxString& details)
	{
		if (!m_pnlSymbolDownloadResult || !m_bmpSymbolDownloadResult || !m_lblSymbolDownloadResult)
		{
			return;
		}

		if (summary.empty() && details.empty())
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

		wxString message = summary;
		if (!details.empty())
		{
			if (!message.empty())
			{
				message += L"\n\n";
			}
			message += details;
		}

		m_symbolDownloadResultText = message;
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
			UpdateSymbolDownloadResult(wxART_INFORMATION, wxEmptyString, wxEmptyString);
			break;
		case SymbolDownloadResult::Cancelled:
			UpdateSymbolDownloadProgress(SymbolDownloadProgress{
				m_gaugeSymbolDownload ? m_gaugeSymbolDownload->GetValue() : 0,
				false,
				L"Symbol download cancelled.",
				L"No further network requests will be started."
			});
			UpdateSymbolDownloadResult(wxART_WARNING, wxEmptyString, outcome.details);
			break;
		case SymbolDownloadResult::Failed:
		default:
			UpdateSymbolDownloadProgress(SymbolDownloadProgress{
				m_gaugeSymbolDownload ? m_gaugeSymbolDownload->GetValue() : 0,
				false,
				L"Symbol download failed.",
				outcome.summary
			});
			UpdateSymbolDownloadResult(wxART_ERROR, wxEmptyString, outcome.details);
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

	bool MainFrame::IsSystemSettingKey(const std::wstring& key) const
	{
		return key == L"DisableGlassOnBattery"
			|| key == L"DisabledHooks"
			|| key == L"GlassSafetyZoneMode";
	}

	RegistryConfig* MainFrame::GetConfigForKey(const std::wstring& key) const
	{
		if (IsSystemSettingKey(key) && m_systemConfig)
		{
			return m_systemConfig.get();
		}
		return m_config.get();
	}

	ResolvedRegistryValue<DWORD> MainFrame::ResolveOverridableDword(
		const std::wstring& key,
		const std::wstring& overrideKey,
		DWORD defaultValue
	) const
	{
		auto query = [](const RegistryConfig* config, const std::wstring& name) -> std::optional<DWORD>
		{
			DWORD value{};
			if (config && config->TryGetDword(name, value))
			{
				return value;
			}
			return std::nullopt;
		};

		std::optional<DWORD> userOverride;
		std::optional<DWORD> userBase;
		const RegistryConfig* machineConfig = m_systemConfig.get();
		if (m_config && !m_config->IsHklm())
		{
			userOverride = query(m_config.get(), overrideKey);
			userBase = query(m_config.get(), key);
		}
		else if (m_config)
		{
			machineConfig = m_config.get();
		}

		return ResolveOverridableRegistryValue(
			userOverride,
			userBase,
			query(machineConfig, overrideKey),
			query(machineConfig, key),
			defaultValue
		);
	}

	void MainFrame::ResetOverridableDword(const std::wstring& key, const std::wstring& overrideKey)
	{
		RegistryConfig* config = GetConfigForKey(key);
		if (!config || !config->HasValue(overrideKey))
		{
			return;
		}

		TrackSettingChange(overrideKey);
		config->DeleteValue(overrideKey);
		SetDirty(true);
		NotifySettingsChange(ChangeType::Colorization);
		LoadSettings(false);
	}

	void MainFrame::BackupCurrentSetting(const std::wstring& name)
	{
		RegistryConfig* config = GetConfigForKey(name);
		if (!config || !config->HasValue(name))
		{
			m_backupSettings[name] = std::monostate{};
			return;
		}

		std::wstring strVal = config->GetString(name, L"__MISSING__");
		if (strVal != L"__MISSING__")
		{
			m_backupSettings[name] = strVal;
			return;
		}

		m_backupSettings[name] = config->GetDword(name, 0);
	}

	void MainFrame::TrackSettingChange(const std::wstring& name)
	{
		if (name.empty())
		{
			return;
		}
		if (m_dirtyKeys.insert(name).second)
		{
			BackupCurrentSetting(name);
		}
	}

	void MainFrame::SaveSettings()
	{
		if (!m_config)
		{
			return;
		}
		for (const auto& key : m_dirtyKeys)
		{
			BackupCurrentSetting(key);
		}
		m_dirtyKeys.clear();
		SetDirty(false);
		UpdateOptionStatusIcons();
		UpdatePathWarningIcons();
	}

	void MainFrame::RevertSettings()
	{
		if (!m_config)
		{
			return;
		}
		if (m_dirtyKeys.empty())
		{
			SetDirty(false);
			return;
		}
		for (const auto& key : m_dirtyKeys)
		{
			if (!m_isAdmin && IsSystemSettingKey(key))
			{
				continue;
			}
			auto it = m_backupSettings.find(key);
			if (it == m_backupSettings.end())
			{
				continue;
			}
			const auto& val = it->second;
			RegistryConfig* config = GetConfigForKey(key);
			if (!config)
			{
				continue;
			}
			if (std::holds_alternative<std::monostate>(val))
			{
				config->DeleteValue(key);
			}
			else if (std::holds_alternative<DWORD>(val))
			{
				config->SetDword(key, std::get<DWORD>(val));
			}
			else if (std::holds_alternative<std::wstring>(val))
			{
				config->SetString(key, std::get<std::wstring>(val));
			}
		}
		NotifySettingsChange();
		LoadSettings(false);
		m_dirtyKeys.clear();
		SetDirty(false);
	}

	void MainFrame::AddProperty(wxWindow* parent, wxSizer* sizer, const wxString& label, wxWindow* control, const std::wstring& key, const std::wstring& overrideKey)
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
		if (!key.empty())
		{
			AddOptionStatus(parent, row, key, overrideKey);
		}
		sizer->Add(row, 0, wxEXPAND | wxALL, 2);
	}

	void MainFrame::AddOptionStatus(wxWindow* parent, wxBoxSizer* row, const std::wstring& key, const std::wstring& overrideKey)
	{
		if (!parent || !row)
		{
			return;
		}

		const wxSize iconSize(16, 16);
		const wxBitmap infoBmp = wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_MESSAGE_BOX, iconSize);
		const wxBitmap warnBmp = wxArtProvider::GetBitmap(wxART_WARNING, wxART_MESSAGE_BOX, iconSize);
		auto* info = new wxStaticBitmap(parent, wxID_ANY, infoBmp);
		wxButton* reset = nullptr;
		if (!overrideKey.empty())
		{
			reset = new wxButton(parent, wxID_ANY, L"↶", wxDefaultPosition, wxSize(28, -1), wxBU_EXACTFIT);
			reset->SetName(L"Reset Override");
			reset->SetToolTip(L"Remove the Override value in the current scope and use the next inherited value.");
			reset->Hide();
			reset->Bind(wxEVT_BUTTON, [this, key, overrideKey](wxCommandEvent&)
			{
				ResetOverridableDword(key, overrideKey);
			});
		}
		auto* warn = new wxStaticBitmap(parent, wxID_ANY, warnBmp);
		info->SetMinSize(iconSize);
		warn->SetMinSize(iconSize);
		info->SetToolTip(L"Effective value is coming from an Override key.");
		warn->SetToolTip(L"You are editing HKLM, but HKCU has a value. \nHKCU takes precedence.");
		info->Hide();
		warn->Hide();

		row->Add(info, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxLEFT, 2);
		if (reset)
		{
			row->Add(reset, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxLEFT, 2);
		}
		row->Add(warn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxLEFT, 2);

		const bool vistaIrrelevant = key.find(L"Afterglow") != std::wstring::npos
			|| key.find(L"Balance") != std::wstring::npos;
		const bool win7Irrelevant = key == L"GlassOpacityInactive"
			|| key == L"ColorizationColorInactive"
			|| key == L"GlassOpacity";
		m_optionStatus.push_back({ info, reset, warn, key, overrideKey, vistaIrrelevant, win7Irrelevant });
	}

	void MainFrame::UpdateOptionStatusIcons()
	{
		if (!m_config)
		{
			return;
		}

		RegistryConfig userConfig(false);
		bool needLayout = false;

		const bool isVista = (m_rbGlassType && m_rbGlassType->GetSelection() == 0);
		const bool hasUserKey = userConfig.HasKey();
		std::unordered_map<std::wstring, bool> hkcuHasValueCache;
		std::unordered_map<std::wstring, bool> activeHasValueCache;

		for (auto& item : m_optionStatus)
		{
			// Check relevancy to current Mode
			bool isRelevant = true;
			if (isVista)
			{
				if (item.vistaIrrelevant)
				{
					isRelevant = false;
				}
			}
			else
			{
				if (item.win7Irrelevant)
				{
					isRelevant = false;
				}
			}
			if (!isRelevant)
			{
				if (item.overrideIcon && item.overrideIcon->IsShown())
				{
					item.overrideIcon->Show(false);
					needLayout = true;
				}
				if (item.resetOverrideButton && item.resetOverrideButton->IsShown())
				{
					item.resetOverrideButton->Show(false);
					needLayout = true;
				}
				if (item.hkcuWarnIcon && item.hkcuWarnIcon->IsShown())
				{
					item.hkcuWarnIcon->Show(false);
					needLayout = true;
				}
				continue;
			}

			RegistryConfig* activeConfig = GetConfigForKey(item.key);
			if (!activeConfig)
			{
				if (item.overrideIcon && item.overrideIcon->IsShown())
				{
					item.overrideIcon->Show(false);
					needLayout = true;
				}
				if (item.hkcuWarnIcon && item.hkcuWarnIcon->IsShown())
				{
					item.hkcuWarnIcon->Show(false);
					needLayout = true;
				}
				continue;
			}

			bool selectedOverrideExists = false;
			if (!item.overrideKey.empty())
			{
				auto it = activeHasValueCache.find(item.overrideKey);
				if (it == activeHasValueCache.end())
				{
					selectedOverrideExists = activeConfig->HasValue(item.overrideKey);
					activeHasValueCache.emplace(item.overrideKey, selectedOverrideExists);
				}
				else
				{
					selectedOverrideExists = it->second;
				}
			}
			const bool editingHklm = activeConfig->IsHklm();
			bool hkcuHasValue = false;
			if (editingHklm && hasUserKey)
			{
				auto hasCachedUserValue = [&](const std::wstring& name)
				{
					auto it = hkcuHasValueCache.find(name);
					if (it != hkcuHasValueCache.end())
					{
						return it->second;
					}
					const bool exists = userConfig.HasValue(name);
					hkcuHasValueCache.emplace(name, exists);
					return exists;
				};
				hkcuHasValue = hasCachedUserValue(item.key)
					|| (!item.overrideKey.empty() && hasCachedUserValue(item.overrideKey));
			}

			if (item.overrideIcon)
			{
				bool show = false;
				if (!item.overrideKey.empty())
				{
					const auto resolved = ResolveOverridableDword(item.key, item.overrideKey, 0);
					show = isRelevant && resolved.IsOverride();
					if (resolved.source == RegistryValueSource::MachineOverride && !editingHklm)
					{
						item.overrideIcon->SetToolTip(L"Effective value is inherited from an HKLM Override key.");
					}
					else
					{
						item.overrideIcon->SetToolTip(L"Effective value is coming from an Override key in the current scope.");
					}
				}
				if (item.key == L"ColorizationColorInactive")
				{
					show = false;
				}
				if (item.overrideIcon->IsShown() != show)
				{
					item.overrideIcon->Show(show);
					needLayout = true;
				}
			}
			if (item.resetOverrideButton)
			{
				const bool show = isRelevant && selectedOverrideExists;
				if (item.resetOverrideButton->IsShown() != show)
				{
					item.resetOverrideButton->Show(show);
					needLayout = true;
				}
			}
			if (item.hkcuWarnIcon)
			{
				bool show = isRelevant && hkcuHasValue;
				if (item.hkcuWarnIcon->IsShown() != show)
				{
					item.hkcuWarnIcon->Show(show);
					needLayout = true;
				}
			}
		}

		if (needLayout)
		{
			if (m_glassColorsPanel)
			{
				m_glassColorsPanel->Layout();
				if (wxScrolledWindow* scrolled = wxDynamicCast(m_glassColorsPanel, wxScrolledWindow))
				{
					scrolled->FitInside();
				}
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

		m_pathWarnings.push_back({ picker, checkbox, warn, title, wxString(), false, false, false });
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
			if (keyCode == WXK_ESCAPE)
			{
				if (m_isDirty)
				{
					RevertSettings();
					return;
				}
				e.Skip();
				return;
			}
			e.Skip();
		});

		auto updateDword = [this](const std::wstring& name, DWORD val, ChangeType type = ChangeType::Both) {
			RegistryConfig* config = GetConfigForKey(name);
			if (!config)
			{
				return;
			}
			TrackSettingChange(name);
			config->SetDword(name, val);
			SetDirty(true);
			NotifySettingsChange(type);
		};
		auto updateOverridableDword = [this, updateDword](const std::wstring& overrideKey, DWORD value) {
			updateDword(overrideKey, value, ChangeType::Colorization);
			UpdateOptionStatusIcons();
		};
		auto blurRadiusToDeviation = [](int radius) -> DWORD {
			int val = (radius * 10 + 2) / 3;
			if (val < 0) val = 0;
			if (val > 100) val = 100;
			return static_cast<DWORD>(val);
		};
		auto blurDeviationToRadius = [](DWORD deviation) -> int {
			int val = (static_cast<int>(deviation) * 3 + 5) / 10;
			if (val < 0) val = 0;
			if (val > 30) val = 30;
			return val;
		};

		auto colorToDwordWithAlpha = [](const wxColour& c) -> DWORD {
			return (c.Alpha() << 24) | (c.Red() << 16) | (c.Green() << 8) | c.Blue();
		};
		auto colorToDwordBgr = [](const wxColour& c) -> DWORD {
			return (c.Red()) | (c.Green() << 8) | (c.Blue() << 16);
		};
		auto colorToDwordIgnoreAlpha = [this](const std::wstring& key, const wxColour& c) -> DWORD {
			DWORD alpha = 0;
			RegistryConfig* config = GetConfigForKey(key);
			if (config && config->HasValue(key))
			{
				alpha = (config->GetDword(key, 0) >> 24) & 0xFF;
			}
			return (alpha << 24) | (c.Red() << 16) | (c.Green() << 8) | c.Blue();
		};

		auto hasValue = [this](const std::wstring& key) {
			RegistryConfig* config = GetConfigForKey(key);
			return config && config->HasValue(key);
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

		auto updateInheritance = [this, hasValue, syncSliderTooltip]() {
			if (!m_config)
			{
				return;
			}
			if (!m_chkEnableInactiveColor->IsChecked())
			{
				m_cpColorizationColorInactive->SetColour(m_cpColorizationColor->GetColour());
			}

			if (m_chkEnableInactiveOpacity && !m_chkEnableInactiveOpacity->IsChecked())
			{
				m_slGlassOpacityInactive->SetValue(m_slGlassOpacity->GetValue());
				syncSliderTooltip(m_slGlassOpacityInactive);
			}

			DWORD captionActive = m_config->GetDword(L"ColorizationColorCaption", 0xFFFFFFFD);
			DWORD captionInactive = hasValue(L"ColorizationColorCaptionInactive")
				? m_config->GetDword(L"ColorizationColorCaptionInactive", captionActive)
				: captionActive;
			DWORD captionMaximized = hasValue(L"ColorizationColorCaptionMaximized")
				? m_config->GetDword(L"ColorizationColorCaptionMaximized", captionActive)
				: captionActive;
			DWORD captionInactiveMaximized = hasValue(L"ColorizationColorCaptionInactiveMaximized")
				? m_config->GetDword(L"ColorizationColorCaptionInactiveMaximized", captionInactive)
				: captionInactive;

			if (!hasValue(L"ColorizationColorCaptionInactive"))
			{
				ApplyChoiceColorEx(m_chModeColorCaptionInactive, m_cpColorCaptionInactive, captionInactive, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
			}
			if (!hasValue(L"ColorizationColorCaptionMaximized"))
			{
				ApplyChoiceColorEx(m_chModeColorCaptionMaximized, m_cpColorCaptionMaximized, captionMaximized, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
			}
			if (!hasValue(L"ColorizationColorCaptionInactiveMaximized"))
			{
				ApplyChoiceColorEx(m_chModeColorCaptionInactiveMaximized, m_cpColorCaptionInactiveMaximized, captionInactiveMaximized, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
			}

			DWORD refOpacityActive = m_config->GetDword(L"ColorizationGlassReflectionOpacity", 0xFFFFFFFE);
			DWORD refOpacityInactive = hasValue(L"ColorizationGlassReflectionOpacityInactive")
				? m_config->GetDword(L"ColorizationGlassReflectionOpacityInactive", refOpacityActive)
				: refOpacityActive;
			DWORD refOpacityMaximized = hasValue(L"ColorizationGlassReflectionOpacityMaximized")
				? m_config->GetDword(L"ColorizationGlassReflectionOpacityMaximized", refOpacityActive)
				: refOpacityActive;
			DWORD refOpacityInactiveMaximized = hasValue(L"ColorizationGlassReflectionOpacityInactiveMaximized")
				? m_config->GetDword(L"ColorizationGlassReflectionOpacityInactiveMaximized", refOpacityInactive)
				: refOpacityInactive;

			if (!hasValue(L"ColorizationGlassReflectionOpacityInactive"))
			{
				ApplyChoiceSlider(m_chModeReflectionOpacityInactive, m_slReflectionOpacityInactive, refOpacityInactive, 0xFFFFFFFF, 0xFFFFFFFE, 50);
				syncSliderTooltip(m_slReflectionOpacityInactive);
			}
			if (!hasValue(L"ColorizationGlassReflectionOpacityMaximized"))
			{
				ApplyChoiceSlider(m_chModeReflectionOpacityMaximized, m_slReflectionOpacityMaximized, refOpacityMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 50);
				syncSliderTooltip(m_slReflectionOpacityMaximized);
			}
			if (!hasValue(L"ColorizationGlassReflectionOpacityInactiveMaximized"))
			{
				ApplyChoiceSlider(m_chModeReflectionOpacityInactiveMaximized, m_slReflectionOpacityInactiveMaximized, refOpacityInactiveMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 50);
				syncSliderTooltip(m_slReflectionOpacityInactiveMaximized);
			}

			DWORD colorOpacityActive = m_config->GetDword(L"ColorizationOpacity", 0xFFFFFFFE);
			DWORD colorOpacityInactive = hasValue(L"ColorizationOpacityInactive")
				? m_config->GetDword(L"ColorizationOpacityInactive", colorOpacityActive)
				: colorOpacityActive;
			DWORD colorOpacityMaximized = hasValue(L"ColorizationOpacityMaximized")
				? m_config->GetDword(L"ColorizationOpacityMaximized", colorOpacityActive)
				: colorOpacityActive;
			DWORD colorOpacityInactiveMaximized = hasValue(L"ColorizationOpacityInactiveMaximized")
				? m_config->GetDword(L"ColorizationOpacityInactiveMaximized", colorOpacityInactive)
				: colorOpacityInactive;

			if (!hasValue(L"ColorizationOpacityInactive"))
			{
				ApplyChoiceSlider(m_chModeColorizationOpacityInactive, m_slColorizationOpacityInactive, colorOpacityInactive, 0xFFFFFFFF, 0xFFFFFFFE, 100);
				syncSliderTooltip(m_slColorizationOpacityInactive);
			}
			if (!hasValue(L"ColorizationOpacityMaximized"))
			{
				ApplyChoiceSlider(m_chModeColorizationOpacityMaximized, m_slColorizationOpacityMaximized, colorOpacityMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 100);
				syncSliderTooltip(m_slColorizationOpacityMaximized);
			}
			if (!hasValue(L"ColorizationOpacityInactiveMaximized"))
			{
				ApplyChoiceSlider(m_chModeColorizationOpacityInactiveMaximized, m_slColorizationOpacityInactiveMaximized, colorOpacityInactiveMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 100);
				syncSliderTooltip(m_slColorizationOpacityInactiveMaximized);
			}
		};

		auto updateString = [this](const std::wstring& name, const std::wstring& val, ChangeType type = ChangeType::Both) {
			RegistryConfig* config = GetConfigForKey(name);
			if (!config)
			{
				return;
			}
			TrackSettingChange(name);
			config->SetString(name, val);
			SetDirty(true);
			NotifySettingsChange(type);
		};

		auto deleteValue = [this](const std::wstring& name) {
			RegistryConfig* config = GetConfigForKey(name);
			if (!config)
			{
				return;
			}
			TrackSettingChange(name);
			config->DeleteValue(name);
			SetDirty(true);
		};

		auto restorePickerPath = [this](wxFilePickerCtrl* picker, const std::wstring& key) {
			picker->SetPath(m_config->GetString(key, L""));
		};

		auto ensureFilePath = []([[maybe_unused]] const wxString& path, [[maybe_unused]] const wxString& title) -> bool {
			return true;
		};

		m_chkDisableGlassOnBattery->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			if (e.IsChecked())
			{
				deleteValue(L"DisableGlassOnBattery");
				NotifySettingsChange();
			}
			else
			{
				updateDword(L"DisableGlassOnBattery", 0);
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
				deleteValue(L"DisabledHooks");
				NotifySettingsChange();
			}
			else
			{
				updateDword(L"DisabledHooks", mask);
			}
		});

		m_fpCustomThemeAtlas->Bind(wxEVT_FILEPICKER_CHANGED, [this, updateString, ensureFilePath, restorePickerPath](wxFileDirPickerEvent& e) {
			if (m_chkCustomThemeAtlas->IsChecked())
			{
				if (!ensureFilePath(e.GetPath(), L"Theme atlas"))
				{
					restorePickerPath(m_fpCustomThemeAtlas, L"CustomThemeAtlas");
					return;
				}
				updateString(L"CustomThemeAtlas", e.GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});
		m_chkCustomThemeAtlas->Bind(wxEVT_CHECKBOX, [this, updateString, ensureFilePath, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			m_fpCustomThemeAtlas->Enable(checked);
			if (!checked)
			{
				m_fpCustomThemeAtlas->SetPath(wxEmptyString);
				deleteValue(L"CustomThemeAtlas");
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				if (m_fpCustomThemeAtlas->GetPath().empty())
				{
					deleteValue(L"CustomThemeAtlas");
					NotifySettingsChange(ChangeType::Theme);
					UpdatePathWarningIcons();
					return;
				}
				if (!ensureFilePath(m_fpCustomThemeAtlas->GetPath(), L"Theme atlas"))
				{
					m_chkCustomThemeAtlas->SetValue(false);
					m_fpCustomThemeAtlas->Enable(false);
					return;
				}
				updateString(L"CustomThemeAtlas", m_fpCustomThemeAtlas->GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});

		m_fpCustomThemeReflection->Bind(wxEVT_FILEPICKER_CHANGED, [this, updateString, ensureFilePath, restorePickerPath](wxFileDirPickerEvent& e) {
			if (m_chkCustomThemeReflection->IsChecked())
			{
				if (!ensureFilePath(e.GetPath(), L"Reflection texture"))
				{
					restorePickerPath(m_fpCustomThemeReflection, L"CustomThemeReflection");
					return;
				}
				updateString(L"CustomThemeReflection", e.GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});
		m_chkCustomThemeReflection->Bind(wxEVT_CHECKBOX, [this, updateString, ensureFilePath, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			m_fpCustomThemeReflection->Enable(checked);
			if (!checked)
			{
				m_fpCustomThemeReflection->SetPath(wxEmptyString);
				deleteValue(L"CustomThemeReflection");
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				if (m_fpCustomThemeReflection->GetPath().empty())
				{
					deleteValue(L"CustomThemeReflection");
					NotifySettingsChange(ChangeType::Theme);
					UpdatePathWarningIcons();
					return;
				}
				if (!ensureFilePath(m_fpCustomThemeReflection->GetPath(), L"Reflection texture"))
				{
					m_chkCustomThemeReflection->SetValue(false);
					m_fpCustomThemeReflection->Enable(false);
					return;
				}
				updateString(L"CustomThemeReflection", m_fpCustomThemeReflection->GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});

		m_slReflectionIntensity->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			int val = e.GetInt();
			if (val == 0)
			{
				deleteValue(L"ColorizationGlassReflectionIntensity");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"ColorizationGlassReflectionIntensity", val, ChangeType::Colorization);
			}
			setSliderTooltipValue(m_slReflectionIntensity, val);
		});

		auto bindRefOpacity = [&](wxChoice* ch, wxSlider* sl, const std::wstring& key, DWORD themeSentinel, DWORD autoSentinel, bool propagateInheritance) {
			auto update = [this, ch, sl, key, themeSentinel, autoSentinel, updateDword, deleteValue, propagateInheritance, updateInheritance]() {
				int sel = ch->GetSelection();
				sl->Enable(sel == 2);
				if (sel == 0) deleteValue(key);
				else if (sel == 1) updateDword(key, themeSentinel, ChangeType::Colorization);
				else updateDword(key, sl->GetValue(), ChangeType::Colorization);
				sl->SetToolTip(wxString::Format(L"%d", sl->GetValue()));
				NotifySettingsChange(ChangeType::Colorization);
				if (propagateInheritance)
				{
					updateInheritance();
				}
			};
			ch->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			sl->Bind(wxEVT_SLIDER, [update](wxCommandEvent&) { update(); });
		};

		bindRefOpacity(m_chModeReflectionOpacity, m_slReflectionOpacity, L"ColorizationGlassReflectionOpacity", 0xFFFFFFFF, 0xFFFFFFFE, true);
		bindRefOpacity(m_chModeReflectionOpacityInactive, m_slReflectionOpacityInactive, L"ColorizationGlassReflectionOpacityInactive", 0xFFFFFFFF, 0xFFFFFFFE, true);
		bindRefOpacity(m_chModeReflectionOpacityMaximized, m_slReflectionOpacityMaximized, L"ColorizationGlassReflectionOpacityMaximized", 0xFFFFFFFF, 0xFFFFFFFE, false);
		bindRefOpacity(m_chModeReflectionOpacityInactiveMaximized, m_slReflectionOpacityInactiveMaximized, L"ColorizationGlassReflectionOpacityInactiveMaximized", 0xFFFFFFFF, 0xFFFFFFFE, false);

		m_slReflectionParallax->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			int val = e.GetInt();
			if (val == 13)
			{
				deleteValue(L"ColorizationGlassReflectionParallaxIntensity");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"ColorizationGlassReflectionParallaxIntensity", val, ChangeType::Colorization);
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
				deleteValue(L"ColorizationGlassReflectionPolicy");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"ColorizationGlassReflectionPolicy", mask, ChangeType::Colorization);
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
		// Legacy (kept for reference):
		// m_clReflectionPolicy->Bind(wxEVT_CHECKLISTBOX, [this, updateDword, deleteValue](wxCommandEvent&) {
		// 	DWORD mask = 0;
		// 	if (m_clReflectionPolicy->IsChecked(0)) mask |= (1 << 0);
		// 	if (m_clReflectionPolicy->IsChecked(1)) mask |= (1 << 2);
		// 	if (m_clReflectionPolicy->IsChecked(2)) mask |= (1 << 3);
		// 	if ((mask & 0xD) == 0xD)
		// 	{
		// 		deleteValue(L"ColorizationGlassReflectionPolicy");
		// 		NotifySettingsChange(ChangeType::Colorization);
		// 	}
		// 	else
		// 	{
		// 		updateDword(L"ColorizationGlassReflectionPolicy", mask, ChangeType::Colorization);
		// 	}
		// });

		m_fpCustomThemeMaterial->Bind(wxEVT_FILEPICKER_CHANGED, [this, updateString, ensureFilePath, restorePickerPath](wxFileDirPickerEvent& e) {
			if (m_chkCustomThemeMaterial->IsChecked())
			{
				if (!ensureFilePath(e.GetPath(), L"Material texture"))
				{
					restorePickerPath(m_fpCustomThemeMaterial, L"CustomThemeMaterial");
					return;
				}
				updateString(L"CustomThemeMaterial", e.GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});
		m_chkCustomThemeMaterial->Bind(wxEVT_CHECKBOX, [this, updateString, ensureFilePath, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			m_fpCustomThemeMaterial->Enable(checked);
			if (!checked)
			{
				m_fpCustomThemeMaterial->SetPath(wxEmptyString);
				deleteValue(L"CustomThemeMaterial");
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				if (m_fpCustomThemeMaterial->GetPath().empty())
				{
					deleteValue(L"CustomThemeMaterial");
					NotifySettingsChange(ChangeType::Theme);
					UpdatePathWarningIcons();
					return;
				}
				if (!ensureFilePath(m_fpCustomThemeMaterial->GetPath(), L"Material texture"))
				{
					m_chkCustomThemeMaterial->SetValue(false);
					m_fpCustomThemeMaterial->Enable(false);
					return;
				}
				updateString(L"CustomThemeMaterial", m_fpCustomThemeMaterial->GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});

		m_slMaterialOpacity->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			int val = e.GetInt();
			if (val == 0)
			{
				deleteValue(L"MaterialOpacity");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"MaterialOpacity", val, ChangeType::Colorization);
			}
			setSliderTooltipValue(m_slMaterialOpacity, val);
		});
		m_slBlurDeviation->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, blurRadiusToDeviation, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			DWORD val = blurRadiusToDeviation(e.GetInt());
			if (val == 30)
			{
				deleteValue(L"BlurDeviation");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"BlurDeviation", val, ChangeType::Colorization);
			}
			setSliderTooltipValue(m_slBlurDeviation, e.GetInt());
		});
		m_chBlurOptimization->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				deleteValue(L"BlurOptimization");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"BlurOptimization", sel, ChangeType::Colorization);
			}
		});
		auto updateD3DControls = [this, blurDeviationToRadius]() {
			if (m_chkUseD3D->IsChecked())
			{
				m_slBlurDeviation->SetValue(3);
				m_slBlurDeviation->Enable(false);
				m_slBlurDeviation->SetToolTip(wxString::Format(L"%d", m_slBlurDeviation->GetValue()));

				m_chBlurOptimization->SetSelection(0);
				m_chBlurOptimization->Enable(false);
			}
			else
			{
				m_slBlurDeviation->Enable(true);
				DWORD blurDeviation = m_config->GetDword(L"BlurDeviation", 30);
				m_slBlurDeviation->SetValue(blurDeviationToRadius(blurDeviation));
				m_slBlurDeviation->SetToolTip(wxString::Format(L"%d", m_slBlurDeviation->GetValue()));

				m_chBlurOptimization->Enable(true);
				int blurOpt = static_cast<int>(m_config->GetDword(L"BlurOptimization", 0));
				if (blurOpt < 0) blurOpt = 0;
				if (blurOpt > 2) blurOpt = 2;
				m_chBlurOptimization->SetSelection(blurOpt);
			}
		};

		m_chkUseD3D->Bind(wxEVT_CHECKBOX, [this, updateDword, updateD3DControls, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			if (!checked)
			{
				deleteValue(L"UseDirect3DRendering");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"UseDirect3DRendering", 1, ChangeType::Colorization);
			}
			updateD3DControls();
		});
		m_chkGlassSafetyZone->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			if (checked)
			{
				updateDword(L"GlassSafetyZoneMode", 0, ChangeType::Colorization);
			}
			else
			{
				deleteValue(L"GlassSafetyZoneMode");
				NotifySettingsChange(ChangeType::Colorization);
			}
		});

		m_chRoundRectProfile->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				m_scRoundRectRadius->SetValue(0);
				m_scRoundRectRadius->Disable();
				deleteValue(L"RoundRectRadius");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else if (sel == 1)
			{
				m_scRoundRectRadius->SetValue(6);
				m_scRoundRectRadius->Disable();
				updateDword(L"RoundRectRadius", 6, ChangeType::Colorization);
			}
			else
			{
				m_scRoundRectRadius->Enable();
				DWORD r = m_scRoundRectRadius->GetValue();
				if (r == 0)
				{
					deleteValue(L"RoundRectRadius");
					NotifySettingsChange(ChangeType::Colorization);
				}
				else
				{
					updateDword(L"RoundRectRadius", r, ChangeType::Colorization);
				}
			}
		});
		m_scRoundRectRadius->Bind(wxEVT_SPINCTRL, [this, updateDword, deleteValue](wxSpinEvent& e) {
			DWORD val = e.GetPosition();
			if (val == 0)
			{
				deleteValue(L"RoundRectRadius");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"RoundRectRadius", val, ChangeType::Colorization);
			}
		});

		auto updateGlow = [this, updateDword, deleteValue]() {
			int mode = m_chTextGlowMode->GetSelection();
			int size = m_scTextGlowSize->GetValue();
			DWORD val = (DWORD)mode | ((DWORD)size << 16);
			if (val == 1)
			{
				deleteValue(L"TextGlowMode");
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				updateDword(L"TextGlowMode", val, ChangeType::Theme);
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
				deleteValue(L"CaptionButtons");
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				updateDword(L"CaptionButtons", sel);
			}
		});
		m_chCenterCaption->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				deleteValue(L"CenterCaption");
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				updateDword(L"CenterCaption", sel, ChangeType::Theme);
			}
		});
		m_chkDisableModernBorders->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			if (!e.IsChecked())
			{
				deleteValue(L"DisableModernBorders");
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				updateDword(L"DisableModernBorders", 1, ChangeType::Theme);
			}
		});

		m_rbGlassType->Bind(wxEVT_RADIOBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				deleteValue(L"GlassType");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"GlassType", sel, ChangeType::Colorization);
			}
			UpdateUIVisibility();
		});

		m_chkOpaqueBlend->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			if (!e.IsChecked())
			{
				deleteValue(L"ColorizationOpaqueBlend");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"ColorizationOpaqueBlend", 1, ChangeType::Colorization);
			}
		});

		m_cpColorizationColor->Bind(wxEVT_COLOURPICKER_CHANGED, [this, updateOverridableDword, updateInheritance](wxColourPickerEvent& e) {
			const DWORD currentValue = ResolveOverridableDword(
				L"ColorizationColor",
				L"ColorizationColorOverride",
				0xFF000000
			).value;
			const wxColour color = e.GetColour();
			const DWORD value = (currentValue & 0xFF000000)
				| (color.Red() << 16)
				| (color.Green() << 8)
				| color.Blue();
			updateOverridableDword(L"ColorizationColorOverride", value);
			updateInheritance();
		});

		m_chkEnableInactiveColor->Bind(wxEVT_CHECKBOX, [this, updateDword, updateInheritance, colorToDwordIgnoreAlpha, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool enabled = e.IsChecked();
			m_cpColorizationColorInactive->Enable(enabled);
			if (enabled) {
				updateDword(L"ColorizationColorInactive", colorToDwordIgnoreAlpha(L"ColorizationColorInactive", m_cpColorizationColorInactive->GetColour()), ChangeType::Colorization);
			} else {
				deleteValue(L"ColorizationColorInactive");
				NotifySettingsChange(ChangeType::Colorization);
				updateInheritance();
			}
		});

		m_cpColorizationColorInactive->Bind(wxEVT_COLOURPICKER_CHANGED, [this, updateDword, colorToDwordIgnoreAlpha](wxColourPickerEvent& e) {
			if (m_chkEnableInactiveColor->IsChecked())
				updateDword(L"ColorizationColorInactive", colorToDwordIgnoreAlpha(L"ColorizationColorInactive", e.GetColour()), ChangeType::Colorization);
		});

		m_slGlassOpacity->Bind(wxEVT_SLIDER, [this, updateDword, updateInheritance, deleteValue, setSliderTooltipValue](wxCommandEvent& e) {
			int val = e.GetInt();
			if (val == 63)
			{
				deleteValue(L"GlassOpacity");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"GlassOpacity", val, ChangeType::Colorization);
			}
			setSliderTooltipValue(m_slGlassOpacity, val);
			updateInheritance();
		});

		m_chkEnableInactiveOpacity->Bind(wxEVT_CHECKBOX, [this, updateDword, updateInheritance, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool enabled = e.IsChecked();
			m_slGlassOpacityInactive->Enable(enabled);
			if (enabled)
			{
				updateDword(L"GlassOpacityInactive", m_slGlassOpacityInactive->GetValue(), ChangeType::Colorization);
			}
			else
			{
				deleteValue(L"GlassOpacityInactive");
				NotifySettingsChange(ChangeType::Colorization);
				updateInheritance();
			}
		});

		m_slGlassOpacityInactive->Bind(wxEVT_SLIDER, [this, updateDword, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			if (!m_chkEnableInactiveOpacity->IsChecked())
			{
				return;
			}
			int val = e.GetInt();
			setSliderTooltipValue(m_slGlassOpacityInactive, val);
			updateDword(L"GlassOpacityInactive", val, ChangeType::Colorization);
		});

		auto bindChoiceColor = [&](wxChoice* ch, wxColourPickerCtrl* cp, const std::wstring& key, DWORD themeSentinel, DWORD autoSentinel, bool propagateInheritance) {
			auto update = [this, ch, cp, key, themeSentinel, autoSentinel, updateDword, deleteValue, colorToDwordWithAlpha, propagateInheritance, updateInheritance]() {
				int sel = ch->GetSelection();
				cp->Enable(sel == 2);
				if (sel == 0) deleteValue(key);
				else if (sel == 1) updateDword(key, themeSentinel, ChangeType::Colorization);
				else updateDword(key, colorToDwordWithAlpha(cp->GetColour()), ChangeType::Colorization);
				if (sel == 0) NotifySettingsChange(ChangeType::Colorization);
				if (propagateInheritance)
				{
					updateInheritance();
				}
			};
			ch->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			cp->Bind(wxEVT_COLOURPICKER_CHANGED, [update](wxColourPickerEvent&) { update(); });
		};
		auto bindChoiceColorEx = [&](wxChoice* ch, wxColourPickerCtrl* cp, const std::wstring& key, DWORD themeSentinel, DWORD autoSentinel, DWORD systemSentinel, bool propagateInheritance) {
			auto update = [this, ch, cp, key, themeSentinel, autoSentinel, systemSentinel, updateDword, deleteValue, colorToDwordBgr, propagateInheritance, updateInheritance]() {
				int sel = ch->GetSelection();
				cp->Enable(sel == 2);
				if (sel == 0) deleteValue(key);
				else if (sel == 1) updateDword(key, themeSentinel, ChangeType::Colorization);
				else if (sel == 3) updateDword(key, systemSentinel, ChangeType::Colorization);
				else updateDword(key, colorToDwordBgr(cp->GetColour()), ChangeType::Colorization);
				if (sel == 0) NotifySettingsChange(ChangeType::Colorization);
				if (propagateInheritance)
				{
					updateInheritance();
				}
			};
			ch->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			cp->Bind(wxEVT_COLOURPICKER_CHANGED, [update](wxColourPickerEvent&) { update(); });
		};

		bindChoiceColorEx(m_chModeColorCaption, m_cpColorCaption, L"ColorizationColorCaption", 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF, true);
		bindChoiceColorEx(m_chModeColorCaptionInactive, m_cpColorCaptionInactive, L"ColorizationColorCaptionInactive", 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF, true);
		bindChoiceColorEx(m_chModeColorCaptionMaximized, m_cpColorCaptionMaximized, L"ColorizationColorCaptionMaximized", 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF, false);
		bindChoiceColorEx(m_chModeColorCaptionInactiveMaximized, m_cpColorCaptionInactiveMaximized, L"ColorizationColorCaptionInactiveMaximized", 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF, false);

		auto bindBaseColor = [this, updateDword, deleteValue](wxChoice* choice, wxColourPickerCtrl* picker, wxSpinCtrl* alphaSpin, const std::wstring& keyName, DWORD themeVal, DWORD autoVal) {
			auto update = [this, choice, picker, alphaSpin, keyName, autoVal, themeVal, updateDword, deleteValue]() {
				int sel = choice->GetSelection();
				if (sel == 0)
				{
					picker->Disable();
					alphaSpin->Disable();
					deleteValue(keyName);
					NotifySettingsChange(ChangeType::Colorization);
				}
				else if (sel == 1)
				{
					picker->Disable();
					alphaSpin->Disable();
					updateDword(keyName, themeVal, ChangeType::Colorization);
				}
				else
				{
					picker->Enable();
					alphaSpin->Enable();
					wxColour c = picker->GetColour();
					int a = alphaSpin->GetValue();
					DWORD val = (a << 24) | (c.Red() << 16) | (c.Green() << 8) | c.Blue();
					updateDword(keyName, val, ChangeType::Colorization);
				}
			};

			choice->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			picker->Bind(wxEVT_COLOURPICKER_CHANGED, [update](wxColourPickerEvent&) { update(); });
			alphaSpin->Bind(wxEVT_SPINCTRL, [update](wxSpinEvent&) { update(); });
			alphaSpin->Bind(wxEVT_TEXT, [update](wxCommandEvent&) { update(); });
		};

		bindBaseColor(m_chModeBaseTransparent, m_cpBaseTransparent, m_scBaseTransparentAlpha, L"ColorizationBaseTransparent", 0xFFFFFFFF, 0xFFFFFFFE);
		bindBaseColor(m_chModeBaseMaximized, m_cpBaseMaximized, m_scBaseMaximizedAlpha, L"ColorizationBaseMaximized", 0xFFFFFFFF, 0xFFFFFFFE);
		bindBaseColor(m_chModeBaseOpaque, m_cpBaseOpaque, m_scBaseOpaqueAlpha, L"ColorizationBaseOpaque", 0xFFFFFFFF, 0xFFFFFFFE);

		m_chOpaqueBlendPriority->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue](wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 2)
			{
				deleteValue(L"ColorizationOpaqueBlendPriority");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"ColorizationOpaqueBlendPriority", sel, ChangeType::Colorization);
			}
		});

		auto bindOpacity = [this, updateDword, deleteValue, updateInheritance](wxChoice* ch, wxSlider* sl, const std::wstring& key, DWORD themeSentinel, DWORD autoSentinel, bool propagateInheritance) {
			auto update = [this, ch, sl, key, themeSentinel, autoSentinel, updateDword, deleteValue, propagateInheritance, updateInheritance]() {
				int sel = ch->GetSelection();
				sl->Enable(sel == 2);
				if (sel == 0) {
					deleteValue(key);
					NotifySettingsChange(ChangeType::Colorization);
				}
				else if (sel == 1) updateDword(key, themeSentinel, ChangeType::Colorization);
				else updateDword(key, sl->GetValue(), ChangeType::Colorization);
				sl->SetToolTip(wxString::Format(L"%d", sl->GetValue()));
				if (propagateInheritance)
				{
					updateInheritance();
				}
			};
			ch->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			sl->Bind(wxEVT_SLIDER, [update](wxCommandEvent&) { update(); });
		};

		bindOpacity(m_chModeColorizationOpacity, m_slColorizationOpacity, L"ColorizationOpacity", 0xFFFFFFFF, 0xFFFFFFFE, true);
		bindOpacity(m_chModeColorizationOpacityInactive, m_slColorizationOpacityInactive, L"ColorizationOpacityInactive", 0xFFFFFFFF, 0xFFFFFFFE, true);
		bindOpacity(m_chModeColorizationOpacityMaximized, m_slColorizationOpacityMaximized, L"ColorizationOpacityMaximized", 0xFFFFFFFF, 0xFFFFFFFE, false);
		bindOpacity(m_chModeColorizationOpacityInactiveMaximized, m_slColorizationOpacityInactiveMaximized, L"ColorizationOpacityInactiveMaximized", 0xFFFFFFFF, 0xFFFFFFFE, false);

		m_slBlurBalance->Bind(wxEVT_SLIDER, [this, updateOverridableDword, setSliderTooltipValue](wxCommandEvent& e) {
			updateOverridableDword(L"ColorizationBlurBalanceOverride", e.GetInt());
			setSliderTooltipValue(m_slBlurBalance, e.GetInt());
		});
		m_slAfterglowBalance->Bind(wxEVT_SLIDER, [this, updateOverridableDword, setSliderTooltipValue](wxCommandEvent& e) {
			updateOverridableDword(L"ColorizationAfterglowBalanceOverride", e.GetInt());
			setSliderTooltipValue(m_slAfterglowBalance, e.GetInt());
		});
		m_slColorBalance->Bind(wxEVT_SLIDER, [this, updateOverridableDword, setSliderTooltipValue](wxCommandEvent& e) {
			updateOverridableDword(L"ColorizationColorBalanceOverride", e.GetInt());
			setSliderTooltipValue(m_slColorBalance, e.GetInt());
		});
		m_cpAfterglow->Bind(wxEVT_COLOURPICKER_CHANGED, [this, updateOverridableDword](wxColourPickerEvent& e) {
			const DWORD currentValue = ResolveOverridableDword(
				L"ColorizationAfterglow",
				L"ColorizationAfterglowOverride",
				0
			).value;
			const wxColour color = e.GetColour();
			const DWORD value = (currentValue & 0xFF000000)
				| (color.Red() << 16)
				| (color.Green() << 8)
				| color.Blue();
			updateOverridableDword(L"ColorizationAfterglowOverride", value);
		});
		m_btnPersistCompositionParameters->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			RegistryConfig* config = GetConfigForKey(L"ColorizationColorBalance");
			if (!config)
			{
				return;
			}

			const std::pair<PCWSTR, int> values[]
			{
				{ L"ColorizationBlurBalanceOverride", m_slBlurBalance->GetValue() },
				{ L"ColorizationAfterglowBalanceOverride", m_slAfterglowBalance->GetValue() },
				{ L"ColorizationColorBalanceOverride", m_slColorBalance->GetValue() }
			};
			for (const auto& [name, value] : values)
			{
				TrackSettingChange(name);
				config->SetDword(name, static_cast<DWORD>(value));
			}
			SetDirty(true);
			NotifySettingsChange(ChangeType::Colorization);
			UpdateOptionStatusIcons();
		});

		m_chkGlassOverrideAccent->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue](wxCommandEvent& e) {
			if (!e.IsChecked())
			{
				deleteValue(L"GlassOverrideAccent");
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(L"GlassOverrideAccent", 1, ChangeType::Colorization);
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

			//wxMessageBox(L"System theme atlas exported successfully!\nA layout file was also generated.", L"Success", wxICON_INFORMATION);
		});

		m_btnDownloadSymbols->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			StartSymbolDownload();
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
		if (m_initCanceled || !m_config)
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
		RevertSettings();
		event.Skip();
	}
}
