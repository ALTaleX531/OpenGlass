#include "pch.h"
#include "ApplicationPaths.hpp"
#include "PngAssetValidation.hpp"
#include "PresetPackage.hpp"

#include <bcrypt.h>
#include <sddl.h>

#pragma comment(lib, "bcrypt.lib")

namespace OpenGlass::PresetPackages
{
	namespace
	{
		constexpr std::size_t MaximumEntryCount = 32;
		constexpr std::size_t MaximumEntrySize = 64ull * 1024 * 1024;
		constexpr std::size_t MaximumTotalSize = 128ull * 1024 * 1024;
		constexpr std::size_t MaximumMetadataSize = 1024ull * 1024;
		constexpr std::size_t MaximumCompressionRatio = 200;
		constexpr std::size_t MaximumEntryPathLength = 240;
		constexpr std::size_t MaximumReportedIgnoredSettings = 64;
		constexpr std::size_t MaximumReportedSettingNameLength = 160;

		std::wstring DisplaySettingName(std::wstring_view value)
		{
			std::wstring result;
			for (const auto character : value)
			{
				if (result.size() >= MaximumReportedSettingNameLength)
				{
					result += L"...";
					break;
				}
				if (character < L' ' || character == 0x7f)
				{
					result += std::format(L"\\u{:04X}", static_cast<unsigned>(character));
				}
				else
				{
					result += character;
				}
			}
			return result.empty() ? L"<empty name>" : result;
		}

		std::string ToUtf8(std::wstring_view value)
		{
			if (value.empty())
			{
				return {};
			}
			const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
			THROW_LAST_ERROR_IF(size <= 0);
			std::string result(size, '\0');
			THROW_LAST_ERROR_IF(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr) <= 0);
			return result;
		}

		std::wstring FromUtf8(std::string_view value)
		{
			if (value.empty())
			{
				return {};
			}
			const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION), size <= 0);
			std::wstring result(size, L'\0');
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_NO_UNICODE_TRANSLATION), MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), size) <= 0);
			return result;
		}

		std::filesystem::path PathFromUtf8(std::string_view value)
		{
			return std::filesystem::path(FromUtf8(value));
		}

		std::string Hex(std::span<const std::byte> bytes)
		{
			constexpr char digits[] = "0123456789abcdef";
			std::string result(bytes.size() * 2, '\0');
			for (std::size_t index = 0; index < bytes.size(); ++index)
			{
				const auto value = std::to_integer<unsigned char>(bytes[index]);
				result[index * 2] = digits[value >> 4];
				result[index * 2 + 1] = digits[value & 0xf];
			}
			return result;
		}

		std::string Sha256(std::span<const std::byte> bytes)
		{
			BCRYPT_ALG_HANDLE algorithm{};
			THROW_IF_NTSTATUS_FAILED(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0));
			const auto closeAlgorithm = wil::scope_exit([&] { BCryptCloseAlgorithmProvider(algorithm, 0); });
			BCRYPT_HASH_HANDLE hash{};
			THROW_IF_NTSTATUS_FAILED(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0));
			const auto destroyHash = wil::scope_exit([&] { BCryptDestroyHash(hash); });
			THROW_IF_NTSTATUS_FAILED(BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data())), static_cast<ULONG>(bytes.size()), 0));
			std::array<std::byte, 32> digest{};
			THROW_IF_NTSTATUS_FAILED(BCryptFinishHash(hash, reinterpret_cast<PUCHAR>(digest.data()), static_cast<ULONG>(digest.size()), 0));
			return Hex(digest);
		}

		bool HasInvalidMetadataCharacters(std::wstring_view value, bool multiline)
		{
			return std::ranges::any_of(value, [multiline](wchar_t character)
			{
				if (character == 0x7f) return true;
				if (character >= 0x20) return false;
				return !multiline || (character != L'\r' && character != L'\n' && character != L'\t');
			});
		}

		bool IsCanonicalUuid(const std::string& value)
		{
			if (value.size() != 36)
			{
				return false;
			}
			for (std::size_t index = 0; index < value.size(); ++index)
			{
				if (index == 8 || index == 13 || index == 18 || index == 23)
				{
					if (value[index] != '-') return false;
				}
				else if (!((value[index] >= '0' && value[index] <= '9') || (value[index] >= 'a' && value[index] <= 'f')))
				{
					return false;
				}
			}
			return true;
		}

		std::string CreateUuid()
		{
			GUID guid{};
			THROW_IF_FAILED(CoCreateGuid(&guid));
			char text[37]{};
			std::snprintf(
				text,
				sizeof(text),
				"%08lx-%04x-%04x-%04x-%012llx",
				guid.Data1,
				guid.Data2,
				guid.Data3,
				(static_cast<unsigned>(guid.Data4[0]) << 8) | guid.Data4[1],
				(static_cast<unsigned long long>(guid.Data4[2]) << 40)
					| (static_cast<unsigned long long>(guid.Data4[3]) << 32)
					| (static_cast<unsigned long long>(guid.Data4[4]) << 24)
					| (static_cast<unsigned long long>(guid.Data4[5]) << 16)
					| (static_cast<unsigned long long>(guid.Data4[6]) << 8)
					| guid.Data4[7]
			);
			return text;
		}

		bool IsSafeEntryPath(std::string_view value)
		{
			if (
				value.empty()
				|| value.size() > MaximumEntryPathLength
				|| value.find('\0') != std::string_view::npos
				|| value.front() == '/'
				|| value.front() == '\\'
				|| value.find('\\') != std::string_view::npos
				|| value.find(':') != std::string_view::npos
			)
			{
				return false;
			}
			std::filesystem::path path = PathFromUtf8(value);
			if (path.is_absolute() || path.has_root_path())
			{
				return false;
			}
			for (const auto& component : path)
			{
				const auto text = component.native();
				if (component == L".." || component == L"." || text.empty() || text.ends_with(L'.') || text.ends_with(L' '))
				{
					return false;
				}
			}
			return true;
		}

		using EntryMap = std::map<std::string, std::vector<std::byte>, std::less<>>;

		bool IsReparsePoint(const std::filesystem::path& path)
		{
			const DWORD attributes = GetFileAttributesW(path.c_str());
			THROW_LAST_ERROR_IF(attributes == INVALID_FILE_ATTRIBUTES);
			return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
		}

		EntryMap ReadArchiveEntries(const std::filesystem::path& path)
		{
			wxFFileInputStream input(path.wstring());
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), !input.IsOk());
			wxZipInputStream zip(input);
			EntryMap entries;
			std::set<std::string, std::less<>> names;
			std::size_t total{};
			while (const auto entry = std::unique_ptr<wxZipEntry>(zip.GetNextEntry()))
			{
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), entry->IsDir() || (entry->GetFlags() & 1) != 0);
				const auto unixType = (entry->GetExternalAttributes() >> 16) & 0170000;
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), unixType != 0 && unixType != 0100000);
				const auto name = entry->GetName(wxPATH_UNIX).ToStdString(wxConvUTF8);
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), !IsSafeEntryPath(name));
				std::string folded = name;
				std::ranges::transform(folded, folded.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_DUP_NAME), !names.emplace(std::move(folded)).second);
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_TOO_MANY_NAMES), entries.size() >= MaximumEntryCount);
				const auto announcedSize = entry->GetSize();
				const auto compressedSize = entry->GetCompressedSize();
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), announcedSize != wxInvalidOffset && static_cast<std::uint64_t>(announcedSize) > MaximumEntrySize);
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID),
					(announcedSize > 0 && compressedSize == 0)
					|| (announcedSize > 0 && compressedSize > 0 && static_cast<std::uint64_t>(announcedSize) > static_cast<std::uint64_t>(compressedSize) * MaximumCompressionRatio)
				);
				std::vector<std::byte> bytes;
				if (announcedSize > 0 && announcedSize != wxInvalidOffset) bytes.reserve(static_cast<std::size_t>(announcedSize));
				std::array<std::byte, 64 * 1024> buffer{};
				for (;;)
				{
					zip.Read(buffer.data(), buffer.size());
					const auto count = zip.LastRead();
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), bytes.size() + count > MaximumEntrySize || total + bytes.size() + count > MaximumTotalSize);
					bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + count);
					if (count == 0 || zip.GetLastError() == wxSTREAM_EOF) break;
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), zip.GetLastError() != wxSTREAM_NO_ERROR);
				}
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), announcedSize != wxInvalidOffset && static_cast<std::uint64_t>(announcedSize) != bytes.size());
				total += bytes.size();
				entries.emplace(name, std::move(bytes));
			}
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_INVALID), entries.empty() || (zip.GetLastError() != wxSTREAM_EOF && zip.GetLastError() != wxSTREAM_NO_ERROR));
			return entries;
		}

		std::string BytesToString(const std::vector<std::byte>& bytes)
		{
			return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
		}

		void ValidateLicense(const std::string& text)
		{
			THROW_HR_IF(E_INVALIDARG, text.empty() || text.size() > MaximumMetadataSize || text.find('\0') != std::string::npos);
			const auto wide = FromUtf8(text);
			THROW_HR_IF(E_INVALIDARG,
				std::ranges::all_of(wide, [](wchar_t c) { return iswspace(c); })
				|| std::ranges::any_of(wide, [](wchar_t c) { return c < 0x20 && c != L'\r' && c != L'\n' && c != L'\t'; })
			);
		}

		std::string ExpectedAssetPath(Settings::AssetRole role)
		{
			switch (role)
			{
			case Settings::AssetRole::ThemeAtlas: return "assets/theme-atlas.png";
			case Settings::AssetRole::Reflection: return "assets/reflection.png";
			case Settings::AssetRole::Material: return "assets/material.png";
			default: THROW_HR(E_INVALIDARG);
			}
		}

		wil::com_ptr<IWICImagingFactory> CreateWicFactory()
		{
			wil::com_ptr<IWICImagingFactory> factory;
			THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
			return factory;
		}

		void ValidateImageWithWic(std::span<const std::byte> bytes)
		{
			PngAssetValidation::ImageInfo expectedInfo{};
			THROW_IF_FAILED(PngAssetValidation::ValidateStructure(bytes, expectedInfo));
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_BAD_FORMAT), bytes.empty() || bytes.size() > MAXDWORD);
			const auto initialization = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			THROW_HR_IF(initialization, FAILED(initialization) && initialization != RPC_E_CHANGED_MODE);
			const auto uninitialize = wil::scope_exit([&] { if (SUCCEEDED(initialization)) CoUninitialize(); });
			auto factory = CreateWicFactory();
			wil::com_ptr<IWICStream> stream;
			THROW_IF_FAILED(factory->CreateStream(&stream));
			THROW_IF_FAILED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<std::byte*>(bytes.data())), static_cast<DWORD>(bytes.size())));
			wil::com_ptr<IWICFormatConverter> converter;
			THROW_IF_FAILED(PngAssetValidation::CreateValidatedWicSource(
				factory.get(),
				stream.get(),
				&expectedInfo,
				&converter
			));
		}

		void ValidateImageWithWic(const std::filesystem::path& path)
		{
			std::ifstream stream(path, std::ios::binary);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !stream);
			std::vector<char> chars((std::istreambuf_iterator<char>(stream)), {});
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), chars.size() > PngAssetValidation::MaximumFileSize);
			ValidateImageWithWic({ reinterpret_cast<const std::byte*>(chars.data()), chars.size() });
		}

		std::string CalculateContentDigest(const std::string& manifest, const std::string& license, const std::map<std::string, std::vector<std::byte>>& assets)
		{
			std::vector<std::byte> content;
			auto append = [&](std::string_view name, std::span<const std::byte> bytes)
			{
				const auto size = static_cast<std::uint64_t>(bytes.size());
				content.insert(content.end(), reinterpret_cast<const std::byte*>(name.data()), reinterpret_cast<const std::byte*>(name.data() + name.size()));
				content.insert(content.end(), reinterpret_cast<const std::byte*>(&size), reinterpret_cast<const std::byte*>(&size + 1));
				content.insert(content.end(), bytes.begin(), bytes.end());
			};
			append("manifest.json", { reinterpret_cast<const std::byte*>(manifest.data()), manifest.size() });
			if (!license.empty())
			{
				append("LICENSE", { reinterpret_cast<const std::byte*>(license.data()), license.size() });
			}
			for (const auto& [name, bytes] : assets)
			{
				append(name, bytes);
			}
			return Sha256(content);
		}

		Package ParsePackage(EntryMap entries, const std::filesystem::path& source, bool deployed)
		{
			const auto manifestIt = entries.find("manifest.json");
			THROW_HR_IF(E_INVALIDARG, manifestIt == entries.end());
			Package package;
			package.source = source;
			package.deployed = deployed;
			package.manifestText = BytesToString(manifestIt->second);

			THROW_HR_IF(E_INVALIDARG, package.manifestText.size() > MaximumMetadataSize || package.manifestText.find('\0') != std::string::npos);
			bool duplicateJsonKey{};
			std::vector<std::set<std::string>> objectKeys;
			const auto manifest = nlohmann::json::parse(package.manifestText, [&objectKeys, &duplicateJsonKey](int, nlohmann::json::parse_event_t event, nlohmann::json& parsed)
			{
				if (event == nlohmann::json::parse_event_t::object_start) objectKeys.emplace_back();
				else if (event == nlohmann::json::parse_event_t::key)
				{
					if (objectKeys.empty() || !objectKeys.back().emplace(parsed.get<std::string>()).second) duplicateJsonKey = true;
				}
				else if (event == nlohmann::json::parse_event_t::object_end && !objectKeys.empty()) objectKeys.pop_back();
				return true;
			});
			THROW_HR_IF(E_INVALIDARG, duplicateJsonKey || !manifest.is_object() || manifest.size() != 9);
			const auto schemaVersion = manifest.value("schema_version", 0u);
			THROW_HR_IF(E_INVALIDARG, schemaVersion != 1 && schemaVersion != 2);
			package.catalogVersion = manifest.value("catalog_version", 0u);
			THROW_HR_IF(E_INVALIDARG, package.catalogVersion == 0);
			package.metadata.uuid = manifest.at("uuid").get<std::string>();
			THROW_HR_IF(E_INVALIDARG, !IsCanonicalUuid(package.metadata.uuid));
			package.metadata.name = FromUtf8(manifest.at("name").get<std::string>());
			package.metadata.description = FromUtf8(manifest.at("description").get<std::string>());
			const auto& author = manifest.at("author");
			THROW_HR_IF(E_INVALIDARG, !author.is_object() || author.size() != 2);
			package.metadata.authorName = FromUtf8(author.at("name").get<std::string>());
			package.metadata.authorHomepage = FromUtf8(author.at("homepage").get<std::string>());
			const auto& license = manifest.at("license");
			const auto licenseIt = entries.find("LICENSE");
			if (schemaVersion == 1)
			{
				THROW_HR_IF(E_INVALIDARG, !license.is_object() || (license.size() != 2 && license.size() != 3));
				package.metadata.licenseName = FromUtf8(license.at("name").get<std::string>());
				THROW_HR_IF(E_INVALIDARG, license.at("file").get<std::string>() != "LICENSE" || licenseIt == entries.end());
				if (license.contains("homepage"))
				{
					THROW_HR_IF(E_INVALIDARG, !IsValidHomepageUrl(FromUtf8(license.at("homepage").get<std::string>())));
				}
				package.licenseText = BytesToString(licenseIt->second);
				ValidateLicense(package.licenseText);
			}
			else if (license.is_null())
			{
				THROW_HR_IF(E_INVALIDARG, licenseIt != entries.end());
			}
			else
			{
				THROW_HR_IF(E_INVALIDARG, !license.is_object() || license.size() != 1 || license.at("file").get<std::string>() != "LICENSE" || licenseIt == entries.end());
				package.licenseText = BytesToString(licenseIt->second);
				ValidateLicense(package.licenseText);
				package.metadata.licenseName = InferLicenseName(package.licenseText);
			}
			THROW_HR_IF(E_INVALIDARG,
				package.metadata.name.empty()
				|| package.metadata.name.size() > 128
				|| HasInvalidMetadataCharacters(package.metadata.name, false)
				|| package.metadata.description.size() > 4096
				|| HasInvalidMetadataCharacters(package.metadata.description, true)
				|| package.metadata.authorName.empty()
				|| package.metadata.authorName.size() > 256
				|| HasInvalidMetadataCharacters(package.metadata.authorName, false)
				|| package.metadata.licenseName.size() > 256
				|| (!package.metadata.licenseName.empty() && HasInvalidMetadataCharacters(package.metadata.licenseName, false))
				|| !IsValidHomepageUrl(package.metadata.authorHomepage)
			);

			const auto& settings = manifest.at("settings");
			THROW_HR_IF(E_INVALIDARG, !settings.is_object());
			for (const auto& [name, _] : settings.items())
			{
				const auto decodedName = FromUtf8(name);
				const auto spec = Settings::Find(decodedName);
				if (spec && Settings::IsPresetPackSetting(*spec, package.catalogVersion)) continue;
				++package.ignoredSettingCount;
				if (package.ignoredSettingNames.size() < MaximumReportedIgnoredSettings)
				{
					package.ignoredSettingNames.push_back(DisplaySettingName(decodedName));
				}
			}
			for (const auto& spec : Settings::Catalog)
			{
				if (!Settings::IsPresetPackSetting(spec, package.catalogVersion)) continue;
				const auto name = ToUtf8(spec.name);
				THROW_HR_IF(E_INVALIDARG, !settings.contains(name));
				const auto& value = settings.at(name);
				if (value.is_null())
				{
					package.settings.emplace(spec.id, std::monostate{});
				}
				else if (spec.type == Settings::ValueType::Dword)
				{
					THROW_HR_IF(E_INVALIDARG, !value.is_number_unsigned() || value.get<std::uint64_t>() > MAXDWORD);
					package.settings.emplace(spec.id, static_cast<DWORD>(value.get<std::uint64_t>()));
				}
				else
				{
					THROW_HR_IF(E_INVALIDARG, spec.assetRole == Settings::AssetRole::None || !value.is_object() || !value.contains("asset"));
					const auto assetPath = value.at("asset").get<std::string>();
					THROW_HR_IF(E_INVALIDARG, assetPath != ExpectedAssetPath(spec.assetRole));
					package.settings.emplace(spec.id, AssetReference{ assetPath });
				}
			}

			const auto& assetSpecs = manifest.at("assets");
			THROW_HR_IF(E_INVALIDARG, !assetSpecs.is_object());
			for (const auto& [path, spec] : assetSpecs.items())
			{
				const bool knownPath = path == "assets/theme-atlas.png"
					|| path == "assets/theme-atlas.png.layout"
					|| path == "assets/reflection.png"
					|| path == "assets/material.png";
				THROW_HR_IF(E_INVALIDARG, !knownPath || !spec.is_object() || spec.size() != 2);
				const auto entry = entries.find(path);
				THROW_HR_IF(E_INVALIDARG, entry == entries.end());
				THROW_HR_IF(E_INVALIDARG, spec.at("size").get<std::uint64_t>() != entry->second.size());
				THROW_HR_IF(E_INVALIDARG, spec.at("sha256").get<std::string>() != Sha256(entry->second));
				package.assets.emplace(path, entry->second);
			}
			for (const auto& [path, bytes] : package.assets)
			{
				if (path.ends_with(".layout"))
				{
					THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), bytes.size() > MaximumMetadataSize);
				}
				else ValidateImageWithWic(bytes);
				package.assetSummary.emplace_back(path, bytes.size());
			}
			for (const auto& [_, value] : package.settings)
			{
				if (const auto asset = std::get_if<AssetReference>(&value))
				{
					THROW_HR_IF(E_INVALIDARG, !package.assets.contains(asset->path));
				}
			}
			THROW_HR_IF(E_INVALIDARG, package.assets.contains("assets/theme-atlas.png.layout") && !package.assets.contains("assets/theme-atlas.png"));
			THROW_HR_IF(E_INVALIDARG, entries.size() != package.assets.size() + 1 + (package.licenseText.empty() ? 0 : 1));
			package.digest = CalculateContentDigest(manifest.dump(), package.licenseText, package.assets);
			return package;
		}

		EntryMap ReadDeployedEntries(const std::filesystem::path& directory)
		{
			THROW_HR_IF(E_INVALIDARG, !std::filesystem::is_directory(directory) || IsReparsePoint(directory));
			EntryMap entries;
			std::set<std::string, std::less<>> names;
			std::size_t total{};
			for (const auto& path : std::filesystem::recursive_directory_iterator(directory))
			{
				THROW_HR_IF(E_INVALIDARG, IsReparsePoint(path.path()));
				if (path.is_directory()) continue;
				THROW_HR_IF(E_INVALIDARG, !path.is_regular_file() || entries.size() >= MaximumEntryCount);
				const auto relative = ToUtf8(std::filesystem::relative(path.path(), directory).generic_wstring());
				THROW_HR_IF(E_INVALIDARG, !IsSafeEntryPath(relative));
				std::string folded = relative;
				std::ranges::transform(folded, folded.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_DUP_NAME), !names.emplace(std::move(folded)).second);
				const auto size = path.file_size();
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), size > MaximumEntrySize || total + size > MaximumTotalSize);
				std::ifstream stream(path.path(), std::ios::binary);
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_READ_FAULT), !stream);
				std::vector<char> chars((std::istreambuf_iterator<char>(stream)), {});
				THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_READ_FAULT), chars.size() != size);
				std::vector<std::byte> bytes(chars.size());
				if (!chars.empty()) std::memcpy(bytes.data(), chars.data(), chars.size());
				entries.emplace(relative, std::move(bytes));
				total += size;
			}
			return entries;
		}

		void WriteFileBytes(const std::filesystem::path& path, std::span<const std::byte> bytes)
		{
			std::filesystem::create_directories(path.parent_path());
			std::ofstream stream(path, std::ios::binary | std::ios::trunc);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_WRITE_FAULT), !stream);
			stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_WRITE_FAULT), !stream.good());
		}

		void ApplyPresetAcl(const std::filesystem::path& directory)
		{
			PSECURITY_DESCRIPTOR descriptor{};
			THROW_IF_WIN32_BOOL_FALSE(ConvertStringSecurityDescriptorToSecurityDescriptorW(
				L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;GRGX;;;BU)(A;OICI;GRGX;;;S-1-5-90-0)",
				SDDL_REVISION_1,
				&descriptor,
				nullptr
			));
			wil::unique_hlocal securityDescriptor{ descriptor };
			THROW_IF_WIN32_BOOL_FALSE(SetFileSecurityW(directory.c_str(), DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, descriptor));
		}

		void HardenDirectory(const std::filesystem::path& directory)
		{
			ApplyPresetAcl(directory);
			for (const auto& item : std::filesystem::recursive_directory_iterator(directory))
			{
				THROW_IF_WIN32_BOOL_FALSE(SetFileAttributesW(item.path().c_str(), item.is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_READONLY));
			}
		}

		std::string BuildManifest(CreateRequest& request, const std::map<std::string, std::vector<std::byte>>& assets)
		{
			nlohmann::ordered_json manifest;
			manifest["schema_version"] = 2;
			manifest["catalog_version"] = Settings::CatalogVersion;
			manifest["uuid"] = request.metadata.uuid;
			manifest["name"] = ToUtf8(request.metadata.name);
			manifest["description"] = ToUtf8(request.metadata.description);
			manifest["author"] = { { "name", ToUtf8(request.metadata.authorName) }, { "homepage", ToUtf8(request.metadata.authorHomepage) } };
			manifest["license"] = request.licenseText.empty()
				? nlohmann::ordered_json(nullptr)
				: nlohmann::ordered_json{ { "file", "LICENSE" } };
			for (const auto& spec : Settings::Catalog)
			{
				if (!Settings::IsPresetPackSetting(spec)) continue;
				const auto value = request.settings.find(spec.id);
				THROW_HR_IF(E_INVALIDARG, value == request.settings.end());
				auto& output = manifest["settings"][ToUtf8(spec.name)];
				if (std::holds_alternative<std::monostate>(value->second)) output = nullptr;
				else if (const auto dword = std::get_if<DWORD>(&value->second)) output = *dword;
				else if (const auto asset = std::get_if<AssetReference>(&value->second))
				{
					THROW_HR_IF(E_INVALIDARG, asset->path != ExpectedAssetPath(spec.assetRole) || !assets.contains(asset->path));
					output = { { "asset", asset->path } };
				}
				else THROW_HR(E_INVALIDARG);
			}
			manifest["assets"] = nlohmann::ordered_json::object();
			for (const auto& [path, bytes] : assets)
			{
				const bool knownPath = path == "assets/theme-atlas.png"
					|| path == "assets/theme-atlas.png.layout"
					|| path == "assets/reflection.png"
					|| path == "assets/material.png";
				THROW_HR_IF(E_INVALIDARG, !knownPath || (path == "assets/theme-atlas.png.layout" && !assets.contains("assets/theme-atlas.png")));
				manifest["assets"][path] = { { "size", bytes.size() }, { "sha256", Sha256(bytes) } };
			}
			return manifest.dump(2) + "\n";
		}
	}

	std::wstring InferLicenseName(std::string_view licenseText)
	{
		if (licenseText.empty()) return {};
		auto text = FromUtf8(licenseText);
		if (!text.empty() && text.front() == 0xfeff) text.erase(text.begin());
		auto lower = text;
		std::ranges::transform(lower, lower.begin(), [](wchar_t value) { return static_cast<wchar_t>(::towlower(value)); });
		const auto header = std::wstring_view(lower).substr(0, std::min<std::size_t>(lower.size(), 4096));

		const auto trim = [](std::wstring_view value)
		{
			while (!value.empty() && iswspace(value.front())) value.remove_prefix(1);
			while (!value.empty() && iswspace(value.back())) value.remove_suffix(1);
			return value;
		};
		constexpr std::wstring_view spdxPrefix = L"spdx-license-identifier:";
		if (const auto position = header.find(spdxPrefix); position != std::wstring::npos)
		{
			const auto start = position + spdxPrefix.size();
			const auto end = text.find_first_of(L"\r\n", start);
			const auto identifier = trim(std::wstring_view(text).substr(start, end == std::wstring::npos ? text.size() - start : end - start));
			if (!identifier.empty() && identifier.size() <= 128) return std::wstring(identifier);
		}

		struct KnownLicense
		{
			std::wstring_view marker;
			std::wstring_view name;
		};
		constexpr KnownLicense known[]
		{
			{ L"mozilla public license version 2.0", L"MPL-2.0" },
			{ L"gnu lesser general public license", L"LGPL" },
			{ L"gnu affero general public license", L"AGPL" },
			{ L"gnu general public license", L"GPL" },
			{ L"apache license\nversion 2.0", L"Apache-2.0" },
			{ L"apache license\r\nversion 2.0", L"Apache-2.0" },
			{ L"boost software license - version 1.0", L"BSL-1.0" },
			{ L"bsd 3-clause license", L"BSD-3-Clause" },
			{ L"bsd 2-clause license", L"BSD-2-Clause" },
			{ L"isc license", L"ISC" },
			{ L"mit license", L"MIT" },
			{ L"the unlicense", L"Unlicense" }
		};
		for (const auto& candidate : known)
		{
			if (header.find(candidate.marker) != std::wstring::npos)
			{
				std::wstring name(candidate.name);
				if ((name == L"GPL" || name == L"LGPL" || name == L"AGPL") && header.find(L"version 3") != std::wstring::npos) name += L"-3.0";
				else if ((name == L"GPL" || name == L"LGPL") && header.find(L"version 2.1") != std::wstring::npos) name += L"-2.1";
				else if ((name == L"GPL" || name == L"LGPL") && header.find(L"version 2") != std::wstring::npos) name += L"-2.0";
				return name;
			}
		}

		std::wstring_view remaining(text);
		while (!remaining.empty())
		{
			const auto end = remaining.find_first_of(L"\r\n");
			const auto line = trim(remaining.substr(0, end));
			if (!line.empty())
			{
				auto lineLower = std::wstring(line);
				std::ranges::transform(lineLower, lineLower.begin(), [](wchar_t value) { return static_cast<wchar_t>(::towlower(value)); });
				if (line.size() <= 128 && (lineLower.contains(L"license") || lineLower.contains(L"licence"))) return std::wstring(line);
				break;
			}
			if (end == std::wstring::npos) break;
			remaining.remove_prefix(end + 1);
		}
		return L"Custom license";
	}

	Package LoadArchive(const std::filesystem::path& path)
	{
		return ParsePackage(ReadArchiveEntries(path), path, false);
	}

	Package LoadDeployed(const std::filesystem::path& directory)
	{
		return ParsePackage(ReadDeployedEntries(directory), directory, true);
	}

	std::filesystem::path GetPresetRoot()
	{
		return ApplicationPaths::GetProgramDataSubdirectory(L"Presets");
	}

	std::string GeneratePackageUuid()
	{
		return CreateUuid();
	}

	bool IsValidHomepageUrl(std::wstring_view value)
	{
		if (value.empty()) return false;
		wxURI uri{ wxString(value.data(), value.size()) };
		const auto scheme = uri.GetScheme().Lower();
		return (scheme == L"http" || scheme == L"https") && !uri.GetServer().empty();
	}

	std::vector<Package> EnumerateInstalled()
	{
		std::vector<Package> result;
		const auto root = GetPresetRoot();
		if (!std::filesystem::exists(root))
		{
			return result;
		}
		for (const auto& item : std::filesystem::directory_iterator(root))
		{
			if (!item.is_directory()) continue;
			try
			{
				auto package = LoadDeployed(item.path());
				if (item.path().filename().string() == package.metadata.uuid)
				{
					package.assets.clear();
					result.push_back(std::move(package));
				}
			}
			catch (...) {}
		}
		std::ranges::sort(result, {}, [](const Package& package) { return package.metadata.name; });
		return result;
	}

	DeploymentResult Deploy(const Package& package)
	{
		const auto root = GetPresetRoot();
		std::filesystem::create_directories(root);
		THROW_HR_IF(E_INVALIDARG, IsReparsePoint(root));
		ApplyPresetAcl(root);
		const auto destination = root / PathFromUtf8(package.metadata.uuid);
		if (std::filesystem::exists(destination))
		{
			const auto installed = LoadDeployed(destination);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_DUP_NAME), installed.digest != package.digest);
			HardenDirectory(destination);
			return { destination, false };
		}
		const auto staging = root / PathFromUtf8(package.metadata.uuid + ".staging-" + CreateUuid());
		auto cleanup = wil::scope_exit([&] { std::error_code error; std::filesystem::remove_all(staging, error); });
		std::filesystem::create_directories(staging);
		WriteFileBytes(staging / L"manifest.json", { reinterpret_cast<const std::byte*>(package.manifestText.data()), package.manifestText.size() });
		if (!package.licenseText.empty())
		{
			WriteFileBytes(staging / L"LICENSE", { reinterpret_cast<const std::byte*>(package.licenseText.data()), package.licenseText.size() });
		}
		for (const auto& [name, bytes] : package.assets)
		{
			const auto path = staging / PathFromUtf8(name);
			WriteFileBytes(path, bytes);
			if (!path.wstring().ends_with(L".layout"))
			{
				ValidateImageWithWic(path);
			}
		}
		HardenDirectory(staging);
		THROW_IF_WIN32_BOOL_FALSE(MoveFileExW(staging.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH));
		cleanup.release();
		return { destination, true };
	}

	void CreateArchive(const std::filesystem::path& path, CreateRequest request)
	{
		if (!request.licenseText.empty())
		{
			ValidateLicense(request.licenseText);
			request.metadata.licenseName = InferLicenseName(request.licenseText);
		}
		THROW_HR_IF(E_INVALIDARG,
			request.metadata.name.empty()
			|| request.metadata.authorName.empty()
			|| !IsValidHomepageUrl(request.metadata.authorHomepage)
		);
		if (request.metadata.uuid.empty()) request.metadata.uuid = CreateUuid();
		THROW_HR_IF(E_INVALIDARG, !IsCanonicalUuid(request.metadata.uuid));
		THROW_HR_IF(E_INVALIDARG, request.settings.size() != Settings::PresetPackSettingCount());
		std::map<std::string, std::vector<std::byte>> assets;
		for (const auto& [name, source] : request.assetSources)
		{
			THROW_HR_IF(E_INVALIDARG, !name.starts_with("assets/") || !IsSafeEntryPath(name));
			if (!name.ends_with(".layout")) ValidateImageWithWic(source);
			std::ifstream stream(source, std::ios::binary);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !stream);
			std::vector<char> chars((std::istreambuf_iterator<char>(stream)), {});
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), chars.size() > (name.ends_with(".layout") ? MaximumMetadataSize : MaximumEntrySize));
			std::vector<std::byte> bytes(chars.size());
			if (!chars.empty()) std::memcpy(bytes.data(), chars.data(), chars.size());
			assets.emplace(name, std::move(bytes));
		}
		const auto manifest = BuildManifest(request, assets);
		THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_EXISTS), std::filesystem::exists(path));
		const auto temporary = path.wstring() + L".tmp-" + FromUtf8(CreateUuid());
		auto cleanup = wil::scope_exit([&]
		{
			std::error_code error;
			std::filesystem::remove(temporary, error);
		});
		wxFFileOutputStream output(temporary);
		THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_WRITE_FAULT), !output.IsOk());
		wxZipOutputStream zip(output, 9);
		auto writeEntry = [&](const wxString& name, std::span<const std::byte> bytes)
		{
			const wxDateTime stableTimestamp(1, wxDateTime::Jan, 2000, 0, 0, 0);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_WRITE_FAULT), !zip.PutNextEntry(name, stableTimestamp, bytes.size()));
			zip.Write(bytes.data(), bytes.size());
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_WRITE_FAULT), !zip.IsOk());
		};
		writeEntry(L"manifest.json", { reinterpret_cast<const std::byte*>(manifest.data()), manifest.size() });
		if (!request.licenseText.empty())
		{
			writeEntry(L"LICENSE", { reinterpret_cast<const std::byte*>(request.licenseText.data()), request.licenseText.size() });
		}
		for (const auto& [name, bytes] : assets)
		{
			writeEntry(wxString::FromUTF8(name), bytes);
		}
		zip.Close();
		output.Close();
		// The creator and importer deliberately share one validation path. Never publish
		// an archive that the importer would reject.
		[[maybe_unused]] const auto validated = LoadArchive(temporary);
		THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_WRITE_FAULT), !MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH));
		cleanup.release();
		THROW_IF_WIN32_BOOL_FALSE(SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY));
	}

	void Remove(const Package& package)
	{
		THROW_HR_IF(E_INVALIDARG,
			!package.deployed
			|| std::filesystem::weakly_canonical(package.source.parent_path()) != std::filesystem::weakly_canonical(GetPresetRoot())
			|| IsReparsePoint(package.source)
		);
		for (const auto& item : std::filesystem::recursive_directory_iterator(package.source))
		{
			THROW_IF_WIN32_BOOL_FALSE(SetFileAttributesW(item.path().c_str(), item.is_directory() ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL));
		}
		std::error_code error;
		std::filesystem::remove_all(package.source, error);
		THROW_HR_IF(HRESULT_FROM_WIN32(error.value()), error);
	}

	std::wstring ResolveAssetPath(const Package& package, const AssetReference& asset)
	{
		const auto root = package.deployed ? package.source : GetPresetRoot() / PathFromUtf8(package.metadata.uuid);
		const auto result = std::filesystem::weakly_canonical(root / PathFromUtf8(asset.path));
		THROW_HR_IF(E_INVALIDARG, result.wstring().size() >= MAX_PATH);
		return result.wstring();
	}
}
