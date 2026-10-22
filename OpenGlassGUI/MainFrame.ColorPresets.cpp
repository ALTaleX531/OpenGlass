#include "pch.h"
#include "MainFrame.hpp"
#include "ColorPresetList.hpp"
#include "EffectiveConfiguration.hpp"

namespace OpenGlass
{
	void MainFrame::ApplyColorizationBalances(DWORD argb)
	{
		const auto parameters = ColorizationPresets::CalculateWindows7Parameters(
			argb,
			!m_chkEnableTransparency->IsChecked());
		const std::pair<Settings::Id, DWORD> values[]
		{
			{ Settings::Id::ColorizationColorBalanceOverride, parameters.colorBalance },
			{ Settings::Id::ColorizationAfterglowBalanceOverride, parameters.afterglowBalance },
			{ Settings::Id::ColorizationBlurBalanceOverride, parameters.blurBalance }
		};
		for (const auto& [id, value] : values)
		{
			TrackSettingChange(id);
			const std::wstring name(Settings::Get(id).name);
			THROW_IF_FAILED(m_config->SetDword(name, value));
		}
		LoadColorizationBalances();
	}

	void MainFrame::ApplyAccentColor(std::optional<DWORD> argb)
	{
		THROW_IF_FAILED(m_colorPreference.Apply(m_targetUserSid.ToStdWstring(), argb, [this](const auto& before)
		{
			m_resources.TrackColor(before);
			TrackSettingChange(Settings::Scope::User, Settings::Id::ColorizationColor);
			TrackSettingChange(Settings::Scope::User, Settings::Id::ColorizationAfterglow);
		}));
		const auto baseline = m_colorPreference.Baseline();
		if (!argb && (!baseline || !baseline->applyChoice || baseline->RestoresAutomatic()))
		{
			// Returning to Automatic leaves no fixed RGB customization to undo.
			m_preview.DiscardBaseline({ Settings::Scope::User, Settings::Id::ColorizationColor });
			m_preview.DiscardBaseline({ Settings::Scope::User, Settings::Id::ColorizationAfterglow });
		}
	}

	void MainFrame::ApplyColorizationColor(std::optional<DWORD> argb, ColorizationPresets::Family family)
	{
		if (!m_config)
		{
			return;
		}

		if (RunPreview([&]
		{
			auto setDword = [this](Settings::Id id, DWORD value) {
				const std::wstring name(Settings::Get(id).name);
				TrackSettingChange(id);
				THROW_IF_FAILED(m_config->SetDword(name, value));
			};

			const auto cleanup = EffectiveConfiguration::PlanColorCleanup(EffectiveConfiguration::Read(*m_userConfig, false), EffectiveConfiguration::Read(*m_systemConfig));
			THROW_IF_FAILED(m_userConfig->CheckDeleteAccess());
			THROW_IF_FAILED(m_systemConfig->CheckDeleteAccess());
			ApplyAccentColor(argb);
			for (const auto& change : cleanup)
			{
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), GetConfigForScope(change.scope)->ReadRaw(change.Name()) != change.before);
				TrackSettingChange(change.scope, change.id);
				THROW_IF_FAILED(GetConfigForScope(change.scope)->WriteRaw(change.Name(), change.after));
			}
			DWORD color = argb.value_or(0);
			// Native image calculation requires Automatic to be enabled. Its alpha
			// is independent of the Shell's final ColorizationColor alpha.
			if (!argb) THROW_IF_FAILED(m_colorPreference.ComputeWallpaperColor(m_targetUserSid.ToStdWstring(), color));
			setDword(Settings::Id::GlassOpacity, ColorizationPresets::CalculateVistaOpacity(color));
			if (family == ColorizationPresets::Family::Windows7) ApplyColorizationBalances(color);
		}, Settings::UpdateType::Backdrop)) LoadSettings();
	}

	void MainFrame::ApplyColorizationPreset(const ColorizationPresets::Preset& preset)
	{
		ApplyColorizationColor(preset.argb, preset.family);
	}

	const ColorizationPresets::Preset* MainFrame::FindMatchingWindows7Preset(bool opaque) const
	{
		if (!m_config || !m_rbGlassType || m_rbGlassType->GetSelection() != 1)
		{
			return nullptr;
		}

		const DWORD color = m_userConfig->GetDword(L"ColorizationColor", 0xFF000000);
		const DWORD afterglow = m_userConfig->GetDword(L"ColorizationAfterglow", 0);
		const DWORD colorBalance = m_config->GetDword(L"ColorizationColorBalanceOverride", 10);
		const DWORD afterglowBalance = m_config->GetDword(L"ColorizationAfterglowBalanceOverride", 10);
		const DWORD blurBalance = m_config->GetDword(L"ColorizationBlurBalanceOverride", 50);

		for (const auto& preset : ColorizationPresets::Windows7)
		{
			if (ColorizationPresets::MatchesWindows7Preset(preset,
				{ color, afterglow, colorBalance, afterglowBalance, blurBalance },
				m_config->GetDword(L"GlassOpacity", 63), opaque))
			{
				return &preset;
			}
		}

		return nullptr;
	}

	WXLRESULT MainFrame::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
	{
		// Theme notifications can bracket a mode change: retain each deferred update.
		const auto result = wxFrame::MSWWindowProc(message, wParam, lParam);
		if (message == WM_DWMCOLORIZATIONCOLORCHANGED || message == WM_SETTINGCHANGE)
		{
			QueueColorizationRefresh();
		}
		return result;
	}

	void MainFrame::QueueColorizationRefresh()
	{
		if (!m_customColorsInitialized || m_colorizationRefreshPending || IsBeingDeleted()) return;
		m_colorizationRefreshPending = true;
		CallAfter([this]
		{
			m_colorizationRefreshPending = false;
			// Our own attempt refreshes controls when complete; don't read intermediate writes.
			if (IsBeingDeleted() || m_preview.IsAttemptActive()) return;
			// Read only: external color changes must not alter the preview journal or saved presets.
			UpdateColorizationPresetSelection();
		});
	}

	void MainFrame::UpdateColorizationPresetSelection()
	{
		if (!m_config || !m_rbGlassType)
		{
			return;
		}

		bool automatic{};
		const auto modeResult = m_colorPreference.ReadAutoColorization(m_targetUserSid.ToStdWstring(), automatic);
		const bool manual = SUCCEEDED(modeResult) && !automatic;
		m_colorPresetList->SetAutomaticDescription(SUCCEEDED(modeResult) ? wxString(L"Automatic accent color from the wallpaper")
			: wxString::Format(L"Could not read the automatic accent color setting (0x%08lX).", modeResult));
		const ColorizationPresets::Preset* selectedPreset = nullptr;
		if (m_rbGlassType->GetSelection() == 0)
		{
			const DWORD color = m_userConfig->GetDword(L"ColorizationColor", 0xFF000000);
			const DWORD opacity = m_config->GetDword(L"GlassOpacity", 63);
			for (const auto& preset : ColorizationPresets::Vista)
			{
				if (
					(color & 0xFFFFFF) == (preset.argb & 0xFFFFFF)
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
		DWORD color = m_userConfig->GetDword(L"ColorizationColor", 0xFF000000);
		bool colorAvailable = true;
		if (SUCCEEDED(modeResult) && automatic)
		{
			// Read the original user's current accent, never the elevated account or message payload.
			colorAvailable = SUCCEEDED(m_colorPreference.ReadRgb(m_targetUserSid.ToStdWstring(), color));
		}
		if (colorAvailable && (!m_customColorsInitialized || automatic || selectedPreset == nullptr))
		{
			m_colorPresetList->SetCustomColor((color & 0xFFFFFF) | (ColorizationPresets::CalculateIntensityAlpha(
				std::min<DWORD>(m_config->GetDword(L"GlassOpacity", 63), 100)) << 24));
		}
		long selection = -1;
		if (SUCCEEDED(modeResult) && automatic) selection = 0;
		else if (manual)
		{
			selection = m_colorPresetList->GetCustomIndex();
			for (long index = 1; index < m_colorPresetList->GetCustomIndex(); ++index)
			{
				if (m_colorPresetList->GetPreset(index) == selectedPreset)
				{
					selection = index;
					break;
				}
			}
		}
		m_colorPresetList->SetCurrentSelection(selection);
		m_customColorsInitialized = true;
	}
}
