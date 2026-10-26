#include "pch.h"
#include "TestHarness.hpp"
#include "../OpenGlassGUI/ConfigurationResources.hpp"
#include "PngFixture.hpp"
#include <wx/init.h>
#include <nlohmann/json.hpp>

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestConfigurationResources()
	{
		wxInitializer initializer;
		Check(initializer.IsOk());
		if (!initializer.IsOk()) return;
		const auto uuid = PresetPackages::GeneratePackageUuid();
		const auto directory = std::filesystem::temp_directory_path() / (L"OpenGlassResourceTests-" + std::wstring(uuid.begin(), uuid.end()));
		std::filesystem::create_directories(directory);
		auto cleanup = wil::scope_exit([&]
		{
			for (const auto& path : std::filesystem::recursive_directory_iterator(directory)) SetFileAttributesW(path.path().c_str(), FILE_ATTRIBUTE_NORMAL);
			std::error_code error;
			std::filesystem::remove_all(directory, error);
		});
		const auto validPng = directory / L"reflection.png";
		const auto& png = ValidPng();
		ManagedFiles::Write(validPng, { reinterpret_cast<const std::byte*>(png.data()), png.size() });
		PresetPackages::CreateRequest request;
		request.metadata = { uuid, L"Resource transactions", L"Configuration resource test", L"OpenGlass tests", L"", L"MIT" };
		request.licenseText = "Resource fixture terms";
		for (const auto& spec : Settings::Catalog)
			if (Settings::IsPresetPackSetting(spec)) request.settings.emplace(spec.id, std::monostate{});
		auto assetRequest = request;
		assetRequest.settings[Settings::Id::CustomThemeReflection] = PresetPackages::AssetReference{ "assets/reflection.png" };
		assetRequest.assetSources.emplace("assets/reflection.png", validPng);
		const auto snapshot = PresetPackages::CreateSnapshot(assetRequest);
		const auto library = directory / L"library";
		auto revisedRequest = assetRequest;
		revisedRequest.metadata.uuid = PresetPackages::GeneratePackageUuid();
		revisedRequest.metadata.authorName = L"Updated rights holder";
		revisedRequest.licenseText = "Own replacement terms";
		revisedRequest.settings[Settings::Id::GlassOpacity] = 31u;
		const auto revised = PresetPackages::Publish(PresetPackages::CreateSnapshot(revisedRequest), "local", {}, library);
		const auto layout = directory / L"atlas.layout";
		const std::string layoutText = "RS1Compatibility=1\n12;1;3602=1,2,3,4\nCaptionHeight=22\n";
		ManagedFiles::Write(layout, { reinterpret_cast<const std::byte*>(layoutText.data()), layoutText.size() });
		auto atlasRequest = request;
		atlasRequest.metadata.uuid = PresetPackages::GeneratePackageUuid();
		atlasRequest.settings[Settings::Id::CustomThemeAtlas] = PresetPackages::AssetReference{ "assets/theme-atlas.png" };
		atlasRequest.assetSources.emplace("assets/theme-atlas.png", validPng);
		atlasRequest.assetSources.emplace("assets/theme-atlas.png.layout", layout);
		const auto loadedAtlas = PresetPackages::CreateSnapshot(atlasRequest);

		ConfigurationResources resources;
		const auto resourceRoot = directory / L"Configuration";
		resources.Initialize(resourceRoot, L"S-1-5-21-1-2-3-1001");
		const auto reflection = resources.Path(Settings::Scope::Machine, Settings::Id::CustomThemeReflection);
		const auto userReflection = resources.Path(Settings::Scope::User, Settings::Id::CustomThemeReflection);
		Check(reflection != userReflection && resources.IsManaged(reflection));
		Check(!resources.NeedsImport(Settings::Scope::Machine, Settings::Id::CustomThemeReflection, reflection));
		Check(resources.NeedsImport(Settings::Scope::User, Settings::Id::CustomThemeReflection, reflection));
		Check(!resources.NeedsImport(Settings::Scope::Machine, Settings::Id::CustomThemeReflection, validPng));
		{
			auto writer = resources.AcquireWriter();
			ConfigurationResources anotherSession; anotherSession.Initialize(resourceRoot, L"S-1-5-21-1-2-3-1002");
			bool busy{}; try { auto conflicting = anotherSession.AcquireWriter(); } catch (...) { busy = true; }
			Check(busy); writer.reset();
			Check(static_cast<bool>(anotherSession.AcquireWriter()));
		}
		const auto prepared = resources.Prepare(revised, Settings::Scope::Machine);
		resources.Begin(); resources.Reconcile();
		Check(!resources.HasAttemptChanges() && !resources.IsDirty());
		{
			ConfigurationResources emptyRecovery; emptyRecovery.Initialize(resourceRoot, L"S-1-5-21-1-2-3-1001");
			Check(emptyRecovery.HasRecovery());
			unsigned callbacks{};
			Check(emptyRecovery.Recover([&](auto, auto, const auto&) { ++callbacks; return false; },
				[&](const auto&) { ++callbacks; return false; }));
			Check(callbacks == 0 && !emptyRecovery.HasRecovery());
		}
		resources.CommitAttempt();
		resources.Begin(); resources.Install(*prepared); Check(resources.HasAttemptChanges()); resources.CommitAttempt();
		Check(resources.IsDirty() && ManagedFiles::Read(reflection) == snapshot.assets.at("assets/reflection.png"));
		resources.Begin(); resources.Install(*prepared);
		Check(!resources.HasAttemptChanges() && resources.IsDirty());
		resources.CommitAttempt(); Check(!resources.HasRecovery());
		resources.Accept(); Check(!resources.IsDirty());
		resources.Begin(); resources.Install(*prepared); resources.CommitAttempt(); Check(!resources.IsDirty());
		// Independently linked resources survive deletion of their source preset.
		PresetPackages::RemoveLibraryEntry(revised.libraryId, library);
		Check(!std::filesystem::exists(revised.source) && std::filesystem::exists(reflection));
		PresetPackages::CreateRequest extracted = request;
		resources.PreserveSource(extracted, reflection);
		Check(extracted.licenseText.find(revised.licenseText) != std::string::npos);
		// The file transaction works on exact bytes, including same-path changes and absence.
		const auto alternate = directory / L"alternate-resource";
		ManagedFiles::Write(alternate, { reinterpret_cast<const std::byte*>("alternate"), 9 });
		ConfigurationResources::Preparation replacement;
		replacement.files[reflection] = alternate;
		resources.Begin(); resources.Install(replacement); resources.CommitAttempt(); Check(resources.IsDirty());
		Check(ManagedFiles::Read(reflection) != snapshot.assets.at("assets/reflection.png"));
		resources.Begin(); resources.Install(*prepared);
		Check(resources.HasAttemptChanges() && !resources.IsDirty()); // Same path returned to baseline still needs a refresh.
		resources.CommitAttempt();
		resources.Begin(); resources.Install(replacement); resources.CommitAttempt();
		Check(resources.Revert()); resources.Accept();
		Check(ManagedFiles::Read(reflection) == snapshot.assets.at("assets/reflection.png"));
		resources.Begin(); resources.Install(replacement);
		Check(resources.RevertAttemptFiles() && resources.RollbackAttempt(true) && !resources.IsDirty());
		// Restore failure retains the baseline and can be retried.
		resources.Begin(); resources.Install(replacement); resources.CommitAttempt();
		{
			wil::unique_hfile held(CreateFileW(reflection.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
			Check(static_cast<bool>(held) && !resources.Revert() && resources.IsDirty());
		}
		Check(resources.Revert()); resources.Accept();
		// Interrupted attempt is recoverable by a new instance with callbacks, without real registry writes.
		ColorPreference::Snapshot recoveryColor{ DWORD{0}, DWORD{0x123456} };
		recoveryColor.accent = RegistryConfig::RawValue{ true, REG_BINARY, { 1, 2, 3 } };
		resources.Begin(recoveryColor);
		resources.TrackRegistry(Settings::Scope::Machine, Settings::Id::GlassOpacity, { true, REG_DWORD, { 42, 0, 0, 0 } });
		resources.Install(replacement);
		ConfigurationResources recovered; recovered.Initialize(resourceRoot, L"S-1-5-21-1-2-3-1001");
		Check(recovered.HasRecovery());
		ConfigurationResources foreign; foreign.Initialize(resourceRoot, L"S-1-5-21-1-2-3-1002");
		Check(foreign.HasForeignRecovery() && !foreign.HasRecovery());
		unsigned registryCalls{}, colorCalls{};
		const auto recoveryPath = resourceRoot / L".operation-S-1-5-21-1-2-3-1001" / L"state.json";
		const auto recoveryBytes = ManagedFiles::Read(recoveryPath);
		auto invalidRecovery = nlohmann::json::parse(reinterpret_cast<const char*>(recoveryBytes.data()), reinterpret_cast<const char*>(recoveryBytes.data()) + recoveryBytes.size());
		invalidRecovery["registry"][0]["name"] = "AnotherSetting";
		std::filesystem::remove(recoveryPath);
		const auto invalidText = invalidRecovery.dump();
		ManagedFiles::Write(recoveryPath, { reinterpret_cast<const std::byte*>(invalidText.data()), invalidText.size() });
		bool rejectedRecovery{};
		try { (void)recovered.Recover([&](auto, auto, const auto&) { ++registryCalls; return true; }, [&](const auto&) { ++colorCalls; return true; }); } catch (...) { rejectedRecovery = true; }
		Check(rejectedRecovery && registryCalls == 0 && colorCalls == 0 && ManagedFiles::Read(reflection) == ManagedFiles::Read(alternate));
		std::filesystem::remove(recoveryPath); ManagedFiles::Write(recoveryPath, recoveryBytes);
		Check(recovered.Recover([&](auto scope, auto id, const auto& value) { ++registryCalls; return scope == Settings::Scope::Machine && id == Settings::Id::GlassOpacity && value.bytes[0] == 42; },
			[&](const auto& choice) { ++colorCalls; return choice == recoveryColor; }));
		Check(registryCalls == 1 && colorCalls == 1 && !recovered.HasRecovery());
		Check(ManagedFiles::Read(reflection) == snapshot.assets.at("assets/reflection.png"));
		resources.Accept();
		// Published recovery IDs must still load after adding catalog settings.
		resources.Begin();
		const auto oldBytes = ManagedFiles::Read(recoveryPath);
		auto oldRecord = nlohmann::json::parse(reinterpret_cast<const char*>(oldBytes.data()), reinterpret_cast<const char*>(oldBytes.data()) + oldBytes.size());
		oldRecord["registry"] = nlohmann::json::array({
			{ { "user", true }, { "id", 28 }, { "name", "CustomThemeReflection" }, { "present", false }, { "type", 0 }, { "bytes", nlohmann::json::array() } },
			{ { "user", true }, { "id", 53 }, { "name", "EnableTransparency" }, { "present", true }, { "type", REG_DWORD }, { "bytes", { 0, 0, 0, 0 } } }
		});
		std::filesystem::remove(recoveryPath);
		const auto oldText = oldRecord.dump();
		ManagedFiles::Write(recoveryPath, { reinterpret_cast<const std::byte*>(oldText.data()), oldText.size() });
		registryCalls = 0;
		Check(recovered.Recover([&](auto scope, auto id, const auto& value)
		{
			++registryCalls;
			return scope == Settings::Scope::User && ((id == Settings::Id::CustomThemeReflection && !value.present)
				|| (id == Settings::Id::EnableTransparency && value.present && value.type == REG_DWORD && value.bytes == std::vector<BYTE>{ 0, 0, 0, 0 }));
		}, [](const auto&) { return false; }));
		Check(registryCalls == 2 && !recovered.HasRecovery());
		resources.Accept();
		// Round-trip independent Explorer/DWM Accent identities, including absence and raw types.
		for (const bool dwm : { false, true })
		for (const auto& accent : { RegistryConfig::RawValue{}, RegistryConfig::RawValue{ true, REG_DWORD, { 1, 2, 3, 255 } },
			RegistryConfig::RawValue{ true, REG_BINARY, { 4, 5, 6 } } })
		{
			ColorPreference::Snapshot accentOnly;
			accentOnly.applyChoice = false;
			(dwm ? accentOnly.dwmAccent : accentOnly.accent) = accent;
			resources.Begin(accentOnly);
			const auto bytes = ManagedFiles::Read(recoveryPath);
			const auto record = nlohmann::json::parse(reinterpret_cast<const char*>(bytes.data()), reinterpret_cast<const char*>(bytes.data()) + bytes.size());
			Check(!record["color"]["apply_choice"].get<bool>() && record["color"]["rgb"].is_null());
			const auto& saved = record["color"][dwm ? "dwm_accent" : "accent"];
			Check(saved["present"] == accent.present && saved["type"] == accent.type
				&& saved["bytes"].get<std::vector<BYTE>>() == accent.bytes);
			registryCalls = colorCalls = 0;
			Check(recovered.Recover([&](auto, auto, const auto&) { ++registryCalls; return false; },
				[&](const auto& choice) { ++colorCalls; return choice == accentOnly; }));
			Check(registryCalls == 0 && colorCalls == 1 && !recovered.HasRecovery());
			resources.Accept();
		}
		// A failed raw DWM restore retains the record for retry.
		resources.Begin(ColorPreference::Snapshot{ 0u, 0x112233u });
		resources.TrackRegistry(Settings::Scope::User, Settings::Id::ColorizationAfterglow, { true, REG_DWORD, { 0x66, 0x55, 0x44, 0xAA } });
		std::vector<int> recoveryOrder;
		auto restoreColor = [&](const auto&) { recoveryOrder.push_back(1); return true; };
		bool allowRegistryRestore{};
		auto restoreRegistry = [&](auto, auto, const auto& value) { recoveryOrder.push_back(2); return allowRegistryRestore && value.bytes[0] == 0x66; };
		Check(!recovered.Recover(restoreRegistry, restoreColor));
		Check(recoveryOrder == std::vector<int>{ 1, 2 } && recovered.HasRecovery());
		recoveryOrder.clear();
		allowRegistryRestore = true;
		Check(recovered.Recover(restoreRegistry, restoreColor));
		Check(recoveryOrder == std::vector<int>{ 1, 2 } && !recovered.HasRecovery());
		resources.Accept();
		// Automatic recovery must not overwrite freshly recomputed RGB with its old derived values.
		resources.Begin();
		resources.TrackColor(ColorPreference::Snapshot{ 1u, std::nullopt });
		resources.TrackRegistry(Settings::Scope::User, Settings::Id::ColorizationColor, { true, REG_DWORD, { 1, 2, 3, 4 } });
		resources.TrackRegistry(Settings::Scope::User, Settings::Id::ColorizationAfterglow, { true, REG_DWORD, { 5, 6, 7, 8 } });
		resources.TrackRegistry(Settings::Scope::User, Settings::Id::GlassOpacity, { true, REG_DWORD, { 42, 0, 0, 0 } });
		registryCalls = colorCalls = 0;
		Check(recovered.Recover([&](auto, auto id, const auto&) { ++registryCalls; return id == Settings::Id::GlassOpacity; },
			[&](const auto& choice) { ++colorCalls; return choice.RestoresAutomatic(); }));
		Check(registryCalls == 1 && colorCalls == 1);
		resources.Accept();
		// Revert itself publishes a durable restore target before modifying files.
		resources.Begin(); resources.Install(replacement); resources.CommitAttempt();
		resources.PrepareRevert({ { Settings::Scope::User, Settings::Id::GlassOpacity, { false, 0, {} } } }, std::nullopt);
		Check(recovered.HasRecovery());
		Check(recovered.Recover([](auto scope, auto id, const auto& value) { return scope == Settings::Scope::User && id == Settings::Id::GlassOpacity && !value.present; }, [](const auto&) { return false; }));
		Check(ManagedFiles::Read(reflection) == snapshot.assets.at("assets/reflection.png"));
		resources.Accept();
		// Atlas and layout are one transaction; a layout-less replacement removes the old sidecar.
		const auto atlas = resources.Path(Settings::Scope::User, Settings::Id::CustomThemeAtlas);
		const auto preparedAtlas = resources.Prepare(loadedAtlas, Settings::Scope::User);
		resources.Begin(); resources.Install(*preparedAtlas); resources.CommitAttempt(); resources.Accept();
		atlasRequest.assetSources.erase("assets/theme-atlas.png.layout");
		atlasRequest.metadata.uuid = PresetPackages::GeneratePackageUuid();
		const auto withoutLayout = resources.Prepare(PresetPackages::CreateSnapshot(atlasRequest), Settings::Scope::User);
		resources.Begin(); resources.Install(*withoutLayout); resources.CommitAttempt();
		Check(resources.IsDirty() && !std::filesystem::exists(atlas.wstring() + L".layout"));
		Check(resources.Revert()); resources.Accept();
		Check(std::filesystem::exists(atlas.wstring() + L".layout") && !std::filesystem::exists(resources.Path(Settings::Scope::Machine, Settings::Id::CustomThemeAtlas)));
		// File sharing and replacement never mutate the other hard-link directory entry.
		const auto link = resourceRoot / L"linked.png";
		Check(ManagedFiles::LinkOrCopy(reflection, link, resourceRoot));
		resources.Begin(); resources.Install(replacement); resources.CommitAttempt(); resources.Accept();
		Check(ManagedFiles::Read(link) == snapshot.assets.at("assets/reflection.png"));
		const auto external = directory / L"external-copy";
		Check(!ManagedFiles::LinkOrCopy(validPng, external, resourceRoot));
		Check(!ManagedFiles::LinkOrCopy(reflection.wstring() + L".source.json", resourceRoot / L"notice-copy.json", resourceRoot)); // Notices never share a file identity.
	}
}
