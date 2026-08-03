#include "pch.h"
#include "MainFrame.hpp"

namespace OpenGlass
{
	namespace
	{
		constexpr int ViewLicenseButtonId = wxID_HIGHEST + 401;
		constexpr int OpenAuthorHomepageButtonId = wxID_HIGHEST + 402;
		constexpr int ImportOnlyButtonId = wxID_HIGHEST + 403;
		constexpr int ImportAndApplyButtonId = wxID_HIGHEST + 404;
		constexpr std::size_t MaximumBatchPackageCount = 32;
		// WM_COPYGLOBALDATA is intentionally undocumented but is part of the
		// WM_DROPFILES transfer used by Explorer across an integrity boundary.
		constexpr UINT WmCopyGlobalData = 0x0049;

		enum class PresetPreviewAction
		{
			Cancel,
			Apply,
			ImportOnly
		};

		void EnableElevatedFileDrop(HWND window)
		{
			// The GUI runs elevated, while Explorer normally does not. Keep this
			// exception window-local and limited to the three shell-drop messages.
			LOG_IF_WIN32_BOOL_FALSE(ChangeWindowMessageFilterEx(window, WM_DROPFILES, MSGFLT_ALLOW, nullptr));
			LOG_IF_WIN32_BOOL_FALSE(ChangeWindowMessageFilterEx(window, WM_COPYDATA, MSGFLT_ALLOW, nullptr));
			LOG_IF_WIN32_BOOL_FALSE(ChangeWindowMessageFilterEx(window, WmCopyGlobalData, MSGFLT_ALLOW, nullptr));
		}

		void OpenConfirmedUrl(wxWindow* parent, const std::wstring& url)
		{
			if (url.empty()) return;
			if (wxMessageBox(
				L"Open this unverified external link in your default browser?\n\n" + url,
				L"Open external link",
				wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
				parent
			) == wxYES)
			{
				wxLaunchDefaultBrowser(url);
			}
		}

		void ShowPackageLicense(wxWindow* parent, const PresetPackages::Package& package)
		{
			if (package.licenseText.empty()) return;
			wxDialog dialog(parent, wxID_ANY, package.metadata.licenseName, wxDefaultPosition, wxSize(700, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
			auto* sizer = new wxBoxSizer(wxVERTICAL);
			sizer->Add(new wxTextCtrl(&dialog, wxID_ANY, wxString::FromUTF8(package.licenseText), wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2), 1, wxEXPAND | wxALL, 8);
			sizer->Add(dialog.CreateButtonSizer(wxOK), 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 8);
			dialog.SetSizer(sizer);
			dialog.ShowModal();
		}

		void ShowPackageInformationMenu(wxWindow* parent, wxWindow* anchor, const PresetPackages::Package& package)
		{
			wxMenu menu;
			menu.Append(ViewLicenseButtonId, L"View LICENSE");
			menu.Enable(ViewLicenseButtonId, !package.licenseText.empty());
			menu.Append(OpenAuthorHomepageButtonId, L"Open author homepage...");
			switch (anchor->GetPopupMenuSelectionFromUser(menu, 0, anchor->GetClientSize().GetHeight()))
			{
			case ViewLicenseButtonId:
				ShowPackageLicense(parent, package);
				break;
			case OpenAuthorHomepageButtonId:
				OpenConfirmedUrl(parent, package.metadata.authorHomepage);
				break;
			}
		}

		std::wstring CurrentValueText(const Settings::Spec& spec, const RegistryConfig& config)
		{
			if (spec.type == Settings::ValueType::Dword)
			{
				DWORD value{};
				return config.TryGetDword(std::wstring(spec.name), value)
					? std::format(L"0x{:08X}", value)
					: L"<absent>";
			}
			std::wstring value;
			return config.TryGetString(std::wstring(spec.name), value) ? value : L"<absent>";
		}

		std::wstring PackageValueText(const PresetPackages::SettingValue& value)
		{
			if (std::holds_alternative<std::monostate>(value)) return L"<delete>";
			if (const auto dword = std::get_if<DWORD>(&value)) return std::format(L"0x{:08X}", *dword);
			const auto& asset = std::get<PresetPackages::AssetReference>(value);
			return L"<package>/" + wxString::FromUTF8(asset.path).ToStdWstring();
		}

		bool PackageValueWouldChange(const Settings::Spec& spec, const PresetPackages::SettingValue& value, const RegistryConfig& config)
		{
			const std::wstring name(spec.name);
			if (std::holds_alternative<std::monostate>(value)) return config.HasValue(name);
			if (const auto dword = std::get_if<DWORD>(&value))
			{
				DWORD current{};
				return !config.TryGetDword(name, current) || current != *dword;
			}
			// The final canonical path is known only after immutable deployment.
			return true;
		}

		void AppendIgnoredSettings(std::wstring& output, const PresetPackages::Package& package)
		{
			if (package.ignoredSettingCount == 0) return;
			output += std::format(
				L"\r\nIgnored settings: {} (not recognized by this OpenGlass build and not applied)\r\n",
				package.ignoredSettingCount
			);
			for (const auto& name : package.ignoredSettingNames)
			{
				output += L"  - ";
				output += name;
				output += L"\r\n";
			}
			if (package.ignoredSettingCount > package.ignoredSettingNames.size())
			{
				output += std::format(
					L"  ... and {} more\r\n",
					package.ignoredSettingCount - package.ignoredSettingNames.size()
				);
			}
		}

		std::wstring SensitiveChangeSummary(const PresetPackages::Package& package, const RegistryConfig& config)
		{
			std::wstring result;
			for (const auto& [id, value] : package.settings)
			{
				const auto& spec = Settings::Get(id);
				if (spec.sensitive && PackageValueWouldChange(spec, value, config))
				{
					result += L"\n  - ";
					result += spec.name;
				}
			}
			return result;
		}

		PresetPreviewAction ShowPresetPreview(
			wxWindow* parent,
			const PresetPackages::Package& package,
			const RegistryConfig& config,
			bool importing
		)
		{
			wxDialog dialog(parent, wxID_ANY, L"Review preset pack", wxDefaultPosition, wxSize(780, 680), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
			auto* root = new wxBoxSizer(wxVERTICAL);
			const auto licenseName = package.licenseText.empty()
				? wxString(L"Not provided (all rights reserved by default)")
				: wxString(package.metadata.licenseName);
			root->Add(new wxStaticText(&dialog, wxID_ANY, wxString::Format(
				L"%s\nUUID: %s\nCatalog version: %u\nAuthor: %s (unverified)\nLicense: %s",
				package.metadata.name.c_str(),
				wxString::FromUTF8(package.metadata.uuid),
				package.catalogVersion,
				package.metadata.authorName.c_str(),
				licenseName
			)), 0, wxEXPAND | wxALL, 10);

			std::wstring details =
				L"Application scope: system-wide OpenGlass configuration; Windows colorization values remain specific to the current user.\r\n\r\n"
				L"Configuration changes if applied (complete Replace):\r\n";
			bool hasSensitive{};
			bool restartRequired{};
			for (const auto& [id, value] : package.settings)
			{
				const auto& spec = Settings::Get(id);
				const bool changes = PackageValueWouldChange(spec, value, config);
				details += std::format(
					L"{} {}: {} -> {}{}{}\r\n",
					std::holds_alternative<std::monostate>(value) ? L"DELETE" : L"SET",
					spec.name,
					CurrentValueText(spec, config),
					PackageValueText(value),
					spec.sensitive && changes ? L" [SENSITIVE]" : L"",
					spec.impact == Settings::UpdateImpact::RestartRequired && changes ? L" [RESTART REQUIRED]" : L""
				);
				hasSensitive |= spec.sensitive && changes;
				restartRequired |= spec.impact == Settings::UpdateImpact::RestartRequired && changes;
			}
			AppendIgnoredSettings(details, package);
			details += L"\r\nAssets:\r\n";
			for (const auto& [name, size] : package.assetSummary)
			{
				details += std::format(L"  {} ({} bytes)\r\n", wxString::FromUTF8(name).ToStdWstring(), size);
			}
			if (hasSensitive) details += L"\r\nSensitive settings and graphics decoded by dwm.exe require special review.\r\n";
			if (restartRequired) details += L"Restart-required settings will not trigger an automatic DWM or service restart.\r\n";
			details += package.licenseText.empty()
				? L"The package author is not verified. No license was provided; the entire package grants no additional permission to modify or redistribute its contents."
				: L"The package author is not verified. Unless the LICENSE says otherwise, it applies package-wide to all copyrightable contents the author is authorized to license. Third-party asset terms must be identified in that LICENSE.";

			auto* text = new wxTextCtrl(&dialog, wxID_ANY, details, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
			root->Add(text, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
			auto* packageInformation = new wxButton(&dialog, wxID_ANY, L"Package information...");
			root->Add(packageInformation, 0, wxLEFT | wxRIGHT | wxTOP, 10);
			if (importing)
			{
				auto* buttons = new wxBoxSizer(wxHORIZONTAL);
				auto* importAndApply = new wxButton(&dialog, ImportAndApplyButtonId, L"Import and apply");
				auto* importOnly = new wxButton(&dialog, ImportOnlyButtonId, L"Import only");
				auto* cancel = new wxButton(&dialog, wxID_CANCEL);
				importAndApply->SetDefault();
				buttons->AddStretchSpacer();
				buttons->Add(importAndApply, 0, wxRIGHT, 6);
				buttons->Add(importOnly, 0, wxRIGHT, 6);
				buttons->Add(cancel, 0);
				root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
				importAndApply->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(ImportAndApplyButtonId); });
				importOnly->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent&) { dialog.EndModal(ImportOnlyButtonId); });
			}
			else
			{
				auto* buttons = dialog.CreateSeparatedButtonSizer(wxOK | wxCANCEL);
				if (auto* apply = dialog.FindWindow(wxID_OK))
				{
					apply->SetLabel(L"Apply");
				}
				root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
			}
			dialog.SetSizer(root);
			packageInformation->Bind(wxEVT_BUTTON, [&dialog, &package, packageInformation](wxCommandEvent&)
			{
				ShowPackageInformationMenu(&dialog, packageInformation, package);
			});
			switch (dialog.ShowModal())
			{
			case wxID_OK:
			case ImportAndApplyButtonId:
				return PresetPreviewAction::Apply;
			case ImportOnlyButtonId:
				return PresetPreviewAction::ImportOnly;
			default:
				return PresetPreviewAction::Cancel;
			}
		}

		bool ShowBatchImportPreview(wxWindow* parent, std::span<const PresetPackages::Package> packages)
		{
			wxDialog dialog(parent, wxID_ANY, L"Review preset pack import", wxDefaultPosition, wxSize(720, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
			auto* root = new wxBoxSizer(wxVERTICAL);
			root->Add(new wxStaticText(
				&dialog,
				wxID_ANY,
				wxString::Format(
					L"%zu preset packs passed initial validation. \r\nImporting them adds immutable packs to the local library and does not change the current configuration.",
					packages.size()
				)
			), 0, wxEXPAND | wxALL, 10);
			std::wstring details;
			for (const auto& package : packages)
			{
				const auto licenseName = package.licenseText.empty()
					? std::wstring(L"Not provided (all rights reserved by default)")
					: package.metadata.licenseName;
				details += std::format(
					L"{}\r\n  File: {}\r\n  UUID: {}\r\n  Author: {} (unverified)\r\n  License: {}\r\n  Assets: {}\r\n",
					package.metadata.name,
					package.source.filename().wstring(),
					wxString::FromUTF8(package.metadata.uuid).ToStdWstring(),
					package.metadata.authorName,
					licenseName,
					package.assetSummary.size()
				);
				AppendIgnoredSettings(details, package);
				details += L"\r\n";
			}
			root->Add(new wxTextCtrl(&dialog, wxID_ANY, details, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2), 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
			auto* buttons = dialog.CreateSeparatedButtonSizer(wxOK | wxCANCEL);
			if (auto* importAll = dialog.FindWindow(wxID_OK)) importAll->SetLabel(L"Import all");
			root->Add(buttons, 0, wxEXPAND | wxALL, 10);
			dialog.SetSizer(root);
			return dialog.ShowModal() == wxID_OK;
		}

		bool PathIsWithin(const std::filesystem::path& path, const std::filesystem::path& parent)
		{
			const auto child = std::filesystem::weakly_canonical(path).native();
			auto root = std::filesystem::weakly_canonical(parent).native();
			if (!root.ends_with(L'\\')) root += L'\\';
			return child.size() > root.size() && _wcsnicmp(child.c_str(), root.c_str(), root.size()) == 0;
		}

		std::wstring SanitizePackageFileName(std::wstring_view name)
		{
			std::wstring result;
			for (const auto character : name)
			{
				if (iswalnum(character) || character == L'-' || character == L'_') result += character;
				else if (iswspace(character) && !result.empty() && result.back() != L'-') result += L'-';
				if (result.size() == 64) break;
			}
			while (!result.empty() && result.back() == L'-') result.pop_back();
			return result.empty() ? L"openglass-preset" : result;
		}

		std::optional<DWORD> GetPackagePreviewColor(const PresetPackages::Package& package)
		{
			for (const auto id : { Settings::Id::ColorizationColorOverride, Settings::Id::ColorizationColor })
			{
				const auto value = package.settings.find(id);
				if (value == package.settings.end()) continue;
				if (const auto color = std::get_if<DWORD>(&value->second)) return *color;
			}
			return std::nullopt;
		}

		struct PackagePreviewMetrics
		{
			int canvasDip;
			int insetDip;
		};

		PackagePreviewMetrics GetPackagePreviewMetrics(const wxWindow* window)
		{
			// At 100%, compact rows matter more than absolute swatch size. At higher
			// DPI, slightly increase the swatch-to-row ratio so it does not recede.
			return window->GetDPIScaleFactor() >= 1.25
				? PackagePreviewMetrics{ 28, 3 }
				: PackagePreviewMetrics{ 24, 4 };
		}

		wxBitmap CreatePackagePreviewBitmap(
			wxWindow* window,
			const PresetPackages::Package& package,
			PackagePreviewMetrics metrics
		)
		{
			const auto argb = GetPackagePreviewColor(package);
			const wxColour color = argb
				? wxColour((*argb >> 16) & 0xFF, (*argb >> 8) & 0xFF, *argb & 0xFF)
				: wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
			const auto light = color.ChangeLightness(150);
			const auto dark = color.ChangeLightness(85);
			const auto border = wxSystemSettings::GetAppearance().IsDark()
				? wxColour(120, 120, 120)
				: wxColour(128, 128, 128);

			const auto canvasSize = window->FromDIP(wxSize(metrics.canvasDip, metrics.canvasDip));
			wxBitmap bitmap(canvasSize.GetWidth(), canvasSize.GetHeight(), 32);
			bitmap.UseAlpha();
			const auto size = bitmap.GetSize();
			wxMemoryDC memoryDc(bitmap);
			memoryDc.SetBackground(wxBrush(wxColour(0, 0, 0, 0)));
			memoryDc.Clear();
			{
				std::unique_ptr<wxGraphicsContext> context{ wxGraphicsContext::Create(memoryDc) };
				THROW_HR_IF(E_OUTOFMEMORY, !context);
				const auto borderWidth = std::max(1, window->FromDIP(1));
				const auto inset = window->FromDIP(metrics.insetDip);
				const auto swatchWidth = size.GetWidth() - 2 * inset;
				const auto swatchHeight = size.GetHeight() - 2 * inset;
				context->SetPen(*wxTRANSPARENT_PEN);
				context->SetBrush(wxBrush(border));
				context->DrawRectangle(inset, inset, swatchWidth, swatchHeight);
				context->SetBrush(context->CreateLinearGradientBrush(
					inset,
					inset,
					inset + swatchWidth,
					inset + swatchHeight,
					light,
					dark
				));
				context->DrawRectangle(
					inset + borderWidth,
					inset + borderWidth,
					swatchWidth - 2 * borderWidth,
					swatchHeight - 2 * borderWidth
				);
			}
			memoryDc.SelectObject(wxNullBitmap);
			return bitmap;
		}

		class CreatePresetDialog final : public wxDialog
		{
		public:
			CreatePresetDialog(
				wxWindow* parent,
				const wxString& defaultAuthor,
				const wxString& defaultHomepage,
				bool includeLicense,
				const wxString& licenseText,
				bool installAfterCreate
			)
				: wxDialog(parent, wxID_ANY, L"Create preset ZIP", wxDefaultPosition, wxSize(650, 700), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
			{
				auto* root = new wxBoxSizer(wxVERTICAL);
				auto* scopeNote = new wxStaticText(
					this,
					wxID_ANY,
					L"Captures the current preview, including unsaved changes; Save is not required. Preset packs apply system-wide except for Windows colorization."
				);
				scopeNote->Wrap(610);
				root->Add(scopeNote, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
				auto* grid = new wxFlexGridSizer(2, 8, 8);
				grid->AddGrowableCol(1, 1);
				auto add = [&](const wxString& label, wxWindow* control)
				{
					grid->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
					grid->Add(control, 1, wxEXPAND);
				};
				m_name = new wxTextCtrl(this, wxID_ANY);
				m_description = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, 70), wxTE_MULTILINE);
				m_author = new wxTextCtrl(this, wxID_ANY, defaultAuthor);
				m_authorHomepage = new wxTextCtrl(this, wxID_ANY, defaultHomepage);
				m_authorHomepage->SetHint(L"https://example.com");
				add(L"Name", m_name);
				add(L"Description (optional)", m_description);
				add(L"Author", m_author);
				add(L"Author homepage", m_authorHomepage);
				root->Add(grid, 0, wxEXPAND | wxALL, 10);

				auto* licenseHeader = new wxBoxSizer(wxHORIZONTAL);
				m_includeLicense = new wxCheckBox(this, wxID_ANY, L"Include LICENSE");
				m_includeLicense->SetValue(includeLicense);
				licenseHeader->Add(m_includeLicense, 0, wxALIGN_CENTER_VERTICAL);
				licenseHeader->AddStretchSpacer();
				auto* loadLicense = new wxButton(this, wxID_ANY, L"Load from file...");
				licenseHeader->Add(loadLicense, 0);
				root->Add(licenseHeader, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
				m_detectedLicense = new wxStaticText(this, wxID_ANY, L"Detected license: Custom license");
				root->Add(m_detectedLicense, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
				auto* licenseScope = new wxStaticText(
					this,
					wxID_ANY,
					L"The LICENSE applies to the whole preset pack unless its text says otherwise. Only include content you may license, and identify any third-party asset terms in the LICENSE."
				);
				licenseScope->Wrap(610);
				root->Add(licenseScope, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
				m_licenseText = new wxTextCtrl(this, wxID_ANY, licenseText, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_RICH2);
				root->Add(m_licenseText, 1, wxEXPAND | wxALL, 10);
				m_installAfterCreate = new wxCheckBox(this, wxID_ANY, L"Install in Preset packs after creating");
				m_installAfterCreate->SetValue(installAfterCreate);
				m_installAfterCreate->SetToolTip(L"Deploy the newly created immutable ZIP to the local OpenGlass preset library. It will not be applied to the current configuration.");
				root->Add(m_installAfterCreate, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
				root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
				SetSizer(root);
				loadLicense->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
				{
					wxFileDialog dialog(this, L"Select LICENSE text", wxEmptyString, wxEmptyString, L"Text files (*.txt;LICENSE)|*.txt;LICENSE|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
					if (dialog.ShowModal() != wxID_OK) return;
					wxFFileInputStream input(dialog.GetPath());
					if (!input.IsOk()) return;
					wxStringOutputStream output;
					input.Read(output);
					m_licenseText->SetValue(output.GetString());
				});
				auto updateLicenseState = [this, loadLicense]()
				{
					const bool included = m_includeLicense->GetValue();
					loadLicense->Enable(included);
					m_licenseText->Enable(included);
					const auto name = included
						? PresetPackages::InferLicenseName(m_licenseText->GetValue().ToStdString(wxConvUTF8))
						: std::wstring{};
					m_detectedLicense->SetLabel(included
						? wxString(L"Detected license: ") + wxString(name.empty() ? L"(enter or load LICENSE text)" : name)
						: wxString(L"No LICENSE: all rights reserved by default"));
				};
				m_includeLicense->Bind(wxEVT_CHECKBOX, [updateLicenseState](wxCommandEvent&) { updateLicenseState(); });
				m_licenseText->Bind(wxEVT_TEXT, [updateLicenseState](wxCommandEvent&) { updateLicenseState(); });
				updateLicenseState();
				Bind(wxEVT_BUTTON, [this](wxCommandEvent& event)
				{
					if (event.GetId() != wxID_OK)
					{
						event.Skip();
						return;
					}
					if (m_name->GetValue().Trim().empty()
						|| m_author->GetValue().Trim().empty()
						|| m_authorHomepage->GetValue().Trim().empty())
					{
						wxMessageBox(L"Name, author and author homepage are required.", L"Create preset", wxOK | wxICON_ERROR, this);
						return;
					}
					if (m_name->GetValue().length() > 128
						|| m_description->GetValue().length() > 4096
						|| m_author->GetValue().length() > 256)
					{
						wxMessageBox(L"Name may contain at most 128 characters, description 4096, and author 256.", L"Create preset", wxOK | wxICON_ERROR, this);
						return;
					}
					if (!PresetPackages::IsValidHomepageUrl(m_authorHomepage->GetValue().ToStdWstring()))
					{
						wxMessageBox(L"Author homepage must be a complete absolute http:// or https:// URL, for example https://example.com.", L"Create preset", wxOK | wxICON_ERROR, this);
						m_authorHomepage->SetFocus();
						m_authorHomepage->SelectAll();
						return;
					}
					if (m_includeLicense->GetValue() && m_licenseText->GetValue().Trim().empty())
					{
						wxMessageBox(L"Enter or load LICENSE text, or clear Include LICENSE.", L"Create preset", wxOK | wxICON_ERROR, this);
						return;
					}
					EndModal(wxID_OK);
				});
			}

			PresetPackages::Metadata Metadata() const
			{
				return {
					{},
					m_name->GetValue().ToStdWstring(),
					m_description->GetValue().ToStdWstring(),
					m_author->GetValue().ToStdWstring(),
					m_authorHomepage->GetValue().ToStdWstring(),
					{}
				};
			}

			std::string LicenseText() const
			{
				return m_includeLicense->GetValue()
					? m_licenseText->GetValue().ToStdString(wxConvUTF8)
					: std::string{};
			}

			std::string LicenseEditorText() const
			{
				return m_licenseText->GetValue().ToStdString(wxConvUTF8);
			}

			bool IncludeLicense() const
			{
				return m_includeLicense->GetValue();
			}

			bool InstallAfterCreate() const
			{
				return m_installAfterCreate->GetValue();
			}

		private:
			wxTextCtrl* m_name{};
			wxTextCtrl* m_description{};
			wxTextCtrl* m_author{};
			wxTextCtrl* m_authorHomepage{};
			wxCheckBox* m_includeLicense{};
			wxStaticText* m_detectedLicense{};
			wxTextCtrl* m_licenseText{};
			wxCheckBox* m_installAfterCreate{};
		};

		std::string AssetName(Settings::AssetRole role, const std::filesystem::path& source)
		{
			std::wstring extension = source.extension().wstring();
			std::ranges::transform(extension, extension.begin(), [](wchar_t value) { return static_cast<wchar_t>(::towlower(value)); });
			THROW_HR_IF(E_INVALIDARG, extension != L".png");
			std::wstring stem;
			switch (role)
			{
			case Settings::AssetRole::ThemeAtlas: stem = L"theme-atlas"; break;
			case Settings::AssetRole::Reflection: stem = L"reflection"; break;
			case Settings::AssetRole::Material: stem = L"material"; break;
			default: THROW_HR(E_INVALIDARG);
			}
			return wxString(L"assets/" + stem + extension).ToStdString(wxConvUTF8);
		}
	}

	void MainFrame::CreatePresetsTab()
	{
		auto* panel = new wxPanel(m_notebook);
		auto* root = new wxBoxSizer(wxVERTICAL);
		auto* scopeNote = new wxStaticText(
			panel,
			wxID_ANY,
			L"Preset packs: colorization is per-user; all other settings are system-wide."
		);
		scopeNote->SetToolTip(
			L"The OpenGlass GUI and preset packs target a single-user PC. Only Windows colorization remains independent for each user."
		);
		root->Add(scopeNote, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
		auto* content = new wxBoxSizer(wxHORIZONTAL);
		m_lstPresetPackages = new wxListView(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER);
		m_lstPresetPackages->SetMinSize(wxSize(0, 0));
		m_lstPresetPackages->InsertColumn(0, wxEmptyString, wxLIST_FORMAT_LEFT, 1);
		content->Add(m_lstPresetPackages, 1, wxEXPAND | wxRIGHT, 8);
		m_txtPresetDetails = new wxTextCtrl(panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
		content->Add(m_txtPresetDetails, 3, wxEXPAND);
		root->Add(content, 1, wxEXPAND | wxALL, 8);

		auto* buttons = new wxBoxSizer(wxVERTICAL);
		auto* libraryActions = new wxBoxSizer(wxHORIZONTAL);
		auto* authoringActions = new wxBoxSizer(wxHORIZONTAL);
		m_btnImportPreset = new wxButton(panel, wxID_ANY, L"Import...");
		m_btnApplyPreset = new wxButton(panel, wxID_ANY, L"Apply");
		m_btnCreatePreset = new wxButton(panel, wxID_ANY, L"Create from current preview...");
		m_btnCreatePreset->SetToolTip(L"Create a preset pack from the settings currently applied for preview, including unsaved changes. Saving first is not required.");
		m_btnResetPresetSettings = new wxButton(panel, wxID_ANY, L"Reset all settings...");
		m_btnRemovePreset = new wxButton(panel, wxID_ANY, L"Remove");
		m_btnPresetInformation = new wxButton(panel, wxID_ANY, L"Package information...");
		libraryActions->Add(m_btnImportPreset, 0, wxRIGHT, 6);
		libraryActions->Add(m_btnApplyPreset, 0, wxRIGHT, 6);
		libraryActions->Add(m_btnPresetInformation, 0, wxRIGHT, 6);
		libraryActions->AddStretchSpacer();
		libraryActions->Add(m_btnRemovePreset, 0);
		authoringActions->Add(m_btnCreatePreset, 0, wxRIGHT, 6);
		authoringActions->Add(m_btnResetPresetSettings, 0, wxRIGHT, 6);
		buttons->Add(libraryActions, 0, wxEXPAND | wxBOTTOM, 6);
		buttons->Add(authoringActions, 0, wxEXPAND);
		root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
		panel->SetSizer(root);
		panel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
		m_notebook->AddPage(panel, L"Preset packs");

		m_lstPresetPackages->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent&) { UpdatePresetPackageDetails(); });
		m_lstPresetPackages->Bind(wxEVT_SIZE, [this](wxSizeEvent& event)
		{
			event.Skip();
			CallAfter([this]() { ResizePresetPackageColumn(); });
		});
		m_btnImportPreset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ImportPresetPackage(); });
		m_btnApplyPreset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ApplySelectedPresetPackage(); });
		m_btnCreatePreset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { CreatePresetPackage(); });
		m_btnResetPresetSettings->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ResetPresetPackSettings(); });
		m_btnRemovePreset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RemoveSelectedPresetPackage(); });
		m_btnPresetInformation->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
		{
			const long selection = m_lstPresetPackages->GetFirstSelected();
			if (selection == wxNOT_FOUND || static_cast<std::size_t>(selection) >= m_presetPackages.size()) return;
			ShowPackageInformationMenu(this, m_btnPresetInformation, m_presetPackages[selection]);
		});
		DragAcceptFiles(true);
		EnableElevatedFileDrop(reinterpret_cast<HWND>(GetHandle()));
		Bind(wxEVT_DROP_FILES, [this](wxDropFilesEvent& event) { ImportDroppedPresetPackages(event); });
		panel->Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event)
		{
			const auto selection = m_lstPresetPackages->GetFirstSelected();
			const auto selectedUuid = selection != wxNOT_FOUND && static_cast<std::size_t>(selection) < m_presetPackages.size()
				? m_presetPackages[selection].metadata.uuid
				: std::string{};
			CallAfter([this, selectedUuid]() { RebuildPresetPackageList(selectedUuid); });
			event.Skip();
		});
		panel->Bind(wxEVT_SYS_COLOUR_CHANGED, [this](wxSysColourChangedEvent& event)
		{
			const auto selection = m_lstPresetPackages->GetFirstSelected();
			const auto selectedUuid = selection != wxNOT_FOUND && static_cast<std::size_t>(selection) < m_presetPackages.size()
				? m_presetPackages[selection].metadata.uuid
				: std::string{};
			CallAfter([this, selectedUuid]() { RebuildPresetPackageList(selectedUuid); });
			event.Skip();
		});
		RefreshPresetPackages();
	}

	void MainFrame::RefreshPresetPackages()
	{
		if (!m_lstPresetPackages) return;
		try
		{
			m_presetPackages = PresetPackages::EnumerateInstalled();
		}
		catch (...)
		{
			const auto error = wil::ResultFromCaughtException();
			m_presetPackages.clear();
			wxMessageBox(wxString::Format(L"Installed preset packages could not be enumerated (HRESULT 0x%08lX).", static_cast<unsigned long>(error)), L"OpenGlass presets", wxOK | wxICON_ERROR, this);
		}
		RebuildPresetPackageList();
	}

	void MainFrame::RebuildPresetPackageList(std::string_view selectedUuid)
	{
		if (!m_lstPresetPackages) return;
		m_lstPresetPackages->DeleteAllItems();
		const auto metrics = GetPackagePreviewMetrics(m_lstPresetPackages);
		const auto imageSize = m_lstPresetPackages->FromDIP(wxSize(metrics.canvasDip, metrics.canvasDip));
		auto images = std::make_unique<wxImageList>(
			imageSize.GetWidth(),
			imageSize.GetHeight(),
			true,
			static_cast<int>(std::max<std::size_t>(1, m_presetPackages.size()))
		);
		for (const auto& package : m_presetPackages)
		{
			THROW_HR_IF(E_OUTOFMEMORY, images->Add(CreatePackagePreviewBitmap(m_lstPresetPackages, package, metrics)) == wxNOT_FOUND);
		}
		m_lstPresetPackages->AssignImageList(images.release(), wxIMAGE_LIST_SMALL);
		for (std::size_t index = 0; index < m_presetPackages.size(); ++index)
		{
			m_lstPresetPackages->InsertItem(static_cast<long>(index), m_presetPackages[index].metadata.name, static_cast<int>(index));
		}
		ResizePresetPackageColumn();
		const bool hasPackages = !m_presetPackages.empty();
		m_btnApplyPreset->Enable(hasPackages);
		m_btnRemovePreset->Enable(hasPackages);
		m_btnPresetInformation->Enable(hasPackages);
		if (hasPackages)
		{
			const auto selected = std::ranges::find(m_presetPackages, selectedUuid, [](const auto& package)
			{
				return std::string_view(package.metadata.uuid);
			});
			SelectPresetPackageRow(selected == m_presetPackages.end()
				? 0
				: static_cast<std::size_t>(std::distance(m_presetPackages.begin(), selected)));
		}
		UpdatePresetPackageDetails();
	}

	void MainFrame::ResizePresetPackageColumn()
	{
		if (!m_lstPresetPackages || m_lstPresetPackages->GetColumnCount() == 0) return;
		const int width = std::max(1, m_lstPresetPackages->GetClientSize().GetWidth() - FromDIP(2));
		m_lstPresetPackages->SetColumnWidth(0, width);
	}

	void MainFrame::SelectPresetPackageRow(std::size_t row)
	{
		if (!m_lstPresetPackages || row >= m_presetPackages.size()) return;
		m_lstPresetPackages->Select(static_cast<long>(row));
		m_lstPresetPackages->Focus(static_cast<long>(row));
	}

	void MainFrame::UpdatePresetPackageDetails()
	{
		if (!m_txtPresetDetails) return;
		const long selection = m_lstPresetPackages ? m_lstPresetPackages->GetFirstSelected() : wxNOT_FOUND;
		if (selection == wxNOT_FOUND || static_cast<std::size_t>(selection) >= m_presetPackages.size())
		{
			m_txtPresetDetails->Clear();
			if (m_btnPresetInformation) m_btnPresetInformation->Enable(false);
			return;
		}
		const auto& package = m_presetPackages[selection];
		m_btnPresetInformation->Enable(true);
		m_btnPresetInformation->SetToolTip(L"View the package LICENSE or open the unverified author homepage.");
		std::size_t configured{}, sensitive{};
		for (const auto& [id, value] : package.settings)
		{
			if (!std::holds_alternative<std::monostate>(value)) ++configured;
			if (Settings::Get(id).sensitive && !std::holds_alternative<std::monostate>(value)) ++sensitive;
		}
		const auto licenseName = package.licenseText.empty()
			? wxString(L"Not provided (all rights reserved by default)")
			: wxString(package.metadata.licenseName);
		std::wstring heading = package.metadata.name;
		if (!package.metadata.description.empty()) heading += L"\r\n\r\n" + package.metadata.description;
		std::wstring details = wxString::Format(
			L"%s\n\nUUID: %s\nCatalog version: %u\nAuthor: %s\nHomepage: %s\nLicense: %s\n\nApplication scope: system-wide, except Windows colorization values for the current user\nConfigured settings: %zu\nAssets: %zu\nSensitive settings: %zu",
			heading,
			wxString::FromUTF8(package.metadata.uuid),
			package.catalogVersion,
			package.metadata.authorName,
			package.metadata.authorHomepage,
			licenseName,
			configured,
			package.assetSummary.size(),
			sensitive
		).ToStdWstring();
		if (package.ignoredSettingCount == 0) details += L"\r\n";
		AppendIgnoredSettings(details, package);
		details += L"\r\n";
		details += package.licenseText.empty()
			? L"No permission to modify or redistribute the package contents is granted."
			: L"Author identity is not verified. Unless stated otherwise in the LICENSE, it applies package-wide to all copyrightable contents the author is authorized to license; third-party asset terms must be listed there.";
		m_txtPresetDetails->SetValue(details);
	}

	void MainFrame::ImportPresetPackage()
	{
		wxFileDialog dialog(this, L"Import OpenGlass preset ZIP", wxEmptyString, wxEmptyString, L"ZIP archives (*.zip)|*.zip", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
		if (dialog.ShowModal() != wxID_OK) return;
		ImportPresetPackage(dialog.GetPath().ToStdWstring());
	}

	void MainFrame::ImportDroppedPresetPackages(const wxDropFilesEvent& event)
	{
		if (event.GetNumberOfFiles() == 0) return;
		if (static_cast<std::size_t>(event.GetNumberOfFiles()) > MaximumBatchPackageCount)
		{
			wxMessageBox(wxString::Format(L"At most %zu preset ZIPs can be imported at once.", MaximumBatchPackageCount), L"Preset import", wxOK | wxICON_INFORMATION, this);
			return;
		}
		std::vector<std::filesystem::path> paths;
		paths.reserve(event.GetNumberOfFiles());
		for (int index = 0; index < event.GetNumberOfFiles(); ++index)
		{
			std::filesystem::path path(event.GetFiles()[index].ToStdWstring());
			auto extension = path.extension().wstring();
			std::ranges::transform(extension, extension.begin(), [](wchar_t value) { return static_cast<wchar_t>(::towlower(value)); });
			if (extension != L".zip")
			{
				wxMessageBox(L"Every dropped file must be a standard .zip preset package. Nothing was imported.", L"Preset import", wxOK | wxICON_INFORMATION, this);
				return;
			}
			paths.push_back(std::move(path));
		}
		if (m_notebook && m_lstPresetPackages)
		{
			const auto page = m_notebook->FindPage(m_lstPresetPackages->GetParent());
			if (page != wxNOT_FOUND) m_notebook->SetSelection(page);
		}
		if (paths.size() == 1) ImportPresetPackage(paths.front());
		else ImportPresetPackages(paths);
	}

	void MainFrame::ImportPresetPackages(std::span<const std::filesystem::path> paths)
	{
		std::vector<PresetPackages::Package> packages;
		std::vector<PresetPackages::Package> createdPackages;
		std::map<std::string, std::string> seenUuids;
		std::filesystem::path currentPath;
		bool rollbackIncomplete{};
		try
		{
			packages.reserve(paths.size());
			for (const auto& path : paths)
			{
				currentPath = path;
				auto package = PresetPackages::LoadArchive(path);
				if (const auto [existing, inserted] = seenUuids.emplace(package.metadata.uuid, package.digest); !inserted)
				{
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_DUP_NAME), existing->second != package.digest);
					continue;
				}
				package.assets.clear();
				packages.push_back(std::move(package));
			}
			if (!ShowBatchImportPreview(this, packages)) return;

			std::size_t createdCount{}, reusedCount{};
			try
			{
				for (const auto& package : packages)
				{
					currentPath = package.source;
					auto verifiedPackage = PresetPackages::LoadArchive(package.source);
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), verifiedPackage.digest != package.digest);
					const auto deployment = PresetPackages::Deploy(verifiedPackage);
					if (deployment.created)
					{
						PresetPackages::Package createdPackage;
						createdPackage.source = deployment.path;
						createdPackage.deployed = true;
						createdPackages.push_back(std::move(createdPackage));
						++createdCount;
					}
					else
					{
						++reusedCount;
					}
					auto deployedPackage = PresetPackages::LoadDeployed(deployment.path);
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), deployedPackage.digest != package.digest);
				}
			}
			catch (...)
			{
				for (auto package = createdPackages.rbegin(); package != createdPackages.rend(); ++package)
				{
					try { PresetPackages::Remove(*package); }
					catch (...) { rollbackIncomplete = true; }
				}
				throw;
			}

			RefreshPresetPackages();
			if (!packages.empty())
			{
				const auto installed = std::ranges::find(m_presetPackages, packages.front().metadata.uuid, [](const auto& candidate)
				{
					return candidate.metadata.uuid;
				});
				if (installed != m_presetPackages.end())
				{
					SelectPresetPackageRow(static_cast<std::size_t>(std::distance(m_presetPackages.begin(), installed)));
					UpdatePresetPackageDetails();
				}
			}
			wxMessageBox(wxString::Format(
				L"Preset pack import completed.\n\nNewly imported: %zu\nAlready installed: %zu\nThe current configuration was not changed.",
				createdCount,
				reusedCount
			), L"Preset import", wxOK | wxICON_INFORMATION, this);
		}
		catch (...)
		{
			const auto error = wil::ResultFromCaughtException();
			wxMessageBox(wxString::Format(
				rollbackIncomplete
					? L"The preset batch could not be imported, and cleanup of newly deployed packages was incomplete.\n\nFile: %s\nHRESULT: 0x%08lX"
					: L"The preset batch could not be imported. Nothing from this batch was retained.\n\nFile: %s\nHRESULT: 0x%08lX",
				currentPath.filename().wstring(),
				static_cast<unsigned long>(error)
			), L"Preset import", wxOK | wxICON_ERROR, this);
			RefreshPresetPackages();
		}
	}

	void MainFrame::ImportPresetPackage(const std::filesystem::path& path)
	{
		try
		{
			auto package = PresetPackages::LoadArchive(path);
			const auto action = ShowPresetPreview(this, package, *m_config, true);
			if (action == PresetPreviewAction::Cancel) return;
			if (action == PresetPreviewAction::Apply)
			{
				ApplyPresetPackage(package, true);
				RefreshPresetPackages();
				return;
			}

			auto verifiedPackage = PresetPackages::LoadArchive(package.source);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), verifiedPackage.digest != package.digest);
			const auto deployment = PresetPackages::Deploy(verifiedPackage);
			try
			{
				auto deployedPackage = PresetPackages::LoadDeployed(deployment.path);
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), deployedPackage.digest != package.digest);
			}
			catch (...)
			{
				if (deployment.created)
				{
					PresetPackages::Package deployedPackage;
					deployedPackage.source = deployment.path;
					deployedPackage.deployed = true;
					try { PresetPackages::Remove(deployedPackage); }
					catch (...) {}
				}
				throw;
			}
			RefreshPresetPackages();
			const auto installed = std::ranges::find(m_presetPackages, package.metadata.uuid, [](const auto& candidate)
			{
				return candidate.metadata.uuid;
			});
			if (installed != m_presetPackages.end())
			{
				SelectPresetPackageRow(static_cast<std::size_t>(std::distance(m_presetPackages.begin(), installed)));
				UpdatePresetPackageDetails();
			}
			wxMessageBox(
				deployment.created
					? L"The preset pack was imported without changing the current configuration."
					: L"The identical preset pack was already installed; the current configuration was not changed.",
				L"Preset import",
				wxOK | wxICON_INFORMATION,
				this
			);
		}
		catch (...)
		{
			const auto error = wil::ResultFromCaughtException();
			wxMessageBox(wxString::Format(L"The preset ZIP could not be imported (HRESULT 0x%08lX).", static_cast<unsigned long>(error)), L"Preset import", wxOK | wxICON_ERROR, this);
		}
	}

	void MainFrame::ApplySelectedPresetPackage()
	{
		const long selection = m_lstPresetPackages->GetFirstSelected();
		if (selection == wxNOT_FOUND || static_cast<std::size_t>(selection) >= m_presetPackages.size()) return;
		ApplyPresetPackage(m_presetPackages[selection]);
	}

	bool MainFrame::ApplyPresetPackage(const PresetPackages::Package& inputPackage, bool previewAccepted)
	{
		if (!previewAccepted
			&& ShowPresetPreview(this, inputPackage, *m_config, false) != PresetPreviewAction::Apply)
		{
			return false;
		}
		const auto sensitiveChanges = SensitiveChangeSummary(inputPackage, *m_config);
		if (!sensitiveChanges.empty()
			&& wxMessageBox(
				L"This preset changes security- or stability-sensitive settings, including graphics that dwm.exe may decode:\n" + sensitiveChanges + L"\n\nApply these changes?",
				L"Confirm sensitive preset settings",
				wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
				this
			) != wxYES)
		{
			return false;
		}

		PresetPackages::Package package = inputPackage;
		std::filesystem::path deployedPath;
		bool deployedByThisApply{};
		bool restartRequired{};
		bool registryMutationStarted{};
		const auto previousDirtyKeys = m_dirtyKeys;
		const auto previousBackups = m_backupSettings;
		std::map<TrackedSetting, std::variant<std::monostate, DWORD, std::wstring>> previousValues;

		try
		{
			if (package.deployed)
			{
				auto verifiedPackage = PresetPackages::LoadDeployed(package.source);
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), verifiedPackage.digest != package.digest);
				package = std::move(verifiedPackage);
			}
			else
			{
				auto verifiedPackage = PresetPackages::LoadArchive(package.source);
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), verifiedPackage.digest != package.digest);
				const auto deployment = PresetPackages::Deploy(verifiedPackage);
				deployedPath = deployment.path;
				deployedByThisApply = deployment.created;
				package = PresetPackages::LoadDeployed(deployedPath);
			}
			for (const auto& [id, value] : package.settings)
			{
				const auto& spec = Settings::Get(id);
				const std::wstring name(spec.name);
				for (const auto scope : { Settings::Scope::User, Settings::Scope::Machine })
				{
					auto* config = GetConfigForScope(scope);
					if (spec.type == Settings::ValueType::Dword)
					{
						DWORD current{};
						previousValues.emplace(
							TrackedSetting{ scope, id },
							config->TryGetDword(name, current)
								? decltype(previousValues)::mapped_type{ current }
								: decltype(previousValues)::mapped_type{ std::monostate{} }
						);
					}
					else
					{
						std::wstring current;
						previousValues.emplace(
							TrackedSetting{ scope, id },
							config->TryGetString(name, current)
								? decltype(previousValues)::mapped_type{ std::move(current) }
								: decltype(previousValues)::mapped_type{ std::monostate{} }
						);
					}
				}
			}
			registryMutationStarted = true;
			for (const auto& [id, value] : package.settings)
			{
				const auto& spec = Settings::Get(id);
				restartRequired |= spec.impact == Settings::UpdateImpact::RestartRequired && PackageValueWouldChange(spec, value, *m_config);
				const std::wstring name(spec.name);
				const auto wrongScope = spec.scope == Settings::Scope::User ? Settings::Scope::Machine : Settings::Scope::User;
				TrackSettingChange(spec.scope, id);
				TrackSettingChange(wrongScope, id);
				THROW_IF_FAILED(GetConfigForScope(wrongScope)->DeleteValue(name));
				if (std::holds_alternative<std::monostate>(value))
				{
					THROW_IF_FAILED(m_config->DeleteValue(name));
				}
				else if (const auto dword = std::get_if<DWORD>(&value))
				{
					THROW_IF_FAILED(m_config->SetDword(name, *dword));
				}
				else if (const auto asset = std::get_if<PresetPackages::AssetReference>(&value))
				{
					THROW_IF_FAILED(m_config->SetString(name, PresetPackages::ResolveAssetPath(package, *asset)));
				}
			}
			SetDirty(true);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_NOT_READY), !NotifySettingsChange(ChangeType::Both));
			LoadSettings(false);
			if (restartRequired)
			{
				wxMessageBox(L"Some settings require restarting DWM or signing out before they become active. OpenGlass will not restart DWM automatically.", L"Preset applied", wxOK | wxICON_INFORMATION, this);
			}
			return true;
		}
		catch (...)
		{
			const auto error = wil::ResultFromCaughtException();
			HRESULT rollbackFailure{ S_OK };
			if (registryMutationStarted)
			{
				for (const auto& [setting, value] : previousValues)
				{
					auto* config = GetConfigForScope(setting.scope);
					const std::wstring name(Settings::Get(setting.id).name);
					const auto result = std::holds_alternative<std::monostate>(value)
						? config->DeleteValue(name)
						: std::holds_alternative<DWORD>(value)
							? config->SetDword(name, std::get<DWORD>(value))
							: config->SetString(name, std::get<std::wstring>(value));
					if (SUCCEEDED(rollbackFailure) && FAILED(result)) rollbackFailure = result;
				}
				if (SUCCEEDED(rollbackFailure))
				{
					m_dirtyKeys = previousDirtyKeys;
					m_backupSettings = previousBackups;
				}
				SetDirty(!m_dirtyKeys.empty());
				NotifySettingsChange(ChangeType::Both);
				LoadSettings(false);
			}
			const bool rolledBack = SUCCEEDED(rollbackFailure);
			if (rolledBack && deployedByThisApply && !deployedPath.empty())
			{
				try
				{
					PresetPackages::Package deployed;
					deployed.source = deployedPath;
					deployed.deployed = true;
					PresetPackages::Remove(deployed);
				}
				catch (...) {}
			}
			wxMessageBox(wxString::Format(
				rolledBack
					? L"Applying the preset failed and this attempt was reverted (HRESULT 0x%08lX)."
					: L"Applying the preset failed, and reverting this attempt was incomplete (HRESULT 0x%08lX). The deployed package was retained to avoid breaking an asset path.",
				static_cast<unsigned long>(error)
			), L"Preset apply", wxOK | wxICON_ERROR, this);
			return false;
		}
	}

	void MainFrame::CreatePresetPackage()
	{
		wxString defaultAuthor = m_lastPresetAuthorName;
		if (defaultAuthor.empty())
		{
			defaultAuthor = m_targetUserLabel;
			if (const auto separator = defaultAuthor.Find(L'\\', true); separator != wxNOT_FOUND)
			{
				defaultAuthor = defaultAuthor.Mid(separator + 1);
			}
		}
		CreatePresetDialog dialog(
			this,
			defaultAuthor,
			m_lastPresetAuthorHomepage,
			m_lastPresetIncludeLicense,
			wxString::FromUTF8(m_lastPresetLicenseText),
			m_lastPresetInstallAfterCreate
		);
		if (dialog.ShowModal() != wxID_OK) return;
		auto metadata = dialog.Metadata();
		m_lastPresetAuthorName = metadata.authorName;
		m_lastPresetAuthorHomepage = metadata.authorHomepage;
		m_lastPresetIncludeLicense = dialog.IncludeLicense();
		m_lastPresetLicenseText = dialog.LicenseEditorText();
		m_lastPresetInstallAfterCreate = dialog.InstallAfterCreate();

		PresetPackages::CreateRequest request;
		request.metadata = std::move(metadata);
		request.metadata.uuid = PresetPackages::GeneratePackageUuid();
		request.licenseText = dialog.LicenseText();
		const auto packageUuid = request.metadata.uuid;
		std::filesystem::path archivePath;
		bool archiveCreated{};
		try
		{
			for (const auto& spec : Settings::Catalog)
			{
				if (!Settings::IsPresetPackSetting(spec)) continue;
				const std::wstring name(spec.name);
				if (spec.type == Settings::ValueType::Dword)
				{
					DWORD value{};
					request.settings.emplace(spec.id, m_config->TryGetDword(name, value) ? PresetPackages::SettingValue{ value } : PresetPackages::SettingValue{});
				}
				else
				{
					std::wstring value;
					if (!m_config->TryGetString(name, value) || value.empty())
					{
						request.settings.emplace(spec.id, std::monostate{});
						continue;
					}
					const std::filesystem::path source(value);
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !std::filesystem::is_regular_file(source));
					const auto assetName = AssetName(spec.assetRole, source);
					request.assetSources.emplace(assetName, source);
					request.settings.emplace(spec.id, PresetPackages::AssetReference{ assetName });
					if (spec.assetRole == Settings::AssetRole::ThemeAtlas)
					{
						const std::filesystem::path layout(value + L".layout");
						if (std::filesystem::is_regular_file(layout)) request.assetSources.emplace(assetName + ".layout", layout);
					}
				}
			}
			const auto suggestedName = SanitizePackageFileName(request.metadata.name) + L"-" + wxString::FromUTF8(request.metadata.uuid).ToStdWstring() + L".zip";
			wxFileDialog save(this, L"Save preset ZIP", wxEmptyString, suggestedName, L"ZIP archives (*.zip)|*.zip", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
			if (save.ShowModal() != wxID_OK) return;
			archivePath = save.GetPath().ToStdWstring();
			PresetPackages::CreateArchive(archivePath, std::move(request));
			archiveCreated = true;
			if (m_lastPresetInstallAfterCreate)
			{
				auto package = PresetPackages::LoadArchive(archivePath);
				const auto deployment = PresetPackages::Deploy(package);
				try
				{
					auto deployedPackage = PresetPackages::LoadDeployed(deployment.path);
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), deployedPackage.digest != package.digest);
				}
				catch (...)
				{
					if (deployment.created)
					{
						PresetPackages::Package deployedPackage;
						deployedPackage.source = deployment.path;
						deployedPackage.deployed = true;
						try { PresetPackages::Remove(deployedPackage); }
						catch (...) {}
					}
					throw;
				}
				RefreshPresetPackages();
				const auto installed = std::ranges::find(m_presetPackages, packageUuid, [](const auto& candidate)
				{
					return candidate.metadata.uuid;
				});
				if (installed != m_presetPackages.end())
				{
					SelectPresetPackageRow(static_cast<std::size_t>(std::distance(m_presetPackages.begin(), installed)));
					UpdatePresetPackageDetails();
				}
				wxMessageBox(L"The immutable preset ZIP was created and installed successfully. The current configuration was not changed.", L"Create preset", wxOK | wxICON_INFORMATION, this);
			}
			else
			{
				wxMessageBox(L"The immutable preset ZIP was created successfully.", L"Create preset", wxOK | wxICON_INFORMATION, this);
			}
		}
		catch (...)
		{
			const auto error = wil::ResultFromCaughtException();
			const auto message = archiveCreated
				? wxString::Format(
					L"The preset ZIP was created at:\n%s\n\nbut it could not be installed (HRESULT 0x%08lX).",
					archivePath.wstring(),
					static_cast<unsigned long>(error)
				)
				: wxString::Format(
					L"The preset ZIP could not be created (HRESULT 0x%08lX).",
					static_cast<unsigned long>(error)
				);
			wxMessageBox(
				message,
				L"Create preset",
				wxOK | wxICON_ERROR,
				this
			);
		}
	}

	void MainFrame::ResetPresetPackSettings()
	{
		using RawValue = std::variant<DWORD, std::wstring>;
		struct Snapshot
		{
			TrackedSetting setting;
			RawValue value;
		};

		std::vector<Snapshot> snapshots;
		for (const auto& spec : Settings::Catalog)
		{
			if (!Settings::IsPresetPackSetting(spec)) continue;
			for (const auto scope : { Settings::Scope::User, Settings::Scope::Machine })
			{
				auto* config = GetConfigForScope(scope);
				if (spec.type == Settings::ValueType::Dword)
				{
					DWORD value{};
					if (config->TryGetDword(std::wstring(spec.name), value))
					{
						snapshots.push_back({ { scope, spec.id }, value });
					}
				}
				else
				{
					std::wstring value;
					if (config->TryGetString(std::wstring(spec.name), value))
					{
						snapshots.push_back({ { scope, spec.id }, std::move(value) });
					}
				}
			}
		}

		if (snapshots.empty())
		{
			wxMessageBox(
				L"All preset-pack settings are already using their default or inherited values.",
				L"Reset all settings",
				wxOK | wxICON_INFORMATION,
				this
			);
			return;
		}
		if (wxMessageBox(
			wxString::Format(
				L"Delete %zu stored preset-pack value(s) and return every packaged setting to its default or inherited value?\n\n"
				L"This includes Windows colorization values for the target user and system-wide OpenGlass settings. The change is immediate; use Revert to restore the current values before saving.",
				snapshots.size()
			),
			L"Reset all settings",
			wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
			this
		) != wxYES)
		{
			return;
		}

		const auto previousDirtyKeys = m_dirtyKeys;
		const auto previousBackups = m_backupSettings;
		HRESULT failure{ S_OK };
		for (const auto& snapshot : snapshots)
		{
			TrackSettingChange(snapshot.setting.scope, snapshot.setting.id);
			failure = GetConfigForScope(snapshot.setting.scope)->DeleteValue(std::wstring(Settings::Get(snapshot.setting.id).name));
			if (FAILED(failure)) break;
		}

		if (FAILED(failure))
		{
			HRESULT rollbackFailure{ S_OK };
			for (const auto& snapshot : snapshots)
			{
				auto* config = GetConfigForScope(snapshot.setting.scope);
				const std::wstring name(Settings::Get(snapshot.setting.id).name);
				const auto result = std::holds_alternative<DWORD>(snapshot.value)
					? config->SetDword(name, std::get<DWORD>(snapshot.value))
					: config->SetString(name, std::get<std::wstring>(snapshot.value));
				if (SUCCEEDED(rollbackFailure) && FAILED(result)) rollbackFailure = result;
			}
			m_dirtyKeys = previousDirtyKeys;
			m_backupSettings = previousBackups;
			SetDirty(!m_dirtyKeys.empty());
			NotifySettingsChange(ChangeType::Both);
			LoadSettings(false);
			wxMessageBox(
				FAILED(rollbackFailure)
					? wxString::Format(L"Reset failed (HRESULT 0x%08lX), and restoring the previous registry state was incomplete (HRESULT 0x%08lX).", static_cast<unsigned long>(failure), static_cast<unsigned long>(rollbackFailure))
					: wxString::Format(L"Reset failed (HRESULT 0x%08lX). The previous registry state was restored.", static_cast<unsigned long>(failure)),
				L"Reset all settings",
				wxOK | wxICON_ERROR,
				this
			);
			return;
		}

		SetDirty(true);
		NotifySettingsChange(ChangeType::Both);
		LoadSettings(false);
	}

	void MainFrame::RemoveSelectedPresetPackage()
	{
		const long selection = m_lstPresetPackages->GetFirstSelected();
		if (selection == wxNOT_FOUND || static_cast<std::size_t>(selection) >= m_presetPackages.size()) return;
		try
		{
			const auto& package = m_presetPackages[selection];
			for (const auto& spec : Settings::Catalog)
			{
				if (spec.assetRole == Settings::AssetRole::None) continue;
				std::wstring path;
				if (m_config->TryGetString(std::wstring(spec.name), path)
					&& PathIsWithin(path, package.source))
				{
					wxMessageBox(L"This package is still referenced by the active configuration. Apply another preset or clear its assets before removing it.", L"Remove preset", wxOK | wxICON_WARNING, this);
					return;
				}
			}
			if (wxMessageBox(L"Remove the selected deployed preset package?", L"Remove preset", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES) return;
			PresetPackages::Remove(package);
			RefreshPresetPackages();
		}
		catch (...)
		{
			const auto error = wil::ResultFromCaughtException();
			wxMessageBox(wxString::Format(L"The package could not be removed (HRESULT 0x%08lX).", static_cast<unsigned long>(error)), L"Remove preset", wxOK | wxICON_ERROR, this);
		}
	}
}
