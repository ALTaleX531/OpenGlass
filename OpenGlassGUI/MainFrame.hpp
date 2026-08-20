#pragma once
#include "pch.h"
#include "Diagnostics.hpp"
#include "ColorizationPresets.hpp"
#include "RegistryValueResolver.hpp"
#include "RegistryConfig.hpp"
#include "Symbols.hpp"
#include "PresetPackage.hpp"

namespace OpenGlass
{
	class ColorSwatchButton;

	enum class ChangeType {
		Colorization,
		Theme,
		Both
	};

	class MainFrame : public wxFrame
	{
	public:
		MainFrame(const wxString& title, std::wstring userSid);
		[[nodiscard]] bool IsInitializationCanceled() const
		{
			return m_initCanceled;
		}

	private:
		void CreateControls();
		void CreateSystemTab();
		void CreatePresetsTab();
		void CreateDiagnosticsTab();
		void CreateThemeTab();
		void CreateAppearanceTab(); // Text and Caption settings
		void CreateGlassColorsTab();
		// void CreateAccentTab();
		void CreateBottomControls(wxSizer* parentSizer);
		
		void BindEvents();
		void LoadSettings(bool saveBackup = false);
		bool RevertSettings();
		void SaveSettings();
		
		// Helpers
		void AddProperty(
			wxWindow* parent,
			wxSizer* sizer,
			const wxString& label,
			wxWindow* control,
			std::optional<Settings::Id> setting = std::nullopt,
			std::optional<Settings::Id> overrideSetting = std::nullopt
		);
		void AddOptionStatus(
			wxWindow* parent,
			wxBoxSizer* row,
			Settings::Id setting,
			std::optional<Settings::Id> overrideSetting = std::nullopt
		);
		void UpdateOptionStatusIcons();
		void AddPathWarningIcon(wxWindow* parent, wxBoxSizer* row, wxFilePickerCtrl* picker, wxCheckBox* checkbox, const wxString& title);
		void UpdatePathWarningIcons();
		void ApplyColorizationColor(DWORD argb, ColorizationPresets::Family family);
		void ApplyColorizationPreset(const ColorizationPresets::Preset& preset);
		[[nodiscard]] const ColorizationPresets::Preset* FindMatchingWindows7Preset(bool opaque) const;
		void UpdateColorizationPresetSelection();
		bool NotifySettingsChange(ChangeType type = ChangeType::Both);
		void UpdateUIVisibility();
		void OnClose(wxCloseEvent& event);
		[[nodiscard]] RegistryConfig* GetConfigForSetting(Settings::Id id) const;
		[[nodiscard]] RegistryConfig* GetConfigForScope(Settings::Scope scope) const;
		[[nodiscard]] ResolvedRegistryValue<DWORD> ResolveOverridableDword(
			Settings::Id setting,
			Settings::Id overrideSetting,
			DWORD defaultValue
		) const;
		void ResetOverridableDword(Settings::Id setting, Settings::Id overrideSetting);
		void SetDirty(bool dirty);
		void UpdateWindowTitle();
		void UpdateStatusBar();
		void ApplyChoiceColor(wxChoice* ch, wxColourPickerCtrl* cp, DWORD value, DWORD themeSentinel, DWORD autoSentinel) const;
		void ApplyChoiceColorEx(wxChoice* ch, wxColourPickerCtrl* cp, DWORD value, DWORD themeSentinel, DWORD autoSentinel, DWORD systemSentinel) const;
		void ApplyChoiceSlider(wxChoice* ch, wxSlider* sl, DWORD value, DWORD themeSentinel, DWORD autoSentinel, int disabledValue) const;
		void TrackSettingChange(Settings::Id id);
		void TrackSettingChange(Settings::Scope scope, Settings::Id id);
		bool CheckRegistryWrite(HRESULT result, const std::wstring& name);
		void StartSymbolDownload();
		void RefreshDiagnosticsLayout();
		void RefreshTransparencyDiagnostics();
		void UpdateSymbolDownloadProgress(const SymbolDownloadProgress& progress);
		void UpdateSymbolDownloadResult(wxArtID iconId, const wxString& summary, const wxString& details = wxEmptyString);
		void FinishSymbolDownload(const SymbolDownloadOutcome& outcome);
		void RefreshDwmCrashDumpConfiguration();
		void SetDwmCrashDumpsEnabled(bool enabled);
		void RefreshPresetPackages();
		void RebuildPresetPackageList(std::string_view selectedUuid = {});
		void ResizePresetPackageColumn();
		void SelectPresetPackageRow(std::size_t row);
		void UpdatePresetPackageDetails();
		void ImportPresetPackage();
		void ImportPresetPackage(const std::filesystem::path& path);
		void ImportPresetPackages(std::span<const std::filesystem::path> paths);
		void ImportDroppedPresetPackages(const wxDropFilesEvent& event);
		void ApplySelectedPresetPackage();
		void CreatePresetPackage();
		void ResetPresetPackSettings();
		void RemoveSelectedPresetPackage();
		bool ApplyPresetPackage(const PresetPackages::Package& package, bool previewAccepted = false);
		
		// Save/Revert identity includes the canonical registry scope and stable catalog ID.
		struct TrackedSetting
		{
			Settings::Scope scope;
			Settings::Id id;
			auto operator<=>(const TrackedSetting&) const = default;
		};
		std::map<TrackedSetting, std::variant<std::monostate, DWORD, std::wstring>> m_backupSettings;
		std::set<TrackedSetting> m_dirtyKeys;
		void BackupCurrentSetting(TrackedSetting setting);

		// UI Elements
		wxNotebook* m_notebook{ nullptr };
		
		// Buttons
		wxButton* m_btnSave{ nullptr };
		wxButton* m_btnRevert{ nullptr };

		// System Tab
		wxCheckBox* m_chkDisableGlassOnBattery{ nullptr };
		wxCheckListBox* m_clDisabledHooks{ nullptr };

		// Diagnostics Tab
		wxStaticText* m_lblWindowsTransparencyStatus{ nullptr };
		wxStaticText* m_lblOpaqueBlendStatus{ nullptr };
		wxStaticText* m_lblPowerModeStatus{ nullptr };
		wxStaticText* m_lblDisableOnBatteryStatus{ nullptr };
		wxStaticBitmap* m_bmpEffectiveTransparencyWarning{ nullptr };
		wxStaticText* m_lblEffectiveTransparencyStatus{ nullptr };
		wxButton* m_btnRefreshTransparencyDiagnostics{ nullptr };
		wxGauge* m_gaugeSymbolDownload{ nullptr };
		wxStaticText* m_lblSymbolDownloadPhase{ nullptr };
		wxStaticText* m_lblSymbolDownloadDetail{ nullptr };
		wxString m_symbolDownloadDetailText;
		wxPanel* m_pnlSymbolDownloadResult{ nullptr };
		wxStaticBitmap* m_bmpSymbolDownloadResult{ nullptr };
		wxStaticText* m_lblSymbolDownloadResult{ nullptr };
		wxString m_symbolDownloadResultText;
		wxDirPickerCtrl* m_dpSymbolCacheDirectory{ nullptr };
		wxButton* m_btnDownloadSymbols{ nullptr };
		wxButton* m_btnCancelSymbolDownload{ nullptr };
		wxDirPickerCtrl* m_dpDwmCrashDumpFolder{ nullptr };
		wxStaticText* m_lblDwmCrashDumpStatus{ nullptr };
		wxString m_dwmCrashDumpStatusText;
		wxButton* m_btnEnableDwmCrashDumps{ nullptr };
		wxButton* m_btnDisableDwmCrashDumps{ nullptr };

		// Presets Tab
		wxListView* m_lstPresetPackages{ nullptr };
		wxTextCtrl* m_txtPresetDetails{ nullptr };
		wxButton* m_btnImportPreset{ nullptr };
		wxButton* m_btnApplyPreset{ nullptr };
		wxButton* m_btnCreatePreset{ nullptr };
		wxButton* m_btnResetPresetSettings{ nullptr };
		wxButton* m_btnRemovePreset{ nullptr };
		wxButton* m_btnPresetInformation{ nullptr };
		std::vector<PresetPackages::Package> m_presetPackages;
		std::wstring m_lastPresetAuthorName;
		std::wstring m_lastPresetAuthorHomepage;
		std::string m_lastPresetLicenseText;
		bool m_lastPresetIncludeLicense{ true };
		bool m_lastPresetInstallAfterCreate{ true };

		// Theme Tab
		wxCheckBox* m_chkCustomThemeAtlas{ nullptr };
		wxFilePickerCtrl* m_fpCustomThemeAtlas{ nullptr };
		wxCheckBox* m_chkCustomThemeReflection{ nullptr };
		wxFilePickerCtrl* m_fpCustomThemeReflection{ nullptr };
		wxSlider* m_slReflectionIntensity{ nullptr };
		
		// Reflection Opacity & Variants
		wxChoice* m_chModeReflectionOpacity{ nullptr };
		wxSlider* m_slReflectionOpacity{ nullptr };
		
		wxChoice* m_chModeReflectionOpacityInactive{ nullptr };
		wxSlider* m_slReflectionOpacityInactive{ nullptr };
		
		wxChoice* m_chModeReflectionOpacityMaximized{ nullptr };
		wxSlider* m_slReflectionOpacityMaximized{ nullptr };
		
		wxChoice* m_chModeReflectionOpacityInactiveMaximized{ nullptr };
		wxSlider* m_slReflectionOpacityInactiveMaximized{ nullptr };

		wxSlider* m_slReflectionParallax{ nullptr };
		wxCheckBox* m_chkReflectionPolicyTitlebar{ nullptr };
		wxCheckBox* m_chkReflectionPolicyPeek{ nullptr };
		wxCheckBox* m_chkReflectionPolicySnap{ nullptr };
		// Legacy (kept for reference):
		// wxCheckListBox* m_clReflectionPolicy{ nullptr };
		wxCheckBox* m_chkCustomThemeMaterial{ nullptr }; // Added
		wxFilePickerCtrl* m_fpCustomThemeMaterial{ nullptr };
		wxSlider* m_slMaterialOpacity{ nullptr };
		wxSlider* m_slBlurAmount{ nullptr };
		wxChoice* m_chBlurOptimization{ nullptr };
		wxCheckBox* m_chkUseD3D{ nullptr };
		wxCheckBox* m_chkGlassSafetyZone{ nullptr };
		
		wxChoice* m_chRoundRectProfile{ nullptr }; // Added
		wxSpinCtrl* m_scRoundRectRadius{ nullptr };
		
		wxChoice* m_chTextGlowMode{ nullptr };
		wxSpinCtrl* m_scTextGlowSize{ nullptr }; // Added
		
		wxChoice* m_chCaptionButtons{ nullptr };
		wxChoice* m_chCenterCaption{ nullptr };
		wxCheckBox* m_chkDisableModernBorders{ nullptr };
		wxButton* m_btnExportAtlas{ nullptr };

		// Glass Colors Tab
		wxScrolledWindow* m_glassColorsPanel{ nullptr };
		wxSizer* m_glassColorsRootSizer{ nullptr };
		wxRadioBox* m_rbGlassType{ nullptr };
		wxStaticBoxSizer* m_colorPresetsGroupSizer{ nullptr };
		wxWrapSizer* m_vistaPresetSizer{ nullptr };
		wxWrapSizer* m_windows7PresetSizer{ nullptr };
		std::vector<std::pair<const ColorizationPresets::Preset*, ColorSwatchButton*>> m_presetButtons;
		std::vector<ColorSwatchButton*> m_customColorButtons;
		bool m_customColorsInitialized{ false };
		wxCheckBox* m_chkEnableTransparency{ nullptr };
		wxSlider* m_slColorIntensity{ nullptr };
		wxSizer* m_detailedColorizationSizer{ nullptr };
		wxColourPickerCtrl* m_cpColorizationColor{ nullptr };
		
		wxCheckBox* m_chkEnableInactiveColor{ nullptr }; // Added
		wxColourPickerCtrl* m_cpColorizationColorInactive{ nullptr };
		
		wxCheckBox* m_chkEnableInactiveOpacity{ nullptr };
		wxSlider* m_slGlassOpacityInactive{ nullptr };
		
		wxChoice* m_chModeColorCaption{ nullptr };
		wxColourPickerCtrl* m_cpColorCaption{ nullptr };
		
		// Sizers for dynamic visibility
		wxSizer* m_win7GroupSizer{ nullptr };
		wxSizer* m_inactiveColumnSizer{ nullptr };
		wxSizer* m_afterglowColumnSizer{ nullptr }; // Added
		wxSizer* m_vistaOpacitySizer{ nullptr };
		wxSizer* m_colorsRowSizer{ nullptr };
		wxSizer* m_glassColorsGroupSizer{ nullptr };

		wxColourPickerCtrl* m_cpColorCaptionInactive{ nullptr };
		wxChoice* m_chModeColorCaptionInactive{ nullptr };
		
		// Advanced Colors
		wxColourPickerCtrl* m_cpColorCaptionMaximized{ nullptr };
		wxChoice* m_chModeColorCaptionMaximized{ nullptr };
		
		wxColourPickerCtrl* m_cpColorCaptionInactiveMaximized{ nullptr };
		wxChoice* m_chModeColorCaptionInactiveMaximized{ nullptr };

		wxChoice* m_chOpaqueBlendPriority{ nullptr };
		
		wxChoice* m_chModeBaseTransparent{ nullptr };
		wxColourPickerCtrl* m_cpBaseTransparent{ nullptr };
		wxSpinCtrl* m_scBaseTransparentAlpha{ nullptr };
		
		wxChoice* m_chModeBaseMaximized{ nullptr };
		wxColourPickerCtrl* m_cpBaseMaximized{ nullptr };
		wxSpinCtrl* m_scBaseMaximizedAlpha{ nullptr };
		
		wxChoice* m_chModeBaseOpaque{ nullptr };
		wxColourPickerCtrl* m_cpBaseOpaque{ nullptr };
		wxSpinCtrl* m_scBaseOpaqueAlpha{ nullptr };
		
		wxSlider* m_slColorizationOpacity{ nullptr };
		wxChoice* m_chModeColorizationOpacity{ nullptr };
		
		wxSlider* m_slColorizationOpacityInactive{ nullptr };
		wxChoice* m_chModeColorizationOpacityInactive{ nullptr };
		
		wxSlider* m_slColorizationOpacityMaximized{ nullptr };
		wxChoice* m_chModeColorizationOpacityMaximized{ nullptr };

		wxSlider* m_slColorizationOpacityInactiveMaximized{ nullptr };
		wxChoice* m_chModeColorizationOpacityInactiveMaximized{ nullptr };

		// Composition Parameters
		wxSlider* m_slBlurBalance{ nullptr };
		wxSlider* m_slAfterglowBalance{ nullptr };
		wxSlider* m_slColorBalance{ nullptr };
		wxButton* m_btnPersistCompositionParameters{ nullptr };
		wxColourPickerCtrl* m_cpAfterglow{ nullptr };
		
		// Accent Tab
		wxCheckBox* m_chkGlassOverrideAccent{ nullptr };

		// Registry state
		struct OptionStatus
		{
			wxStaticBitmap* overrideIcon{};
			wxButton* resetOverrideButton{};
			Settings::Id setting{};
			std::optional<Settings::Id> overrideSetting;
			bool vistaIrrelevant{ false };
			bool win7Irrelevant{ false };
		};
		std::vector<OptionStatus> m_optionStatus;
		struct PathWarningStatus
		{
			wxFilePickerCtrl* picker{};
			wxCheckBox* checkbox{};
			wxStaticBitmap* icon{};
			wxString title;
			wxString lastPath;
			bool lastEnabled{ false };
			bool lastExists{ false };
			bool initialized{ false };
		};
		std::vector<PathWarningStatus> m_pathWarnings;
		std::unique_ptr<RegistryConfig> m_config;
		std::unique_ptr<RegistryConfig> m_userConfig;
		std::unique_ptr<RegistryConfig> m_systemConfig;
		bool m_isAdmin{ false };
		bool m_isDirty{ false };
		wxString m_baseTitle;
		wxString m_targetUserLabel;
		wxString m_targetUserSid;
		HWND m_dwmWindow{ nullptr };
		bool m_initCanceled{ false };
		bool m_symbolDownloadRunning{ false };
		bool m_closeWhenSymbolDownloadStops{ false };
		std::jthread m_symbolDownloadThread{};
	};
}
