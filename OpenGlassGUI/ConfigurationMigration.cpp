#include "pch.h"
#include "ConfigurationMigration.hpp"
#include "RegistryConfig.hpp"
#include "SettingsCatalog.hpp"
#include "ConfigurationMigrationPolicy.hpp"

namespace OpenGlass::ConfigurationMigration
{
	namespace
	{
		using RawValue = std::variant<std::monostate, DWORD, std::wstring>;

		RawValue ReadRaw(const RegistryConfig& config, const Settings::Spec& spec)
		{
			if (spec.type == Settings::ValueType::Dword)
			{
				DWORD value{};
				return config.TryGetDword(std::wstring(spec.name), value) ? RawValue{ value } : RawValue{};
			}
			std::wstring value;
			return config.TryGetString(std::wstring(spec.name), value) ? RawValue{ std::move(value) } : RawValue{};
		}

		HRESULT WriteRaw(RegistryConfig& config, const Settings::Spec& spec, const RawValue& value)
		{
			const std::wstring name(spec.name);
			if (const auto dword = std::get_if<DWORD>(&value))
			{
				return config.SetDword(name, *dword);
			}
			if (const auto string = std::get_if<std::wstring>(&value))
			{
				return config.SetString(name, *string);
			}
			return config.DeleteValue(name);
		}

		HRESULT RestoreAll(
			RegistryConfig& user,
			RegistryConfig& machine,
			const std::array<std::pair<RawValue, RawValue>, Settings::Catalog.size()>& backup
		) noexcept
		{
			HRESULT firstFailure{ S_OK };
			for (std::size_t index = 0; index < Settings::Catalog.size(); ++index)
			{
				const auto& spec = Settings::Catalog[index];
				const auto userResult = WriteRaw(user, spec, backup[index].first);
				const auto machineResult = WriteRaw(machine, spec, backup[index].second);
				if (SUCCEEDED(firstFailure) && FAILED(userResult)) firstFailure = userResult;
				if (SUCCEEDED(firstFailure) && FAILED(machineResult)) firstFailure = machineResult;
			}
			return firstFailure;
		}

		bool ConfirmMigration(std::size_t moveCount)
		{
			wxDialog dialog(nullptr, wxID_ANY, L"OpenGlass configuration migration", wxDefaultPosition, wxSize(620, 420), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
			auto* root = new wxBoxSizer(wxVERTICAL);
			const auto message = wxString::Format(
				L"OpenGlass now stores Windows colorization values for the original interactive user and all other OpenGlass settings system-wide.\n\n"
				L"%zu value(s) need to be moved or removed. Existing effective values will be preserved, but non-color settings will become shared by every user of this computer.\n\n"
				L"Migration is transactional. If any registry operation fails, both hives are restored and the editor will not open.",
				moveCount
			);
			auto* label = new wxStaticText(&dialog, wxID_ANY, message);
			label->Wrap(570);
			root->Add(label, 1, wxEXPAND | wxALL, 16);
			auto* buttons = new wxBoxSizer(wxHORIZONTAL);
			buttons->AddStretchSpacer();
			auto* exitButton = new wxButton(&dialog, wxID_CANCEL, L"Exit and migrate later");
			auto* migrateButton = new wxButton(&dialog, wxID_OK, L"Migrate and continue");
			buttons->Add(exitButton, 0, wxRIGHT, 8);
			buttons->Add(migrateButton, 0);
			root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
			dialog.SetSizer(root);
			dialog.SetEscapeId(wxID_CANCEL);
			migrateButton->SetDefault();
			return dialog.ShowModal() == wxID_OK;
		}
	}

	bool EnsureCanonicalConfiguration(const std::wstring& userSid)
	{
		RegistryConfig user(RegistryConfig::Mode::User, userSid);
		RegistryConfig machine(RegistryConfig::Mode::Machine, userSid);
		std::array<std::pair<RawValue, RawValue>, Settings::Catalog.size()> backup{};
		std::size_t moveCount{};
		for (std::size_t index = 0; index < Settings::Catalog.size(); ++index)
		{
			const auto& spec = Settings::Catalog[index];
			backup[index] = { ReadRaw(user, spec), ReadRaw(machine, spec) };
			const bool userPresent = !std::holds_alternative<std::monostate>(backup[index].first);
			const bool machinePresent = !std::holds_alternative<std::monostate>(backup[index].second);
			if ((spec.scope == Settings::Scope::User && machinePresent)
				|| (spec.scope == Settings::Scope::Machine && userPresent))
			{
				++moveCount;
			}
		}

		if (moveCount == 0)
		{
			return true;
		}

		if (!ConfirmMigration(moveCount))
		{
			return false;
		}

		HRESULT failure{ S_OK };
		for (std::size_t index = 0; index < Settings::Catalog.size(); ++index)
		{
			const auto& spec = Settings::Catalog[index];
			const auto& [userValue, machineValue] = backup[index];
			const auto canonical = ConfigurationMigrationPolicy::Canonicalize(spec.scope, ConfigurationMigrationPolicy::HiveValues<RawValue>{ userValue, machineValue });
			failure = WriteRaw(user, spec, canonical.user);
			if (SUCCEEDED(failure)) failure = WriteRaw(machine, spec, canonical.machine);
			if (FAILED(failure))
			{
				break;
			}
		}
		if (FAILED(failure))
		{
			const auto rollbackFailure = RestoreAll(user, machine, backup);
			wxMessageBox(
				FAILED(rollbackFailure)
					? wxString::Format(L"The configuration migration failed (HRESULT 0x%08lX), and restoring both registry hives was incomplete (HRESULT 0x%08lX).", static_cast<unsigned long>(failure), static_cast<unsigned long>(rollbackFailure))
					: wxString::Format(L"The configuration migration failed (HRESULT 0x%08lX). Both registry hives were restored.", static_cast<unsigned long>(failure)),
				L"OpenGlass configuration migration",
				wxOK | wxICON_ERROR
			);
			return false;
		}
		return true;
	}
}
