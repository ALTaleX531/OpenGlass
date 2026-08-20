#include "pch.h"
#include "MainFrame.hpp"
#include "ColorSwatchButton.hpp"
#include "Symbols.hpp"
#include "UiControls.hpp"
#include "BlurSettings.hpp"

namespace OpenGlass
{
	namespace
	{
		wxCollapsiblePane* AddCollapsibleSection(
			wxScrolledWindow* panel,
			wxSizer* parentSizer,
			const wxString& label
		)
		{
			auto* pane = new wxCollapsiblePane(
				panel,
				wxID_ANY,
				label,
				wxDefaultPosition,
				wxDefaultSize,
				wxCP_DEFAULT_STYLE | wxCP_NO_TLW_RESIZE
			);
			pane->Collapse(true);
			if (wxControl* header = pane->GetControlWidget())
			{
				if (wxSizer* headerSizer = header->GetContainingSizer())
				{
					if (wxSizerItem* headerItem = headerSizer->GetItem(header))
					{
						headerItem->SetFlag(headerItem->GetFlag() & ~wxLEFT);
						pane->InvalidateBestSize();
					}
				}
			}
			parentSizer->Add(pane, 0, wxEXPAND | wxRIGHT | wxBOTTOM, 5);
			pane->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED, [panel](wxCollapsiblePaneEvent&)
			{
				panel->Layout();
				panel->FitInside();
			});
			return pane;
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

		void AddAdminRequiredTip(wxPanel* panel, wxBoxSizer* sizer, const wxString& message)
		{
			if (!panel || !sizer)
			{
				return;
			}

			auto* tipPanel = new wxPanel(panel);
			tipPanel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOBK));

			auto* tipSizer = new wxBoxSizer(wxHORIZONTAL);
			auto* icon = new wxStaticBitmap(
				tipPanel,
				wxID_ANY,
				wxArtProvider::GetBitmap(wxART_WARNING, wxART_MESSAGE_BOX, wxSize(16, 16))
			);
			tipSizer->Add(icon, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

			auto* label = new wxStaticText(tipPanel, wxID_ANY, message);
			label->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_INFOTEXT));
			tipSizer->Add(label, 1, wxEXPAND);

			tipPanel->SetSizer(tipSizer);
			tipPanel->Bind(wxEVT_SIZE, [label, message](wxSizeEvent& event)
			{
				WrapStaticTextToParentWidth(label, message);
				event.Skip();
			});
			tipPanel->CallAfter([tipPanel, label, message]
			{
				tipPanel->Layout();
				WrapStaticTextToParentWidth(label, message);
			});
			sizer->Insert(0, tipPanel, 0, wxEXPAND | wxALL, 8);

			for (wxWindowList::compatibility_iterator node = panel->GetChildren().GetFirst(); node; node = node->GetNext())
			{
				wxWindow* child = node->GetData();
				if (child != tipPanel)
				{
					child->Enable(false);
				}
			}
		}
	}

	void MainFrame::CreateSystemTab()
	{
		wxPanel* panel = new wxPanel(m_notebook);
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
		
		wxStaticBoxSizer* globalGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"Global");
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkDisableGlassOnBattery = new wxCheckBox(panel, wxID_ANY, L"Disable transparency on battery");
			row->Add(m_chkDisableGlassOnBattery, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::DisableGlassOnBattery);
			globalGroup->Add(row, 0, wxEXPAND | wxALL, 2);
		}
		m_chkDisableGlassOnBattery->SetToolTip(L"If checked, glass effect will be opaque when energy/battery saver is on.");
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkGlassSafetyZone = new wxCheckBox(panel, wxID_ANY, L"Disable glass safety zone");
			row->Add(m_chkGlassSafetyZone, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::GlassSafetyZoneMode);
			globalGroup->Add(row, 0, wxEXPAND | wxALL, 2);
		}
		m_chkGlassSafetyZone->SetToolTip(L"Disabling this may cause visual artifacts. Default is Enabled (Unchecked).");

		// Disabled Hooks
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(new wxStaticText(panel, wxID_ANY, L"Disabled hooks (advanced): *"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::DisabledHooks);
			globalGroup->Add(row, 0, wxLEFT | wxTOP, 2);
		}
		wxArrayString hooks;
		hooks.Add(L"Caption text handler");    // 0x1
		hooks.Add(L"Accent overrider");        // 0x2
		hooks.Add(L"Glass frame handler");     // 0x4
		hooks.Add(L"Glass reflection handler");// 0x8
		hooks.Add(L"Caption metrics tweaker");// 0x10
		m_clDisabledHooks = new wxCheckListBox(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, hooks);
		globalGroup->Add(m_clDisabledHooks, 0, wxEXPAND | wxALL, 2);
		m_clDisabledHooks->SetToolTip(L"Controls which module's hooks are disabled.\nDo not modify unless maintaining compatibility.");

		sizer->Add(globalGroup, 0, wxEXPAND | wxALL, 2);
		if (!m_isAdmin)
		{
			AddAdminRequiredTip(panel, sizer, L"Requires Administrator privileges to edit system-wide settings.");
		}

		panel->SetSizer(sizer);
		panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); 
		m_notebook->AddPage(panel, L"System");
	}

	void MainFrame::CreateDiagnosticsTab()
	{
		wxScrolledWindow* panel = new wxScrolledWindow(m_notebook);
		panel->SetScrollRate(5, 5);
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

		wxStaticBoxSizer* transparencyGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"Transparency status");
		const wxString transparencyDescriptionText = L"Current conditions that can make glass opaque.";
		wxStaticText* transparencyDescription = new wxStaticText(
			panel,
			wxID_ANY,
			transparencyDescriptionText
		);
		transparencyGroup->Add(transparencyDescription, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

		wxFlexGridSizer* transparencyGrid = new wxFlexGridSizer(2, 8, 12);
		transparencyGrid->AddGrowableCol(1, 1);
		auto addTransparencyRow = [&](const wxString& label, wxStaticText*& value)
		{
			wxStaticText* key = new wxStaticText(panel, wxID_ANY, label);
			wxFont keyFont = key->GetFont();
			keyFont.SetWeight(wxFONTWEIGHT_BOLD);
			key->SetFont(keyFont);
			transparencyGrid->Add(key, 0, wxALIGN_TOP);
			value = new wxStaticText(panel, wxID_ANY, L"Reading...");
			transparencyGrid->Add(value, 1, wxEXPAND);
		};
		addTransparencyRow(L"Windows transparency", m_lblWindowsTransparencyStatus);
		addTransparencyRow(L"Opaque blend", m_lblOpaqueBlendStatus);
		addTransparencyRow(L"Power mode", m_lblPowerModeStatus);
		addTransparencyRow(L"Opaque on saver", m_lblDisableOnBatteryStatus);
		wxStaticText* resultKey = new wxStaticText(panel, wxID_ANY, L"Result");
		wxFont resultKeyFont = resultKey->GetFont();
		resultKeyFont.SetWeight(wxFONTWEIGHT_BOLD);
		resultKey->SetFont(resultKeyFont);
		transparencyGrid->Add(resultKey, 0, wxALIGN_TOP);
		wxBoxSizer* transparencyResultRow = new wxBoxSizer(wxHORIZONTAL);
		m_bmpEffectiveTransparencyWarning = new wxStaticBitmap(
			panel,
			wxID_ANY,
			wxArtProvider::GetBitmap(wxART_WARNING, wxART_MESSAGE_BOX, wxSize(16, 16))
		);
		m_bmpEffectiveTransparencyWarning->Hide();
		transparencyResultRow->Add(m_bmpEffectiveTransparencyWarning, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
		m_lblEffectiveTransparencyStatus = new wxStaticText(panel, wxID_ANY, L"Reading...");
		transparencyResultRow->Add(m_lblEffectiveTransparencyStatus, 1, wxEXPAND);
		transparencyGrid->Add(transparencyResultRow, 1, wxEXPAND);
		transparencyGroup->Add(transparencyGrid, 0, wxEXPAND | wxALL, 8);

		wxBoxSizer* transparencyButtonRow = new wxBoxSizer(wxHORIZONTAL);
		transparencyButtonRow->AddStretchSpacer();
		m_btnRefreshTransparencyDiagnostics = new wxButton(panel, wxID_ANY, L"Refresh");
		transparencyButtonRow->Add(m_btnRefreshTransparencyDiagnostics, 0);
		transparencyGroup->Add(transparencyButtonRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

		wxStaticBoxSizer* downloadGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"Symbols");
		const wxString descriptionText = L"Download the public PDB files required for the current Windows build. The default runtime cache is %ProgramData%\\OpenGlass\\symbols; another folder may be selected for manual use.";
		wxStaticText* description = new wxStaticText(
			panel,
			wxID_ANY,
			descriptionText
		);
		downloadGroup->Add(description, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

		wxFlexGridSizer* infoGrid = new wxFlexGridSizer(2, 8, 12);
		infoGrid->AddGrowableCol(1, 1);
		auto addInfoRow = [&](const wxString& label, wxWindow* value)
		{
			wxStaticText* key = new wxStaticText(panel, wxID_ANY, label);
			wxFont keyFont = key->GetFont();
			keyFont.SetWeight(wxFONTWEIGHT_BOLD);
			key->SetFont(keyFont);
			infoGrid->Add(key, 0, wxALIGN_TOP);
			infoGrid->Add(value, 1, wxEXPAND);
		};

		wxStaticText* modulesValue = new wxStaticText(panel, wxID_ANY, L"uDWM.dll, dwmcore.dll");
		addInfoRow(L"Modules", modulesValue);

		m_dpSymbolCacheDirectory = new wxDirPickerCtrl(
			panel,
			wxID_ANY,
			GetSymbolCacheDirectory(),
			L"Select a symbol cache folder",
			wxDefaultPosition,
			wxDefaultSize,
			wxDIRP_USE_TEXTCTRL
		);
		m_dpSymbolCacheDirectory->Enable(m_isAdmin);
		m_dpSymbolCacheDirectory->SetToolTip(L"Choose where downloaded PDB files are stored. The folder is created when needed.");
		addInfoRow(L"Cache path", m_dpSymbolCacheDirectory);

		wxStaticText* timeoutValue = new wxStaticText(
			panel,
			wxID_ANY,
			wxString::Format(L"%d seconds per request", SymbolDownloadTimeoutSeconds)
		);
		addInfoRow(L"Network timeout", timeoutValue);
		downloadGroup->Add(infoGrid, 0, wxEXPAND | wxALL, 8);

		wxBoxSizer* buttonRow = new wxBoxSizer(wxHORIZONTAL);
		buttonRow->AddStretchSpacer();
		m_btnDownloadSymbols = new wxButton(
			panel,
			wxID_ANY,
			L"Download symbols"
		);
		buttonRow->Add(m_btnDownloadSymbols, 0, wxRIGHT, 6);

		m_btnCancelSymbolDownload = new wxButton(panel, wxID_ANY, L"Cancel");
		m_btnCancelSymbolDownload->Enable(false);
		buttonRow->Add(m_btnCancelSymbolDownload, 0);
		downloadGroup->Add(buttonRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

		wxStaticBoxSizer* statusGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"Symbol download status");
		m_gaugeSymbolDownload = new wxGauge(panel, wxID_ANY, 100);
		statusGroup->Add(m_gaugeSymbolDownload, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

		m_lblSymbolDownloadPhase = new wxStaticText(panel, wxID_ANY, L"Idle");
		wxFont phaseFont = m_lblSymbolDownloadPhase->GetFont();
		phaseFont.SetWeight(wxFONTWEIGHT_BOLD);
		m_lblSymbolDownloadPhase->SetFont(phaseFont);
		statusGroup->Add(m_lblSymbolDownloadPhase, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

		m_symbolDownloadDetailText = L"Ready to download uDWM.dll and dwmcore.dll.";
		m_lblSymbolDownloadDetail = new wxStaticText(panel, wxID_ANY, m_symbolDownloadDetailText);
		statusGroup->Add(m_lblSymbolDownloadDetail, 0, wxEXPAND | wxALL, 8);

		m_pnlSymbolDownloadResult = new wxPanel(panel);
		wxBoxSizer* resultRow = new wxBoxSizer(wxHORIZONTAL);
		m_bmpSymbolDownloadResult = new wxStaticBitmap(
			m_pnlSymbolDownloadResult,
			wxID_ANY,
			wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_MESSAGE_BOX, wxSize(16, 16))
		);
		m_bmpSymbolDownloadResult->SetMinSize(wxSize(16, 16));
		resultRow->Add(m_bmpSymbolDownloadResult, 0, wxALIGN_TOP | wxRIGHT, 8);

		m_lblSymbolDownloadResult = new wxStaticText(m_pnlSymbolDownloadResult, wxID_ANY, wxEmptyString);
		resultRow->Add(m_lblSymbolDownloadResult, 1, wxEXPAND);

		m_pnlSymbolDownloadResult->SetSizer(resultRow);
		m_pnlSymbolDownloadResult->Hide();
		statusGroup->Add(m_pnlSymbolDownloadResult, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

		wxStaticBoxSizer* dumpGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"WER crash dumps");
		const wxString dumpDescriptionText = L"Collect full user-mode crash dumps for dwm.exe using its per-application Windows Error Reporting configuration. Full dumps can be large. The default folder is %ProgramData%\\OpenGlass\\dumps.";
		wxStaticText* dumpDescription = new wxStaticText(panel, wxID_ANY, dumpDescriptionText);
		dumpGroup->Add(dumpDescription, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

		wxFlexGridSizer* dumpGrid = new wxFlexGridSizer(2, 8, 12);
		dumpGrid->AddGrowableCol(1, 1);
		wxStaticText* dumpFolderLabel = new wxStaticText(panel, wxID_ANY, L"Dump folder");
		wxFont dumpFolderFont = dumpFolderLabel->GetFont();
		dumpFolderFont.SetWeight(wxFONTWEIGHT_BOLD);
		dumpFolderLabel->SetFont(dumpFolderFont);
		dumpGrid->Add(dumpFolderLabel, 0, wxALIGN_CENTER_VERTICAL);
		m_dpDwmCrashDumpFolder = new wxDirPickerCtrl(
			panel,
			wxID_ANY,
			GetDefaultDwmCrashDumpFolder(),
			L"Select a folder for DWM crash dumps",
			wxDefaultPosition,
			wxDefaultSize,
			wxDIRP_USE_TEXTCTRL
		);
		m_dpDwmCrashDumpFolder->Enable(m_isAdmin);
		m_dpDwmCrashDumpFolder->SetToolTip(L"The folder ACL must allow WER to collect a dump for the crashing DWM process. Relative paths are resolved beside OpenGlassGUI.exe.");
		dumpGrid->Add(m_dpDwmCrashDumpFolder, 1, wxEXPAND);
		dumpGroup->Add(dumpGrid, 0, wxEXPAND | wxALL, 8);

		m_dwmCrashDumpStatusText = L"Reading the current dwm.exe dump configuration...";
		m_lblDwmCrashDumpStatus = new wxStaticText(panel, wxID_ANY, m_dwmCrashDumpStatusText);
		dumpGroup->Add(m_lblDwmCrashDumpStatus, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

		wxBoxSizer* dumpButtonRow = new wxBoxSizer(wxHORIZONTAL);
		dumpButtonRow->AddStretchSpacer();
		m_btnEnableDwmCrashDumps = new wxButton(panel, wxID_ANY, L"Enable full dumps");
		m_btnEnableDwmCrashDumps->Enable(m_isAdmin);
		dumpButtonRow->Add(m_btnEnableDwmCrashDumps, 0, wxRIGHT, 6);
		m_btnDisableDwmCrashDumps = new wxButton(panel, wxID_ANY, L"Disable dumps");
		m_btnDisableDwmCrashDumps->Enable(false);
		m_btnDisableDwmCrashDumps->SetToolTip(L"Remove the per-application LocalDumps\\dwm.exe registry key. System-wide WER settings are not changed.");
		dumpButtonRow->Add(m_btnDisableDwmCrashDumps, 0);
		dumpGroup->Add(dumpButtonRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);

		sizer->Add(transparencyGroup, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
		sizer->Add(downloadGroup, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
		sizer->Add(statusGroup, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
		sizer->Add(dumpGroup, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
		sizer->AddStretchSpacer();
		if (!m_isAdmin)
		{
			AddAdminRequiredTip(panel, sizer, L"Requires Administrator privileges to download symbols or change WER crash dump settings.");
		}

		panel->SetSizer(sizer);
		panel->Bind(wxEVT_SIZE, [this, panel, transparencyDescription, transparencyDescriptionText, description, descriptionText, dumpDescription, dumpDescriptionText](wxSizeEvent& event)
		{
			WrapStaticTextToParentWidth(transparencyDescription, transparencyDescriptionText);
			WrapStaticTextToParentWidth(description, descriptionText);
			WrapStaticTextToParentWidth(m_lblSymbolDownloadDetail, m_symbolDownloadDetailText);
			WrapStaticTextToParentWidth(m_lblSymbolDownloadResult, m_symbolDownloadResultText);
			WrapStaticTextToParentWidth(dumpDescription, dumpDescriptionText);
			WrapStaticTextToParentWidth(m_lblDwmCrashDumpStatus, m_dwmCrashDumpStatusText);
			panel->FitInside();
			event.Skip();
		});
		panel->CallAfter([this, panel, transparencyDescription, transparencyDescriptionText, description, descriptionText, dumpDescription, dumpDescriptionText]
		{
			panel->Layout();
			WrapStaticTextToParentWidth(transparencyDescription, transparencyDescriptionText);
			WrapStaticTextToParentWidth(description, descriptionText);
			WrapStaticTextToParentWidth(m_lblSymbolDownloadDetail, m_symbolDownloadDetailText);
			WrapStaticTextToParentWidth(m_lblSymbolDownloadResult, m_symbolDownloadResultText);
			WrapStaticTextToParentWidth(dumpDescription, dumpDescriptionText);
			WrapStaticTextToParentWidth(m_lblDwmCrashDumpStatus, m_dwmCrashDumpStatusText);
			panel->Layout();
			panel->FitInside();
		});
		RefreshTransparencyDiagnostics();
		RefreshDwmCrashDumpConfiguration();
		panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
		m_notebook->AddPage(panel, L"Diagnostics");
	}

	void MainFrame::CreateThemeTab()
	{
		wxScrolledWindow* panel = new wxScrolledWindow(m_notebook);
		panel->SetScrollRate(5, 5);
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

		// Theme Resources Group
		wxStaticBoxSizer* themeGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"Theme");

		// Custom Theme Atlas Row
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkCustomThemeAtlas = new wxCheckBox(panel, wxID_ANY, L"Theme atlas image");
			m_chkCustomThemeAtlas->SetMinSize(wxSize(300, -1));
			row->Add(m_chkCustomThemeAtlas, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			m_fpCustomThemeAtlas = new wxFilePickerCtrl(panel, wxID_ANY, wxEmptyString, L"Select Theme atlas", L"PNG files (*.png)|*.png", wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL | wxFLP_OPEN | wxFLP_FILE_MUST_EXIST);
			row->Add(m_fpCustomThemeAtlas, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::CustomThemeAtlas);
			AddPathWarningIcon(panel, row, m_fpCustomThemeAtlas, m_chkCustomThemeAtlas, L"Theme atlas");
			themeGroup->Add(row, 0, wxEXPAND | wxALL, 2);
		}

		// Export Button
		m_btnExportAtlas = new wxButton(panel, wxID_ANY, L"Export current theme atlas");
		themeGroup->Add(m_btnExportAtlas, 0, wxALL, 2);

		sizer->Add(themeGroup, 0, wxEXPAND | wxALL, 2);

		// Reflection Group
		wxStaticBoxSizer* reflectionGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"Reflection");

		// Custom Reflection Row
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkCustomThemeReflection = new wxCheckBox(panel, wxID_ANY, L"Glass reflection image");
			m_chkCustomThemeReflection->SetMinSize(wxSize(300, -1));
			row->Add(m_chkCustomThemeReflection, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			m_fpCustomThemeReflection = new wxFilePickerCtrl(panel, wxID_ANY, wxEmptyString, L"Select reflection texture", L"Image files (*.png;*.jpg;*.bmp)|*.png;*.jpg;*.bmp", wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL | wxFLP_OPEN | wxFLP_FILE_MUST_EXIST);
			row->Add(m_fpCustomThemeReflection, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::CustomThemeReflection);
			AddPathWarningIcon(panel, row, m_fpCustomThemeReflection, m_chkCustomThemeReflection, L"Reflection texture");
			reflectionGroup->Add(row, 0, wxEXPAND | wxALL, 2);
		}

		// Reflection Intensity
		m_slReflectionIntensity = new NativeSlider(panel, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_AUTOTICKS);
		AddProperty(panel, reflectionGroup, L"Glass reflection intensity:", m_slReflectionIntensity, Settings::Id::ColorizationGlassReflectionIntensity);

		// Reflection Parallax
		m_slReflectionParallax = new NativeSlider(panel, wxID_ANY, 13, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_AUTOTICKS);
		AddProperty(panel, reflectionGroup, L"Glass parallax intensity:", m_slReflectionParallax, Settings::Id::ColorizationGlassReflectionParallaxIntensity);

		// Reflection Policy
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(new wxStaticText(panel, wxID_ANY, L"Glass reflection policy:", wxDefaultPosition, wxSize(300, -1)), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			m_chkReflectionPolicyTitlebar = new wxCheckBox(panel, wxID_ANY, L"Titlebar");
			row->Add(m_chkReflectionPolicyTitlebar, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
			m_chkReflectionPolicyPeek = new wxCheckBox(panel, wxID_ANY, L"Aero Peek");
			row->Add(m_chkReflectionPolicyPeek, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
			m_chkReflectionPolicySnap = new wxCheckBox(panel, wxID_ANY, L"Aero Snap");
			row->Add(m_chkReflectionPolicySnap, 0, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::ColorizationGlassReflectionPolicy);
			reflectionGroup->Add(row, 0, wxEXPAND | wxALL, 2);
		}
		// Legacy (kept for reference):
		// wxArrayString policies;
		// policies.Add(L"Titlebar");    // 1<<0
		// policies.Add(L"Aero Peek");   // 1<<2
		// policies.Add(L"Aero Snap");   // 1<<3
		// m_clReflectionPolicy = new wxCheckListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(-1, 60), policies);
		// AddProperty(panel, reflectionGroup, L"Glass Reflection Policy:", m_clReflectionPolicy, L"ColorizationGlassReflectionPolicy");
		
		// Reflection Opacity & Variants
		auto* reflectionOpacityPane = AddCollapsibleSection(
			panel,
			reflectionGroup,
			L"Advanced opacity settings"
		);
		wxWindow* reflectionOpacityPanel = reflectionOpacityPane->GetPane();
		auto* reflectionOpacitySizer = new wxBoxSizer(wxVERTICAL);

		auto addRefOpacity = [&](const wxString& label, wxChoice*& ch, wxSlider*& sl) {
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText* st = new wxStaticText(reflectionOpacityPanel, wxID_ANY, label, wxDefaultPosition, wxSize(300, -1)); // Aligned width
			row->Add(st, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			wxArrayString choices;
			choices.Add(L"Auto");
			choices.Add(L"From theme");
			choices.Add(L"Custom");
			ch = new wxChoice(reflectionOpacityPanel, wxID_ANY, wxDefaultPosition, wxSize(120, -1), choices);
			ch->SetSelection(0);
			row->Add(ch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			sl = new NativeSlider(reflectionOpacityPanel, wxID_ANY, 50, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_AUTOTICKS);
			sl->Disable();
			row->Add(sl, 1, wxALIGN_CENTER_VERTICAL); // Flex 1 for slider

			reflectionOpacitySizer->Add(row, 0, wxEXPAND | wxALL, 2);
		};

		addRefOpacity(L"Base opacity:", m_chModeReflectionOpacity, m_slReflectionOpacity);
		AddOptionStatus(reflectionOpacityPanel, dynamic_cast<wxBoxSizer*>(reflectionOpacitySizer->GetItem(reflectionOpacitySizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationGlassReflectionOpacity);
		addRefOpacity(L"Base inactive opacity:", m_chModeReflectionOpacityInactive, m_slReflectionOpacityInactive);
		AddOptionStatus(reflectionOpacityPanel, dynamic_cast<wxBoxSizer*>(reflectionOpacitySizer->GetItem(reflectionOpacitySizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationGlassReflectionOpacityInactive);
		addRefOpacity(L"Base maximized opacity:", m_chModeReflectionOpacityMaximized, m_slReflectionOpacityMaximized);
		AddOptionStatus(reflectionOpacityPanel, dynamic_cast<wxBoxSizer*>(reflectionOpacitySizer->GetItem(reflectionOpacitySizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationGlassReflectionOpacityMaximized);
		addRefOpacity(L"Base inactive maximized opacity:", m_chModeReflectionOpacityInactiveMaximized, m_slReflectionOpacityInactiveMaximized);
		AddOptionStatus(reflectionOpacityPanel, dynamic_cast<wxBoxSizer*>(reflectionOpacitySizer->GetItem(reflectionOpacitySizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationGlassReflectionOpacityInactiveMaximized);
		reflectionOpacityPanel->SetSizer(reflectionOpacitySizer);

		sizer->Add(reflectionGroup, 0, wxEXPAND | wxALL, 2);

		// Material Group
		wxStaticBoxSizer* materialGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"Material");

		// CustomThemeMaterial
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkCustomThemeMaterial = new wxCheckBox(panel, wxID_ANY, L"Custom material texture");
			m_chkCustomThemeMaterial->SetMinSize(wxSize(300, -1));
			row->Add(m_chkCustomThemeMaterial, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			m_fpCustomThemeMaterial = new wxFilePickerCtrl(panel, wxID_ANY, wxEmptyString, L"Select material texture", L"Image files (*.png;*.jpg;*.bmp)|*.png;*.jpg;*.bmp", wxDefaultPosition, wxDefaultSize, wxFLP_USE_TEXTCTRL | wxFLP_OPEN | wxFLP_FILE_MUST_EXIST);
			row->Add(m_fpCustomThemeMaterial, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::CustomThemeMaterial);
			AddPathWarningIcon(panel, row, m_fpCustomThemeMaterial, m_chkCustomThemeMaterial, L"Material texture");
			materialGroup->Add(row, 0, wxEXPAND | wxALL, 2);
		}

		// Material Opacity
		m_slMaterialOpacity = new NativeSlider(panel, wxID_ANY, 0, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_AUTOTICKS);
		AddProperty(panel, materialGroup, L"Material opacity:", m_slMaterialOpacity, Settings::Id::MaterialOpacity);

		sizer->Add(materialGroup, 0, wxEXPAND | wxALL, 2);

		panel->SetSizer(sizer);
		panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); 
		m_notebook->AddPage(panel, L"Theme");
	}

	void MainFrame::CreateAppearanceTab()
	{
		wxScrolledWindow* panel = new wxScrolledWindow(m_notebook);
		panel->SetScrollRate(5, 5);
		panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); 
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

		// General Group
		wxStaticBoxSizer* generalGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"General");
		
		// CCustomBlur's projected BlurAmount. BlurDeviation is its historical registry encoding.
		m_slBlurAmount = new NativeSlider(
			panel,
			wxID_ANY,
			BlurSettings::DecodeGuiBlurAmount(BlurSettings::DefaultEncodedDeviation),
			BlurSettings::GuiMinimumBlurAmount,
			BlurSettings::GuiMaximumBlurAmount,
			wxDefaultPosition,
			wxDefaultSize,
			wxSL_HORIZONTAL | wxSL_AUTOTICKS
		);
		AddProperty(panel, generalGroup, L"Blur amount:", m_slBlurAmount, Settings::Id::BlurDeviation);
		// Blur Optimization
		wxArrayString blurOpts;
		blurOpts.Add(L"Speed first");
		blurOpts.Add(L"Balanced");
		blurOpts.Add(L"Quality first");
		m_chBlurOptimization = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, blurOpts);
		AddProperty(panel, generalGroup, L"Blur optimization:", m_chBlurOptimization, Settings::Id::BlurOptimization);

		// Renderer Settings
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkUseD3D = new wxCheckBox(panel, wxID_ANY, L"Use Direct3D renderer");
			row->Add(m_chkUseD3D, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::UseDirect3DRendering);
			generalGroup->Add(row, 0, wxEXPAND | wxALL, 2);
		}
		m_chkUseD3D->SetToolTip(L"Use the Direct3D 11 backend. It ignores BlurDeviation and BlurOptimization and uses a separate fixed Gaussian standard deviation of 3 px.");

		sizer->Add(generalGroup, 0, wxEXPAND | wxALL, 2);
		// Window Group
		wxStaticBoxSizer* geometryGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"Window");

		// Round Rect Radius
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText* label = new wxStaticText(panel, wxID_ANY, L"Glass geometry radius:", wxDefaultPosition, wxSize(300, -1));
			row->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			wxArrayString profiles;
			profiles.Add(L"Windows 8 style (vanilla)");
			profiles.Add(L"Windows 7 style");
			profiles.Add(L"Custom");
			m_chRoundRectProfile = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, profiles);
			row->Add(m_chRoundRectProfile, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			m_scRoundRectRadius = new wxSpinCtrl(panel, wxID_ANY);
			m_scRoundRectRadius->SetRange(0, 50);
			row->Add(m_scRoundRectRadius, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::RoundRectRadius);
			geometryGroup->Add(row, 0, wxEXPAND | wxALL, 2);
		}

		// Caption Buttons
		wxArrayString captionStyles;
		captionStyles.Add(L"Vanilla style");
		captionStyles.Add(L"Windows Vista style");
		captionStyles.Add(L"Windows 7 style");
		captionStyles.Add(L"Windows 8 style");
		m_chCaptionButtons = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, captionStyles);
		AddProperty(panel, geometryGroup, L"Caption buttons style:", m_chCaptionButtons, Settings::Id::CaptionButtons);

		// Disable Modern Borders
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkDisableModernBorders = new wxCheckBox(panel, wxID_ANY, L"Disable modern borders");
			row->Add(m_chkDisableModernBorders, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::DisableModernBorders);
			geometryGroup->Add(row, 0, wxEXPAND | wxALL, 2);
		}

		sizer->Add(geometryGroup, 0, wxEXPAND | wxALL, 2);

		// Caption Group
		wxStaticBoxSizer* textGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"Caption");

		// Center Caption
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText* label = new wxStaticText(panel, wxID_ANY, L"Text centering:", wxDefaultPosition, wxSize(300, -1));
			row->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			wxArrayString modes;
			modes.Add(L"Disabled (vanilla)");
			modes.Add(L"Regular");
			modes.Add(L"Windows 8 style");
			m_chCenterCaption = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, modes);
			row->Add(m_chCenterCaption, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::CenterCaption);
			textGroup->Add(row, 0, wxEXPAND | wxALL, 2);
		}

		// Text Glow Mode
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText* label = new wxStaticText(panel, wxID_ANY, L"Text glow mode:", wxDefaultPosition, wxSize(300, -1));
			row->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			wxArrayString glowModes;
			glowModes.Add(L"No glow");
			glowModes.Add(L"Use theme atlas");
			glowModes.Add(L"Use theme atlas (opacity)");
			glowModes.Add(L"Composited (theme settings)");
			m_chTextGlowMode = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, glowModes);
			row->Add(m_chTextGlowMode, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			row->Add(new wxStaticText(panel, wxID_ANY, L"Size:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 3);
			m_scTextGlowSize = new wxSpinCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80, -1));
			m_scTextGlowSize->SetRange(0, 100);
			row->Add(m_scTextGlowSize, 0, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(panel, row, Settings::Id::TextGlowMode);

			textGroup->Add(row, 0, wxEXPAND | wxALL, 2);
		}
		
		// Text color overrides
		auto* textColorOverridesPane = AddCollapsibleSection(
			panel,
			textGroup,
			L"Text color overrides"
		);
		wxWindow* textColorOverridesPanel = textColorOverridesPane->GetPane();
		auto* textColorOverridesSizer = new wxBoxSizer(wxVERTICAL);

		// Helper to add choice + picker
		auto addColorOverride = [&](wxChoice*& ch, wxColourPickerCtrl*& cp, const wxString& label) {
			wxBoxSizer* r = new wxBoxSizer(wxHORIZONTAL);
			r->Add(new wxStaticText(textColorOverridesPanel, wxID_ANY, label, wxDefaultPosition, wxSize(180, -1)), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			wxArrayString modes;
			modes.Add(L"Auto");
			modes.Add(L"From theme");
			modes.Add(L"Custom");
			modes.Add(L"System determined");
			ch = new wxChoice(textColorOverridesPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, modes);
			r->Add(ch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			cp = new wxColourPickerCtrl(textColorOverridesPanel, wxID_ANY);
			cp->Enable(false); // Default disabled
			r->Add(cp, 1, wxALIGN_CENTER_VERTICAL);
			textColorOverridesSizer->Add(r, 0, wxEXPAND | wxALL, 2);
		};

		addColorOverride(m_chModeColorCaption, m_cpColorCaption, L"Active:");
		AddOptionStatus(textColorOverridesPanel, dynamic_cast<wxBoxSizer*>(textColorOverridesSizer->GetItem(textColorOverridesSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationColorCaption);
		addColorOverride(m_chModeColorCaptionInactive, m_cpColorCaptionInactive, L"Inactive:");
		AddOptionStatus(textColorOverridesPanel, dynamic_cast<wxBoxSizer*>(textColorOverridesSizer->GetItem(textColorOverridesSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationColorCaptionInactive);
		addColorOverride(m_chModeColorCaptionMaximized, m_cpColorCaptionMaximized, L"Active maximized:");
		AddOptionStatus(textColorOverridesPanel, dynamic_cast<wxBoxSizer*>(textColorOverridesSizer->GetItem(textColorOverridesSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationColorCaptionMaximized);
		addColorOverride(m_chModeColorCaptionInactiveMaximized, m_cpColorCaptionInactiveMaximized, L"Inactive maximized:");
		AddOptionStatus(textColorOverridesPanel, dynamic_cast<wxBoxSizer*>(textColorOverridesSizer->GetItem(textColorOverridesSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationColorCaptionInactiveMaximized);
		textColorOverridesPanel->SetSizer(textColorOverridesSizer);

		sizer->Add(textGroup, 0, wxEXPAND | wxALL, 2);

		wxStaticBoxSizer* accentGroup = new wxStaticBoxSizer(wxVERTICAL, panel, L"Accent");
		wxBoxSizer* accentRow = new wxBoxSizer(wxHORIZONTAL);
		m_chkGlassOverrideAccent = new wxCheckBox(panel, wxID_ANY, L"Override accent");
		m_chkGlassOverrideAccent->SetToolTip(L"Overrides accent blur surfaces with OpenGlass effects.");
		accentRow->Add(m_chkGlassOverrideAccent, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
		AddOptionStatus(panel, accentRow, Settings::Id::GlassOverrideAccent);
		accentGroup->Add(accentRow, 0, wxEXPAND | wxALL, 2);
		sizer->Add(accentGroup, 0, wxEXPAND | wxALL, 2);

		panel->SetSizer(sizer);
		m_notebook->AddPage(panel, L"Appearance");
	}

	void MainFrame::CreateGlassColorsTab()
	{
		// Use ScrolledWindow for large content
		wxScrolledWindow* panel = new wxScrolledWindow(m_notebook);
		panel->SetScrollRate(5, 5);
		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
		m_glassColorsPanel = panel;
		m_glassColorsRootSizer = sizer;

		// Glass Type
		wxArrayString glassTypes;
		glassTypes.Add(L"Windows Vista Aero");
		glassTypes.Add(L"Windows 7 Aero");
		m_rbGlassType = new wxRadioBox(panel, wxID_ANY, L"Glass type", wxDefaultPosition, wxDefaultSize, glassTypes, 2, wxRA_SPECIFY_COLS);
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(m_rbGlassType, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::GlassType);
			sizer->Add(row, 0, wxALL, 5);
		}

		// Built-in Vista and Windows 7 color presets. Both groups are created once;
		// UpdateUIVisibility shows only the family selected above.
		m_colorPresetsGroupSizer = new wxStaticBoxSizer(wxVERTICAL, panel, L"Color presets");
		m_vistaPresetSizer = new wxWrapSizer(wxHORIZONTAL, wxREMOVE_LEADING_SPACES);
		m_windows7PresetSizer = new wxWrapSizer(wxHORIZONTAL, wxREMOVE_LEADING_SPACES);

		auto addPresetButtons = [this, panel](
			wxWrapSizer* presetSizer,
			std::span<const ColorizationPresets::Preset> presets
		) {
			for (const auto& preset : presets)
			{
				const wxString label{ preset.name.data(), preset.name.size() };
				auto* button = new ColorSwatchButton(
					panel,
					wxID_ANY,
					label,
					preset.argb
				);
				auto* caption = new wxStaticText(
					panel,
					wxID_ANY,
					label,
					wxDefaultPosition,
					FromDIP(wxSize(64, -1)),
					wxALIGN_CENTER_HORIZONTAL
				);
				auto* cell = new wxBoxSizer(wxVERTICAL);
				cell->Add(button, 0, wxALIGN_CENTER_HORIZONTAL);
				cell->Add(caption, 0, wxEXPAND | wxTOP, 2);
				presetSizer->Add(cell, 0, wxALL, 1);
				m_presetButtons.emplace_back(&preset, button);
			}
		};

		addPresetButtons(m_vistaPresetSizer, ColorizationPresets::Get(ColorizationPresets::Family::Vista));
		addPresetButtons(m_windows7PresetSizer, ColorizationPresets::Get(ColorizationPresets::Family::Windows7));
		auto addCustomColorButton = [this, panel](wxWrapSizer* presetSizer) {
			constexpr DWORD InitialColor = 0xFF000000;
			auto* button = new ColorSwatchButton(panel, wxID_ANY, L"Custom", InitialColor);
			auto* caption = new wxStaticText(
				panel,
				wxID_ANY,
				L"Custom",
				wxDefaultPosition,
				FromDIP(wxSize(64, -1)),
				wxALIGN_CENTER_HORIZONTAL
			);
			auto* cell = new wxBoxSizer(wxVERTICAL);
			cell->Add(button, 0, wxALIGN_CENTER_HORIZONTAL);
			cell->Add(caption, 0, wxEXPAND | wxTOP, 2);
			presetSizer->Add(cell, 0, wxALL, 1);
			m_customColorButtons.push_back(button);
		};
		addCustomColorButton(m_vistaPresetSizer);
		addCustomColorButton(m_windows7PresetSizer);
		m_colorPresetsGroupSizer->Add(m_vistaPresetSizer, 0, wxEXPAND | wxALL, 2);
		m_colorPresetsGroupSizer->Add(m_windows7PresetSizer, 0, wxEXPAND | wxALL, 2);
		sizer->Add(m_colorPresetsGroupSizer, 0, wxEXPAND | wxALL, 2);

		// Keep the frequently used controls visible, matching the original
		// Vista/Windows 7 control-panel flow.
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkEnableTransparency = new wxCheckBox(panel, wxID_ANY, L"Enable transparency");
			row->Add(m_chkEnableTransparency, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(panel, row, Settings::Id::ColorizationOpaqueBlend);
			sizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
		}
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(
				new wxStaticText(panel, wxID_ANY, L"Color intensity:", wxDefaultPosition, FromDIP(wxSize(150, -1))),
				0,
				wxALIGN_CENTER_VERTICAL | wxRIGHT,
				5
			);
			m_slColorIntensity = new NativeSlider(
				panel,
				wxID_ANY,
				63,
				ColorizationPresets::ClassicIntensityMinimum,
				ColorizationPresets::ClassicIntensityMaximum,
				wxDefaultPosition,
				wxDefaultSize,
				wxSL_HORIZONTAL | wxSL_AUTOTICKS
			);
			row->Add(m_slColorIntensity, 1, wxALIGN_CENTER_VERTICAL);
			sizer->Add(row, 0, wxEXPAND | wxALL, 8);
		}

		auto* detailedColorizationPane = AddCollapsibleSection(
			panel,
			sizer,
			L"Detailed colorization settings"
		);
		wxWindow* detailsPanel = detailedColorizationPane->GetPane();
		m_detailedColorizationSizer = new wxBoxSizer(wxVERTICAL);

		// Detailed colorization
		wxBoxSizer* colorsGroup = new wxBoxSizer(wxVERTICAL);
		m_glassColorsGroupSizer = colorsGroup;

		// Horizontal Row for Colors
		wxBoxSizer* colorRow = new wxBoxSizer(wxHORIZONTAL);
		m_colorsRowSizer = colorRow;

		// Active Column
		wxBoxSizer* activeCol = new wxBoxSizer(wxVERTICAL);
		activeCol->Add(new wxStaticText(detailsPanel, wxID_ANY, L"Active"), 0, wxBOTTOM | wxTOP, 5);
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_cpColorizationColor = new wxColourPickerCtrl(detailsPanel, wxID_ANY);
			row->Add(m_cpColorizationColor, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(detailsPanel, row, Settings::Id::ColorizationColor, Settings::Id::ColorizationColorOverride);
			activeCol->Add(row, 0, wxEXPAND);
		}
		colorRow->Add(activeCol, 1, wxEXPAND | wxRIGHT, 10);

		// Inactive Column
		m_inactiveColumnSizer = new wxBoxSizer(wxVERTICAL);
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_chkEnableInactiveColor = new wxCheckBox(detailsPanel, wxID_ANY, L"Custom inactive color");
			m_chkEnableInactiveColor->SetValue(false); // Default logic
			row->Add(m_chkEnableInactiveColor, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(detailsPanel, row, Settings::Id::ColorizationColorInactive);
			m_inactiveColumnSizer->Add(row, 0, wxEXPAND | wxBOTTOM, 5);
		}
		
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_cpColorizationColorInactive = new wxColourPickerCtrl(detailsPanel, wxID_ANY);
			m_cpColorizationColorInactive->Enable(false);
			row->Add(m_cpColorizationColorInactive, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			m_inactiveColumnSizer->Add(row, 0, wxEXPAND);
		}
		colorRow->Add(m_inactiveColumnSizer, 1, wxEXPAND | wxLEFT, 10);

		// Afterglow Column
		m_afterglowColumnSizer = new wxBoxSizer(wxVERTICAL);
		m_afterglowColumnSizer->Add(new wxStaticText(detailsPanel, wxID_ANY, L"Afterglow"), 0, wxBOTTOM | wxTOP, 5);
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			m_cpAfterglow = new wxColourPickerCtrl(detailsPanel, wxID_ANY);
			row->Add(m_cpAfterglow, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			AddOptionStatus(detailsPanel, row, Settings::Id::ColorizationAfterglow, Settings::Id::ColorizationAfterglowOverride);
			m_afterglowColumnSizer->Add(row, 0, wxEXPAND);
		}
		colorRow->Add(m_afterglowColumnSizer, 1, wxEXPAND | wxLEFT, 10);
		
		colorsGroup->Add(colorRow, 0, wxEXPAND | wxALL, 2);

		// Vista-only inactive opacity. Active opacity is the always-visible
		// color-intensity slider above.
		m_vistaOpacitySizer = new wxBoxSizer(wxVERTICAL);
		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			wxPanel* labelPanel = new wxPanel(detailsPanel);
			labelPanel->SetMinSize(wxSize(180, -1));
			wxBoxSizer* labelSizer = new wxBoxSizer(wxHORIZONTAL);
			m_chkEnableInactiveOpacity = new wxCheckBox(labelPanel, wxID_ANY, wxEmptyString);
			m_chkEnableInactiveOpacity->SetValue(false);
			labelSizer->Add(m_chkEnableInactiveOpacity, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			labelSizer->Add(new wxStaticText(labelPanel, wxID_ANY, L"Inactive opacity:"), 0, wxALIGN_CENTER_VERTICAL);
			labelPanel->SetSizer(labelSizer);
			row->Add(labelPanel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			// Spacer to align with the "Auto" dropdown column below
			row->Add(new wxPanel(detailsPanel, wxID_ANY, wxDefaultPosition, wxSize(70, 1)), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

			m_slGlassOpacityInactive = new NativeSlider(detailsPanel, wxID_ANY, 63, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_AUTOTICKS);
			m_slGlassOpacityInactive->Enable(false);
			row->Add(m_slGlassOpacityInactive, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(detailsPanel, row, Settings::Id::GlassOpacityInactive);
			
			m_vistaOpacitySizer->Add(row, 0, wxEXPAND | wxTOP, 2);
		}
		
		colorsGroup->Add(m_vistaOpacitySizer, 0, wxEXPAND | wxALL, 2);
		m_detailedColorizationSizer->Add(colorsGroup, 0, wxEXPAND | wxALL, 2);

		// Win7 Style Parameters
		wxStaticBoxSizer* win7Group = new wxStaticBoxSizer(wxVERTICAL, detailsPanel, L"Composition parameters");
		m_win7GroupSizer = win7Group; // Assign to member
		
		m_slBlurBalance = new NativeSlider(detailsPanel, wxID_ANY, 50, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_AUTOTICKS);
		AddProperty(detailsPanel, win7Group, L"Blur balance:", m_slBlurBalance, Settings::Id::ColorizationBlurBalance, Settings::Id::ColorizationBlurBalanceOverride);
		
		m_slAfterglowBalance = new NativeSlider(detailsPanel, wxID_ANY, 10, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_AUTOTICKS);
		AddProperty(detailsPanel, win7Group, L"Afterglow balance:", m_slAfterglowBalance, Settings::Id::ColorizationAfterglowBalance, Settings::Id::ColorizationAfterglowBalanceOverride);

		m_slColorBalance = new NativeSlider(detailsPanel, wxID_ANY, 10, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_AUTOTICKS);
		AddProperty(detailsPanel, win7Group, L"Color balance:", m_slColorBalance, Settings::Id::ColorizationColorBalance, Settings::Id::ColorizationColorBalanceOverride);

		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->AddStretchSpacer();
			m_btnPersistCompositionParameters = new wxButton(detailsPanel, wxID_ANY, L"Keep current values");
			m_btnPersistCompositionParameters->SetToolTip(L"Copy the three displayed composition parameters to persistent per-user Override values.");
			row->Add(m_btnPersistCompositionParameters, 0);
			win7Group->Add(row, 0, wxEXPAND | wxALL, 2);
		}

		m_detailedColorizationSizer->Add(win7Group, 0, wxEXPAND | wxALL, 2);
		detailsPanel->SetSizer(m_detailedColorizationSizer);

		// Advanced colorization
		auto* advancedColorizationPane = AddCollapsibleSection(
			panel,
			sizer,
			L"Advanced colorization settings"
		);
		wxWindow* advancedPanel = advancedColorizationPane->GetPane();
		auto* advancedSizer = new wxBoxSizer(wxVERTICAL);
		
		auto addBlurBase = [&](wxChoice*& ch, wxColourPickerCtrl*& cp, wxSpinCtrl*& sc, const wxString& label) {
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(new wxStaticText(advancedPanel, wxID_ANY, label, wxDefaultPosition, wxSize(260, -1)), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			wxArrayString modes;
			modes.Add(L"Auto");
			modes.Add(L"From theme");
			modes.Add(L"Custom");
			ch = new wxChoice(advancedPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, modes);
			row->Add(ch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			cp = new wxColourPickerCtrl(advancedPanel, wxID_ANY);
			cp->Enable(false);
			row->Add(cp, 1, wxALIGN_CENTER_VERTICAL);
			
			row->Add(new wxStaticText(advancedPanel, wxID_ANY, L"Alpha:"), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
			sc = new wxSpinCtrl(advancedPanel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(80, -1));
			sc->SetRange(0, 255);
			sc->Enable(false);
			row->Add(sc, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 2);

			advancedSizer->Add(row, 0, wxEXPAND | wxALL, 2);
		};

		addBlurBase(m_chModeBaseTransparent, m_cpBaseTransparent, m_scBaseTransparentAlpha, L"Base color (transparent):");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationBaseTransparent);
		addBlurBase(m_chModeBaseMaximized, m_cpBaseMaximized, m_scBaseMaximizedAlpha, L"Base color (maximized):");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationBaseMaximized);
		addBlurBase(m_chModeBaseOpaque, m_cpBaseOpaque, m_scBaseOpaqueAlpha, L"Base color (opaque):");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationBaseOpaque);

		wxArrayString priorities;
		priorities.Add(L"Windows Vista");
		priorities.Add(L"Windows 7");
		priorities.Add(L"Auto");

		{
			wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
			row->Add(new wxStaticText(advancedPanel, wxID_ANY, L"Opaque blend priority:", wxDefaultPosition, wxSize(260, -1)), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			m_chOpaqueBlendPriority = new wxChoice(advancedPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, priorities);
			row->Add(m_chOpaqueBlendPriority, 1, wxALIGN_CENTER_VERTICAL);
			AddOptionStatus(advancedPanel, row, Settings::Id::ColorizationOpaqueBlendPriority);
			
			advancedSizer->Add(row, 0, wxEXPAND | wxALL, 2);
		}
		
		auto addOpacityOverride = [&](wxChoice*& ch, wxSlider*& sl, const wxString& label) {
			wxBoxSizer* r = new wxBoxSizer(wxHORIZONTAL);
			wxStaticText* st = new wxStaticText(advancedPanel, wxID_ANY, label, wxDefaultPosition, wxSize(260, -1));
			r->Add(st, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			wxArrayString modes;
			modes.Add(L"Auto");
			modes.Add(L"From theme");
			modes.Add(L"Custom");
			ch = new wxChoice(advancedPanel, wxID_ANY, wxDefaultPosition, wxSize(120, -1), modes); // Aligned width
			r->Add(ch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
			
			sl = new NativeSlider(advancedPanel, wxID_ANY, 100, 0, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_AUTOTICKS);
			sl->Enable(false);
			r->Add(sl, 1, wxALIGN_CENTER_VERTICAL);
			
			advancedSizer->Add(r, 0, wxEXPAND | wxALL, 2);
		};

		addOpacityOverride(m_chModeColorizationOpacity, m_slColorizationOpacity, L"Base opacity:");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationOpacity);
		addOpacityOverride(m_chModeColorizationOpacityInactive, m_slColorizationOpacityInactive, L"Base inactive opacity:");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationOpacityInactive);
		addOpacityOverride(m_chModeColorizationOpacityMaximized, m_slColorizationOpacityMaximized, L"Base maximized opacity:");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationOpacityMaximized);
		addOpacityOverride(m_chModeColorizationOpacityInactiveMaximized, m_slColorizationOpacityInactiveMaximized, L"Base inactive maximized opacity:");
		AddOptionStatus(advancedPanel, dynamic_cast<wxBoxSizer*>(advancedSizer->GetItem(advancedSizer->GetItemCount() - 1)->GetSizer()), Settings::Id::ColorizationOpacityInactiveMaximized);

		advancedPanel->SetSizer(advancedSizer);

		panel->SetSizer(sizer);
		panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)); 
		m_notebook->AddPage(panel, L"Glass colors");
	}
}
