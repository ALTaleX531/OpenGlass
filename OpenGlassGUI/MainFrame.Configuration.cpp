#include "pch.h"
#include "MainFrame.hpp"
#include "ConfigurationMigration.hpp"
#include "EffectiveConfiguration.hpp"

namespace OpenGlass
{
	RegistryConfig* MainFrame::GetConfigForScope(Settings::Scope scope) const
	{
		return scope == Settings::Scope::User ? m_userConfig.get() : m_systemConfig.get();
	}

	void MainFrame::EnsurePreviewWriter()
	{
		if (m_previewWriter) return;
		m_previewWriter = m_resources.AcquireWriter();
		auto releaseOnFailure = wil::scope_exit([this] { m_previewWriter.reset(); });
		if (m_resources.HasForeignRecovery())
		{
			m_previewWriter.reset();
			wxMessageBox(L"An interrupted operation belongs to another user. Open the editor as that user to recover it before changing configuration.", L"Configuration recovery", wxOK | wxICON_WARNING, this);
			THROW_HR(HRESULT_FROM_WIN32(ERROR_RECOVERY_FAILURE));
		}
		if (m_resources.HasRecovery())
		{
			if (wxMessageBox(L"An interrupted configuration operation needs recovery before editing. Restore its previous state now?", L"Recover configuration", wxYES_NO | wxICON_WARNING, this) != wxYES)
			{
				m_previewWriter.reset(); THROW_HR(HRESULT_FROM_WIN32(ERROR_RECOVERY_FAILURE));
			}
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RECOVERY_FAILURE), !m_resources.Recover(
				[this](auto scope, auto id, const auto& value) { return SUCCEEDED(GetConfigForScope(scope)->WriteRaw(std::wstring(Settings::Get(id).name), value)); },
				[this](const auto& color)
				{
					ColorPreference::Snapshot current;
					return SUCCEEDED(m_colorPreference.Capture(m_targetUserSid.ToStdWstring(), current))
						&& SUCCEEDED(m_colorPreference.RecoverSnapshot(color));
				}));
			m_colorPreference.Accept(); NotifySettingsChange(); LoadSettings();
		}
		releaseOnFailure.release();
	}

	void MainFrame::TrackSettingChange(Settings::Id id)
	{
		TrackSettingChange(m_editScope, id);
	}

	void MainFrame::TrackSettingChange(Settings::Scope scope, Settings::Id id)
	{
		EnsurePreviewWriter();
		const auto before = GetConfigForScope(scope)->ReadRaw(std::wstring(Settings::Get(id).name));
		// Durable recovery and in-memory undo describe the same before-value.
		m_resources.TrackRegistry(scope, id, before);
		m_preview.Touch({ scope, id }, [&](const TrackedSetting&) { return before; });
	}

	void MainFrame::ReconcilePreview()
	{
		m_resources.Reconcile();
		m_preview.Reconcile([this](const TrackedSetting& key) { return GetConfigForScope(key.scope)->ReadRaw(key.Name()); });
		SetDirty(m_preview.IsDirty() || m_colorPreference.IsDirty() || m_resources.IsDirty());
	}

	void MainFrame::ReportRegistryError(HRESULT result, const std::wstring& name)
	{
		if (SUCCEEDED(result)) return;
		if (m_preview.IsAttemptActive()) THROW_IF_FAILED(result);
		SetDirty(m_preview.IsDirty() || m_colorPreference.IsDirty() || m_resources.IsDirty());
		NotifySettingsChange();
		LoadSettings();
		wxMessageBox(
			wxString::Format(L"The setting '%s' could not be updated (HRESULT 0x%08lX).", name.c_str(), static_cast<unsigned long>(result)),
			L"OpenGlass configuration",
			wxOK | wxICON_ERROR,
			this
		);
	}

	void MainFrame::RestoreDefaults()
	{
		if (m_presetWorkActive || m_configurationOperationActive) return;
		if (!m_config || (m_editScope == Settings::Scope::Machine && !m_isAdmin)) return;
		if (wxMessageBox(wxString::Format(
			L"Restore all OpenGlass settings in %s to defaults? This removes custom values in this scope; values from the other scope may still apply. Windows accent color and diagnostics settings will be preserved. Use Revert to undo this preview.",
			m_editScope == Settings::Scope::User ? L"HKCU" : L"HKLM"), L"Restore defaults", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES) return;
		bool restartRequired{};
		const bool applied = RunPreview([this, &restartRequired]
		{
			const auto plan = EffectiveConfiguration::PlanReset(EffectiveConfiguration::Read(*m_config, false), m_editScope);
			for (const auto& change : plan)
			{
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), m_config->ReadRaw(change.Name()) != change.before);
				restartRequired |= Settings::Get(change.id).impact == Settings::UpdateType::RestartRequired;
				TrackSettingChange(change.id);
				THROW_IF_FAILED(m_config->WriteRaw(change.Name(), change.after));
			}
		});
		if (applied) LoadSettings();
		if (applied && restartRequired) wxMessageBox(L"Some settings take effect after restarting DWM or signing out. DWM has not been restarted.",
			L"Restore defaults", wxOK | wxICON_INFORMATION, this);
	}

	void MainFrame::MergeConfiguration()
	{
		if (m_presetWorkActive || m_configurationOperationActive) return;
		if (m_editScope == Settings::Scope::Machine && !m_isAdmin) return;
		const wxString title = m_editScope == Settings::Scope::User ? L"Merge into HKCU" : L"Merge into HKLM";
		if (m_isDirty)
		{
			wxMessageBox(L"Save pending changes before merging configuration.", title, wxOK | wxICON_INFORMATION, this);
			return;
		}
		try
		{
			auto prepareChanges = [&]
			{
				auto changes = ConfigurationMigration::Prepare(*m_userConfig, *m_systemConfig, m_editScope);
				const auto effective = EffectiveConfiguration::Capture(EffectiveConfiguration::Read(*m_userConfig, false), EffectiveConfiguration::Read(*m_systemConfig), Settings::CatalogVersion, false);
				for (const auto& [id, value] : effective)
				{
					if (Settings::Get(id).assetRole == Settings::AssetRole::None) continue;
					const auto source = std::get_if<std::wstring>(&value);
					if (!source || source->empty() || CompareStringOrdinal(source->c_str(), -1, m_resources.Path(m_editScope, id).c_str(), -1, TRUE) == CSTR_EQUAL) continue;
					if (std::ranges::any_of(changes, [&](const auto& change) { return change.scope == m_editScope && change.id == id; })) continue;
					const std::wstring name(Settings::Get(id).name);
					changes.push_back({ m_editScope, id, m_config->ReadRaw(name), EffectiveConfiguration::Encode(value), L"Copy " + name + L" to this layer's fixed resource location." });
				}
				std::stable_partition(changes.begin(), changes.end(), [&](const auto& change) { return change.scope == m_editScope; });
				return changes;
			};
			const auto changes = prepareChanges();
			if (changes.empty())
			{
				wxMessageBox(L"No configuration values need merging.", L"Merge configuration", wxOK, this);
				return;
			}
			wxString description = m_editScope == Settings::Scope::User
				? L"Merge the effective OpenGlass configuration into the original interactive user's HKCU. HKLM will be preserved."
				: L"Merge the effective OpenGlass configuration into HKLM. Copied OpenGlass values will be removed from the original interactive user's HKCU.";
			description += L" The five Windows base color values will be preserved in both layers. Other users are not inspected. This is a preview: Save accepts it; Revert restores the previous configuration.\n\n";
			for (const auto& change : changes) description += change.description + L"\n";
			wxDialog review(this, wxID_ANY, title, wxDefaultPosition, wxSize(760, 560), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
			auto* layout = new wxBoxSizer(wxVERTICAL);
			layout->Add(new wxTextCtrl(&review, wxID_ANY, description, wxDefaultPosition, wxDefaultSize,
				wxTE_MULTILINE | wxTE_READONLY), 1, wxEXPAND | wxALL, 12);
			layout->Add(review.CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 12);
			review.SetSizer(layout);
			if (review.ShowModal() != wxID_OK) return;
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), prepareChanges() != changes);
			std::map<std::pair<Settings::Scope, Settings::Id>, std::unique_ptr<ConfigurationResources::Preparation>> prepared;
			for (const auto& change : changes)
				if (Settings::Get(change.id).assetRole != Settings::AssetRole::None)
				{
					const auto value = EffectiveConfiguration::Decode(change.after, Settings::Get(change.id));
					if (const auto source = std::get_if<std::wstring>(&value); source && !source->empty())
						prepared[{ change.scope, change.id }] = m_resources.PrepareFile(change.scope, change.id, *source);
				}
			if (RunPreview([&]
			{
				for (const auto& change : changes)
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), GetConfigForScope(change.scope)->ReadRaw(change.Name()) != change.before);
				for (const auto& [key, files] : prepared) m_resources.Install(*files);
				for (const auto& change : changes)
				{
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), GetConfigForScope(change.scope)->ReadRaw(change.Name()) != change.before);
					TrackSettingChange(change.scope, change.id);
					if (prepared.contains({ change.scope, change.id })) THROW_IF_FAILED(GetConfigForScope(change.scope)->SetString(change.Name(), m_resources.Path(change.scope, change.id).wstring()));
					else THROW_IF_FAILED(GetConfigForScope(change.scope)->WriteRaw(change.Name(), change.after));
				}
			})) LoadSettings();
		}
		catch (...)
		{
			const auto failure = wil::ResultFromCaughtException();
			const bool restored = RevertSettings();
			wxMessageBox(wxString::Format(L"Merge failed (0x%08lX). %s", failure,
				restored ? L"The previous state was restored." : L"Recovery is incomplete. Use Revert to retry."),
				L"Merge configuration", wxOK | wxICON_ERROR, this);
		}
	}

	bool MainFrame::RunPreview(const std::function<void()>& operation, Settings::UpdateType impact)
	{
		if (m_presetWorkActive || m_configurationOperationActive) return false;
		// Native modal loops and synchronous Windows calls can dispatch queued
		// commands. A rejected nested operation must not roll back this attempt.
		m_configurationOperationActive = true;
		const auto operationScope = wil::scope_exit([this] { m_configurationOperationActive = false; });
		const bool wasDirty = m_isDirty;
		const bool colorWasDirty = m_colorPreference.IsDirty();
		const auto needsRefresh = [this]
		{
			// Explicit color operations can refresh Windows-derived values without
			// changing the selected mode or RGB (for example, reselecting Automatic).
			return m_preview.HasAttemptChanges() || m_colorPreference.IsAttemptActive() || m_resources.HasAttemptChanges();
		};
		bool refreshRequired{};
		try
		{
			EnsurePreviewWriter();
			m_resources.Begin();
			m_preview.Begin();
			operation();
			m_preview.VisitRestore(true, [this](const TrackedSetting& key, const auto&)
			{
				if (Settings::Get(key.id).assetRole != Settings::AssetRole::None)
					m_resources.RemoveUnused(key.scope, key.id, GetConfigForScope(key.scope)->GetString(key.Name(), L""));
			});
			ReconcilePreview();
			// An empty reset or unchanged write needs no DWM delivery. Returning to
			// the session baseline still does, even though the preview is now clean.
			refreshRequired = needsRefresh();
			// A no-op cannot accept an earlier failed recovery.
			if (!refreshRequired && wasDirty) SetDirty(true);
			if (refreshRequired) NotifySettingsChange(impact);
			// Preserve the control being edited (for example an empty file picker or
			// a newly selected Custom mode). Whole-configuration operations reload explicitly.
			UpdateOptionStatusIcons();
			UpdatePathWarningIcons();
			UpdateColorizationPresetSelection();
			if (!m_isDirty) m_presetProvenance.Revert();
			m_resources.CommitAttempt();
			if (m_resources.TakeCleanupWarning()) wxMessageBox(L"Configuration was updated, but temporary-file cleanup is pending. It will be retried on the next operation.", L"Configuration cleanup", wxOK | wxICON_INFORMATION, this);
			m_preview.CommitAttempt();
			m_colorPreference.CommitAttempt();
			if (!m_isDirty) m_previewWriter.reset();
			return true;
		}
		catch (...)
		{
			const auto failure = wil::ResultFromCaughtException();
			try { m_preview.Reconcile([this](const auto& key) { return GetConfigForScope(key.scope)->ReadRaw(key.Name()); }); }
			catch (...) { LOG_CAUGHT_EXCEPTION(); }
			refreshRequired |= needsRefresh();
			const auto colorBefore = m_colorPreference.Baseline(true);
			bool restored = SUCCEEDED(m_colorPreference.RollbackAttempt());
			restored = m_resources.RevertAttemptFiles() && restored;
			if (m_preview.IsAttemptActive()) restored = m_preview.RollbackAttempt(
				[this, &colorBefore](const TrackedSetting& key, const RegistryConfig::RawValue& value)
				{
					if (colorBefore && colorBefore->RestoresAutomatic() && key.scope == Settings::Scope::User
						&& (key.id == Settings::Id::ColorizationColor || key.id == Settings::Id::ColorizationAfterglow)) return true;
					return SUCCEEDED(GetConfigForScope(key.scope)->WriteRaw(key.Name(), value));
				}, restored);
			try { restored = m_resources.RollbackAttempt(restored) && restored; }
			catch (...) { LOG_CAUGHT_EXCEPTION(); restored = false; }
			if (refreshRequired) NotifySettingsChange();
			if (restored && !colorWasDirty) m_colorPreference.Accept();
			SetDirty(wasDirty || !restored || m_preview.IsDirty() || m_colorPreference.IsDirty() || m_resources.IsDirty());
			if (!m_isDirty && !m_resources.HasRecovery()) m_previewWriter.reset();
			LoadSettings();
			wxMessageBox(wxString::Format(L"Preview failed (0x%08lX). %s", failure,
				restored ? L"This attempt was restored; earlier previews are preserved." : L"Recovery is incomplete. Use Revert to retry."),
				L"Configuration preview", wxOK | wxICON_ERROR, this);
			return false;
		}
	}

	void MainFrame::SaveSettings()
	{
		if (m_presetWorkActive || m_configurationOperationActive) return;
		if (!m_config)
		{
			return;
		}
		if (!RunPreview([&]
		{
			const auto sid = m_targetUserSid.ToStdWstring();
			std::optional<DWORD> rgb;
			THROW_IF_FAILED(m_colorPreference.ReadRgb(sid, rgb));
			THROW_IF_FAILED(m_colorPreference.SaveAccent(sid, rgb, [this](const auto& before)
			{
				m_resources.TrackColor(before);
			}));
		}, Settings::UpdateType::Backdrop)) return;
		m_configurationOperationActive = true;
		const auto operationScope = wil::scope_exit([this] { m_configurationOperationActive = false; });
		try { m_resources.Accept(); }
		catch (...) { wxMessageBox(L"The recovery record could not be completed. Retry Save or Revert.", L"Configuration recovery", wxOK | wxICON_ERROR, this); return; }
		if (m_resources.TakeCleanupWarning()) wxMessageBox(L"The configuration checkpoint is complete. Temporary-file cleanup will be retried on the next operation.", L"Configuration cleanup", wxOK | wxICON_INFORMATION, this);
		m_previewWriter.reset();
		m_colorPreference.Accept();
		m_preview.Accept();
		m_presetProvenance.Accept();
		SetDirty(false);
		UpdateOptionStatusIcons();
		UpdatePathWarningIcons();
	}

	bool MainFrame::RevertSettings()
	{
		if (m_presetWorkActive || m_configurationOperationActive) return false;
		m_configurationOperationActive = true;
		const auto operationScope = wil::scope_exit([this] { m_configurationOperationActive = false; });
		if (!m_config)
		{
			return false;
		}
		if (!m_isDirty && !m_colorPreference.IsDirty() && !m_resources.IsDirty()) return true;
		try
		{
			std::vector<ConfigurationResources::RegistryBefore> registry;
			m_preview.VisitRestore(false, [&](const TrackedSetting& key, const RegistryConfig::RawValue& value) { registry.push_back({ key.scope, key.id, value }); });
			m_resources.PrepareRevert(std::move(registry), m_colorPreference.Baseline());
		}
		catch (...) { wxMessageBox(L"The recovery record could not be prepared. No Revert changes were made; retry Revert.", L"Configuration recovery", wxOK | wxICON_ERROR, this); return false; }
		const auto colorBefore = m_colorPreference.Baseline();
		const auto colorResult = m_colorPreference.Revert();
		const bool filesRestored = m_resources.Revert();
		const bool restored = m_preview.Revert([this, &colorBefore](const TrackedSetting& key, const RegistryConfig::RawValue& value)
		{
			// Automatic recovery recomputes wallpaper RGB; do not replace it with an old derived color.
			if (colorBefore && colorBefore->RestoresAutomatic() && key.scope == Settings::Scope::User
				&& (key.id == Settings::Id::ColorizationColor || key.id == Settings::Id::ColorizationAfterglow)) return true;
			return SUCCEEDED(GetConfigForScope(key.scope)->WriteRaw(key.Name(), value));
		}, SUCCEEDED(colorResult) && filesRestored, false);
		if (!restored)
		{
			SetDirty(true);
			wxMessageBox(L"Recovery is incomplete. The preview remains pending; use Revert to retry.", L"Revert", wxOK | wxICON_ERROR, this);
			return false;
		}
		NotifySettingsChange();
		LoadSettings();
		try { m_resources.Accept(); }
		catch (...) { wxMessageBox(L"The recovery record could not be completed. Retry Save or Revert.", L"Configuration recovery", wxOK | wxICON_ERROR, this); return false; }
		if (m_resources.TakeCleanupWarning()) wxMessageBox(L"The configuration checkpoint is complete. Temporary-file cleanup will be retried on the next operation.", L"Configuration cleanup", wxOK | wxICON_INFORMATION, this);
		m_previewWriter.reset();
		m_colorPreference.Accept();
		m_preview.Accept();
		SetDirty(false);
		m_presetProvenance.Revert();
		return true;
	}
}
