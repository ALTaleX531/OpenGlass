#pragma once
#include "SettingsCatalog.hpp"

#include <Windows.h>
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace OpenGlass::PresetPackages
{
	struct AssetReference
	{
		std::string path;
	};

	using SettingValue = std::variant<std::monostate, DWORD, AssetReference>;

	struct Metadata
	{
		std::string uuid;
		std::wstring name;
		std::wstring description;
		std::wstring authorName;
		std::wstring authorHomepage;
		std::wstring licenseName;
	};

	struct Package
	{
		Metadata metadata;
		std::map<Settings::Id, SettingValue> settings;
		std::vector<std::wstring> ignoredSettingNames;
		std::map<std::string, std::vector<std::byte>> assets;
		std::vector<std::pair<std::string, std::uint64_t>> assetSummary;
		std::string licenseText;
		std::string manifestText;
		std::string digest;
		std::filesystem::path source;
		unsigned catalogVersion{};
		std::size_t ignoredSettingCount{};
		bool deployed{};
	};

	struct CreateRequest
	{
		Metadata metadata;
		std::map<Settings::Id, SettingValue> settings;
		std::map<std::string, std::filesystem::path> assetSources;
		std::string licenseText;
	};

	struct DeploymentResult
	{
		std::filesystem::path path;
		bool created{};
	};

	[[nodiscard]] Package LoadArchive(const std::filesystem::path& path);
	[[nodiscard]] Package LoadDeployed(const std::filesystem::path& directory);
	[[nodiscard]] std::vector<Package> EnumerateInstalled();
	[[nodiscard]] std::filesystem::path GetPresetRoot();
	[[nodiscard]] std::string GeneratePackageUuid();
	[[nodiscard]] bool IsValidHomepageUrl(std::wstring_view value);
	[[nodiscard]] std::wstring InferLicenseName(std::string_view licenseText);
	[[nodiscard]] DeploymentResult Deploy(const Package& package);
	void CreateArchive(const std::filesystem::path& path, CreateRequest request);
	void Remove(const Package& package);
	[[nodiscard]] std::wstring ResolveAssetPath(const Package& package, const AssetReference& asset);
}
