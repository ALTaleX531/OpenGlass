#pragma once
#include "pch.h"
#include "Diagnostics.hpp"
#include "ColorizationPresets.hpp"
#include "ColorPreference.hpp"
#include "RegistryValueResolver.hpp"
#include "RegistryConfig.hpp"
#include "PreviewJournal.hpp"
#include "Symbols.hpp"
#include "PresetPackage.hpp"
#include "ConfigurationResources.hpp"

namespace OpenGlass
{
	class ColorPresetList;


	class MainFrame : public wxFrame
	{
	public:
		MainFrame(std::wstring userSid, Settings::Scope scope);

	private:
		void CreateControls();
		void CreateSystemTab();
		void CreatePresetsTab();
		void CreateDiagnosticsTab();
		void CreateThemeTab();
		void CreateAppearanceTab(); // Text and Caption settings
		void CreateGlassColorsTab();
		void CreateBottomControls(wxSizer* parentSizer);
		bool CanSwitchEditingScope() const;
		void SwitchEditingScope();

		void BindEvents();
		void LoadSettings();
		bool CanSyncExternalChanges() const;
		void SyncExternalChanges();
		bool RevertSettings();
		void SaveSettings();

		// Helpers
		static std::wstring ResolveAccountName(const std::wstring& sidText, bool includeDomain);
		void AddProperty(
			wxWindow* parent,
			wxSizer* sizer,
			const wxString& label,
			wxWindow* control,
			Settings::Id setting
		);
		void AddOptionStatus(
			wxWindow* parent,
			wxBoxSizer* row,
			Settings::Id setting
		);
		void UpdateOptionStatusIcons();
		void AddPathWarningIcon(wxWindow* parent, wxBoxSizer* row, wxFilePickerCtrl* picker, wxCheckBox* checkbox, const wxString& title);
		void UpdatePathWarningIcons();
		void ApplyColorizationBalances(DWORD intensity);
		void ApplyColorizationColor(std::optional<DWORD> argb, ColorizationPresets::Family family);
		void ApplyColorizationPreset(const ColorizationPresets::Preset& preset);
		[[nodiscard]] const ColorizationPresets::Preset* FindMatchingWindows7Preset(bool opaque) const;
		void UpdateColorizationPresetSelection();
		void QueueColorizationRefresh();
		WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;
		bool m_colorizationRefreshPending{};
		bool m_systemColorChangePending{};
		void NotifySettingsChange(Settings::UpdateImpact impact = Settings::UpdateImpact::Colorization | Settings::UpdateImpact::Theme);
		void UpdateUIVisibility();
		void OnClose(wxCloseEvent& event);
		[[nodiscard]] RegistryConfig* GetConfigForScope(Settings::Scope scope) const;
		void SetDirty(bool dirty);
		void ReconcilePreview();
		void UpdateWindowTitle();
		void ApplyChoiceColor(wxChoice* ch, wxColourPickerCtrl* cp, DWORD value, DWORD themeSentinel, DWORD autoSentinel) const;
		void ApplyChoiceColorEx(wxChoice* ch, wxColourPickerCtrl* cp, DWORD value, DWORD themeSentinel, DWORD autoSentinel, DWORD systemSentinel) const;
		void ApplyChoiceSlider(wxChoice* ch, wxSlider* sl, DWORD value, DWORD themeSentinel, DWORD autoSentinel, int disabledValue) const;
		void TrackSettingChange(Settings::Id id);
		void TrackSettingChange(Settings::Scope scope, Settings::Id id);
		void ReportRegistryError(HRESULT result, const std::wstring& name);
		void StartSymbolDownload();
		void RefreshDiagnosticsLayout();
		void RefreshTransparencyDiagnostics();
		void UpdateSymbolDownloadProgress(const SymbolDownloadProgress& progress);
		void UpdateSymbolDownloadResult(wxArtID iconId, const wxString& details);
		void FinishSymbolDownload(const SymbolDownloadOutcome& outcome);
		void RefreshDwmCrashDumpConfiguration();
		void SetDwmCrashDumpsEnabled(bool enabled);
		void RefreshPresetPackages();
		PresetPackages::PreviewProvenance m_presetProvenance;

		void RebuildPresetPackageList(std::string_view selectedUuid = {});
		void ShowPresetContextMenu(wxPoint screenPosition);
		void SelectPresetPackageRow(std::size_t row);
		void UpdatePresetPackageDetails();
		void ImportPresetPackage();
		void ImportPresetPackage(const std::filesystem::path& path);
		void ImportPresetPackages(std::span<const std::filesystem::path> paths);
		void ImportDroppedPresetPackages(const wxDropFilesEvent& event);
		void ApplySelectedPresetPackage();
		void CreatePresetPackage(bool update);
		void CaptureEffectivePreset(PresetPackages::CreateRequest& request, bool accentColor, const PresetPackages::Package* localSource);
		void ExportSelectedPresetPackage();
		void RemoveSelectedPresetPackage();
		void ApplyPresetPackage(const PresetPackages::Package& package);

		// Save/Revert identity includes the editing or Windows-state registry scope and stable catalog ID.
		struct TrackedSetting
		{
			Settings::Scope scope;
			Settings::Id id;
			std::wstring Name() const { return std::wstring(Settings::Get(id).name); }
			auto operator<=>(const TrackedSetting&) const = default;
		};
		PreviewJournal<TrackedSetting, RegistryConfig::RawValue> m_preview;
		bool RunPreview(const std::function<void()>& operation,
			Settings::UpdateImpact impact = Settings::UpdateImpact::Colorization | Settings::UpdateImpact::Theme);
		ColorPreference m_colorPreference;
		void ApplyAccentColor(std::optional<DWORD> argb);
		ConfigurationResources m_resources;
		wil::unique_hfile m_previewWriter;
		void EnsurePreviewWriter();
		Settings::Scope m_editScope{ Settings::Scope::Machine };
		void MergeConfiguration();
		void RestoreDefaults();

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
		wxStaticText* m_lblPresetEmpty{};
		int m_presetSortColumn{};
		bool m_presetSortAscending{ true };
		std::vector<PresetPackages::Package> m_presetPackages;
		std::wstring m_lastPresetAuthorName;
		std::wstring m_lastPresetAuthorHomepage;
		std::string m_lastPresetLicenseText;
		bool m_lastPresetIncludeLicense{ false };

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
		wxRadioBox* m_rbGlassType{ nullptr };
		wxStaticBoxSizer* m_colorPresetsGroupSizer{ nullptr };
		ColorPresetList* m_colorPresetList{ nullptr };
		bool m_customColorsInitialized{ false };
		wxCheckBox* m_chkEnableTransparency{ nullptr };
		wxSlider* m_slColorIntensity{ nullptr };

		wxChoice* m_chModeColorCaption{ nullptr };
		wxColourPickerCtrl* m_cpColorCaption{ nullptr };

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

		// Accent Tab
		wxCheckBox* m_chkGlassOverrideAccent{ nullptr };

		// Registry state
		struct OptionStatus
		{
			wxStaticBitmap* icon{};
			Settings::Id setting{};
		};
		std::vector<OptionStatus> m_optionStatus;
		struct PathWarningStatus
		{
			wxFilePickerCtrl* picker{};
			wxCheckBox* checkbox{};
			wxStaticBitmap* icon{};
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
		wxString m_targetUserSid;
		HWND m_dwmWindow{ nullptr };
		bool m_symbolDownloadRunning{ false };
		bool m_closeWhenSymbolDownloadStops{ false };
		std::jthread m_symbolDownloadThread{};
	};
}
