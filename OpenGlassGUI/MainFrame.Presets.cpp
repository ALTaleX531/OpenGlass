#include "pch.h"
#include "MainFrame.hpp"
#include "ColorPresetList.hpp"
#include "EffectiveConfiguration.hpp"
#include "UiControls.hpp"
#include <wx/imagpng.h>
#include <wx/dcbuffer.h>
#include <wx/scrolwin.h>

namespace OpenGlass
{
	namespace
	{
		constexpr std::size_t MaximumBatchPackageCount = 32;
		// WM_COPYGLOBALDATA is intentionally undocumented but is part of the
		// WM_DROPFILES transfer used by Explorer across an integrity boundary.
		constexpr UINT WmCopyGlobalData = 0x0049;

		enum class PresetPreviewAction
		{
			Cancel,
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

		std::wstring RightsSummary(const PresetPackages::Package& package)
		{
			std::wstring text = package.legacyLicense
				? L"Inherited legacy licensing: LICENSE retains its original package-wide scope unless its text limits it. No new permissions are granted when LICENSE is absent."
				: L"OpenGlass Attribution v1: configuration parameters, accent RGB and layout may be copied, modified and shared with author and known-source attribution, without proprietary use restrictions. LICENSE governs image assets only; absent LICENSE grants no additional image rights.";
			for (const auto& source : package.attribution) text += L"\r\nSource: " + source;
			return text;
		}

		EffectiveConfiguration::Model PackageModel(const PresetPackages::Package& package, const ConfigurationResources& resources, Settings::Scope scope)
		{
			EffectiveConfiguration::Model model;
			for (const auto& [id, value] : package.settings)
			{
				if (const auto number = std::get_if<DWORD>(&value)) model.emplace(id, *number);
				else if (std::holds_alternative<PresetPackages::AssetReference>(value))
				{
					model.emplace(id, resources.Path(scope, id).wstring());
				}
				else model.emplace(id, std::monostate{});
			}
			return model;
		}

		std::wstring RawText(const RegistryConfig::RawValue& raw)
		{
			if (!raw.present) return L"<absent>";
			if (raw.type == REG_DWORD && raw.bytes.size() == 4)
			{
				DWORD value{}; std::memcpy(&value, raw.bytes.data(), 4); return std::format(L"0x{:08X}", value);
			}
			if (raw.type == REG_SZ && raw.bytes.size() % sizeof(wchar_t) == 0)
			{
				std::wstring text(raw.bytes.size() / sizeof(wchar_t), L'\0');
				std::memcpy(text.data(), raw.bytes.data(), raw.bytes.size());
				if (!text.empty() && !text.back()) text.pop_back();
				return text;
			}
			return std::format(L"<type {}, {} bytes>", raw.type, raw.bytes.size());
		}

		std::wstring PlanSummary(const std::vector<EffectiveConfiguration::Change>& plan)
		{
			std::wstring result;
			for (const auto& change : plan)
			{
				result += std::format(L"{} {}: {} -> {}\r\n", change.scope == Settings::Scope::User ? L"HKCU" : L"HKLM", change.Name(), RawText(change.before), RawText(change.after));
			}
			return result;
		}

		void AppendIgnoredSettings(std::wstring& output, const PresetPackages::Package& package)
		{
			if (package.ignoredSettingCount == 0) return;
			output += std::format(
				L"\r\nIgnored settings: {} (not applied)\r\n",
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

		class PresetImagePreview final : public wxPanel
		{
			wxImage m_image;
			wxBitmap m_bitmap;
		public:
			PresetImagePreview(wxWindow* parent, wxImage image, const wxString& name)
				: wxPanel(parent), m_image(std::move(image))
			{
				SetMinSize(wxSize(0, 0));
				SetBackgroundStyle(wxBG_STYLE_PAINT);
				SetToolTip(name + L"\nDouble-click to view at original size.");
				Bind(wxEVT_SIZE, [this](wxSizeEvent& event) { Refresh(false); event.Skip(); });
				Bind(wxEVT_PAINT, [this](wxPaintEvent&)
				{
					wxAutoBufferedPaintDC dc(this);
					dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour())); dc.Clear();
					const auto available = GetClientSize() - FromDIP(wxSize(8, 8));
					if (available.x <= 0 || available.y <= 0) return;
					const double scale = std::min(double(available.x) / m_image.GetWidth(), double(available.y) / m_image.GetHeight());
					const wxSize size(std::max(1, int(m_image.GetWidth() * scale)), std::max(1, int(m_image.GetHeight() * scale)));
					if (!m_bitmap.IsOk() || m_bitmap.GetSize() != size)
						m_bitmap = wxBitmap(m_image.Scale(size.x, size.y, wxIMAGE_QUALITY_HIGH));
					const wxPoint origin((GetClientSize().x - size.x) / 2, (GetClientSize().y - size.y) / 2);
					const int tile = FromDIP(8);
					const bool dark = wxSystemSettings::GetAppearance().IsDark();
					dc.SetPen(*wxTRANSPARENT_PEN);
					for (int y = 0; y < size.y; y += tile)
						for (int x = 0; x < size.x; x += tile)
						{
							const auto shade = dark ? ((x / tile + y / tile) % 2 ? 68 : 52) : ((x / tile + y / tile) % 2 ? 232 : 248);
							dc.SetBrush(wxBrush(wxColour(shade, shade, shade)));
							dc.DrawRectangle(origin.x + x, origin.y + y, std::min(tile, size.x - x), std::min(tile, size.y - y));
						}
					dc.DrawBitmap(m_bitmap, origin, true);
				});
				Bind(wxEVT_LEFT_DCLICK, [this, name](wxMouseEvent&)
				{
					wxDialog viewer(this, wxID_ANY, name, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
					viewer.SetClientSize(viewer.FromDIP(wxSize(900, 640)));
					auto* root = new wxBoxSizer(wxVERTICAL);
					auto* scroll = new wxScrolledWindow(&viewer, wxID_ANY);
					scroll->SetScrollRate(16, 16);
					auto* content = new wxBoxSizer(wxVERTICAL);
					content->Add(new wxStaticBitmap(scroll, wxID_ANY, wxBitmap(m_image)), 0);
					scroll->SetSizer(content); scroll->FitInside();
					root->Add(scroll, 1, wxEXPAND | wxALL, 8);
					root->Add(viewer.CreateSeparatedButtonSizer(wxOK), 0, wxEXPAND | wxALL, 8);
					viewer.FindWindow(wxID_OK)->SetLabel(L"Close"); viewer.SetSizer(root); viewer.ShowModal();
				});
			}
		};

		PresetPreviewAction ShowPresetPreview(
			wxWindow* parent,
			const PresetPackages::Package& package,
			const RegistryConfig& config,
			bool importing, const RegistryConfig& user, const RegistryConfig& machine, bool systemTransparency
		)
		{
			wxDialog dialog(parent, wxID_ANY, importing ? L"Import preset" : L"Preset properties", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
			dialog.SetClientSize(dialog.FromDIP(wxSize(680, 520)));
			dialog.SetMinSize(dialog.FromDIP(wxSize(540, 440)));
			auto* root = new wxBoxSizer(wxVERTICAL);
			auto* pages = new wxNotebook(&dialog, wxID_ANY);
			auto* general = new wxPanel(pages);
			auto* summary = new wxBoxSizer(wxVERTICAL);
			auto* name = new wxStaticText(general, wxID_ANY, package.metadata.name, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
			name->SetFont(name->GetFont().Bold());
			name->SetMinSize(wxSize(0, -1));
			name->SetToolTip(package.metadata.name);
			summary->Add(name, 0, wxEXPAND | wxALL, 12);
			auto* fields = new wxFlexGridSizer(2, general->FromDIP(6), general->FromDIP(16));
			fields->AddGrowableCol(1);
			auto field = [&](const wxString& label, const wxString& value, const wxString& explanation = wxEmptyString)
			{
				fields->Add(new wxStaticText(general, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
				auto* text = new wxStaticText(general, wxID_ANY, value, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
				text->SetMinSize(wxSize(0, -1));
				text->SetToolTip(explanation.empty() ? value : value + L"\n" + explanation);
				fields->Add(text, 1, wxEXPAND);
			};
			const auto authorMeaning = package.legacyLicense ? L"Original author information from the legacy package."
				: package.licenseText.empty() ? L"Reference source; no LICENSE was provided." : L"Declared LICENSE rights holder; identity is not verified.";
			field(L"Author", package.metadata.authorName, authorMeaning);
			field(L"License", package.licenseText.empty() ? wxString(L"Not provided") : wxString(package.metadata.licenseName));
			field(L"Accent color", package.accentColor ? wxString::Format(L"#%06X", *package.accentColor) : wxString(L"Automatic"));
			field(L"Apply to", config.GetMode() == RegistryConfig::Mode::User ? L"HKCU" : L"HKLM");
			if (!package.conversions.empty() || package.ignoredSettingCount)
				field(L"Compatibility", wxString::Format(L"%zu conversions, %zu ignored settings. See Changes.", package.conversions.size(), package.ignoredSettingCount));
			summary->Add(fields, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
			if (!package.metadata.description.empty())
			{
				auto* description = new wxTextCtrl(general, wxID_ANY, package.metadata.description, wxDefaultPosition, wxDefaultSize,
					wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE);
				FollowSystemColours(description, std::nullopt);
				description->SetMinSize(wxSize(0, description->GetCharHeight() + general->FromDIP(4)));
				description->Bind(wxEVT_SIZE, [description, general](wxSizeEvent& event)
				{
					// On Windows, GetNumberOfLines includes soft-wrapped display lines.
					const auto height = std::clamp(description->GetNumberOfLines(), 1, 3) * description->GetCharHeight() + general->FromDIP(4);
					if (description->GetMinSize().y != height)
					{
						description->SetMinSize(wxSize(0, height)); general->Layout();
					}
					event.Skip();
				});
				summary->Add(description, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
			}
			auto* previews = new wxBoxSizer(wxHORIZONTAL);
			for (const auto& [assetName, bytes] : package.assets)
			{
				if (!assetName.ends_with(".png")) continue;
				wxMemoryInputStream stream(bytes.data(), bytes.size());
				wxPNGHandler decoder; wxImage picture;
				if (!decoder.LoadFile(&picture, stream, false) || !picture.IsOk()) continue;
				auto* preview = new PresetImagePreview(general, std::move(picture), wxString::FromUTF8(assetName));
				previews->Add(preview, 1, wxEXPAND | wxLEFT | wxRIGHT, 6);
			}
			if (!previews->IsEmpty()) summary->Add(previews, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
			else { delete previews; summary->AddStretchSpacer(); }
			if (!package.metadata.authorHomepage.empty())
			{
				auto* homepage = new wxButton(general, wxID_ANY, L"Author homepage...");
				homepage->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { OpenConfirmedUrl(&dialog, package.metadata.authorHomepage); });
				summary->Add(homepage, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
			}
			general->SetSizer(summary);
			pages->AddPage(general, L"General");
			auto addDetails = [&](const wxString& title, const wxString& text)
			{
				auto* page = new wxPanel(pages);
				auto* layout = new wxBoxSizer(wxVERTICAL);
				layout->Add(new wxTextCtrl(page, wxID_ANY, text, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2), 1, wxEXPAND | wxALL, 8);
				page->SetSizer(layout); pages->AddPage(page, title);
			};

			ConfigurationResources resources; resources.Initialize(PresetPackages::GetPresetRoot().parent_path() / L"Configuration", user.UserSid());
			const auto scope = config.GetMode() == RegistryConfig::Mode::User ? Settings::Scope::User : Settings::Scope::Machine;
			const auto plan = EffectiveConfiguration::Plan(EffectiveConfiguration::Read(user, systemTransparency), EffectiveConfiguration::Read(machine), PackageModel(package, resources, scope), scope, package.catalogVersion, systemTransparency);
			std::wstring changes = scope == Settings::Scope::User
				? L"Applies to HKCU; HKLM configuration is preserved.\r\n"
				: L"Applies to HKLM; conflicting HKCU values for customized settings are removed.\r\n";
			changes += L"Default settings remove customization in this layer and allow inheritance.\r\n";
			changes += package.accentColor ? std::format(L"\r\nSet the original user's accent color to #{:06X}.\r\n", *package.accentColor)
				: L"\r\nEnable automatic accent coloring from the original user's wallpaper.\r\n";
			changes += L"Both layers' color Overrides are removed. Balance Overrides follow the editing scope.\r\n";
			if (systemTransparency) changes += L"Transparency applies to the original user's Windows transparency effects. Default enables transparency.\r\n";
			const bool restartRequired = std::ranges::any_of(plan, [](const auto& change) { return Settings::Get(change.id).impact == Settings::UpdateType::RestartRequired; });
			if (restartRequired) changes += L"Some settings take effect after restarting DWM or signing out; applying does not restart DWM.\r\n";
			changes += L"\r\nRegistry changes:\r\n" + (plan.empty() ? std::wstring(L"None.\r\n") : PlanSummary(plan));
			for (const auto& conversion : package.conversions) changes += L"\r\nConverted: " + conversion + L"\r\n";
			AppendIgnoredSettings(changes, package);
			addDetails(L"Changes", changes);

			std::wstring contents = L"Saved settings\r\n\r\n";
			for (const auto& [id, value] : package.settings)
			{
				std::wstring text = L"Default (inherit)";
				if (const auto number = std::get_if<DWORD>(&value)) text = std::format(L"0x{:08X}", *number);
				else if (const auto asset = std::get_if<PresetPackages::AssetReference>(&value)) text = wxString::FromUTF8(asset->path).ToStdWstring();
				contents += std::wstring(Settings::Get(id).name) + L": " + text + L"\r\n";
			}
			contents += L"\r\nFiles\r\n\r\n";
			for (const auto& [assetName, size] : package.assetSummary)
				contents += std::format(L"{} ({} bytes)\r\n", wxString::FromUTF8(assetName).ToStdWstring(), size);
			if (package.assetSummary.empty()) contents += L"No image resources.\r\n";
			contents += std::format(L"\r\nUUID: {}\r\nCatalog version: {}\r\nContent digest: {}", wxString::FromUTF8(package.metadata.uuid).ToStdWstring(), package.catalogVersion, wxString::FromUTF8(package.digest).ToStdWstring());
			addDetails(L"Contents", contents);
			addDetails(L"License", std::wstring(authorMeaning) + L"\r\n\r\n" + RightsSummary(package)
				+ L"\r\n\r\nLICENSE\r\n\r\n" + (package.licenseText.empty() ? wxString(L"Not provided.") : wxString::FromUTF8(package.licenseText)));
			root->Add(pages, 1, wxEXPAND | wxALL, 10);
			if (importing)
			{
				auto* note = new wxStaticText(&dialog, wxID_ANY, L"Import saves this preset without applying it.\nTrust accepts this content, not the author's identity.");
				root->Add(note, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
			}
			auto* buttons = dialog.CreateSeparatedButtonSizer(importing ? wxOK | wxCANCEL : wxOK);
			dialog.FindWindow(wxID_OK)->SetLabel(importing ? L"Trust and import" : L"Close");
			root->Add(buttons, 0, wxEXPAND | wxALL, 10);
			dialog.SetSizer(root);
			return dialog.ShowModal() == wxID_OK && importing ? PresetPreviewAction::ImportOnly : PresetPreviewAction::Cancel;
		}

		bool ShowBatchImportPreview(wxWindow* parent, std::span<const PresetPackages::Package> packages, const RegistryConfig& config, const RegistryConfig& user, const RegistryConfig& machine, bool systemTransparency)
		{
			wxDialog dialog(parent, wxID_ANY, L"Import presets", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
			dialog.SetClientSize(dialog.FromDIP(wxSize(740, 440)));
			dialog.SetMinSize(dialog.FromDIP(wxSize(560, 360)));
			auto* root = new wxBoxSizer(wxVERTICAL);
			root->Add(new wxStaticText(&dialog, wxID_ANY, wxString::Format(L"%zu presets. Double-click a row to view its details.", packages.size())), 0, wxEXPAND | wxALL, 10);
			auto* list = new wxListView(&dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
			const wchar_t* headings[]{ L"Name", L"Accent color", L"Author", L"License", L"Compatibility" };
			const int widths[]{ 200, 95, 130, 130, 150 };
			for (int column = 0; column < 5; ++column) list->InsertColumn(column, headings[column], wxLIST_FORMAT_LEFT, dialog.FromDIP(widths[column]));
			for (std::size_t index = 0; index < packages.size(); ++index)
			{
				const auto& package = packages[index];
				const auto row = static_cast<long>(index);
				list->InsertItem(row, package.metadata.name);
				list->SetItem(row, 2, package.metadata.authorName);
				list->SetItem(row, 3, package.licenseText.empty() ? wxString(L"Not provided") : wxString(package.metadata.licenseName));
				list->SetItem(row, 1, package.accentColor ? wxString::Format(L"#%06X", *package.accentColor) : wxString(L"Automatic"));
				if (!package.conversions.empty() || package.ignoredSettingCount)
					list->SetItem(row, 4, wxString::Format(L"%zu converted, %zu ignored", package.conversions.size(), package.ignoredSettingCount));
			}
			root->Add(list, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);
			auto viewSelected = [&]()
			{
				const auto row = list->GetFirstSelected();
				if (row == wxNOT_FOUND) return;
				try
				{
					const auto package = PresetPackages::LoadArchive(packages[row].source);
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), package.digest != packages[row].digest);
					ShowPresetPreview(&dialog, package, config, false, user, machine, systemTransparency);
				}
				catch (...)
				{
					wxMessageBox(wxString::Format(L"The preset could not be reviewed (0x%08lX). Cancel and import it again.", wil::ResultFromCaughtException()),
						L"Preset properties", wxOK | wxICON_ERROR, &dialog);
				}
			};
			list->Bind(wxEVT_LIST_ITEM_ACTIVATED, [&](wxListEvent&) { viewSelected(); });
			auto* details = new wxButton(&dialog, wxID_ANY, L"Properties...");
			details->Enable(false);
			details->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) { viewSelected(); });
			list->Bind(wxEVT_LIST_ITEM_SELECTED, [details](wxListEvent&) { details->Enable(true); });
			list->Bind(wxEVT_LIST_ITEM_DESELECTED, [details, list](wxListEvent&) { details->Enable(list->GetFirstSelected() != wxNOT_FOUND); });
			root->Add(details, 0, wxALL, 10);
			root->Add(new wxStaticText(&dialog, wxID_ANY, L"Trust includes every listed preset. Import does not apply configuration."), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
			auto* buttons = dialog.CreateSeparatedButtonSizer(wxOK | wxCANCEL);
			dialog.FindWindow(wxID_OK)->SetLabel(L"Trust and import all");
			root->Add(buttons, 0, wxEXPAND | wxALL, 10);
			dialog.SetSizer(root);
			return dialog.ShowModal() == wxID_OK;
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

		struct PackagePreviewMetrics
		{
			int canvasDip;
			int insetDip;
		};

		wxBitmap CreatePackagePreviewBitmap(
			wxWindow* window,
			const PresetPackages::Package& package,
			PackagePreviewMetrics metrics
		)
		{
			const auto argb = package.accentColor;
			const wxColour color = argb
				? wxColour((*argb >> 16) & 0xFF, (*argb >> 8) & 0xFF, *argb & 0xFF)
				: wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
			wxGraphicsGradientStops gradient(color.ChangeLightness(150), color.ChangeLightness(85));
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
				const wxRect2DDouble bounds(inset + borderWidth, inset + borderWidth,
					swatchWidth - 2 * borderWidth, swatchHeight - 2 * borderWidth);
				context->SetPen(*wxTRANSPARENT_PEN);
				context->SetBrush(wxBrush(border));
				context->DrawRectangle(inset, inset, swatchWidth, swatchHeight);
				if (!package.accentColor)
				{
					DrawAutomaticColorSwatch(*context, bounds);
				}
				else
				{
					context->SetBrush(context->CreateLinearGradientBrush(inset, inset,
						inset + swatchWidth, inset + swatchHeight, gradient));
					context->DrawRectangle(bounds.m_x, bounds.m_y, bounds.m_width, bounds.m_height);
				}
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
				bool includeAccentColor
			)
				: wxDialog(parent, wxID_ANY, L"New preset", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
			{
				auto* root = new wxBoxSizer(wxVERTICAL);
				auto* scopeNote = new wxStaticText(this, wxID_ANY,
					L"Extracts the saved effective HKCU + HKLM configuration into a local preset.");
				scopeNote->Wrap(FromDIP(610));
				root->Add(scopeNote, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

				auto addField = [](wxWindow* parent, wxFlexGridSizer* grid, const wxString& label, wxWindow* control)
				{
					grid->Add(new wxStaticText(parent, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
					grid->Add(control, 1, wxEXPAND);
				};
				auto* required = new wxFlexGridSizer(2, 8, 8);
				required->AddGrowableCol(1, 1);
				m_name = new wxTextCtrl(this, wxID_ANY, L"Untitled preset");
				m_author = new wxTextCtrl(this, wxID_ANY, defaultAuthor);
				addField(this, required, L"Name", m_name);
				addField(this, required, L"Author", m_author);
				root->Add(required, 0, wxEXPAND | wxALL, 10);

				m_options = new wxCollapsiblePane(this, wxID_ANY, L"Optional settings");
				auto* options = m_options->GetPane();
				auto* optionalSizer = new wxBoxSizer(wxVERTICAL);
				auto* details = new wxFlexGridSizer(2, 8, 8);
				details->AddGrowableCol(1, 1);
				m_description = new wxTextCtrl(options, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(-1, 70)), wxTE_MULTILINE);
				m_authorHomepage = new wxTextCtrl(options, wxID_ANY, defaultHomepage);
				m_authorHomepage->SetHint(L"https://example.com");
				addField(options, details, L"Description", m_description);
				addField(options, details, L"Author homepage", m_authorHomepage);
				optionalSizer->Add(details, 0, wxEXPAND | wxALL, 10);

				m_includeAccentColor = new wxCheckBox(options, wxID_ANY, L"Include accent color");
				m_includeAccentColor->SetValue(includeAccentColor);
				m_includeAccentColor->SetToolTip(L"Checked: apply this RGB in manual mode. Unchecked: use automatic accent color from the wallpaper.");
				optionalSizer->Add(m_includeAccentColor, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
				auto* licenseHeader = new wxBoxSizer(wxHORIZONTAL);
				m_includeLicense = new wxCheckBox(options, wxID_ANY, L"Include image asset LICENSE");
				m_includeLicense->SetValue(includeLicense);
				licenseHeader->Add(m_includeLicense, 0, wxALIGN_CENTER_VERTICAL);
				licenseHeader->AddStretchSpacer();
				m_loadLicense = new wxButton(options, wxID_ANY, L"Load from file...");
				licenseHeader->Add(m_loadLicense, 0);
				optionalSizer->Add(licenseHeader, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
				m_detectedLicense = new wxStaticText(options, wxID_ANY, L"Detected license: Custom license");
				optionalSizer->Add(m_detectedLicense, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
				auto* licenseScope = new wxStaticText(options, wxID_ANY,
					L"New presets: LICENSE covers image assets only (textures, materials, reflections and atlases). Configuration, accent RGB and layout use OpenGlass Attribution v1: copy, modify and share with author/source attribution. Legacy derivatives retain their original package licensing. Known-source notices and inherited terms are retained automatically, including when updating another preset.");
				licenseScope->Wrap(FromDIP(570));
				optionalSizer->Add(licenseScope, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);
				m_licenseText = new wxTextCtrl(options, wxID_ANY, licenseText, wxDefaultPosition, FromDIP(wxSize(-1, 120)), wxTE_MULTILINE | wxTE_RICH2);
				optionalSizer->Add(m_licenseText, 1, wxEXPAND | wxALL, 10);
				options->SetSizer(optionalSizer);
				m_options->Collapse(true);
				root->Add(m_options, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

				root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 10);
				SetSizerAndFit(root);
				m_name->SetFocus();
				m_loadLicense->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
				{
					wxFileDialog dialog(this, L"Select LICENSE text", wxEmptyString, wxEmptyString, L"Text files (*.txt;LICENSE)|*.txt;LICENSE|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
					if (dialog.ShowModal() != wxID_OK) return;
					wxFFileInputStream input(dialog.GetPath());
					if (!input.IsOk()) return;
					wxStringOutputStream output;
					input.Read(output);
					m_licenseText->SetValue(output.GetString());
				});
				auto updateLicenseState = [this]()
				{
					const bool included = m_includeLicense->GetValue();
					m_author->SetToolTip(included ? L"Declared rights holder for the supplied LICENSE; not verified by OpenGlass." : L"Reference source. Without LICENSE this field does not declare ownership.");
					m_loadLicense->Enable(included );
					m_licenseText->Enable(included );
					const auto name = included
						? PresetPackages::InferLicenseName(m_licenseText->GetValue().ToStdString(wxConvUTF8))
						: std::wstring{};
					m_detectedLicense->SetLabel(included
						? wxString(L"Detected license: ") + wxString(name.empty() ? L"(enter or load LICENSE text)" : name)
						: wxString(L"No additional LICENSE text; inherited terms are retained"));
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
						|| m_author->GetValue().Trim().empty())
					{
						wxMessageBox(L"Name and author are required.", L"New preset", wxOK | wxICON_ERROR, this);
						return;
					}
					if (m_name->GetValue().length() > 128
						|| m_description->GetValue().length() > 4096
						|| m_author->GetValue().length() > 256)
					{
						wxMessageBox(L"Name may contain at most 128 characters, description 4096, and author 256.", L"New preset", wxOK | wxICON_ERROR, this);
						return;
					}
					if (!m_authorHomepage->GetValue().empty() && !PresetPackages::IsValidHomepageUrl(m_authorHomepage->GetValue().ToStdWstring()))
					{
						wxMessageBox(L"Author homepage must be a complete absolute http:// or https:// URL, for example https://example.com.", L"New preset", wxOK | wxICON_ERROR, this);
						m_options->Expand();
						m_authorHomepage->SetFocus();
						m_authorHomepage->SelectAll();
						return;
					}
					if (m_includeLicense->GetValue() && m_licenseText->GetValue().Trim().empty())
					{
						m_options->Expand();
						wxMessageBox(L"Enter or load LICENSE text, or clear Include LICENSE.", L"New preset", wxOK | wxICON_ERROR, this);
						m_licenseText->SetFocus();
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

			void SetSource(const PresetPackages::Package& package)
			{
				m_name->SetValue(package.metadata.name);
				m_description->SetValue(package.metadata.description);

			}

			bool IncludeAccentColor() const { return m_includeAccentColor->GetValue(); }



		private:
			wxButton* m_loadLicense{};
			wxCollapsiblePane* m_options{};
			wxTextCtrl* m_name{};
			wxTextCtrl* m_description{};
			wxTextCtrl* m_author{};
			wxTextCtrl* m_authorHomepage{};
			wxCheckBox* m_includeLicense{};
			wxStaticText* m_detectedLicense{};
			wxTextCtrl* m_licenseText{};
			wxCheckBox* m_includeAccentColor{};
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
		FollowSystemColours(panel, wxSYS_COLOUR_WINDOW);
		auto* root = new wxBoxSizer(wxVERTICAL);
		m_lstPresetPackages = new wxListView(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
		const wchar_t* headings[]{ L"Name", L"Accent color", L"Author", L"License", L"Status" };
		const int widths[]{ 180, 90, 100, 100, 80 };
		for (int column = 0; column < 5; ++column) m_lstPresetPackages->InsertColumn(column, headings[column], wxLIST_FORMAT_LEFT, FromDIP(widths[column]));
		ListView_SetExtendedListViewStyleEx(reinterpret_cast<HWND>(m_lstPresetPackages->GetHandle()), LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		root->Add(m_lstPresetPackages, 1, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 8);
		root->Add(new wxStaticText(panel, wxID_ANY, wxString::Format(L"Apply to %s. Double-click to preview; Save accepts and Revert restores your checkpoint.", m_editScope == Settings::Scope::User ? L"HKCU" : L"HKLM")), 0, wxEXPAND | wxALL, 8);
		m_lblPresetEmpty = new wxStaticText(panel, wxID_ANY, L"Right-click to import or create a preset. You can also drop ZIP files here.");
		root->Add(m_lblPresetEmpty, 0, wxEXPAND | wxALL, 8);
		panel->SetSizer(root); m_notebook->AddPage(panel, L"Preset library");
		m_lstPresetPackages->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { ApplySelectedPresetPackage(); });
		m_lstPresetPackages->Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent& event) { ShowPresetContextMenu(event.GetPosition()); });
		m_lstPresetPackages->Bind(wxEVT_LIST_COL_CLICK, [this](wxListEvent& event)
		{
			const auto selection = m_lstPresetPackages->GetFirstSelected();
			const auto id = selection == wxNOT_FOUND ? std::string{} : m_presetPackages[selection].libraryId;
			if (m_presetSortColumn == event.GetColumn()) m_presetSortAscending = !m_presetSortAscending;
			else { m_presetSortColumn = event.GetColumn(); m_presetSortAscending = true; }
			RebuildPresetPackageList(id);
		});
		auto clear = [this]() { const auto row = m_lstPresetPackages->GetFirstSelected(); if (row != wxNOT_FOUND) m_lstPresetPackages->Select(row, false); };
		m_lstPresetPackages->Bind(wxEVT_LEFT_DOWN, [this, clear](wxMouseEvent& event)
		{
			int flags{}; if (m_lstPresetPackages->HitTest(event.GetPosition(), flags) == wxNOT_FOUND) clear(); event.Skip();
		});
		m_lstPresetPackages->Bind(wxEVT_CHAR_HOOK, [this, clear](wxKeyEvent& event)
		{
			if (event.GetKeyCode() == WXK_ESCAPE) clear();
			else if (event.GetKeyCode() == WXK_DELETE) RemoveSelectedPresetPackage();
			else if (event.GetKeyCode() == WXK_F5) RefreshPresetPackages();
			else event.Skip(); // Native activation and keyboard context-menu events run once.
		});
		DragAcceptFiles(true); EnableElevatedFileDrop(reinterpret_cast<HWND>(GetHandle()));
		Bind(wxEVT_DROP_FILES, [this](wxDropFilesEvent& event) { ImportDroppedPresetPackages(event); });
		// Appearance changes affect thumbnails, not rows, selection or scrolling.
		auto refresh = [this]() { RebuildPresetPackageImages(); };
		panel->Bind(wxEVT_DPI_CHANGED, [this, refresh](wxDPIChangedEvent& event) { event.Skip(); CallAfter(refresh); });
		panel->Bind(wxEVT_SYS_COLOUR_CHANGED, [this, refresh](wxSysColourChangedEvent& event) { event.Skip(); CallAfter(refresh); });
		RefreshPresetPackages();
	}

	void MainFrame::ShowPresetContextMenu(wxPoint screenPosition)
	{
		if (screenPosition != wxDefaultPosition)
		{
			int flags{};
			const auto row = m_lstPresetPackages->HitTest(m_lstPresetPackages->ScreenToClient(screenPosition), flags);
			const auto previous = m_lstPresetPackages->GetFirstSelected();
			if (previous != wxNOT_FOUND) m_lstPresetPackages->Select(previous, false);
			if (row != wxNOT_FOUND) SelectPresetPackageRow(static_cast<std::size_t>(row));
		}
		const auto selection = m_lstPresetPackages->GetFirstSelected();
		const bool selected = selection != wxNOT_FOUND;
		enum { Apply = wxID_HIGHEST + 430, Update, Export, Properties, Delete, Import, Refresh, NewPreset };
		wxMenu menu;
		if (selected)
		{
			menu.Append(Apply, L"Apply"); menu.Append(Update, m_isDirty ? L"Update... (Save or Revert first)" : L"Update..."); menu.Append(Export, L"Export...");
			menu.Append(Delete, L"Delete..."); menu.AppendSeparator(); menu.Append(Properties, L"Properties...");
			const auto& package = m_presetPackages[selection];
			menu.Enable(Apply, package.loadError.empty() && package.trusted);
			SetMenuDefaultItem(reinterpret_cast<HMENU>(menu.GetHMenu()), Apply, FALSE);
			menu.Enable(Update, !m_isDirty && package.loadError.empty());
			menu.Enable(Export, package.loadError.empty() && package.trusted);
			menu.SetHelpString(Update, L"Capture saved effective settings. Save or Revert pending changes first.");
		}
		else
		{
			menu.Append(NewPreset, m_isDirty ? L"New preset... (Save or Revert first)" : L"New preset...");
			menu.Enable(NewPreset, !m_isDirty); menu.SetHelpString(NewPreset, L"Save or Revert pending changes first.");
			menu.Append(Import, L"Import..."); menu.Append(Refresh, L"Refresh");
		}
		const auto point = screenPosition == wxDefaultPosition ? wxDefaultPosition : m_lstPresetPackages->ScreenToClient(screenPosition);
		switch (m_lstPresetPackages->GetPopupMenuSelectionFromUser(menu, point))
		{
		case Apply: ApplySelectedPresetPackage(); break;
		case Update: CreatePresetPackage(true); break;
		case NewPreset: CreatePresetPackage(false); break;
		case Export: ExportSelectedPresetPackage(); break;
		case Properties: UpdatePresetPackageDetails(); break;
		case Delete: RemoveSelectedPresetPackage(); break;
		case Import: ImportPresetPackage(); break;
		case Refresh: RefreshPresetPackages(); break;
		}
	}

	void MainFrame::RefreshPresetPackages()
	{
		if (!m_lstPresetPackages) return;
		const auto selected = m_lstPresetPackages->GetFirstSelected();
		const auto id = selected == wxNOT_FOUND ? std::string{} : m_presetPackages[selected].libraryId;
		try
		{
			std::vector<std::wstring> maintenance;
			m_presetPackages = PresetPackages::EnumerateInstalled(PresetPackages::GetPresetRoot(), &maintenance);
			if (!maintenance.empty())
			{
				std::wstring message; for (const auto& item : maintenance) message += item + L"\n";
				wxMessageBox(message + L"Refresh retries cleanup; disk space has not yet been released.", L"Preset library maintenance", wxOK | wxICON_WARNING, this);
			}
		}
		catch (...)
		{
			const auto error = wil::ResultFromCaughtException();
			m_presetPackages.clear();
			wxMessageBox(wxString::Format(L"Installed preset packages could not be enumerated (HRESULT 0x%08lX).", static_cast<unsigned long>(error)), L"OpenGlass presets", wxOK | wxICON_ERROR, this);
		}
		RebuildPresetPackageList(id);
	}

	void MainFrame::RebuildPresetPackageList(std::string_view selectedId)
	{
		if (!m_lstPresetPackages) return;
		const std::string selectionId(selectedId);
		auto field = [](const auto& package, int column) -> std::wstring
		{
			switch (column)
			{
			case 1: return package.accentColor ? std::format(L"#{:06X}", *package.accentColor) : L"Automatic";
			case 2: return package.metadata.authorName;
			case 3: return package.licenseText.empty() ? L"Not provided" : package.metadata.licenseName;
			case 4: return !package.loadError.empty() ? package.loadError : !package.trusted ? L"Import to trust"
				: L"";
			default: return package.metadata.name;
			}
		};
		std::ranges::stable_sort(m_presetPackages, [&](const auto& a, const auto& b)
		{
			const auto left = field(a, m_presetSortColumn), right = field(b, m_presetSortColumn);
			const int result = CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE);
			return result == CSTR_EQUAL ? a.libraryId < b.libraryId : m_presetSortAscending ? result == CSTR_LESS_THAN : result == CSTR_GREATER_THAN;
		});
		m_lstPresetPackages->DeleteAllItems();
		RebuildPresetPackageImages();
		for (std::size_t index = 0; index < m_presetPackages.size(); ++index)
		{
			const auto& package = m_presetPackages[index];
			const auto row = static_cast<long>(index);
			m_lstPresetPackages->InsertItem(row, field(package, 0), static_cast<int>(index));
			for (int column = 1; column < 5; ++column) m_lstPresetPackages->SetItem(row, column, field(package, column));
			if (package.libraryId == selectionId) SelectPresetPackageRow(index);
		}
		m_lblPresetEmpty->Show(m_presetPackages.empty());
		m_lstPresetPackages->GetParent()->Layout();
	}

	void MainFrame::RebuildPresetPackageImages()
	{
		// A 24-DIP image slot gives report rows breathing room around 16-DIP swatches.
		const PackagePreviewMetrics metrics{ 24, 4 };
		const auto size = m_lstPresetPackages->FromDIP(metrics.canvasDip);
		auto images = std::make_unique<wxImageList>(size, size, true);
		for (const auto& package : m_presetPackages) images->Add(CreatePackagePreviewBitmap(m_lstPresetPackages, package, metrics));
		m_lstPresetPackages->AssignImageList(images.release(), wxIMAGE_LIST_SMALL);
		m_lstPresetPackages->Refresh(false);
	}

	void MainFrame::SelectPresetPackageRow(std::size_t row)
	{
		if (!m_lstPresetPackages || row >= m_presetPackages.size()) return;
		m_lstPresetPackages->Select(static_cast<long>(row));
		m_lstPresetPackages->Focus(static_cast<long>(row));
	}

	void MainFrame::UpdatePresetPackageDetails()
	{
		const auto row = m_lstPresetPackages->GetFirstSelected();
		if (row == wxNOT_FOUND) return;
		try
		{
			const auto& selected = m_presetPackages[row];
			if (!selected.loadError.empty()) { wxMessageBox(selected.loadError, L"Preset properties", wxOK | wxICON_ERROR, this); return; }
			auto package = PresetPackages::LoadLibraryEntry(selected);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), package.digest != selected.digest);

			if (!package.loadError.empty()) { wxMessageBox(package.loadError, L"Preset properties", wxOK | wxICON_ERROR, this); return; }
			const auto action = ShowPresetPreview(this, package, *m_config, !package.trusted, *m_userConfig, *m_systemConfig, m_systemTransparency);
			if (!package.trusted && action == PresetPreviewAction::ImportOnly)
			{
				const auto verified = PresetPackages::LoadLibraryEntry(package);
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), verified.digest != package.digest);
				(void)PresetPackages::Publish(verified, "imported", package.libraryId, PresetPackages::GetPresetRoot(), package.digest);
				RefreshPresetPackages();
			}
		}
		catch (...) { wxMessageBox(L"The preset could not be read. Refresh the library.", L"Preset properties", wxOK | wxICON_ERROR, this); }
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
		std::size_t imported{};
		try
		{
			std::vector<PresetPackages::Package> packages;
			for (const auto& path : paths)
			{
				auto package = PresetPackages::LoadArchive(path);
				package.assets.clear(); // Load one package's images on demand during review.
				packages.push_back(std::move(package));
			}
			if (!ShowBatchImportPreview(this, packages, *m_config, *m_userConfig, *m_systemConfig, m_systemTransparency)) return;
			for (const auto& package : packages)
			{
				auto verified = PresetPackages::LoadArchive(package.source);
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), verified.digest != package.digest);
				(void)PresetPackages::Publish(verified, "imported");
				++imported;
			}
		}
		catch (...)
		{
			wxMessageBox(wxString::Format(L"Import stopped (0x%08lX). %zu completed imports remain in the library. The configuration was not changed.",
				wil::ResultFromCaughtException(), imported), L"Preset import", wxOK | wxICON_ERROR, this);
		}
		RefreshPresetPackages();
	}

	void MainFrame::ImportPresetPackage(const std::filesystem::path& path)
	{
		try
		{
			const auto reviewed = PresetPackages::LoadArchive(path);
			const auto action = ShowPresetPreview(this, reviewed, *m_config, true, *m_userConfig, *m_systemConfig, m_systemTransparency);
			if (action == PresetPreviewAction::Cancel) return;
			const auto verified = PresetPackages::LoadArchive(path);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), verified.digest != reviewed.digest);
			const auto saved = PresetPackages::Publish(verified, "imported");

			RefreshPresetPackages();
			RebuildPresetPackageList(saved.libraryId);
		}
		catch (...)
		{
			wxMessageBox(wxString::Format(L"The preset could not be imported (0x%08lX).", wil::ResultFromCaughtException()),
				L"Preset import", wxOK | wxICON_ERROR, this);
		}
	}

	void MainFrame::ApplySelectedPresetPackage()
	{
		const long selection = m_lstPresetPackages->GetFirstSelected();
		if (selection == wxNOT_FOUND || static_cast<std::size_t>(selection) >= m_presetPackages.size()) return;
		const auto selected = m_presetPackages[selection];
		if (!selected.trusted || !selected.loadError.empty())
		{
			wxMessageBox(L"This content is not accepted. Refresh, then open Properties to validate and trust it before applying.", L"Preset apply", wxOK | wxICON_INFORMATION, this);
			return;
		}
		ApplyPresetPackage(selected);
	}

	void MainFrame::ApplyPresetPackage(const PresetPackages::Package& inputPackage)
	{
		try
		{
			auto package = PresetPackages::LoadTrusted(inputPackage);
			const auto prepared = m_resources.Prepare(package, m_editScope);
			const auto model = PackageModel(package, m_resources, m_editScope);
			const auto plan = EffectiveConfiguration::Plan(EffectiveConfiguration::Read(*m_userConfig, m_systemTransparency), EffectiveConfiguration::Read(*m_systemConfig), model, m_editScope, package.catalogVersion, m_systemTransparency);
			bool restartRequired{};
			for (const auto& change : plan)
				restartRequired |= Settings::Get(change.id).impact == Settings::UpdateType::RestartRequired;
			const bool applied = RunPreview([&]
			{
				for (const auto& change : plan)
				{
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), GetConfigForScope(change.scope)->ReadRaw(change.Name()) != change.before);
				}
				THROW_IF_FAILED(m_userConfig->CheckDeleteAccess());
				THROW_IF_FAILED(m_systemConfig->CheckDeleteAccess());
				ApplyAccentColor(package.accentColor);
				m_resources.Install(*prepared);
				for (const auto& change : plan)
				{
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_RETRY), GetConfigForScope(change.scope)->ReadRaw(change.Name()) != change.before);
					TrackSettingChange(change.scope, change.id);
					THROW_IF_FAILED(GetConfigForScope(change.scope)->WriteRaw(change.Name(), change.after));
				}
			});
			if (applied)
			{
				LoadSettings();
				m_presetProvenance.SetOrigin(inputPackage, m_isDirty);
				RebuildPresetPackageList(package.libraryId);
			}
			if (applied && restartRequired) wxMessageBox(L"Some settings take effect after restarting DWM or signing out. DWM has not been restarted.", L"Preset applied", wxOK | wxICON_INFORMATION, this);
		}
		catch (...)
		{
			wxMessageBox(wxString::Format(L"The preset could not be prepared (0x%08lX).", wil::ResultFromCaughtException()),
				L"Preset apply", wxOK | wxICON_ERROR, this);
		}
	}

	void MainFrame::CaptureEffectivePreset(PresetPackages::CreateRequest& request, bool accentColor, const PresetPackages::Package* localSource)
	{
		const auto model = EffectiveConfiguration::Capture(EffectiveConfiguration::Read(*m_userConfig, m_systemTransparency), EffectiveConfiguration::Read(*m_systemConfig), Settings::CatalogVersion, m_systemTransparency);
		for (const auto& [id, value] : model)
		{
			const auto& spec = Settings::Get(id);
			if (const auto number = std::get_if<DWORD>(&value)) request.settings.emplace(id, *number);
			else if (const auto path = std::get_if<std::wstring>(&value); path && !path->empty())
			{
				const std::filesystem::path assetSource(*path);
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !std::filesystem::is_regular_file(assetSource));

				m_resources.PreserveSource(request, assetSource, localSource);
				const auto assetName = AssetName(spec.assetRole, assetSource);
				request.assetSources.emplace(assetName, assetSource);
				request.settings.emplace(id, PresetPackages::AssetReference{ assetName });
				if (spec.assetRole == Settings::AssetRole::ThemeAtlas)
				{
					const std::filesystem::path layout(*path + L".layout");
					if (std::filesystem::is_regular_file(layout)) request.assetSources.emplace(assetName + ".layout", layout);
				}
			}
			else request.settings.emplace(id, std::monostate{});
		}
		if (accentColor)
		{
			DWORD rgb{};
			THROW_IF_FAILED(m_colorPreference.ReadRgb(m_targetUserSid.ToStdWstring(), rgb));
			request.accentColor = rgb;
		}
	}

	void MainFrame::CreatePresetPackage(bool update)
	{
		if (m_isDirty)
		{
			wxMessageBox(L"Save or Revert pending changes before creating or updating a preset.", L"New preset", wxOK | wxICON_INFORMATION, this);
			return;
		}
		wxString defaultAuthor = m_lastPresetAuthorName;
		const auto origin = m_presetProvenance.Origin();
		std::optional<PresetPackages::Package> source = origin;
		if (update)
		{
			const auto selection = m_lstPresetPackages->GetFirstSelected();
			if (selection == wxNOT_FOUND || static_cast<std::size_t>(selection) >= m_presetPackages.size()) return;
			source = m_presetPackages.at(selection);
		}
		if (!source && defaultAuthor.empty())
		{
			defaultAuthor = ResolveAccountName(m_targetUserSid.ToStdWstring(), false);
		}

		bool autoColorization{};
		const auto colorModeResult = m_colorPreference.ReadAutoColorization(m_targetUserSid.ToStdWstring(), autoColorization);
		if (FAILED(colorModeResult))
		{
			wxMessageBox(wxString::Format(L"Could not read the original user's automatic accent color setting (0x%08lX).", colorModeResult),
				L"New preset", wxOK | wxICON_ERROR, this);
			return;
		}
		CreatePresetDialog dialog(this, source ? wxString(source->metadata.authorName) : defaultAuthor,
			source ? wxString(source->metadata.authorHomepage) : wxString(m_lastPresetAuthorHomepage),
			source ? !source->licenseText.empty() : m_lastPresetIncludeLicense,
			wxString::FromUTF8(source ? source->licenseText : m_lastPresetLicenseText), !autoColorization);
		if (update) dialog.SetTitle(L"Update preset");
		if (source) dialog.SetSource(*source);
		if (dialog.ShowModal() != wxID_OK) return;
		PresetPackages::CreateRequest request;
		request.metadata = dialog.Metadata();
		request.metadata.uuid = PresetPackages::GeneratePackageUuid();
		request.licenseText = dialog.LicenseText();
		PresetPackages::PreserveRevisionProvenance(request, source ? &*source : nullptr, origin ? &*origin : nullptr);
		m_lastPresetAuthorName = request.metadata.authorName;
		m_lastPresetAuthorHomepage = request.metadata.authorHomepage;
		m_lastPresetIncludeLicense = dialog.IncludeLicense();
		m_lastPresetLicenseText = dialog.LicenseEditorText();

		try
		{
			CaptureEffectivePreset(request, dialog.IncludeAccentColor(), source ? &*source : nullptr);
			auto snapshot = PresetPackages::CreateSnapshot(std::move(request));
			const auto saved = PresetPackages::Publish(snapshot, "local", update ? source->libraryId : "", PresetPackages::GetPresetRoot(), update ? source->digest : "");
			m_presetProvenance.SetOrigin(saved, false);
			RefreshPresetPackages();
			RebuildPresetPackageList(saved.libraryId);
		}
		catch (...)
		{
			wxMessageBox(wxString::Format(L"The preset could not be saved (0x%08lX). The saved entry has not been replaced.", wil::ResultFromCaughtException()),
				L"New preset", wxOK | wxICON_ERROR, this);
		}
	}

	void MainFrame::ExportSelectedPresetPackage()
	{
		const auto selection = m_lstPresetPackages->GetFirstSelected();
		if (selection == wxNOT_FOUND || static_cast<std::size_t>(selection) >= m_presetPackages.size()) return;
		const auto& package = m_presetPackages.at(selection);
		wxFileDialog save(this, L"Export saved preset", wxEmptyString,
			SanitizePackageFileName(package.metadata.name) + L"-" + wxString::FromUTF8(package.metadata.uuid) + L".zip",
			L"ZIP archives (*.zip)|*.zip", wxFD_SAVE);
		if (save.ShowModal() != wxID_OK) return;
		try { PresetPackages::ExportArchive(save.GetPath().ToStdWstring(), PresetPackages::LoadTrusted(package)); }
		catch (...)
		{
			wxMessageBox(wxString::Format(L"Export failed (0x%08lX). Choose a new file name if the destination already exists.", wil::ResultFromCaughtException()),
				L"Export preset", wxOK | wxICON_ERROR, this);
		}
	}

	void MainFrame::RemoveSelectedPresetPackage()
	{
		const long selection = m_lstPresetPackages->GetFirstSelected();
		if (selection == wxNOT_FOUND || static_cast<std::size_t>(selection) >= m_presetPackages.size()) return;
		try
		{
			const auto& package = m_presetPackages[selection];
			if (wxMessageBox(L"Delete this preset and its library files? Current configuration and Revert keep their independent resources.", L"Delete preset", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES) return;
			PresetPackages::RemoveLibraryEntry(package.libraryId);
			RefreshPresetPackages();
		}
		catch (...)
		{
			const auto error = wil::ResultFromCaughtException();
			wxMessageBox(wxString::Format(L"Deletion or cleanup is incomplete (HRESULT 0x%08lX). Refresh the library to retry cleanup.", static_cast<unsigned long>(error)), L"Delete preset", wxOK | wxICON_ERROR, this);
		}
	}
}
