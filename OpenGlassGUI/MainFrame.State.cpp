#include "pch.h"
#include "MainFrame.hpp"

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

	void MainFrame::LoadSettings(bool saveBackup)
	{
		if (saveBackup)
		{
			m_backupSettings.clear();
			m_dirtyKeys.clear();
		}

		RegistryConfig* systemConfig = m_systemConfig ? m_systemConfig.get() : m_config.get();

		// System
		m_chkDisableGlassOnBattery->SetValue(systemConfig->GetDword(L"DisableGlassOnBattery", 1) != 0);

		DWORD hookMask = systemConfig->GetDword(L"DisabledHooks", 0);
		m_clDisabledHooks->Check(0, (hookMask & 0x1) != 0);
		m_clDisabledHooks->Check(1, (hookMask & 0x2) != 0);
		m_clDisabledHooks->Check(2, (hookMask & 0x4) != 0);
		m_clDisabledHooks->Check(3, (hookMask & 0x8) != 0);
		m_clDisabledHooks->Check(4, (hookMask & 0x10) != 0);

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
		DWORD refOpacityInactive = m_config->HasValue(L"ColorizationGlassReflectionOpacityInactive")
			? m_config->GetDword(L"ColorizationGlassReflectionOpacityInactive", refOpacityActive)
			: refOpacityActive;
		DWORD refOpacityMaximized = m_config->HasValue(L"ColorizationGlassReflectionOpacityMaximized")
			? m_config->GetDword(L"ColorizationGlassReflectionOpacityMaximized", refOpacityActive)
			: refOpacityActive;
		DWORD refOpacityInactiveMaximized = m_config->HasValue(L"ColorizationGlassReflectionOpacityInactiveMaximized")
			? m_config->GetDword(L"ColorizationGlassReflectionOpacityInactiveMaximized", refOpacityInactive)
			: refOpacityInactive;

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
		// Legacy (kept for reference):
		// if (refPolicy == 0xFFFFFFFF)
		// {
		// 	for (unsigned int i = 0; i < m_clReflectionPolicy->GetCount(); ++i)
		// 		m_clReflectionPolicy->Check(i, true);
		// }
		// else
		// {
		// 	m_clReflectionPolicy->Check(0, (refPolicy & (1 << 0)) != 0);
		// 	m_clReflectionPolicy->Check(1, (refPolicy & (1 << 2)) != 0);
		// 	m_clReflectionPolicy->Check(2, (refPolicy & (1 << 3)) != 0);
		// }

		std::wstring material = m_config->GetString(L"CustomThemeMaterial", L"");
		m_fpCustomThemeMaterial->SetPath(material);
		m_chkCustomThemeMaterial->SetValue(!material.empty());
		m_fpCustomThemeMaterial->Enable(!material.empty());

		m_slMaterialOpacity->SetValue(m_config->GetDword(L"MaterialOpacity", 0));
		syncSliderTooltip(m_slMaterialOpacity);

		bool useD3D = m_config->GetDword(L"UseDirect3DRendering", 0) != 0;
		m_chkUseD3D->SetValue(useD3D);
		auto blurDeviationToRadius = [](DWORD deviation) -> int {
			int val = (static_cast<int>(deviation) * 3 + 5) / 10;
			if (val < 0) val = 0;
			if (val > 30) val = 30;
			return val;
		};

		if (useD3D)
		{
			m_slBlurDeviation->SetValue(3);
			m_slBlurDeviation->Enable(false);
			syncSliderTooltip(m_slBlurDeviation);

			m_chBlurOptimization->SetSelection(0);
			m_chBlurOptimization->Enable(false);
		}
		else
		{
			m_slBlurDeviation->SetValue(blurDeviationToRadius(m_config->GetDword(L"BlurDeviation", 30)));
			m_slBlurDeviation->Enable(true);
			syncSliderTooltip(m_slBlurDeviation);

			m_chBlurOptimization->SetSelection(std::clamp<int>(m_config->GetDword(L"BlurOptimization", 0), 0, 2));
			m_chBlurOptimization->Enable(true);
		}
		m_chkGlassSafetyZone->SetValue(systemConfig->GetDword(L"GlassSafetyZoneMode", 1) == 0);

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
		m_chkOpaqueBlend->SetValue(m_config->GetDword(L"ColorizationOpaqueBlend", 0) != 0);

		const DWORD activeColor = ResolveOverridableDword(
			L"ColorizationColor",
			L"ColorizationColorOverride",
			0xFF000000
		).value;
		m_cpColorizationColor->SetColour(dwordToColor(activeColor));
		m_slGlassOpacity->SetValue(m_config->GetDword(L"GlassOpacity", 63));
		syncSliderTooltip(m_slGlassOpacity);

		auto loadOptColor = [&](wxCheckBox* chk, wxColourPickerCtrl* cp, const std::wstring& key, DWORD defVal) {
			DWORD val = m_config->GetDword(key, 0xFFFFFFFE);
			// If missing (returns sentinel) or explicitly set to "Auto" (FE) or "Theme" (FF) -> Uncheck
			if (val == 0xFFFFFFFE || val == 0xFFFFFFFF) {
				chk->SetValue(false);
				cp->Enable(false);
				if (defVal == 0xFF000000)
					cp->SetColour(m_cpColorizationColor->GetColour());
				else
					cp->SetColour(dwordToColor(defVal));
			} else {
				chk->SetValue(true);
				cp->Enable(true);
				cp->SetColour(dwordToColor(val));
			}
		};

		// Inactive Color
		loadOptColor(m_chkEnableInactiveColor, m_cpColorizationColorInactive, L"ColorizationColorInactive", 0xFF000000); // 0xFF000000 means use active as visual default

		// Inactive Opacity
		const DWORD activeOpacity = m_config->GetDword(L"GlassOpacity", 63);
		const bool hasInactiveOpacity = m_config->HasValue(L"GlassOpacityInactive");
		DWORD inactiveOp = hasInactiveOpacity ? m_config->GetDword(L"GlassOpacityInactive", activeOpacity) : activeOpacity;
		m_chkEnableInactiveOpacity->SetValue(hasInactiveOpacity);
		m_slGlassOpacityInactive->Enable(hasInactiveOpacity);
		m_slGlassOpacityInactive->SetValue(inactiveOp);
		syncSliderTooltip(m_slGlassOpacityInactive);

		DWORD captionActive = m_config->GetDword(L"ColorizationColorCaption", 0xFFFFFFFD);
		DWORD captionInactive = m_config->HasValue(L"ColorizationColorCaptionInactive")
			? m_config->GetDword(L"ColorizationColorCaptionInactive", captionActive)
			: captionActive;
		DWORD captionMaximized = m_config->HasValue(L"ColorizationColorCaptionMaximized")
			? m_config->GetDword(L"ColorizationColorCaptionMaximized", captionActive)
			: captionActive;
		DWORD captionInactiveMaximized = m_config->HasValue(L"ColorizationColorCaptionInactiveMaximized")
			? m_config->GetDword(L"ColorizationColorCaptionInactiveMaximized", captionInactive)
			: captionInactive;

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
		DWORD colorOpacityInactive = m_config->HasValue(L"ColorizationOpacityInactive")
			? m_config->GetDword(L"ColorizationOpacityInactive", colorOpacityActive)
			: colorOpacityActive;
		DWORD colorOpacityMaximized = m_config->HasValue(L"ColorizationOpacityMaximized")
			? m_config->GetDword(L"ColorizationOpacityMaximized", colorOpacityActive)
			: colorOpacityActive;
		DWORD colorOpacityInactiveMaximized = m_config->HasValue(L"ColorizationOpacityInactiveMaximized")
			? m_config->GetDword(L"ColorizationOpacityInactiveMaximized", colorOpacityInactive)
			: colorOpacityInactive;

		ApplyChoiceSlider(m_chModeColorizationOpacity, m_slColorizationOpacity, colorOpacityActive, 0xFFFFFFFF, 0xFFFFFFFE, 100);
		ApplyChoiceSlider(m_chModeColorizationOpacityInactive, m_slColorizationOpacityInactive, colorOpacityInactive, 0xFFFFFFFF, 0xFFFFFFFE, 100);
		ApplyChoiceSlider(m_chModeColorizationOpacityMaximized, m_slColorizationOpacityMaximized, colorOpacityMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 100);
		ApplyChoiceSlider(m_chModeColorizationOpacityInactiveMaximized, m_slColorizationOpacityInactiveMaximized, colorOpacityInactiveMaximized, 0xFFFFFFFF, 0xFFFFFFFE, 100);
		syncSliderTooltip(m_slColorizationOpacity);
		syncSliderTooltip(m_slColorizationOpacityInactive);
		syncSliderTooltip(m_slColorizationOpacityMaximized);
		syncSliderTooltip(m_slColorizationOpacityInactiveMaximized);

		m_slBlurBalance->SetValue(ResolveOverridableDword(
			L"ColorizationBlurBalance",
			L"ColorizationBlurBalanceOverride",
			50
		).value);
		syncSliderTooltip(m_slBlurBalance);

		m_slAfterglowBalance->SetValue(ResolveOverridableDword(
			L"ColorizationAfterglowBalance",
			L"ColorizationAfterglowBalanceOverride",
			10
		).value);
		syncSliderTooltip(m_slAfterglowBalance);

		m_slColorBalance->SetValue(ResolveOverridableDword(
			L"ColorizationColorBalance",
			L"ColorizationColorBalanceOverride",
			10
		).value);
		syncSliderTooltip(m_slColorBalance);
		wxColour afterglowColor;
		DWORD afterglowVal = ResolveOverridableDword(
			L"ColorizationAfterglow",
			L"ColorizationAfterglowOverride",
			0
		).value;
		// Format is: AA RR GG BB
		afterglowColor.Set(
			(afterglowVal >> 16) & 0xFF,
			(afterglowVal >> 8) & 0xFF,
			afterglowVal & 0xFF,
			(afterglowVal >> 24) & 0xFF
		);
		m_cpAfterglow->SetColour(afterglowColor);

		// Accent
		m_chkGlassOverrideAccent->SetValue(m_config->GetDword(L"GlassOverrideAccent", 0) != 0);

		// Update UI State
		UpdateUIVisibility();
		UpdateOptionStatusIcons();
		UpdatePathWarningIcons();
	}

	void MainFrame::UpdateUIVisibility()
	{
		// 0 = Vista blur, 1 = Win7 blur
		bool isVista = (m_rbGlassType->GetSelection() == 0);

		// Vista specific groups (Inactive Column is used in both, but layout differs)
		// README says: "GlassOpacityInactive and ColorizationColorInactive ... Only used when GlassType = 0x0"
		// So Inactive Column should be hidden for Win7.
		if (m_colorsRowSizer && m_inactiveColumnSizer)
			m_colorsRowSizer->Show(m_inactiveColumnSizer, isVista, true);

		// Afterglow is ONLY for Win7 style (GlassType=1)
		if (m_afterglowColumnSizer)
		{
			m_colorsRowSizer->Show(m_afterglowColumnSizer, !isVista, true);
		}

		if (m_glassColorsGroupSizer && m_vistaOpacitySizer)
			m_glassColorsGroupSizer->Show(m_vistaOpacitySizer, isVista, true);

		// IMPORTANT: m_rbGlassType->GetContainingSizer() is the *row* that hosts the radiobox,
		// not the page root sizer. Use the Glass Colors page/root sizer for visibility changes.
		wxWindow* page = m_glassColorsPanel;
		wxSizer* root = m_glassColorsRootSizer;
		if (!root && page)
		{
			root = page->GetSizer();
		}
		if (root)
		{
			if (m_vistaGroupSizer)
			{
				root->Show(m_vistaGroupSizer, isVista, true);
			}
			if (m_win7GroupSizer)
			{
				root->Show(m_win7GroupSizer, !isVista, true);
			}
		}

		if (page)
		{
			page->Layout();
			if (wxScrolledWindow* scrolled = wxDynamicCast(page, wxScrolledWindow))
			{
				scrolled->FitInside();
			}
		}

		UpdateOptionStatusIcons();
		UpdatePathWarningIcons();
	}
}
