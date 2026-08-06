#include "pch.h"
#include "MainFrame.hpp"
#include "ColorSwatchButton.hpp"
#include "Symbols.hpp"

namespace OpenGlass
{
	namespace
	{
		std::wstring FormatTargetUser(const std::wstring& sidText)
		{
			PSID sid{};
			if (!ConvertStringSidToSidW(sidText.c_str(), &sid)) return sidText;
			wil::unique_hlocal sidStorage{ sid };
			DWORD nameLength{}, domainLength{};
			SID_NAME_USE use{};
			LookupAccountSidW(nullptr, sid, nullptr, &nameLength, nullptr, &domainLength, &use);
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) return sidText;
			std::wstring name(nameLength, L'\0');
			std::wstring domain(domainLength, L'\0');
			if (!LookupAccountSidW(nullptr, sid, name.data(), &nameLength, domain.data(), &domainLength, &use)) return sidText;
			if (!name.empty() && name.back() == L'\0') name.pop_back();
			if (!domain.empty() && domain.back() == L'\0') domain.pop_back();
			return domain.empty() ? name : domain + L"\\" + name;
		}

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

	MainFrame::MainFrame(const wxString& title, std::wstring userSid)
		: wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(900, 750))
	{
		m_isAdmin = true;
		SetTitle(title + L" (Administrator)");
		m_baseTitle = GetTitle();
		m_config = std::make_unique<RegistryConfig>(RegistryConfig::Mode::Canonical, userSid);
		m_userConfig = std::make_unique<RegistryConfig>(RegistryConfig::Mode::User, userSid);
		m_systemConfig = std::make_unique<RegistryConfig>(RegistryConfig::Mode::Machine, userSid);
		m_targetUserLabel = FormatTargetUser(userSid);
		m_targetUserSid = userSid;

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
		CreatePresetsTab();
		CreateDiagnosticsTab();

		m_notebook->SetSelection(2);

		GetSizer()->Add(m_notebook, 1, wxEXPAND | wxALL, 5);
		auto* statusBar = CreateStatusBar();
		statusBar->SetToolTip(
			L"Windows colorization is stored for this user (SID: " + m_targetUserSid
			+ L"). All other OpenGlass GUI settings apply system-wide."
		);
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

	bool MainFrame::NotifySettingsChange(ChangeType type)
	{
		if (!m_dwmWindow || !IsWindow(m_dwmWindow))
		{
			m_dwmWindow = FindWindowW(L"Dwm", nullptr);
		}
		HWND notificationWindow = m_dwmWindow;
		if (!notificationWindow) return false;
		bool succeeded = true;
		if (type == ChangeType::Colorization || type == ChangeType::Both)
			succeeded = SendNotifyMessage(notificationWindow, WM_DWMCOLORIZATIONCOLORCHANGED, 0, 0) != FALSE;

		if (type == ChangeType::Theme || type == ChangeType::Both)
			succeeded = SendNotifyMessage(notificationWindow, WM_THEMECHANGED, 0, 0) != FALSE && succeeded;
		return succeeded;
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
		SetStatusText(L"Colorization user: " + m_targetUserLabel + L"; other settings apply system-wide");
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

	RegistryConfig* MainFrame::GetConfigForSetting([[maybe_unused]] Settings::Id id) const
	{
		return m_config.get();
	}

	RegistryConfig* MainFrame::GetConfigForScope(Settings::Scope scope) const
	{
		return scope == Settings::Scope::User ? m_userConfig.get() : m_systemConfig.get();
	}

	ResolvedRegistryValue<DWORD> MainFrame::ResolveOverridableDword(
		Settings::Id setting,
		Settings::Id overrideSetting,
		DWORD defaultValue
	) const
	{
		auto makeReader = [](const RegistryConfig* config)
		{
			return [config](const std::wstring& name) -> std::optional<DWORD>
			{
				DWORD value{};
				if (config && config->TryGetDword(name, value))
				{
					return value;
				}
				return std::nullopt;
			};
		};

		const std::wstring settingName(Settings::Get(setting).name);
		const std::wstring overrideName(Settings::Get(overrideSetting).name);
		return ResolveOverridableRegistryValueFromReaders(
			settingName,
			overrideName,
			defaultValue,
			makeReader(m_userConfig.get()),
			makeReader(m_systemConfig.get())
		);
	}

	void MainFrame::ResetOverridableDword([[maybe_unused]] Settings::Id setting, Settings::Id overrideSetting)
	{
		const auto& overrideSpec = Settings::Get(overrideSetting);
		const std::wstring overrideName(overrideSpec.name);
		RegistryConfig* config = GetConfigForScope(overrideSpec.scope);
		if (!config || !config->HasValue(overrideName))
		{
			return;
		}

		TrackSettingChange(overrideSetting);
		if (!CheckRegistryWrite(config->DeleteValue(overrideName), overrideName)) return;
		SetDirty(true);
		NotifySettingsChange(ChangeType::Colorization);
		LoadSettings(false);
	}

	void MainFrame::BackupCurrentSetting(TrackedSetting setting)
	{
		const auto& spec = Settings::Get(setting.id);
		const std::wstring name(spec.name);
		RegistryConfig* config = GetConfigForScope(setting.scope);
		if (!config || !config->HasValue(name))
		{
			m_backupSettings[setting] = std::monostate{};
			return;
		}

		if (spec.type == Settings::ValueType::String)
		{
			std::wstring value;
			m_backupSettings[setting] = config->TryGetString(name, value)
				? decltype(m_backupSettings)::mapped_type{ std::move(value) }
				: decltype(m_backupSettings)::mapped_type{ std::monostate{} };
			return;
		}

		m_backupSettings[setting] = config->GetDword(name, 0);
	}

	void MainFrame::TrackSettingChange(Settings::Id id)
	{
		TrackSettingChange(Settings::Get(id).scope, id);
	}

	void MainFrame::TrackSettingChange(Settings::Scope scope, Settings::Id id)
	{
		const TrackedSetting setting{ scope, id };
		if (m_dirtyKeys.insert(setting).second)
		{
			BackupCurrentSetting(setting);
		}
	}

	bool MainFrame::CheckRegistryWrite(HRESULT result, const std::wstring& name)
	{
		if (SUCCEEDED(result)) return true;
		SetDirty(!m_dirtyKeys.empty());
		NotifySettingsChange(ChangeType::Both);
		LoadSettings(false);
		wxMessageBox(
			wxString::Format(L"The registry value '%s' could not be updated (HRESULT 0x%08lX).", name.c_str(), static_cast<unsigned long>(result)),
			L"OpenGlass configuration",
			wxOK | wxICON_ERROR,
			this
		);
		return false;
	}

	void MainFrame::SaveSettings()
	{
		if (!m_config)
		{
			return;
		}
		for (const auto& setting : m_dirtyKeys)
		{
			BackupCurrentSetting(setting);
		}
		m_dirtyKeys.clear();
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
		if (m_dirtyKeys.empty())
		{
			SetDirty(false);
			return true;
		}
		HRESULT failure{ S_OK };
		for (const auto& setting : m_dirtyKeys)
		{
			const std::wstring key(Settings::Get(setting.id).name);
			auto it = m_backupSettings.find(setting);
			if (it == m_backupSettings.end())
			{
				continue;
			}
			const auto& val = it->second;
			RegistryConfig* config = GetConfigForScope(setting.scope);
			if (!config)
			{
				continue;
			}
			if (std::holds_alternative<std::monostate>(val))
			{
				failure = config->DeleteValue(key);
			}
			else if (std::holds_alternative<DWORD>(val))
			{
				failure = config->SetDword(key, std::get<DWORD>(val));
			}
			else if (std::holds_alternative<std::wstring>(val))
			{
				failure = config->SetString(key, std::get<std::wstring>(val));
			}
			if (FAILED(failure)) break;
		}
		if (FAILED(failure))
		{
			wxMessageBox(wxString::Format(L"The registry rollback could not be completed (HRESULT 0x%08lX). The configuration remains dirty.", static_cast<unsigned long>(failure)), L"OpenGlass configuration", wxOK | wxICON_ERROR, this);
			return false;
		}
		NotifySettingsChange();
		LoadSettings(false);
		m_dirtyKeys.clear();
		SetDirty(false);
		return true;
	}

	void MainFrame::AddProperty(
		wxWindow* parent,
		wxSizer* sizer,
		const wxString& label,
		wxWindow* control,
		std::optional<Settings::Id> setting,
		std::optional<Settings::Id> overrideSetting
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
		if (setting)
		{
			AddOptionStatus(parent, row, *setting, overrideSetting);
		}
		sizer->Add(row, 0, wxEXPAND | wxALL, 2);
	}

	void MainFrame::AddOptionStatus(
		wxWindow* parent,
		wxBoxSizer* row,
		Settings::Id setting,
		std::optional<Settings::Id> overrideSetting
	)
	{
		if (!parent || !row)
		{
			return;
		}

		const wxSize iconSize(16, 16);
		const wxBitmap infoBmp = wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_MESSAGE_BOX, iconSize);
		auto* info = new wxStaticBitmap(parent, wxID_ANY, infoBmp);
		wxButton* reset = nullptr;
		if (overrideSetting)
		{
			reset = new wxButton(parent, wxID_ANY, L"↶", wxDefaultPosition, wxSize(28, -1), wxBU_EXACTFIT);
			reset->SetName(L"Reset Override");
			reset->SetToolTip(L"Remove the per-user Override value and use the per-user base value or default.");
			reset->Hide();
			reset->Bind(wxEVT_BUTTON, [this, setting, overrideSetting](wxCommandEvent&)
			{
				ResetOverridableDword(setting, *overrideSetting);
			});
		}
		info->SetMinSize(iconSize);
		info->SetToolTip(L"Effective value is coming from an Override key.");
		info->Hide();

		row->Add(info, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxLEFT, 2);
		if (reset)
		{
			row->Add(reset, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxLEFT, 2);
		}

		const std::wstring_view name = Settings::Get(setting).name;
		const bool vistaIrrelevant = name.find(L"Afterglow") != std::wstring_view::npos
			|| name.find(L"Balance") != std::wstring_view::npos;
		const bool win7Irrelevant = setting == Settings::Id::GlassOpacityInactive
			|| setting == Settings::Id::ColorizationColorInactive
			|| setting == Settings::Id::GlassOpacity;
		m_optionStatus.push_back({ info, reset, setting, overrideSetting, vistaIrrelevant, win7Irrelevant });
	}

	void MainFrame::UpdateOptionStatusIcons()
	{
		if (!m_config)
		{
			return;
		}

		bool needLayout = false;

		const bool isVista = (m_rbGlassType && m_rbGlassType->GetSelection() == 0);
		std::unordered_map<Settings::Id, bool> activeHasValueCache;

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
				continue;
			}

			RegistryConfig* activeConfig = GetConfigForSetting(item.setting);
			if (!activeConfig)
			{
				if (item.overrideIcon && item.overrideIcon->IsShown())
				{
					item.overrideIcon->Show(false);
					needLayout = true;
				}
				continue;
			}

			bool selectedOverrideExists = false;
			if (item.overrideSetting)
			{
				auto it = activeHasValueCache.find(*item.overrideSetting);
				if (it == activeHasValueCache.end())
				{
					const std::wstring overrideName(Settings::Get(*item.overrideSetting).name);
					selectedOverrideExists = activeConfig->HasValue(overrideName);
					activeHasValueCache.emplace(*item.overrideSetting, selectedOverrideExists);
				}
				else
				{
					selectedOverrideExists = it->second;
				}
			}
			if (item.overrideIcon)
			{
				bool show = false;
				if (item.overrideSetting)
				{
					const auto resolved = ResolveOverridableDword(item.setting, *item.overrideSetting, 0);
					show = isRelevant && resolved.IsOverride();
					item.overrideIcon->SetToolTip(
						resolved.source == RegistryValueSource::UserOverride
							? L"Effective value is coming from the current user's Override value."
							: L"Effective value is coming from the machine Override value."
					);
				}
				if (item.setting == Settings::Id::ColorizationColorInactive)
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

		auto updateDword = [this](Settings::Id id, DWORD val, ChangeType type = ChangeType::Both) {
			const auto& spec = Settings::Get(id);
			const std::wstring name(spec.name);
			if (spec.type != Settings::ValueType::Dword)
			{
				CheckRegistryWrite(E_INVALIDARG, name);
				return;
			}
			RegistryConfig* config = GetConfigForSetting(id);
			if (!config)
			{
				return;
			}
			TrackSettingChange(id);
			if (!CheckRegistryWrite(config->SetDword(name, val), name)) return;
			SetDirty(true);
			NotifySettingsChange(type);
		};
		auto updateOverridableDword = [this, updateDword](Settings::Id overrideSetting, DWORD value) {
			updateDword(overrideSetting, value, ChangeType::Colorization);
			UpdateOptionStatusIcons();
			UpdateColorizationPresetSelection();
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

		auto colorToDwordBgr = [](const wxColour& c) -> DWORD {
			return (c.Red()) | (c.Green() << 8) | (c.Blue() << 16);
		};
		auto colorToDwordIgnoreAlpha = [this](Settings::Id id, const wxColour& c) -> DWORD {
			const std::wstring key(Settings::Get(id).name);
			DWORD alpha = 0;
			RegistryConfig* config = GetConfigForSetting(id);
			if (config && config->HasValue(key))
			{
				alpha = (config->GetDword(key, 0) >> 24) & 0xFF;
			}
			return (alpha << 24) | (c.Red() << 16) | (c.Green() << 8) | c.Blue();
		};

		auto hasValue = [this](Settings::Id id) {
			const std::wstring key(Settings::Get(id).name);
			RegistryConfig* config = GetConfigForSetting(id);
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
				m_slGlassOpacityInactive->SetValue(m_slColorIntensity->GetValue());
				syncSliderTooltip(m_slGlassOpacityInactive);
			}

			DWORD captionActive = m_config->GetDword(L"ColorizationColorCaption", 0xFFFFFFFD);
			DWORD captionInactive = hasValue(Settings::Id::ColorizationColorCaptionInactive)
				? m_config->GetDword(L"ColorizationColorCaptionInactive", captionActive)
				: captionActive;
			DWORD captionMaximized = hasValue(Settings::Id::ColorizationColorCaptionMaximized)
				? m_config->GetDword(L"ColorizationColorCaptionMaximized", captionActive)
				: captionActive;
			DWORD captionInactiveMaximized = hasValue(Settings::Id::ColorizationColorCaptionInactiveMaximized)
				? m_config->GetDword(L"ColorizationColorCaptionInactiveMaximized", captionInactive)
				: captionInactive;

			if (!hasValue(Settings::Id::ColorizationColorCaptionInactive))
			{
				ApplyChoiceColorEx(m_chModeColorCaptionInactive, m_cpColorCaptionInactive, captionInactive, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
			}
			if (!hasValue(Settings::Id::ColorizationColorCaptionMaximized))
			{
				ApplyChoiceColorEx(m_chModeColorCaptionMaximized, m_cpColorCaptionMaximized, captionMaximized, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
			}
			if (!hasValue(Settings::Id::ColorizationColorCaptionInactiveMaximized))
			{
				ApplyChoiceColorEx(m_chModeColorCaptionInactiveMaximized, m_cpColorCaptionInactiveMaximized, captionInactiveMaximized, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
			}

			DWORD refOpacityActive = m_config->GetDword(L"ColorizationGlassReflectionOpacity", 0xFFFFFFFE);
			DWORD refOpacityInactive = hasValue(Settings::Id::ColorizationGlassReflectionOpacityInactive)
				? m_config->GetDword(L"ColorizationGlassReflectionOpacityInactive", refOpacityActive)
				: refOpacityActive;
			DWORD refOpacityMaximized = hasValue(Settings::Id::ColorizationGlassReflectionOpacityMaximized)
				? m_config->GetDword(L"ColorizationGlassReflectionOpacityMaximized", refOpacityActive)
				: refOpacityActive;
			DWORD refOpacityInactiveMaximized = hasValue(Settings::Id::ColorizationGlassReflectionOpacityInactiveMaximized)
				? m_config->GetDword(L"ColorizationGlassReflectionOpacityInactiveMaximized", refOpacityInactive)
				: refOpacityInactive;

			if (!hasValue(Settings::Id::ColorizationGlassReflectionOpacityInactive))
			{
				ApplyChoiceSlider(m_chModeReflectionOpacityInactive, m_slReflectionOpacityInactive, refOpacityInactive, 0xFFFFFFFF, 0xFFFFFFFE, 50);
				syncSliderTooltip(m_slReflectionOpacityInactive);
			}
			if (!hasValue(Settings::Id::ColorizationGlassReflectionOpacityMaximized))
			{
				ApplyChoiceSlider(m_chModeReflectionOpacityMaximized, m_slReflectionOpacityMaximized, refOpacityMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 50);
				syncSliderTooltip(m_slReflectionOpacityMaximized);
			}
			if (!hasValue(Settings::Id::ColorizationGlassReflectionOpacityInactiveMaximized))
			{
				ApplyChoiceSlider(m_chModeReflectionOpacityInactiveMaximized, m_slReflectionOpacityInactiveMaximized, refOpacityInactiveMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 50);
				syncSliderTooltip(m_slReflectionOpacityInactiveMaximized);
			}

			DWORD colorOpacityActive = m_config->GetDword(L"ColorizationOpacity", 0xFFFFFFFE);
			DWORD colorOpacityInactive = hasValue(Settings::Id::ColorizationOpacityInactive)
				? m_config->GetDword(L"ColorizationOpacityInactive", colorOpacityActive)
				: colorOpacityActive;
			DWORD colorOpacityMaximized = hasValue(Settings::Id::ColorizationOpacityMaximized)
				? m_config->GetDword(L"ColorizationOpacityMaximized", colorOpacityActive)
				: colorOpacityActive;
			DWORD colorOpacityInactiveMaximized = hasValue(Settings::Id::ColorizationOpacityInactiveMaximized)
				? m_config->GetDword(L"ColorizationOpacityInactiveMaximized", colorOpacityInactive)
				: colorOpacityInactive;

			if (!hasValue(Settings::Id::ColorizationOpacityInactive))
			{
				ApplyChoiceSlider(m_chModeColorizationOpacityInactive, m_slColorizationOpacityInactive, colorOpacityInactive, 0xFFFFFFFF, 0xFFFFFFFE, 100);
				syncSliderTooltip(m_slColorizationOpacityInactive);
			}
			if (!hasValue(Settings::Id::ColorizationOpacityMaximized))
			{
				ApplyChoiceSlider(m_chModeColorizationOpacityMaximized, m_slColorizationOpacityMaximized, colorOpacityMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 100);
				syncSliderTooltip(m_slColorizationOpacityMaximized);
			}
			if (!hasValue(Settings::Id::ColorizationOpacityInactiveMaximized))
			{
				ApplyChoiceSlider(m_chModeColorizationOpacityInactiveMaximized, m_slColorizationOpacityInactiveMaximized, colorOpacityInactiveMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 100);
				syncSliderTooltip(m_slColorizationOpacityInactiveMaximized);
			}
		};

		auto updateString = [this](Settings::Id id, const std::wstring& val, ChangeType type = ChangeType::Both) {
			const auto& spec = Settings::Get(id);
			const std::wstring name(spec.name);
			if (spec.type != Settings::ValueType::String)
			{
				CheckRegistryWrite(E_INVALIDARG, name);
				return;
			}
			RegistryConfig* config = GetConfigForSetting(id);
			if (!config)
			{
				return;
			}
			TrackSettingChange(id);
			if (!CheckRegistryWrite(config->SetString(name, val), name)) return;
			SetDirty(true);
			NotifySettingsChange(type);
		};

		auto deleteValue = [this](Settings::Id id) {
			const std::wstring name(Settings::Get(id).name);
			RegistryConfig* config = GetConfigForSetting(id);
			if (!config)
			{
				return;
			}
			TrackSettingChange(id);
			if (!CheckRegistryWrite(config->DeleteValue(name), name)) return;
			SetDirty(true);
		};

		auto restorePickerPath = [this](wxFilePickerCtrl* picker, Settings::Id id) {
			picker->SetPath(m_config->GetString(std::wstring(Settings::Get(id).name), L""));
		};

		auto ensureFilePath = []([[maybe_unused]] const wxString& path, [[maybe_unused]] const wxString& title) -> bool {
			return true;
		};

		m_chkDisableGlassOnBattery->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			if (e.IsChecked())
			{
				deleteValue(Settings::Id::DisableGlassOnBattery);
				NotifySettingsChange();
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
				NotifySettingsChange();
			}
			else
			{
				updateDword(Settings::Id::DisabledHooks, mask);
			}
		});

		m_fpCustomThemeAtlas->Bind(wxEVT_FILEPICKER_CHANGED, [this, updateString, ensureFilePath, restorePickerPath](wxFileDirPickerEvent& e) {
			if (m_chkCustomThemeAtlas->IsChecked())
			{
				if (!ensureFilePath(e.GetPath(), L"Theme atlas"))
				{
					restorePickerPath(m_fpCustomThemeAtlas, Settings::Id::CustomThemeAtlas);
					return;
				}
				updateString(Settings::Id::CustomThemeAtlas, e.GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});
		m_chkCustomThemeAtlas->Bind(wxEVT_CHECKBOX, [this, updateString, ensureFilePath, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			m_fpCustomThemeAtlas->Enable(checked);
			if (!checked)
			{
				m_fpCustomThemeAtlas->SetPath(wxEmptyString);
				deleteValue(Settings::Id::CustomThemeAtlas);
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				if (m_fpCustomThemeAtlas->GetPath().empty())
				{
					deleteValue(Settings::Id::CustomThemeAtlas);
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
				updateString(Settings::Id::CustomThemeAtlas, m_fpCustomThemeAtlas->GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});

		m_fpCustomThemeReflection->Bind(wxEVT_FILEPICKER_CHANGED, [this, updateString, ensureFilePath, restorePickerPath](wxFileDirPickerEvent& e) {
			if (m_chkCustomThemeReflection->IsChecked())
			{
				if (!ensureFilePath(e.GetPath(), L"Reflection texture"))
				{
					restorePickerPath(m_fpCustomThemeReflection, Settings::Id::CustomThemeReflection);
					return;
				}
				updateString(Settings::Id::CustomThemeReflection, e.GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});
		m_chkCustomThemeReflection->Bind(wxEVT_CHECKBOX, [this, updateString, ensureFilePath, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			m_fpCustomThemeReflection->Enable(checked);
			if (!checked)
			{
				m_fpCustomThemeReflection->SetPath(wxEmptyString);
				deleteValue(Settings::Id::CustomThemeReflection);
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				if (m_fpCustomThemeReflection->GetPath().empty())
				{
					deleteValue(Settings::Id::CustomThemeReflection);
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
				updateString(Settings::Id::CustomThemeReflection, m_fpCustomThemeReflection->GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});

		m_slReflectionIntensity->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			int val = e.GetInt();
			if (val == 0)
			{
				deleteValue(Settings::Id::ColorizationGlassReflectionIntensity);
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(Settings::Id::ColorizationGlassReflectionIntensity, val, ChangeType::Colorization);
			}
			setSliderTooltipValue(m_slReflectionIntensity, val);
		});

		auto bindRefOpacity = [&](wxChoice* ch, wxSlider* sl, Settings::Id id, DWORD themeSentinel, DWORD autoSentinel, bool propagateInheritance) {
			auto update = [this, ch, sl, id, themeSentinel, autoSentinel, updateDword, deleteValue, propagateInheritance, updateInheritance]() {
				int sel = ch->GetSelection();
				sl->Enable(sel == 2);
				if (sel == 0) deleteValue(id);
				else if (sel == 1) updateDword(id, themeSentinel, ChangeType::Colorization);
				else updateDword(id, sl->GetValue(), ChangeType::Colorization);
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

		bindRefOpacity(m_chModeReflectionOpacity, m_slReflectionOpacity, Settings::Id::ColorizationGlassReflectionOpacity, 0xFFFFFFFF, 0xFFFFFFFE, true);
		bindRefOpacity(m_chModeReflectionOpacityInactive, m_slReflectionOpacityInactive, Settings::Id::ColorizationGlassReflectionOpacityInactive, 0xFFFFFFFF, 0xFFFFFFFE, true);
		bindRefOpacity(m_chModeReflectionOpacityMaximized, m_slReflectionOpacityMaximized, Settings::Id::ColorizationGlassReflectionOpacityMaximized, 0xFFFFFFFF, 0xFFFFFFFE, false);
		bindRefOpacity(m_chModeReflectionOpacityInactiveMaximized, m_slReflectionOpacityInactiveMaximized, Settings::Id::ColorizationGlassReflectionOpacityInactiveMaximized, 0xFFFFFFFF, 0xFFFFFFFE, false);

		m_slReflectionParallax->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			int val = e.GetInt();
			if (val == 13)
			{
				deleteValue(Settings::Id::ColorizationGlassReflectionParallaxIntensity);
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(Settings::Id::ColorizationGlassReflectionParallaxIntensity, val, ChangeType::Colorization);
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
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(Settings::Id::ColorizationGlassReflectionPolicy, mask, ChangeType::Colorization);
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
		// 		deleteValue(Settings::Id::ColorizationGlassReflectionPolicy);
		// 		NotifySettingsChange(ChangeType::Colorization);
		// 	}
		// 	else
		// 	{
		// 		updateDword(Settings::Id::ColorizationGlassReflectionPolicy, mask, ChangeType::Colorization);
		// 	}
		// });

		m_fpCustomThemeMaterial->Bind(wxEVT_FILEPICKER_CHANGED, [this, updateString, ensureFilePath, restorePickerPath](wxFileDirPickerEvent& e) {
			if (m_chkCustomThemeMaterial->IsChecked())
			{
				if (!ensureFilePath(e.GetPath(), L"Material texture"))
				{
					restorePickerPath(m_fpCustomThemeMaterial, Settings::Id::CustomThemeMaterial);
					return;
				}
				updateString(Settings::Id::CustomThemeMaterial, e.GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});
		m_chkCustomThemeMaterial->Bind(wxEVT_CHECKBOX, [this, updateString, ensureFilePath, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			m_fpCustomThemeMaterial->Enable(checked);
			if (!checked)
			{
				m_fpCustomThemeMaterial->SetPath(wxEmptyString);
				deleteValue(Settings::Id::CustomThemeMaterial);
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				if (m_fpCustomThemeMaterial->GetPath().empty())
				{
					deleteValue(Settings::Id::CustomThemeMaterial);
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
				updateString(Settings::Id::CustomThemeMaterial, m_fpCustomThemeMaterial->GetPath().ToStdWstring(), ChangeType::Theme);
			}
			UpdatePathWarningIcons();
		});

		m_slMaterialOpacity->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			int val = e.GetInt();
			if (val == 0)
			{
				deleteValue(Settings::Id::MaterialOpacity);
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(Settings::Id::MaterialOpacity, val, ChangeType::Colorization);
			}
			setSliderTooltipValue(m_slMaterialOpacity, val);
		});
		m_slBlurDeviation->Bind(wxEVT_SLIDER, [this, updateDword, deleteValue, blurRadiusToDeviation, setSliderTooltipValue]([[maybe_unused]] wxCommandEvent& e) {
			DWORD val = blurRadiusToDeviation(e.GetInt());
			if (val == 30)
			{
				deleteValue(Settings::Id::BlurDeviation);
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(Settings::Id::BlurDeviation, val, ChangeType::Colorization);
			}
			setSliderTooltipValue(m_slBlurDeviation, e.GetInt());
		});
		m_chBlurOptimization->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				deleteValue(Settings::Id::BlurOptimization);
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(Settings::Id::BlurOptimization, sel, ChangeType::Colorization);
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
				deleteValue(Settings::Id::UseDirect3DRendering);
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(Settings::Id::UseDirect3DRendering, 1, ChangeType::Colorization);
			}
			updateD3DControls();
		});
		m_chkGlassSafetyZone->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool checked = e.IsChecked();
			if (checked)
			{
				updateDword(Settings::Id::GlassSafetyZoneMode, 0, ChangeType::Colorization);
			}
			else
			{
				deleteValue(Settings::Id::GlassSafetyZoneMode);
				NotifySettingsChange(ChangeType::Colorization);
			}
		});

		m_chRoundRectProfile->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				m_scRoundRectRadius->SetValue(0);
				m_scRoundRectRadius->Disable();
				deleteValue(Settings::Id::RoundRectRadius);
				NotifySettingsChange(ChangeType::Colorization);
			}
			else if (sel == 1)
			{
				m_scRoundRectRadius->SetValue(6);
				m_scRoundRectRadius->Disable();
				updateDword(Settings::Id::RoundRectRadius, 6, ChangeType::Colorization);
			}
			else
			{
				m_scRoundRectRadius->Enable();
				DWORD r = m_scRoundRectRadius->GetValue();
				if (r == 0)
				{
					deleteValue(Settings::Id::RoundRectRadius);
					NotifySettingsChange(ChangeType::Colorization);
				}
				else
				{
					updateDword(Settings::Id::RoundRectRadius, r, ChangeType::Colorization);
				}
			}
		});
		m_scRoundRectRadius->Bind(wxEVT_SPINCTRL, [this, updateDword, deleteValue](wxSpinEvent& e) {
			DWORD val = e.GetPosition();
			if (val == 0)
			{
				deleteValue(Settings::Id::RoundRectRadius);
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(Settings::Id::RoundRectRadius, val, ChangeType::Colorization);
			}
		});

		auto updateGlow = [this, updateDword, deleteValue]() {
			int mode = m_chTextGlowMode->GetSelection();
			int size = m_scTextGlowSize->GetValue();
			DWORD val = (DWORD)mode | ((DWORD)size << 16);
			if (val == 1)
			{
				deleteValue(Settings::Id::TextGlowMode);
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				updateDword(Settings::Id::TextGlowMode, val, ChangeType::Theme);
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
				NotifySettingsChange(ChangeType::Theme);
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
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				updateDword(Settings::Id::CenterCaption, sel, ChangeType::Theme);
			}
		});
		m_chkDisableModernBorders->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			if (!e.IsChecked())
			{
				deleteValue(Settings::Id::DisableModernBorders);
				NotifySettingsChange(ChangeType::Theme);
			}
			else
			{
				updateDword(Settings::Id::DisableModernBorders, 1, ChangeType::Theme);
			}
		});

		m_rbGlassType->Bind(wxEVT_RADIOBOX, [this, updateDword, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 0)
			{
				deleteValue(Settings::Id::GlassType);
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(Settings::Id::GlassType, sel, ChangeType::Colorization);
			}
			LoadSettings(false);
		});

		for (const auto& [preset, button] : m_presetButtons)
		{
			button->Bind(wxEVT_TOGGLEBUTTON, [this, preset](wxCommandEvent& event) {
				ApplyColorizationPreset(*preset);
				event.Skip();
			});
		}

		for (auto* button : m_customColorButtons)
		{
			button->Bind(wxEVT_TOGGLEBUTTON, [this, button](wxCommandEvent&) {
				const auto family = m_rbGlassType->GetSelection() == 0
					? ColorizationPresets::Family::Vista
					: ColorizationPresets::Family::Windows7;
				ApplyColorizationColor(button->GetColor(), family);
			});
			button->Bind(wxEVT_LEFT_DCLICK, [this, button](wxMouseEvent&) {
				wxColourData colorData;
				colorData.SetChooseFull(true);
				colorData.SetChooseAlpha(false);
				const DWORD currentValue = button->GetColor();
				colorData.SetColour(wxColour(
					(currentValue >> 16) & 0xFF,
					(currentValue >> 8) & 0xFF,
					currentValue & 0xFF
				));

				wxColourDialog dialog(this, &colorData);
				if (dialog.ShowModal() != wxID_OK)
				{
					UpdateColorizationPresetSelection();
					return;
				}

				const wxColour selected = dialog.GetColourData().GetColour();
				const DWORD alpha = ColorizationPresets::CalculateIntensityAlpha(
					m_slColorIntensity->GetValue()
				) << 24;
				const DWORD argb = alpha
					| (static_cast<DWORD>(selected.Red()) << 16)
					| (static_cast<DWORD>(selected.Green()) << 8)
					| static_cast<DWORD>(selected.Blue());
				const auto family = m_rbGlassType->GetSelection() == 0
					? ColorizationPresets::Family::Vista
					: ColorizationPresets::Family::Windows7;
				button->SetColor(argb);
				ApplyColorizationColor(argb, family);
			});
		}

		m_chkEnableTransparency->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent& e) {
			RegistryConfig* config = GetConfigForSetting(Settings::Id::ColorizationOpaqueBlend);
			if (!config)
			{
				return;
			}

			const bool opaque = !e.IsChecked();
			const ColorizationPresets::Preset* matchedPreset = FindMatchingWindows7Preset(!opaque);
			const std::wstring opaqueBlendName(Settings::Get(Settings::Id::ColorizationOpaqueBlend).name);
			TrackSettingChange(Settings::Id::ColorizationOpaqueBlend);
			if (opaque)
			{
				if (!CheckRegistryWrite(config->SetDword(opaqueBlendName, 1), opaqueBlendName)) return;
			}
			else
			{
				if (!CheckRegistryWrite(config->DeleteValue(opaqueBlendName), opaqueBlendName)) return;
			}

			if (matchedPreset)
			{
				const auto parameters = ColorizationPresets::CalculateWindows7Parameters(
					matchedPreset->argb,
					opaque
				);
				const std::pair<Settings::Id, DWORD> values[]
				{
					{ Settings::Id::ColorizationColorBalanceOverride, parameters.colorBalance },
					{ Settings::Id::ColorizationAfterglowBalanceOverride, parameters.afterglowBalance },
					{ Settings::Id::ColorizationBlurBalanceOverride, parameters.blurBalance }
				};
				for (const auto& [id, value] : values)
				{
					const std::wstring name(Settings::Get(id).name);
					TrackSettingChange(id);
					if (!CheckRegistryWrite(config->SetDword(name, value), name)) return;
				}
			}

			SetDirty(true);
			NotifySettingsChange(ChangeType::Colorization);
			LoadSettings(false);
		});

		m_cpColorizationColor->Bind(wxEVT_COLOURPICKER_CHANGED, [this, updateOverridableDword, updateInheritance](wxColourPickerEvent& e) {
			const DWORD currentValue = ResolveOverridableDword(
				Settings::Id::ColorizationColor,
				Settings::Id::ColorizationColorOverride,
				0xFF000000
			).value;
			const wxColour color = e.GetColour();
			const DWORD value = (currentValue & 0xFF000000)
				| (color.Red() << 16)
				| (color.Green() << 8)
				| color.Blue();
			updateOverridableDword(Settings::Id::ColorizationColorOverride, value);
			updateInheritance();
		});

		m_chkEnableInactiveColor->Bind(wxEVT_CHECKBOX, [this, updateDword, updateInheritance, colorToDwordIgnoreAlpha, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool enabled = e.IsChecked();
			m_cpColorizationColorInactive->Enable(enabled);
			if (enabled) {
				updateDword(Settings::Id::ColorizationColorInactive, colorToDwordIgnoreAlpha(Settings::Id::ColorizationColorInactive, m_cpColorizationColorInactive->GetColour()), ChangeType::Colorization);
			} else {
				deleteValue(Settings::Id::ColorizationColorInactive);
				NotifySettingsChange(ChangeType::Colorization);
				updateInheritance();
			}
		});

		m_cpColorizationColorInactive->Bind(wxEVT_COLOURPICKER_CHANGED, [this, updateDword, colorToDwordIgnoreAlpha](wxColourPickerEvent& e) {
			if (m_chkEnableInactiveColor->IsChecked())
				updateDword(Settings::Id::ColorizationColorInactive, colorToDwordIgnoreAlpha(Settings::Id::ColorizationColorInactive, e.GetColour()), ChangeType::Colorization);
		});

		m_slColorIntensity->Bind(wxEVT_SLIDER, [this, updateDword, updateInheritance, deleteValue, setSliderTooltipValue](wxCommandEvent& e) {
			int val = e.GetInt();
			if (m_rbGlassType->GetSelection() == 0)
			{
				if (val == 63)
				{
					deleteValue(Settings::Id::GlassOpacity);
					NotifySettingsChange(ChangeType::Colorization);
				}
				else
				{
					updateDword(Settings::Id::GlassOpacity, val, ChangeType::Colorization);
				}
				setSliderTooltipValue(m_slColorIntensity, val);
				updateInheritance();
				UpdateColorizationPresetSelection();
				return;
			}

			RegistryConfig* config = GetConfigForSetting(Settings::Id::ColorizationColorOverride);
			if (!config)
			{
				return;
			}

			const DWORD alpha = ColorizationPresets::CalculateIntensityAlpha(val) << 24;
			const DWORD color = alpha | (ResolveOverridableDword(
				Settings::Id::ColorizationColor,
				Settings::Id::ColorizationColorOverride,
				0xFF000000
			).value & 0x00FFFFFF);
			const DWORD afterglow = alpha | (ResolveOverridableDword(
				Settings::Id::ColorizationAfterglow,
				Settings::Id::ColorizationAfterglowOverride,
				0
			).value & 0x00FFFFFF);
			const auto parameters = ColorizationPresets::CalculateWindows7Parameters(
				color,
				!m_chkEnableTransparency->IsChecked()
			);
			const std::pair<Settings::Id, DWORD> values[]
			{
				{ Settings::Id::ColorizationColorOverride, color },
				{ Settings::Id::ColorizationAfterglowOverride, afterglow },
				{ Settings::Id::ColorizationColorBalanceOverride, parameters.colorBalance },
				{ Settings::Id::ColorizationAfterglowBalanceOverride, parameters.afterglowBalance },
				{ Settings::Id::ColorizationBlurBalanceOverride, parameters.blurBalance }
			};
			for (const auto& [id, value] : values)
			{
				const std::wstring name(Settings::Get(id).name);
				TrackSettingChange(id);
				if (!CheckRegistryWrite(config->SetDword(name, value), name)) return;
			}

			SetDirty(true);
			NotifySettingsChange(ChangeType::Colorization);
			setSliderTooltipValue(m_slColorIntensity, val);
			m_slColorBalance->SetValue(parameters.colorBalance);
			m_slAfterglowBalance->SetValue(parameters.afterglowBalance);
			m_slBlurBalance->SetValue(parameters.blurBalance);
			setSliderTooltipValue(m_slColorBalance, parameters.colorBalance);
			setSliderTooltipValue(m_slAfterglowBalance, parameters.afterglowBalance);
			setSliderTooltipValue(m_slBlurBalance, parameters.blurBalance);
			UpdateOptionStatusIcons();
			UpdateColorizationPresetSelection();
		});

		m_chkEnableInactiveOpacity->Bind(wxEVT_CHECKBOX, [this, updateDword, updateInheritance, deleteValue]([[maybe_unused]] wxCommandEvent& e) {
			bool enabled = e.IsChecked();
			m_slGlassOpacityInactive->Enable(enabled);
			if (enabled)
			{
				updateDword(Settings::Id::GlassOpacityInactive, m_slGlassOpacityInactive->GetValue(), ChangeType::Colorization);
			}
			else
			{
				deleteValue(Settings::Id::GlassOpacityInactive);
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
			updateDword(Settings::Id::GlassOpacityInactive, val, ChangeType::Colorization);
		});

		auto bindChoiceColorEx = [&](wxChoice* ch, wxColourPickerCtrl* cp, Settings::Id id, DWORD themeSentinel, DWORD autoSentinel, DWORD systemSentinel, bool propagateInheritance) {
			auto update = [this, ch, cp, id, themeSentinel, autoSentinel, systemSentinel, updateDword, deleteValue, colorToDwordBgr, propagateInheritance, updateInheritance]() {
				int sel = ch->GetSelection();
				cp->Enable(sel == 2);
				if (sel == 0) deleteValue(id);
				else if (sel == 1) updateDword(id, themeSentinel, ChangeType::Colorization);
				else if (sel == 3) updateDword(id, systemSentinel, ChangeType::Colorization);
				else updateDword(id, colorToDwordBgr(cp->GetColour()), ChangeType::Colorization);
				if (sel == 0) NotifySettingsChange(ChangeType::Colorization);
				if (propagateInheritance)
				{
					updateInheritance();
				}
			};
			ch->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			cp->Bind(wxEVT_COLOURPICKER_CHANGED, [update](wxColourPickerEvent&) { update(); });
		};

		bindChoiceColorEx(m_chModeColorCaption, m_cpColorCaption, Settings::Id::ColorizationColorCaption, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF, true);
		bindChoiceColorEx(m_chModeColorCaptionInactive, m_cpColorCaptionInactive, Settings::Id::ColorizationColorCaptionInactive, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF, true);
		bindChoiceColorEx(m_chModeColorCaptionMaximized, m_cpColorCaptionMaximized, Settings::Id::ColorizationColorCaptionMaximized, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF, false);
		bindChoiceColorEx(m_chModeColorCaptionInactiveMaximized, m_cpColorCaptionInactiveMaximized, Settings::Id::ColorizationColorCaptionInactiveMaximized, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF, false);

		auto bindBaseColor = [this, updateDword, deleteValue](wxChoice* choice, wxColourPickerCtrl* picker, wxSpinCtrl* alphaSpin, Settings::Id id, DWORD themeVal, DWORD autoVal) {
			auto update = [this, choice, picker, alphaSpin, id, autoVal, themeVal, updateDword, deleteValue]() {
				int sel = choice->GetSelection();
				if (sel == 0)
				{
					picker->Disable();
					alphaSpin->Disable();
					deleteValue(id);
					NotifySettingsChange(ChangeType::Colorization);
				}
				else if (sel == 1)
				{
					picker->Disable();
					alphaSpin->Disable();
					updateDword(id, themeVal, ChangeType::Colorization);
				}
				else
				{
					picker->Enable();
					alphaSpin->Enable();
					wxColour c = picker->GetColour();
					int a = alphaSpin->GetValue();
					DWORD val = (a << 24) | (c.Red() << 16) | (c.Green() << 8) | c.Blue();
					updateDword(id, val, ChangeType::Colorization);
				}
			};

			choice->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			picker->Bind(wxEVT_COLOURPICKER_CHANGED, [update](wxColourPickerEvent&) { update(); });
			alphaSpin->Bind(wxEVT_SPINCTRL, [update](wxSpinEvent&) { update(); });
			alphaSpin->Bind(wxEVT_TEXT, [update](wxCommandEvent&) { update(); });
		};

		bindBaseColor(m_chModeBaseTransparent, m_cpBaseTransparent, m_scBaseTransparentAlpha, Settings::Id::ColorizationBaseTransparent, 0xFFFFFFFF, 0xFFFFFFFE);
		bindBaseColor(m_chModeBaseMaximized, m_cpBaseMaximized, m_scBaseMaximizedAlpha, Settings::Id::ColorizationBaseMaximized, 0xFFFFFFFF, 0xFFFFFFFE);
		bindBaseColor(m_chModeBaseOpaque, m_cpBaseOpaque, m_scBaseOpaqueAlpha, Settings::Id::ColorizationBaseOpaque, 0xFFFFFFFF, 0xFFFFFFFE);

		m_chOpaqueBlendPriority->Bind(wxEVT_CHOICE, [this, updateDword, deleteValue](wxCommandEvent& e) {
			int sel = e.GetSelection();
			if (sel == 2)
			{
				deleteValue(Settings::Id::ColorizationOpaqueBlendPriority);
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(Settings::Id::ColorizationOpaqueBlendPriority, sel, ChangeType::Colorization);
			}
		});

		auto bindOpacity = [this, updateDword, deleteValue, updateInheritance](wxChoice* ch, wxSlider* sl, Settings::Id id, DWORD themeSentinel, DWORD autoSentinel, bool propagateInheritance) {
			auto update = [this, ch, sl, id, themeSentinel, autoSentinel, updateDword, deleteValue, propagateInheritance, updateInheritance]() {
				int sel = ch->GetSelection();
				sl->Enable(sel == 2);
				if (sel == 0) {
					deleteValue(id);
					NotifySettingsChange(ChangeType::Colorization);
				}
				else if (sel == 1) updateDword(id, themeSentinel, ChangeType::Colorization);
				else updateDword(id, sl->GetValue(), ChangeType::Colorization);
				sl->SetToolTip(wxString::Format(L"%d", sl->GetValue()));
				if (propagateInheritance)
				{
					updateInheritance();
				}
			};
			ch->Bind(wxEVT_CHOICE, [update](wxCommandEvent&) { update(); });
			sl->Bind(wxEVT_SLIDER, [update](wxCommandEvent&) { update(); });
		};

		bindOpacity(m_chModeColorizationOpacity, m_slColorizationOpacity, Settings::Id::ColorizationOpacity, 0xFFFFFFFF, 0xFFFFFFFE, true);
		bindOpacity(m_chModeColorizationOpacityInactive, m_slColorizationOpacityInactive, Settings::Id::ColorizationOpacityInactive, 0xFFFFFFFF, 0xFFFFFFFE, true);
		bindOpacity(m_chModeColorizationOpacityMaximized, m_slColorizationOpacityMaximized, Settings::Id::ColorizationOpacityMaximized, 0xFFFFFFFF, 0xFFFFFFFE, false);
		bindOpacity(m_chModeColorizationOpacityInactiveMaximized, m_slColorizationOpacityInactiveMaximized, Settings::Id::ColorizationOpacityInactiveMaximized, 0xFFFFFFFF, 0xFFFFFFFE, false);

		m_slBlurBalance->Bind(wxEVT_SLIDER, [this, updateOverridableDword, setSliderTooltipValue](wxCommandEvent& e) {
			updateOverridableDword(Settings::Id::ColorizationBlurBalanceOverride, e.GetInt());
			setSliderTooltipValue(m_slBlurBalance, e.GetInt());
		});
		m_slAfterglowBalance->Bind(wxEVT_SLIDER, [this, updateOverridableDword, setSliderTooltipValue](wxCommandEvent& e) {
			updateOverridableDword(Settings::Id::ColorizationAfterglowBalanceOverride, e.GetInt());
			setSliderTooltipValue(m_slAfterglowBalance, e.GetInt());
		});
		m_slColorBalance->Bind(wxEVT_SLIDER, [this, updateOverridableDword, setSliderTooltipValue](wxCommandEvent& e) {
			updateOverridableDword(Settings::Id::ColorizationColorBalanceOverride, e.GetInt());
			setSliderTooltipValue(m_slColorBalance, e.GetInt());
		});
		m_cpAfterglow->Bind(wxEVT_COLOURPICKER_CHANGED, [this, updateOverridableDword](wxColourPickerEvent& e) {
			const DWORD currentValue = ResolveOverridableDword(
				Settings::Id::ColorizationAfterglow,
				Settings::Id::ColorizationAfterglowOverride,
				0
			).value;
			const wxColour color = e.GetColour();
			const DWORD value = (currentValue & 0xFF000000)
				| (color.Red() << 16)
				| (color.Green() << 8)
				| color.Blue();
			updateOverridableDword(Settings::Id::ColorizationAfterglowOverride, value);
		});
		m_btnPersistCompositionParameters->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
			RegistryConfig* config = GetConfigForSetting(Settings::Id::ColorizationColorBalance);
			if (!config)
			{
				return;
			}

			const std::pair<Settings::Id, int> values[]
			{
				{ Settings::Id::ColorizationBlurBalanceOverride, m_slBlurBalance->GetValue() },
				{ Settings::Id::ColorizationAfterglowBalanceOverride, m_slAfterglowBalance->GetValue() },
				{ Settings::Id::ColorizationColorBalanceOverride, m_slColorBalance->GetValue() }
			};
			for (const auto& [id, value] : values)
			{
				const std::wstring name(Settings::Get(id).name);
				TrackSettingChange(id);
				if (!CheckRegistryWrite(config->SetDword(name, static_cast<DWORD>(value)), name)) return;
			}
			SetDirty(true);
			NotifySettingsChange(ChangeType::Colorization);
			UpdateOptionStatusIcons();
		});

		m_chkGlassOverrideAccent->Bind(wxEVT_CHECKBOX, [this, updateDword, deleteValue](wxCommandEvent& e) {
			if (!e.IsChecked())
			{
				deleteValue(Settings::Id::GlassOverrideAccent);
				NotifySettingsChange(ChangeType::Colorization);
			}
			else
			{
				updateDword(Settings::Id::GlassOverrideAccent, 1, ChangeType::Colorization);
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
		if (!RevertSettings())
		{
			event.Veto();
			return;
		}
		event.Skip();
	}
}
