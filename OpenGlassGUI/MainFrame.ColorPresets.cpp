#include "pch.h"
#include "MainFrame.hpp"
#include "ColorSwatchButton.hpp"

namespace OpenGlass
{
	void MainFrame::ApplyColorizationPreset(const ColorizationPresets::Preset& preset)
	{
		RegistryConfig* config = GetConfigForKey(L"ColorizationColorOverride");
		if (!config)
		{
			return;
		}

		const auto application = ColorizationPresets::BuildApplication(
			preset,
			m_chkEnableTransparency && !m_chkEnableTransparency->IsChecked()
		);
		auto setDword = [this, config](PCWSTR name, DWORD value) {
			TrackSettingChange(name);
			config->SetDword(name, value);
		};

		setDword(L"ColorizationColorOverride", application.color);
		if (application.vistaOpacity)
		{
			setDword(L"GlassOpacity", *application.vistaOpacity);
		}
		if (application.windows7)
		{
			const auto& parameters = *application.windows7;
			setDword(L"ColorizationAfterglowOverride", parameters.afterglow);
			setDword(L"ColorizationColorBalanceOverride", parameters.colorBalance);
			setDword(L"ColorizationAfterglowBalanceOverride", parameters.afterglowBalance);
			setDword(L"ColorizationBlurBalanceOverride", parameters.blurBalance);
		}

		SetDirty(true);
		NotifySettingsChange(ChangeType::Colorization);
		LoadSettings(false);
	}

	const ColorizationPresets::Preset* MainFrame::FindMatchingWindows7Preset(bool opaque) const
	{
		if (!m_config || !m_rbGlassType || m_rbGlassType->GetSelection() != 1)
		{
			return nullptr;
		}

		const DWORD color = ResolveOverridableDword(
			L"ColorizationColor",
			L"ColorizationColorOverride",
			0xFF000000
		).value;
		const DWORD afterglow = ResolveOverridableDword(
			L"ColorizationAfterglow",
			L"ColorizationAfterglowOverride",
			0
		).value;
		const DWORD colorBalance = ResolveOverridableDword(
			L"ColorizationColorBalance",
			L"ColorizationColorBalanceOverride",
			10
		).value;
		const DWORD afterglowBalance = ResolveOverridableDword(
			L"ColorizationAfterglowBalance",
			L"ColorizationAfterglowBalanceOverride",
			10
		).value;
		const DWORD blurBalance = ResolveOverridableDword(
			L"ColorizationBlurBalance",
			L"ColorizationBlurBalanceOverride",
			50
		).value;

		for (const auto& preset : ColorizationPresets::Windows7)
		{
			const auto expected = ColorizationPresets::CalculateWindows7Parameters(preset.argb, opaque);
			if (
				color == expected.color
				&& afterglow == expected.afterglow
				&& colorBalance == expected.colorBalance
				&& afterglowBalance == expected.afterglowBalance
				&& blurBalance == expected.blurBalance
			)
			{
				return &preset;
			}
		}

		return nullptr;
	}

	void MainFrame::UpdateColorizationPresetSelection()
	{
		if (!m_config || !m_rbGlassType)
		{
			return;
		}

		const ColorizationPresets::Preset* selectedPreset = nullptr;
		if (m_rbGlassType->GetSelection() == 0)
		{
			const DWORD color = ResolveOverridableDword(
				L"ColorizationColor",
				L"ColorizationColorOverride",
				0xFF000000
			).value;
			const DWORD opacity = m_config->GetDword(L"GlassOpacity", 63);
			for (const auto& preset : ColorizationPresets::Vista)
			{
				if (
					color == preset.argb
					&& opacity == ColorizationPresets::CalculateVistaOpacity(preset.argb)
				)
				{
					selectedPreset = &preset;
					break;
				}
			}
		}
		else
		{
			selectedPreset = FindMatchingWindows7Preset(
				m_chkEnableTransparency && !m_chkEnableTransparency->IsChecked()
			);
		}

		for (const auto& [preset, button] : m_presetButtons)
		{
			if (button)
			{
				button->SetValue(preset == selectedPreset);
			}
		}
	}
}
