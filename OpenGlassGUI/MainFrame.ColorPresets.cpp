#include "pch.h"
#include "MainFrame.hpp"
#include "ColorSwatchButton.hpp"

namespace OpenGlass
{
	void MainFrame::ApplyColorizationPreset(const ColorizationPresets::Preset& preset)
	{
		RegistryConfig* config = GetConfigForSetting(Settings::Id::ColorizationColorOverride);
		if (!config)
		{
			return;
		}

		const auto application = ColorizationPresets::BuildApplication(
			preset,
			m_chkEnableTransparency && !m_chkEnableTransparency->IsChecked()
		);
		auto setDword = [this, config](Settings::Id id, DWORD value) {
			const std::wstring name(Settings::Get(id).name);
			TrackSettingChange(id);
			return CheckRegistryWrite(config->SetDword(name, value), name);
		};

		if (!setDword(Settings::Id::ColorizationColorOverride, application.color)) return;
		if (application.vistaOpacity)
		{
			if (!setDword(Settings::Id::GlassOpacity, *application.vistaOpacity)) return;
		}
		if (application.windows7)
		{
			const auto& parameters = *application.windows7;
			if (!setDword(Settings::Id::ColorizationAfterglowOverride, parameters.afterglow)) return;
			if (!setDword(Settings::Id::ColorizationColorBalanceOverride, parameters.colorBalance)) return;
			if (!setDword(Settings::Id::ColorizationAfterglowBalanceOverride, parameters.afterglowBalance)) return;
			if (!setDword(Settings::Id::ColorizationBlurBalanceOverride, parameters.blurBalance)) return;
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
			Settings::Id::ColorizationColor,
			Settings::Id::ColorizationColorOverride,
			0xFF000000
		).value;
		const DWORD afterglow = ResolveOverridableDword(
			Settings::Id::ColorizationAfterglow,
			Settings::Id::ColorizationAfterglowOverride,
			0
		).value;
		const DWORD colorBalance = ResolveOverridableDword(
			Settings::Id::ColorizationColorBalance,
			Settings::Id::ColorizationColorBalanceOverride,
			10
		).value;
		const DWORD afterglowBalance = ResolveOverridableDword(
			Settings::Id::ColorizationAfterglowBalance,
			Settings::Id::ColorizationAfterglowBalanceOverride,
			10
		).value;
		const DWORD blurBalance = ResolveOverridableDword(
			Settings::Id::ColorizationBlurBalance,
			Settings::Id::ColorizationBlurBalanceOverride,
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
				Settings::Id::ColorizationColor,
				Settings::Id::ColorizationColorOverride,
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
