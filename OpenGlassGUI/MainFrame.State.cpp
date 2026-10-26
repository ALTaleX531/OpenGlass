#include "pch.h"
#include "MainFrame.hpp"
#include "WrappingLayout.hpp"
#include "ColorPresetList.hpp"
#include "BlurSettings.hpp"

namespace OpenGlass
{
	void MainFrame::ApplyChoiceColor(wxChoice* ch, wxColourPickerCtrl* cp, DWORD value, DWORD themeSentinel, DWORD autoSentinel) const
	{
		auto dwordToColor = [](DWORD dw) -> wxColour {
			return wxColour(dw & 0xFF, (dw >> 8) & 0xFF, (dw >> 16) & 0xFF, 0xFF);
		};

		if (value == autoSentinel)
		{
			ch->SetSelection(0); // Auto
			cp->Enable(false);
			cp->SetColour(*wxLIGHT_GREY);
		}
		else if (value == themeSentinel)
		{
			ch->SetSelection(1); // Theme
			cp->Enable(false);
			cp->SetColour(*wxLIGHT_GREY);
		}
		else
		{
			ch->SetSelection(2); // Custom
			cp->Enable(true);
			cp->SetColour(dwordToColor(value));
		}
	}

	void MainFrame::ApplyChoiceColorEx(wxChoice* ch, wxColourPickerCtrl* cp, DWORD value, DWORD themeSentinel, DWORD autoSentinel, DWORD systemSentinel) const
	{
		if (value == systemSentinel)
		{
			ch->SetSelection(3); // System determined
			cp->Enable(false);
			cp->SetColour(*wxLIGHT_GREY);
		}
		else
		{
			ApplyChoiceColor(ch, cp, value, themeSentinel, autoSentinel);
		}
	}

	void MainFrame::ApplyChoiceSlider(wxChoice* ch, wxSlider* sl, DWORD value, DWORD themeSentinel, DWORD autoSentinel, int disabledValue) const
	{
		if (value == autoSentinel)
		{
			ch->SetSelection(0); // Auto
			sl->Enable(false);
			sl->SetValue(disabledValue);
		}
		else if (value == themeSentinel)
		{
			ch->SetSelection(1); // Theme
			sl->Enable(false);
			sl->SetValue(disabledValue);
		}
		else
		{
			ch->SetSelection(2); // Custom
			sl->Enable(true);
			sl->SetValue(std::clamp<int>(value, 0, 100));
		}
	}

	bool MainFrame::CanSyncExternalChanges() const
	{
		return !m_isDirty && !m_preview.IsDirty() && !m_colorPreference.IsDirty() && !m_resources.IsDirty()
			&& !m_preview.IsAttemptActive() && !m_closeWhenSymbolDownloadStops && !IsBeingDeleted();
	}

	void MainFrame::SyncExternalChanges()
	{
		if (!CanSyncExternalChanges()) return;
		// Read current state without accepting or replacing the preview checkpoint.
		// RegistryConfig opens each read afresh, so no registry cache needs clearing.
		LoadSettings();
		RefreshTransparencyDiagnostics();
		RefreshDwmCrashDumpConfiguration();
		RefreshPresetPackages();
	}

	void MainFrame::LoadSettings()
	{
		// System
		m_chkDisableGlassOnBattery->SetValue(m_config->GetDword(L"DisableGlassOnBattery", 1) != 0);

		DWORD hookMask = m_config->GetDword(L"DisabledHooks", 0);
		m_clDisabledHooks->Check(0, (hookMask & 0x1) != 0);
		m_clDisabledHooks->Check(1, (hookMask & 0x2) != 0);
		m_clDisabledHooks->Check(2, (hookMask & 0x4) != 0);
		m_clDisabledHooks->Check(3, (hookMask & 0x8) != 0);
		m_clDisabledHooks->Check(4, (hookMask & 0x10) != 0);
		m_clUxMitigation->Check(0, (m_config->GetDword(L"EnableUxMitigation", UxMitigation::DefaultFlags) & UxMitigation::RestrictAccentOverride) != 0);

		// Theme
		std::wstring atlas = m_config->GetString(L"CustomThemeAtlas", L"");
		m_fpCustomThemeAtlas->SetPath(atlas);
		m_chkCustomThemeAtlas->SetValue(!atlas.empty());
		m_fpCustomThemeAtlas->Enable(!atlas.empty());

		std::wstring reflection = m_config->GetString(L"CustomThemeReflection", L"");
		m_fpCustomThemeReflection->SetPath(reflection);
		m_chkCustomThemeReflection->SetValue(!reflection.empty());
		m_fpCustomThemeReflection->Enable(!reflection.empty());

		auto syncSliderTooltip = [](wxSlider* slider) {
			if (slider)
			{
				slider->SetToolTip(wxString::Format(L"%d", slider->GetValue()));
			}
		};

		m_slReflectionIntensity->SetValue(m_config->GetDword(L"ColorizationGlassReflectionIntensity", 0));
		syncSliderTooltip(m_slReflectionIntensity);

		DWORD refOpacityActive = m_config->GetDword(L"ColorizationGlassReflectionOpacity", 0xFFFFFFFE);
		DWORD refOpacityInactive = m_config->GetDword(L"ColorizationGlassReflectionOpacityInactive", refOpacityActive);
		DWORD refOpacityMaximized = m_config->GetDword(L"ColorizationGlassReflectionOpacityMaximized", refOpacityActive);
		DWORD refOpacityInactiveMaximized = m_config->GetDword(L"ColorizationGlassReflectionOpacityInactiveMaximized", refOpacityInactive);

		ApplyChoiceSlider(m_chModeReflectionOpacity, m_slReflectionOpacity, refOpacityActive, 0xFFFFFFFF, 0xFFFFFFFE, 50);
		ApplyChoiceSlider(m_chModeReflectionOpacityInactive, m_slReflectionOpacityInactive, refOpacityInactive, 0xFFFFFFFF, 0xFFFFFFFE, 50);
		ApplyChoiceSlider(m_chModeReflectionOpacityMaximized, m_slReflectionOpacityMaximized, refOpacityMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 50);
		ApplyChoiceSlider(m_chModeReflectionOpacityInactiveMaximized, m_slReflectionOpacityInactiveMaximized, refOpacityInactiveMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 50);
		syncSliderTooltip(m_slReflectionOpacity);
		syncSliderTooltip(m_slReflectionOpacityInactive);
		syncSliderTooltip(m_slReflectionOpacityMaximized);
		syncSliderTooltip(m_slReflectionOpacityInactiveMaximized);

		m_slReflectionParallax->SetValue(m_config->GetDword(L"ColorizationGlassReflectionParallaxIntensity", 13));
		syncSliderTooltip(m_slReflectionParallax);

		DWORD refPolicy = m_config->GetDword(L"ColorizationGlassReflectionPolicy", 0xFFFFFFFF);
		if (refPolicy == 0xFFFFFFFF)
		{
			if (m_chkReflectionPolicyTitlebar) m_chkReflectionPolicyTitlebar->SetValue(true);
			if (m_chkReflectionPolicyPeek) m_chkReflectionPolicyPeek->SetValue(true);
			if (m_chkReflectionPolicySnap) m_chkReflectionPolicySnap->SetValue(true);
		}
		else
		{
			if (m_chkReflectionPolicyTitlebar) m_chkReflectionPolicyTitlebar->SetValue((refPolicy & (1 << 0)) != 0);
			if (m_chkReflectionPolicyPeek) m_chkReflectionPolicyPeek->SetValue((refPolicy & (1 << 2)) != 0);
			if (m_chkReflectionPolicySnap) m_chkReflectionPolicySnap->SetValue((refPolicy & (1 << 3)) != 0);
		}

		std::wstring material = m_config->GetString(L"CustomThemeMaterial", L"");
		m_fpCustomThemeMaterial->SetPath(material);
		m_chkCustomThemeMaterial->SetValue(!material.empty());
		m_fpCustomThemeMaterial->Enable(!material.empty());

		m_slMaterialOpacity->SetValue(m_config->GetDword(L"MaterialOpacity", 0));
		syncSliderTooltip(m_slMaterialOpacity);

		const bool useD3D = m_config->GetDword(L"UseDirect3DRendering", 0) != 0;
		m_chkUseD3D->SetValue(useD3D);

		const DWORD encodedDeviation = m_config->GetDword(L"BlurDeviation", BlurSettings::DefaultEncodedDeviation);
		m_slBlurAmount->SetValue(BlurSettings::DecodeGuiBlurAmount(encodedDeviation));
		m_slBlurAmount->Enable(!useD3D);
		syncSliderTooltip(m_slBlurAmount);

		m_chBlurOptimization->SetSelection(std::clamp<int>(m_config->GetDword(L"BlurOptimization", 0), 0, 2));
		m_chBlurOptimization->Enable(!useD3D);
		m_chkGlassSafetyZone->SetValue(m_config->GetDword(L"GlassSafetyZoneMode", 1) == 0);

		// RoundRect
		DWORD radius = m_config->GetDword(L"RoundRectRadius", 0);
		m_scRoundRectRadius->SetValue(radius);
		if (radius == 0)
			m_chRoundRectProfile->SetSelection(0); // Win8
		else if (radius == 6)
			m_chRoundRectProfile->SetSelection(1); // Win7
		else
			m_chRoundRectProfile->SetSelection(2); // Custom
		m_scRoundRectRadius->Enable(m_chRoundRectProfile->GetSelection() == 2);

		// TextGlow
		DWORD glowVal = m_config->GetDword(L"TextGlowMode", 1);
		int glowMode = glowVal & 0xFFFF;
		int glowSize = (glowVal >> 16) & 0xFFFF;

		m_chTextGlowMode->SetSelection(std::clamp<int>(glowMode, 0, 3));
		m_scTextGlowSize->SetValue(glowSize);
		m_scTextGlowSize->Enable(glowMode == 3);

		m_chCaptionButtons->SetSelection(std::clamp<int>(m_config->GetDword(L"CaptionButtons", 0), 0, 3));
		m_chCenterCaption->SetSelection(std::clamp<int>(m_config->GetDword(L"CenterCaption", 0), 0, 2));
		m_chkDisableModernBorders->SetValue(m_config->GetDword(L"DisableModernBorders", 0) != 0);

		// Colors
		auto dwordToColor = [](DWORD dw) -> wxColour {
			return wxColour((dw >> 16) & 0xFF, (dw >> 8) & 0xFF, dw & 0xFF, (dw >> 24) & 0xFF);
		};

		m_rbGlassType->SetSelection(std::clamp<int>(m_config->GetDword(L"GlassType", 0), 0, 1));
		m_chkEnableTransparency->SetValue(m_systemTransparency
			? m_userConfig->GetDword(L"EnableTransparency", 0) != 0
			: m_config->GetDword(L"ColorizationOpaqueBlend", 0) == 0);

		m_slColorIntensity->SetRange(
			ColorizationPresets::ClassicIntensityMinimum,
			ColorizationPresets::ClassicIntensityMaximum
		);
		const DWORD colorIntensity = std::clamp<DWORD>(m_config->GetDword(L"GlassOpacity", 63), ColorizationPresets::ClassicIntensityMinimum, ColorizationPresets::ClassicIntensityMaximum);
		m_slColorIntensity->SetValue(colorIntensity);
		syncSliderTooltip(m_slColorIntensity);
		LoadColorizationBalances();

		DWORD captionActive = m_config->GetDword(L"ColorizationColorCaption", 0xFFFFFFFD);
		DWORD captionInactive = m_config->GetDword(L"ColorizationColorCaptionInactive", captionActive);
		DWORD captionMaximized = m_config->GetDword(L"ColorizationColorCaptionMaximized", captionActive);
		DWORD captionInactiveMaximized = m_config->GetDword(L"ColorizationColorCaptionInactiveMaximized", captionInactive);

		ApplyChoiceColorEx(m_chModeColorCaption, m_cpColorCaption, captionActive, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
		ApplyChoiceColorEx(m_chModeColorCaptionInactive, m_cpColorCaptionInactive, captionInactive, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
		ApplyChoiceColorEx(m_chModeColorCaptionMaximized, m_cpColorCaptionMaximized, captionMaximized, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);
		ApplyChoiceColorEx(m_chModeColorCaptionInactiveMaximized, m_cpColorCaptionInactiveMaximized, captionInactiveMaximized, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFF);

		// Advanced Blending
		auto loadBaseColor = [&](wxChoice* ch, wxColourPickerCtrl* cp, wxSpinCtrl* sc, const std::wstring& key, DWORD themeSentinel, DWORD autoSentinel) {
			DWORD val = m_config->GetDword(key, autoSentinel);
			if (val == autoSentinel) {
				ch->SetSelection(0);
				cp->Enable(false);
				sc->Enable(false);
				cp->SetColour(*wxLIGHT_GREY);
				sc->SetValue(255);
			} else if (val == themeSentinel) {
				ch->SetSelection(1);
				cp->Enable(false);
				sc->Enable(false);
				cp->SetColour(*wxLIGHT_GREY);
				sc->SetValue(255);
			} else {
				ch->SetSelection(2);
				cp->Enable(true);
				sc->Enable(true);
				cp->SetColour(dwordToColor(val));
				sc->SetValue((val >> 24) & 0xFF);
			}
		};

		loadBaseColor(m_chModeBaseTransparent, m_cpBaseTransparent, m_scBaseTransparentAlpha, L"ColorizationBaseTransparent", 0xFFFFFFFF, 0xFFFFFFFE);
		loadBaseColor(m_chModeBaseMaximized, m_cpBaseMaximized, m_scBaseMaximizedAlpha, L"ColorizationBaseMaximized", 0xFFFFFFFF, 0xFFFFFFFE);
		loadBaseColor(m_chModeBaseOpaque, m_cpBaseOpaque, m_scBaseOpaqueAlpha, L"ColorizationBaseOpaque", 0xFFFFFFFF, 0xFFFFFFFE);

		DWORD priority = m_config->GetDword(L"ColorizationOpaqueBlendPriority", 0xFFFFFFFF);
		m_chOpaqueBlendPriority->SetSelection(priority == 0xFFFFFFFF ? 2 : std::clamp<int>(priority, 0, 1));

		DWORD colorOpacityActive = m_config->GetDword(L"ColorizationOpacity", 0xFFFFFFFE);
		DWORD colorOpacityInactive = m_config->GetDword(L"ColorizationOpacityInactive", colorOpacityActive);
		DWORD colorOpacityMaximized = m_config->GetDword(L"ColorizationOpacityMaximized", colorOpacityActive);
		DWORD colorOpacityInactiveMaximized = m_config->GetDword(L"ColorizationOpacityInactiveMaximized", colorOpacityInactive);

		ApplyChoiceSlider(m_chModeColorizationOpacity, m_slColorizationOpacity, colorOpacityActive, 0xFFFFFFFF, 0xFFFFFFFE, 100);
		ApplyChoiceSlider(m_chModeColorizationOpacityInactive, m_slColorizationOpacityInactive, colorOpacityInactive, 0xFFFFFFFF, 0xFFFFFFFE, 100);
		ApplyChoiceSlider(m_chModeColorizationOpacityMaximized, m_slColorizationOpacityMaximized, colorOpacityMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 100);
		ApplyChoiceSlider(m_chModeColorizationOpacityInactiveMaximized, m_slColorizationOpacityInactiveMaximized, colorOpacityInactiveMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 100);
		syncSliderTooltip(m_slColorizationOpacity);
		syncSliderTooltip(m_slColorizationOpacityInactive);
		syncSliderTooltip(m_slColorizationOpacityMaximized);
		syncSliderTooltip(m_slColorizationOpacityInactiveMaximized);

		// Accent
		m_chkGlassOverrideAccent->SetValue(m_config->GetDword(L"GlassOverrideAccent", 0) != 0);

		// Update UI State
		UpdateUIVisibility();
		UpdateOptionStatusIcons();
		UpdatePathWarningIcons();
	}

	void MainFrame::LoadColorizationBalances()
	{
		auto load = [this](wxSlider* slider, Settings::Id id, DWORD defaultValue)
		{
			const auto value = std::min<DWORD>(m_config->GetDword(std::wstring(Settings::Get(id).name), defaultValue), 100);
			slider->SetValue(value);
			slider->SetToolTip(wxString::Format(L"%d", slider->GetValue()));
		};
		load(m_slColorBalance, Settings::Id::ColorizationColorBalanceOverride, 10);
		load(m_slAfterglowBalance, Settings::Id::ColorizationAfterglowBalanceOverride, 10);
		load(m_slBlurBalance, Settings::Id::ColorizationBlurBalanceOverride, 50);
	}

	void MainFrame::UpdateUIVisibility()
	{
		// 0 = Vista blur, 1 = Win7 blur
		bool isVista = (m_rbGlassType->GetSelection() == 0);
		m_compositionParametersPane->Show(!isVista);

		if (m_colorPresetList)
		{
			m_colorPresetList->SetFamily(isVista ? ColorizationPresets::Family::Vista : ColorizationPresets::Family::Windows7);
		}

		// IMPORTANT: m_rbGlassType->GetContainingSizer() is the *row* that hosts the radiobox,
		// not the page root sizer. Use the Glass Colors page for visibility changes.
		if (m_glassColorsPanel)
		{
			LayoutWrappingPage(m_glassColorsPanel);
		}

		UpdateOptionStatusIcons();
		UpdatePathWarningIcons();
		UpdateColorizationPresetSelection();
	}
}
