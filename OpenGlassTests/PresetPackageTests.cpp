#include "pch.h"
#include "TestHarness.hpp"
#include "../OpenGlassGUI/PresetPackage.hpp"
#include "../OpenGlassGUI/ManagedFiles.hpp"
#include "../OpenGlassGUI/ColorizationPresets.hpp"
#include "PngFixture.hpp"
#include <wx/init.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>
#include <wx/log.h>
#include <nlohmann/json.hpp>
#include <fstream>

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestPresetProvenance()
	{
		PresetPackages::PreviewProvenance state;
		PresetPackages::Package a, b, c;
		a.metadata.uuid = "A"; a.libraryId = "entry-a"; a.digest = "A"; a.metadata.authorName = L"Author A"; a.sourceType = "local";
		b.metadata.uuid = "B"; b.libraryId = "entry-b"; b.digest = "B"; b.metadata.authorName = L"Author B"; b.sourceType = "imported";
		c.metadata.uuid = "C"; c.libraryId = "entry-c"; c.digest = "C"; c.sourceType = "local-copy";
		Check(!state.Origin());
		state.SetOrigin(a, true); state.Revert(); Check(!state.Origin());
		state.SetOrigin(a, false);
		state.SetOrigin(b, true); state.SetOrigin(c, true); state.Revert();
		Check(state.Origin()->metadata.authorName == L"Author A");
		a.metadata.authorName = L"Updated author A";
		state.SetOrigin(a, false); // A saved snapshot retains known notices.
		state.SetOrigin(b, true); state.Revert();
		Check(state.Origin()->metadata.authorName == L"Updated author A");
		state.SetOrigin(b, true); state.Accept(); state.SetOrigin(c, true); state.Revert();
		Check(state.Origin()->metadata.authorName == L"Author B");
		Check(!PresetPackages::HasInheritedMetadata(a));
		Check(PresetPackages::HasInheritedMetadata(b) && PresetPackages::HasInheritedMetadata(c));
		a.attribution = { L"Known source" }; Check(!PresetPackages::HasInheritedMetadata(a));
		a.legacyLicense = true; Check(PresetPackages::HasInheritedMetadata(a));
		a.legacyLicense = false; a.digest = "digest-A"; a.assets["assets/reflection.png"] = { std::byte{1} };

		b.digest = "digest-B"; b.attribution = { L"Earlier source" }; b.licenseText = "B image terms";
		PresetPackages::CreateRequest update;
		update.metadata = a.metadata;
		update.licenseText = "Edited local terms";
		PresetPackages::PreserveRevisionProvenance(update, &a, &b);
		Check(update.metadata.authorName == a.metadata.authorName);
		Check(std::ranges::find(update.attribution, L"Earlier source") != update.attribution.end());
		Check(std::ranges::any_of(update.attribution, [](const auto& value) { return value.find(L"Author B") != std::wstring::npos; }));
		Check(update.licenseText == "Edited local terms" && update.inheritedLicenses.empty()); // Config-only source does not import image terms.
		const auto notices = update.attribution;
		PresetPackages::PreserveSource(update, b, false);
		Check(update.attribution == notices); // Repeated capture is deduplicated.
		PresetPackages::PreserveSource(update, b, true);
		Check(update.licenseText == "Edited local terms\n\nB image terms");
		Check(update.inheritedLicenses == std::vector<std::string>{ "B image terms" });
		b.legacyLicense = true;
		update.licenseText = "Edited local terms"; update.inheritedLicenses.clear();
		PresetPackages::PreserveRevisionProvenance(update, &a, &b);
		Check(update.legacyLicense && update.licenseText == "Edited local terms\n\nB image terms"); // Legacy package terms also cover configuration.

	}

	void TestPresetPackageRoundTrip()
	{
		auto createArchive = [](const std::filesystem::path& path, const PresetPackages::CreateRequest& request)
		{
			PresetPackages::ExportArchive(path, PresetPackages::CreateSnapshot(request));
		};
		Check(PresetPackages::IsValidHomepageUrl(L"https://example.com"));
		Check(PresetPackages::IsValidHomepageUrl(L"http://example.com/author"));
		Check(!PresetPackages::IsValidHomepageUrl(L"https://"));
		Check(!PresetPackages::IsValidHomepageUrl(L"example.com"));
		Check(!PresetPackages::IsValidHomepageUrl(L"file:///C:/preset"));

		wxInitializer initializer;
		Check(initializer.IsOk());
		if (!initializer.IsOk()) return;
		wxLogNull suppressExpectedArchiveErrors;

		const auto generatedUuid = PresetPackages::GeneratePackageUuid();
		const std::wstring generatedUuidWide(generatedUuid.begin(), generatedUuid.end());
		const auto directory = std::filesystem::temp_directory_path() / (L"OpenGlassPresetTests-" + generatedUuidWide);
		std::filesystem::create_directories(directory);
		auto cleanup = wil::scope_exit([&]
		{
			for (const auto& path : std::filesystem::recursive_directory_iterator(directory)) SetFileAttributesW(path.path().c_str(), FILE_ATTRIBUTE_NORMAL);
			std::error_code error;
			std::filesystem::remove_all(directory, error);
		});

		PresetPackages::CreateRequest request;
		request.metadata = {
			"00112233-4455-6677-8899-aabbccddeeff",
			L"Round trip",
			L"Preset package test",
			L"OpenGlass tests",
			L"https://example.com/author",
			L"MIT"
		};
		request.accentColor = 0x74B8FC;
		request.licenseText = "MIT License\n\nPermission is granted for this test package.\n";
		for (const auto& spec : Settings::Catalog)
		{
			if (Settings::IsPresetPackSetting(spec)) request.settings.emplace(spec.id, std::monostate{});
		}

		const auto first = directory / L"first.zip";
		const auto second = directory / L"second.zip";
		createArchive(first, request);
		createArchive(second, request);
		auto readFile = [](const std::filesystem::path& path)
		{
			std::ifstream stream(path, std::ios::binary);
			return std::vector<char>((std::istreambuf_iterator<char>(stream)), {});
		};
		Check(readFile(first) == readFile(second));
		Check((GetFileAttributesW(first.c_str()) & FILE_ATTRIBUTE_READONLY) != 0);

		const auto loaded = PresetPackages::LoadArchive(first);
		Check(nlohmann::json::parse(loaded.manifestText).at("schema_version") == 3);
		const auto settings = nlohmann::json::parse(loaded.manifestText).at("settings");
		Check(settings.contains("ColorizationOpaqueBlend") && !settings.contains("EnableTransparency"));
		Check(!loaded.legacyLicense);
		auto noHomepage = request; noHomepage.metadata.authorHomepage.clear();
		Check(PresetPackages::CreateSnapshot(noHomepage).metadata.authorHomepage.empty());
		Check(loaded.accentColor == request.accentColor);
		Check(loaded.metadata.uuid == request.metadata.uuid);
		Check(loaded.metadata.name == request.metadata.name);
		Check(loaded.metadata.authorHomepage == request.metadata.authorHomepage);
		Check(loaded.metadata.licenseName == request.metadata.licenseName);
		Check(loaded.settings.size() == Settings::PresetPackSettingCount());
		Check(loaded.catalogVersion == Settings::CatalogVersion);
		Check(loaded.assets.empty());
		Check(!loaded.digest.empty());
		Check(PresetPackages::InferLicenseName("SPDX-License-Identifier: Apache-2.0\n") == L"Apache-2.0");
		Check(PresetPackages::InferLicenseName("GNU GENERAL PUBLIC LICENSE\nVersion 3, 29 June 2007\n") == L"GPL-3.0");
		Check(PresetPackages::InferLicenseName(
			"GNU GENERAL PUBLIC LICENSE\nVersion 3, 29 June 2007\n"
			+ std::string(5000, 'x')
			+ "GNU Affero General Public License\n"
		) == L"GPL-3.0");
		Check(PresetPackages::InferLicenseName("GNU AFFERO GENERAL PUBLIC LICENSE\nVersion 3, 19 November 2007\n") == L"AGPL-3.0");
		Check(PresetPackages::InferLicenseName("Terms for this package.\n") == L"Custom license");

		auto unlicensedRequest = request;
		unlicensedRequest.metadata.uuid = "10112233-4455-6677-8899-aabbccddeeff";
		unlicensedRequest.metadata.licenseName.clear();
		unlicensedRequest.licenseText.clear();
		const auto unlicensedArchive = directory / L"unlicensed.zip";
		createArchive(unlicensedArchive, unlicensedRequest);
		const auto unlicensed = PresetPackages::LoadArchive(unlicensedArchive);
		Check(unlicensed.licenseText.empty());
		Check(unlicensed.metadata.licenseName.empty());

		auto undescribedRequest = request;
		undescribedRequest.metadata.uuid = "20112233-4455-6677-8899-aabbccddeeff";
		undescribedRequest.metadata.description.clear();
		const auto undescribedArchive = directory / L"undescribed.zip";
		createArchive(undescribedArchive, undescribedRequest);
		const auto undescribed = PresetPackages::LoadArchive(undescribedArchive);
		Check(undescribed.metadata.description.empty());

		const auto& png = ValidPng();
		const auto validPng = directory / L"reflection.png";
		{
			std::ofstream output(validPng, std::ios::binary);
			output.write(reinterpret_cast<const char*>(png.data()), png.size());
		}
		auto assetRequest = request;
		assetRequest.metadata.uuid = "11112233-4455-6677-8899-aabbccddeeff";
		assetRequest.settings[Settings::Id::CustomThemeReflection] = PresetPackages::AssetReference{ "assets/reflection.png" };
		assetRequest.assetSources.emplace("assets/reflection.png", validPng);
		const auto assetArchive = directory / L"asset.zip";
		createArchive(assetArchive, assetRequest);
		const auto loadedAsset = PresetPackages::LoadArchive(assetArchive);
		Check(loadedAsset.assets.contains("assets/reflection.png"));
		Check(std::get<PresetPackages::AssetReference>(loadedAsset.settings.at(Settings::Id::CustomThemeReflection)).path == "assets/reflection.png");
		const auto deployed = directory / L"deployed";
		std::filesystem::create_directories(deployed / L"assets");
		auto writeBytes = [](const std::filesystem::path& path, std::span<const std::byte> bytes)
		{
			std::ofstream output(path, std::ios::binary);
			output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		};
		writeBytes(deployed / L"manifest.json", { reinterpret_cast<const std::byte*>(loadedAsset.manifestText.data()), loadedAsset.manifestText.size() });
		writeBytes(deployed / L"LICENSE", { reinterpret_cast<const std::byte*>(loadedAsset.licenseText.data()), loadedAsset.licenseText.size() });
		for (const auto& [name, bytes] : loadedAsset.assets)
		{
			writeBytes(deployed / wxString::FromUTF8(name).ToStdWstring(), bytes);
		}
		const auto loadedDeployment = PresetPackages::LoadDeployed(deployed);
		Check(loadedDeployment.digest == loadedAsset.digest);
		Check(loadedDeployment.deployed);

		const auto validLayout = directory / L"theme-atlas.png.layout";
		{
			std::ofstream output(validLayout, std::ios::binary);
			output << "RS1Compatibility=1\n12;1;3602=1,2,3,4\nCaptionHeight=22\n";
		}
		auto atlasRequest = request;
		atlasRequest.metadata.uuid = "31112233-4455-6677-8899-aabbccddeeff";
		atlasRequest.settings[Settings::Id::CustomThemeAtlas] = PresetPackages::AssetReference{ "assets/theme-atlas.png" };
		atlasRequest.assetSources.emplace("assets/theme-atlas.png", validPng);
		atlasRequest.assetSources.emplace("assets/theme-atlas.png.layout", validLayout);
		const auto atlasArchive = directory / L"atlas.zip";
		createArchive(atlasArchive, atlasRequest);
		const auto loadedAtlas = PresetPackages::LoadArchive(atlasArchive);
		Check(loadedAtlas.assets.contains("assets/theme-atlas.png.layout"));

		const auto invalidLayout = directory / L"invalid.layout";
		{
			std::ofstream output(invalidLayout, std::ios::binary);
			output << "12;1;3602=1,2,3,4,5\n";
		}
		atlasRequest.metadata.uuid = "41112233-4455-6677-8899-aabbccddeeff";
		atlasRequest.assetSources["assets/theme-atlas.png.layout"] = invalidLayout;
		bool rejectedLayout{};
		try { createArchive(directory / L"bad-layout.zip", atlasRequest); }
		catch (...) { rejectedLayout = true; }
		Check(rejectedLayout);

		bool rejectedOverwrite{};
		try { createArchive(first, request); }
		catch (...) { rejectedOverwrite = true; }
		Check(rejectedOverwrite);

		auto invalid = request;
		invalid.metadata.authorHomepage = L"file:///not-allowed";
		bool rejectedUrl{};
		try { createArchive(directory / L"bad-url.zip", invalid); }
		catch (...) { rejectedUrl = true; }
		Check(rejectedUrl);

		invalid = request;
		invalid.settings.erase(Settings::Id::GlassType);
		bool rejectedIncompleteCatalog{};
		try { createArchive(directory / L"bad-catalog.zip", invalid); }
		catch (...) { rejectedIncompleteCatalog = true; }
		Check(rejectedIncompleteCatalog);

		// A library entry owns only its current version; fixtures never touch ProgramData.
		const auto library = directory / L"library";
		const auto snapshot = PresetPackages::CreateSnapshot(assetRequest);
		const auto originalImage = readFile(validPng);
		{ std::ofstream changed(validPng, std::ios::binary | std::ios::trunc); changed << "changed after capture"; }
		const auto local = PresetPackages::Publish(snapshot, "local", {}, library);
		Check(local.trusted && PresetPackages::LoadTrusted(local, library).assets == snapshot.assets);
		{ std::ofstream restored(validPng, std::ios::binary | std::ios::trunc); restored.write(originalImage.data(), originalImage.size()); }
		Check(!std::filesystem::exists(library / L"library.json"));
		{
			wil::unique_hfile writer(CreateFileW((library / L"library.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
			Check(static_cast<bool>(writer));
			bool busy{}; try { (void)PresetPackages::EnumerateInstalled(library); } catch (...) { busy = true; }
			Check(busy);
		}
		// Interrupted directory replacement restores the old accepted entry; abandoned staging is removed.
		const auto previousEntry = local.source.wstring() + L".previous";
		std::filesystem::rename(local.source, previousEntry);
		const auto abandoned = local.source.wstring() + L".staging";
		std::filesystem::create_directory(abandoned);
		Check(PresetPackages::EnumerateInstalled(library).front().digest == local.digest);
		Check(!std::filesystem::exists(previousEntry) && !std::filesystem::exists(abandoned));
		Check(PresetPackages::EnumerateInstalled(library).size() == 1);
		// A pending replacement backup must never resurrect a deliberately deleted entry.
		const auto deleteLibrary = directory / L"delete-library";
		const auto deleting = PresetPackages::Publish(snapshot, "local", {}, deleteLibrary);
		const std::filesystem::path deletePrevious = deleting.source.wstring() + L".previous";
		std::filesystem::copy(deleting.source, deletePrevious, std::filesystem::copy_options::recursive);
		{
			wil::unique_hfile held(CreateFileW((deletePrevious / L"manifest.json").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
			Check(static_cast<bool>(held));
			bool blocked{};
			try { PresetPackages::RemoveLibraryEntry(deleting.libraryId, deleteLibrary); } catch (...) { blocked = true; }
			Check(blocked && std::filesystem::exists(deleting.source));
			Check(PresetPackages::LoadTrusted(deleting, deleteLibrary).digest == deleting.digest);
		}
		PresetPackages::RemoveLibraryEntry(deleting.libraryId, deleteLibrary);
		Check(!std::filesystem::exists(deletePrevious) && PresetPackages::EnumerateInstalled(deleteLibrary).empty());
		// Cleanup does not require the damaged current entry to validate first.
		const auto damaged = PresetPackages::Publish(snapshot, "local", {}, deleteLibrary);
		const std::filesystem::path damagedPrevious = damaged.source.wstring() + L".previous";
		std::filesystem::copy(damaged.source, damagedPrevious, std::filesystem::copy_options::recursive);
		std::filesystem::remove(damaged.source / L"manifest.json");
		PresetPackages::RemoveLibraryEntry(damaged.libraryId, deleteLibrary);
		Check(!std::filesystem::exists(damagedPrevious) && PresetPackages::EnumerateInstalled(deleteLibrary).empty());
		Check(PresetPackages::Publish(snapshot, "imported", {}, library).libraryId == local.libraryId);
		auto revisedRequest = assetRequest;
		revisedRequest.metadata.uuid = PresetPackages::GeneratePackageUuid();
		revisedRequest.metadata.authorName = L"Updated rights holder";
		revisedRequest.licenseText = "Own replacement terms";
		revisedRequest.settings[Settings::Id::GlassOpacity] = 31u;
		const auto revised = PresetPackages::Publish(PresetPackages::CreateSnapshot(revisedRequest), "local", local.libraryId, library, local.digest);
		Check(revised.libraryId == local.libraryId && revised.source == local.source && revised.digest != local.digest);
		Check(PresetPackages::LoadDeployed(local.source).digest == revised.digest);
		bool stale{};
		try { (void)PresetPackages::Publish(snapshot, "local", local.libraryId, library, local.digest); } catch (...) { stale = true; }
		Check(stale && PresetPackages::LoadTrusted(revised, library).digest == revised.digest);
		PresetPackages::ExportArchive(directory / L"saved-entry.zip", PresetPackages::LoadTrusted(revised, library));
		Check(PresetPackages::LoadArchive(directory / L"saved-entry.zip").digest == revised.digest);
		const auto imported = PresetPackages::Publish(loaded, "imported", {}, library);
		auto importUpdate = request; importUpdate.metadata.uuid = PresetPackages::GeneratePackageUuid(); importUpdate.metadata.authorName = L"Editable name";
		PresetPackages::PreserveRevisionProvenance(importUpdate, &imported, &imported);
		const auto updatedImport = PresetPackages::Publish(PresetPackages::CreateSnapshot(importUpdate), "local", imported.libraryId, library, imported.digest);
		Check(updatedImport.libraryId == imported.libraryId && updatedImport.metadata.authorName == L"Editable name");
		Check(!updatedImport.attribution.empty() && updatedImport.licenseText.find(imported.licenseText) != std::string::npos);
		// Publication failure does not replace the accepted directory.
		{
			wil::unique_hfile held(CreateFileW((revised.source / L"manifest.json").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
			auto candidate = revisedRequest; candidate.metadata.uuid = PresetPackages::GeneratePackageUuid();
			bool blocked{}; try { (void)PresetPackages::Publish(PresetPackages::CreateSnapshot(candidate), "local", revised.libraryId, library, revised.digest); } catch (...) { blocked = true; }
			Check(blocked && PresetPackages::LoadTrusted(revised, library).digest == revised.digest);
		}
		// Changed accepted content is not automatically trusted, even if still valid.
		const auto admission = ManagedFiles::Read(updatedImport.source / L".accepted.json");
		std::filesystem::remove(updatedImport.source / L".accepted.json");
		const std::string changedAdmission = R"({"version":1,"digest":"changed"})";
		ManagedFiles::Write(updatedImport.source / L".accepted.json", { reinterpret_cast<const std::byte*>(changedAdmission.data()), changedAdmission.size() });
		bool mismatch{}; try { (void)PresetPackages::LoadTrusted(updatedImport, library); } catch (...) { mismatch = true; }
		Check(mismatch);
		std::filesystem::remove(updatedImport.source / L".accepted.json");
		ManagedFiles::Write(updatedImport.source / L".accepted.json", admission);
		// Admission cannot be forged by changing package metadata or omitting the local record.
		std::filesystem::remove(updatedImport.source / L".accepted.json");
		bool untrusted{}; try { (void)PresetPackages::LoadTrusted(updatedImport, library); } catch (...) { untrusted = true; }
		Check(untrusted);
		Check(std::ranges::any_of(PresetPackages::EnumerateInstalled(library), [](const auto& entry) { return !entry.trusted; }));
		// Broken entries remain independently diagnosable.
		const auto invalidId = PresetPackages::GeneratePackageUuid();
		std::filesystem::create_directory(library / L"Library" / invalidId);
		Check(std::ranges::any_of(PresetPackages::EnumerateInstalled(library), [](const auto& entry) { return !entry.loadError.empty(); }));
		PresetPackages::RemoveLibraryEntry(invalidId, library);

		// A missing captured source cannot invalidate the already validated snapshot.
		auto detached = snapshot; detached.assetSources["assets/reflection.png"] = directory / L"missing-source.png";
		const auto detachedRoot = directory / L"detached-library";
		Check(PresetPackages::Publish(detached, "local", {}, detachedRoot).trusted);
		// Legacy library migration retains existing paths but never resurrects a deleted new entry.
		const auto oldRoot = directory / L"old-library";
		const auto oldDeployment = oldRoot / snapshot.metadata.uuid;
		ManagedFiles::Directory(oldDeployment);
		auto writeLegacyText = [&](const auto& name, const std::string& text)
		{
			ManagedFiles::Write(oldDeployment / name, { reinterpret_cast<const std::byte*>(text.data()), text.size() });
		};
		writeLegacyText(L"manifest.json", snapshot.manifestText);
		if (!snapshot.licenseText.empty()) writeLegacyText(L"LICENSE", snapshot.licenseText);
		for (const auto& [name, bytes] : snapshot.assets)
		{
			ManagedFiles::Directory((oldDeployment / name).parent_path());
			ManagedFiles::Write(oldDeployment / name, bytes);
		}
		// Older deployments used read-only file attributes as well as ACLs.
		for (const auto& item : std::filesystem::recursive_directory_iterator(oldDeployment))
			if (item.is_regular_file()) Check(SetFileAttributesW(item.path().c_str(), FILE_ATTRIBUTE_READONLY) != FALSE);
		const auto oldId = PresetPackages::GeneratePackageUuid();
		{ std::ofstream file(oldRoot / L"library.json"); file << nlohmann::json{ { "version", 1 }, { "entries", nlohmann::json::array({ { { "id", oldId }, { "revision", snapshot.metadata.uuid }, { "digest", snapshot.digest }, { "source_type", "imported" }, { "source_digest", snapshot.digest } } }) } }.dump(); }
		const auto migrated = PresetPackages::EnumerateInstalled(oldRoot);
		Check(migrated.size() == 1 && migrated.front().libraryId == oldId && migrated.front().trusted);
		Check((GetFileAttributesW((oldDeployment / L"assets/reflection.png").c_str()) & FILE_ATTRIBUTE_READONLY) != 0);
		PresetPackages::RemoveLibraryEntry(oldId, oldRoot);
		Check(PresetPackages::EnumerateInstalled(oldRoot).empty() && std::filesystem::exists(oldDeployment));

		auto writeRawZip = [](const std::filesystem::path& path, const std::vector<std::pair<wxString, std::string>>& entries)
		{
			wxFFileOutputStream output(path.wstring());
			wxZipOutputStream zip(output, 9);
			for (const auto& [name, content] : entries)
			{
				zip.PutNextEntry(name);
				zip.Write(content.data(), content.size());
			}
			zip.Close();
			output.Close();
		};
		auto rejectedArchive = [](const std::filesystem::path& path)
		{
			try { static_cast<void>(PresetPackages::LoadArchive(path)); }
			catch (...) { return true; }
			return false;
		};

		// Unpublished development formats are not compatibility contracts.
		{
			auto unsupported = nlohmann::ordered_json::parse(loaded.manifestText);
			unsupported["schema_version"] = 4;
			const auto path = directory / L"unsupported-schema.zip";
			writeRawZip(path, { { L"manifest.json", unsupported.dump() }, { L"LICENSE", request.licenseText } });
			Check(rejectedArchive(path));
			unsupported["schema_version"] = 3;
			unsupported.erase("rights");
			writeRawZip(path, { { L"manifest.json", unsupported.dump() }, { L"LICENSE", request.licenseText } });
			Check(rejectedArchive(path));
		}

		{
			auto modern = nlohmann::ordered_json::parse(loaded.manifestText);
			Check(!modern["settings"].contains("ColorizationColorOverride") && !modern["settings"].contains("ColorizationAfterglowOverride"));
			modern["settings"]["ColorizationColorOverride"] = 0xAA112233u;
			modern["settings"]["ColorizationAfterglowOverride"] = 0xBB445566u;
			const auto path = directory / L"ignored-color-overrides.zip";
			writeRawZip(path, { { L"manifest.json", modern.dump() }, { L"LICENSE", request.licenseText } });
			const auto ignored = PresetPackages::LoadArchive(path);
			Check(ignored.ignoredSettingCount == 2 && !ignored.settings.contains(Settings::Id::ColorizationColorOverride)
				&& !ignored.settings.contains(Settings::Id::ColorizationAfterglowOverride));
			modern["settings"]["ColorizationColorOverride"] = 0xAA778899u;
			writeRawZip(path, { { L"manifest.json", modern.dump() }, { L"LICENSE", request.licenseText } });
			Check(PresetPackages::LoadArchive(path).digest != ignored.digest); // Ignored input remains authenticated.
		}
		for (const unsigned catalog : { 1u, Settings::CatalogVersion, Settings::CatalogVersion + 1 })
		{
			auto modern = nlohmann::ordered_json::parse(loaded.manifestText);
			modern["catalog_version"] = catalog;
			for (const auto id : { Settings::Id::ColorizationColorBalance, Settings::Id::ColorizationAfterglowBalance, Settings::Id::ColorizationBlurBalance })
			{
				const auto name = wxString(Settings::Get(id).name.data()).ToStdString(wxConvUTF8);
				Check(!modern["settings"].contains(name));
				modern["settings"][name] = 123u;
				modern["settings"][name + "Override"] = { { "state", "default" } };
			}
			modern["settings"]["ColorizationBlurBalanceOverride"] = 77u;
			const auto path = directory / (L"ignored-base-balances-" + std::to_wstring(catalog) + L".zip");
			writeRawZip(path, { { L"manifest.json", modern.dump() }, { L"LICENSE", request.licenseText } });
			if (catalog < 2) { Check(rejectedArchive(path)); continue; } // Invalid schema/catalog pairing stays rejected.
			const auto ignored = PresetPackages::LoadArchive(path);
			Check(ignored.ignoredSettingCount == 3 && ignored.ignoredSettingNames.size() == 3);
			for (const auto id : { Settings::Id::ColorizationColorBalance, Settings::Id::ColorizationAfterglowBalance, Settings::Id::ColorizationBlurBalance })
				Check(!ignored.settings.contains(id));
			Check(std::holds_alternative<std::monostate>(ignored.settings.at(Settings::Id::ColorizationColorBalanceOverride)));
			Check(std::holds_alternative<std::monostate>(ignored.settings.at(Settings::Id::ColorizationAfterglowBalanceOverride)));
			Check(std::get<DWORD>(ignored.settings.at(Settings::Id::ColorizationBlurBalanceOverride)) == 77);
			modern["settings"]["ColorizationColorBalance"] = 456u;
			writeRawZip(path, { { L"manifest.json", modern.dump() }, { L"LICENSE", request.licenseText } });
			Check(PresetPackages::LoadArchive(path).digest != ignored.digest);
		}
		for (const unsigned schema : { 1u, 2u })
		{
			auto legacy = nlohmann::ordered_json::parse(loaded.manifestText);
			legacy["schema_version"] = schema;
			legacy.erase("rights");
			for (auto& value : legacy["settings"]) if (value.is_object() && value.value("state", std::string{}) == "default") value = nullptr;
			legacy["catalog_version"] = 1;
			legacy.erase("accent_color");
			if (schema == 1) legacy["license"] = { { "name", "MIT" }, { "file", "LICENSE" } };
			for (const auto& spec : Settings::Catalog) if (Settings::IsWindowsColorBase(spec.id))
				legacy["settings"][wxString(spec.name.data(), spec.name.size()).ToStdString(wxConvUTF8)] = nullptr;
			legacy["settings"]["ColorizationColor"] = 0x8074B8FCu;
			legacy["settings"]["ColorizationAfterglow"] = 0x80553311u;
			legacy["settings"]["ColorizationColorBalance"] = 42u;
			legacy["settings"]["ColorizationBlurBalance"] = 60u;
			legacy["settings"]["ColorizationBlurBalanceOverride"] = 77u;
			legacy["settings"]["GlassType"] = 1u;
			const auto path = directory / (L"legacy-" + std::to_wstring(schema) + L".zip");
			const auto text = legacy.dump();
			writeRawZip(path, { { L"manifest.json", text }, { L"LICENSE", request.licenseText } });
			const auto converted = PresetPackages::LoadArchive(path);
			Check(converted.manifestText == text);
			Check(converted.legacyLicense);
			Check(converted.accentColor == 0x74B8FCu);
			Check(!converted.settings.contains(Settings::Id::ColorizationColor));
			Check(!converted.settings.contains(Settings::Id::ColorizationAfterglowOverride) && converted.ignoredSettingCount > 0);
			Check(std::get<DWORD>(converted.settings.at(Settings::Id::ColorizationColorBalanceOverride)) == 42);
			Check(std::get<DWORD>(converted.settings.at(Settings::Id::ColorizationBlurBalanceOverride)) == 77);
			Check(std::get<DWORD>(converted.settings.at(Settings::Id::GlassOpacity)) == ColorizationPresets::CalculateVistaOpacity(0x8074B8FC));
			Check(!converted.conversions.empty());
			legacy["settings"]["GlassOpacity"] = 37u;
			legacy["settings"]["ColorizationColorOverride"] = 0xAA112233u;
			legacy["settings"]["ColorizationAfterglowOverride"] = 0xBB445566u;
			const auto explicitPath = directory / (L"legacy-explicit-" + std::to_wstring(schema) + L".zip");
			writeRawZip(explicitPath, { { L"manifest.json", legacy.dump() }, { L"LICENSE", request.licenseText } });
			const auto explicitColors = PresetPackages::LoadArchive(explicitPath);
			Check(std::get<DWORD>(explicitColors.settings.at(Settings::Id::GlassOpacity)) == 37);
			Check(!explicitColors.settings.contains(Settings::Id::ColorizationColorOverride));
			Check(!explicitColors.settings.contains(Settings::Id::ColorizationAfterglowOverride));
			Check(explicitColors.accentColor == 0x74B8FCu && explicitColors.ignoredSettingCount >= 3);
			legacy["settings"]["GlassOpacity"] = nullptr;
			writeRawZip(explicitPath, { { L"manifest.json", legacy.dump() }, { L"LICENSE", request.licenseText } });
			Check(std::get<DWORD>(PresetPackages::LoadArchive(explicitPath).settings.at(Settings::Id::GlassOpacity)) == ColorizationPresets::CalculateVistaOpacity(0x8074B8FC));
			for (const auto& spec : Settings::Catalog) if (Settings::IsWindowsColorBase(spec.id))
				legacy["settings"][wxString(spec.name.data(), spec.name.size()).ToStdString(wxConvUTF8)] = nullptr;
			const auto emptyPath = directory / (L"legacy-no-base-" + std::to_wstring(schema) + L".zip");
			writeRawZip(emptyPath, { { L"manifest.json", legacy.dump() }, { L"LICENSE", request.licenseText } });
			const auto empty = PresetPackages::LoadArchive(emptyPath);
			Check(!empty.accentColor && std::holds_alternative<std::monostate>(empty.settings.at(Settings::Id::GlassOpacity)));
		}
		{
			auto legacy = nlohmann::ordered_json::parse(loaded.manifestText);
			legacy["schema_version"] = 2; legacy.erase("rights");
			legacy.erase("accent_color");
			legacy["catalog_version"] = 1;
			for (auto& value : legacy["settings"]) if (value.is_object() && value.value("state", std::string{}) == "default") value = nullptr;
			for (const auto& spec : Settings::Catalog) if (Settings::IsWindowsColorBase(spec.id))
				legacy["settings"][wxString(spec.name.data(), spec.name.size()).ToStdString(wxConvUTF8)] = nullptr;
			const auto path = directory / L"legacy-rights.zip";
			writeRawZip(path, { { L"manifest.json", legacy.dump() }, { L"LICENSE", request.licenseText } });
			const auto old = PresetPackages::LoadArchive(path);
			Check(old.legacyLicense && !old.conversions.empty());
			auto derived = request; derived.legacyLicense = true;
			derived.attribution = { L"Original author: https://example.com/author" };
			const auto saved = PresetPackages::CreateSnapshot(derived);
			Check(saved.legacyLicense && saved.licenseText == old.licenseText && saved.attribution == derived.attribution);
			const auto legacyLibrary = directory / L"legacy-library";
			const auto entry = PresetPackages::Publish(saved, "local-copy", {}, legacyLibrary);
			derived.metadata.uuid = PresetPackages::GeneratePackageUuid(); derived.legacyLicense = false;
			bool refusedRelicense{};
			try { (void)PresetPackages::Publish(PresetPackages::CreateSnapshot(derived), "local-copy", entry.libraryId, legacyLibrary, entry.digest); }
			catch (...) { refusedRelicense = true; }
			Check(refusedRelicense && PresetPackages::EnumerateInstalled(legacyLibrary).front().legacyLicense);

		}


		auto noColor = request;
		noColor.accentColor.reset();
		createArchive(directory / L"no-color.zip", noColor);
		Check(!PresetPackages::LoadArchive(directory / L"no-color.zip").accentColor);
		auto missingColor = nlohmann::ordered_json::parse(loaded.manifestText);
		missingColor.erase("accent_color");
		writeRawZip(directory / L"automatic-color.zip", { { L"manifest.json", missingColor.dump() }, { L"LICENSE", request.licenseText } });
		Check(!PresetPackages::LoadArchive(directory / L"automatic-color.zip").accentColor);
		auto blackColor = request;
		blackColor.accentColor = 0; // Black is manual RGB, not the automatic sentinel.
		createArchive(directory / L"black-color.zip", blackColor);
		Check(PresetPackages::LoadArchive(directory / L"black-color.zip").accentColor == std::optional<DWORD>{ 0 });
		for (const auto rgb : { "#GG1122", "#12345", "112233", "#11223344" })
		{
			auto malformed = nlohmann::ordered_json::parse(loaded.manifestText);
			malformed["accent_color"] = { { "rgb", rgb } };
			const auto path = directory / (L"invalid-rgb-" + std::to_wstring(std::hash<std::string_view>{}(rgb)) + L".zip");
			writeRawZip(path, { { L"manifest.json", malformed.dump() }, { L"LICENSE", request.licenseText } });
			Check(rejectedArchive(path));
		}
		auto extra = nlohmann::ordered_json::parse(loaded.manifestText);
		extra["future_metadata"] = "retained in digest";
		const auto extraPath = directory / L"future-metadata.zip";
		writeRawZip(extraPath, { { L"manifest.json", extra.dump() }, { L"LICENSE", request.licenseText } });
		const auto extraLoaded = PresetPackages::LoadArchive(extraPath);
		Check(extraLoaded.digest != loaded.digest && extraLoaded.settings == loaded.settings);

		auto catalogTwo = nlohmann::ordered_json::parse(loaded.manifestText);
		catalogTwo["catalog_version"] = 2;
		catalogTwo["settings"].erase("EnableUxMitigation");
		const auto catalogTwoArchive = directory / L"catalog-two.zip";
		writeRawZip(catalogTwoArchive, { { L"manifest.json", catalogTwo.dump() }, { L"LICENSE", request.licenseText } });
		const auto catalogTwoLoaded = PresetPackages::LoadArchive(catalogTwoArchive);
		Check(!catalogTwoLoaded.settings.contains(Settings::Id::EnableUxMitigation));
		Check(catalogTwoLoaded.settings.size() == Settings::PresetPackSettingCount(2));

		auto earlierManifest = nlohmann::ordered_json::parse(loaded.manifestText);
		earlierManifest["catalog_version"] = Settings::CatalogVersion + 1;
		earlierManifest["settings"]["MINMAXBUTTONGLOWid"] = 93;
		earlierManifest["settings"]["CLOSEBUTTONGLOWid"] = 92;
		earlierManifest["settings"]["TOOLCLOSEBUTTONGLOWid"] = 94;
		earlierManifest["settings"]["FutureRenderingMode"] = { { "mode", "future" } };
		const auto earlierArchive = directory / L"earlier-preset-catalog.zip";
		writeRawZip(earlierArchive, {
			{ L"manifest.json", earlierManifest.dump(2) + "\n" },
			{ L"LICENSE", request.licenseText }
		});
		const auto earlierLoaded = PresetPackages::LoadArchive(earlierArchive);
		Check(earlierLoaded.settings.size() == Settings::PresetPackSettingCount());
		Check(!earlierLoaded.settings.contains(Settings::Id::MinMaxButtonGlowId));
		Check(!earlierLoaded.settings.contains(Settings::Id::CloseButtonGlowId));
		Check(!earlierLoaded.settings.contains(Settings::Id::ToolCloseButtonGlowId));
		Check(earlierLoaded.catalogVersion == Settings::CatalogVersion + 1);
		Check(earlierLoaded.ignoredSettingCount == 4);
		Check(earlierLoaded.ignoredSettingNames.size() == 4);

		const auto traversal = directory / L"traversal.zip";
		writeRawZip(traversal, { { L"../manifest.json", "{}" }, { L"LICENSE", "license" } });
		Check(rejectedArchive(traversal));

		const auto duplicate = directory / L"duplicate.zip";
		writeRawZip(duplicate, { { L"manifest.json", "{}" }, { L"LICENSE", "license" }, { L"license", "license" } });
		Check(rejectedArchive(duplicate));

		const auto excessiveRatio = directory / L"excessive-ratio.zip";
		writeRawZip(excessiveRatio, { { L"manifest.json", std::string(1024 * 1024, '0') }, { L"LICENSE", "license" } });
		Check(rejectedArchive(excessiveRatio));

		const auto truncated = directory / L"truncated.zip";
		auto truncatedBytes = readFile(first);
		truncatedBytes.resize(truncatedBytes.size() - std::min<std::size_t>(32, truncatedBytes.size()));
		{
			std::ofstream output(truncated, std::ios::binary);
			output.write(truncatedBytes.data(), truncatedBytes.size());
		}
		Check(rejectedArchive(truncated));

		invalid = request;
		invalid.licenseText = std::string("invalid-") + static_cast<char>(0xff);
		bool rejectedLicense{};
		try { createArchive(directory / L"bad-license.zip", invalid); }
		catch (...) { rejectedLicense = true; }
		Check(rejectedLicense);

		const auto fakePng = directory / L"fake.png";
		{
			std::ofstream output(fakePng, std::ios::binary);
			output << "not a PNG";
		}
		invalid = request;
		invalid.settings[Settings::Id::CustomThemeReflection] = PresetPackages::AssetReference{ "assets/reflection.png" };
		invalid.assetSources.emplace("assets/reflection.png", fakePng);
		bool rejectedImage{};
		try { createArchive(directory / L"bad-image.zip", invalid); }
		catch (...) { rejectedImage = true; }
		Check(rejectedImage);
	}
}
