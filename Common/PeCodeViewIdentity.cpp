#include "PeCodeViewIdentity.hpp"

#include <Psapi.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <span>

#include <wil/resource.h>

namespace OpenGlass
{
	namespace
	{
		struct RsdsHeader
		{
			DWORD signature;
			GUID guid;
			DWORD age;
		};

		constexpr DWORD RsdsSignature = 'SDSR';

		[[nodiscard]] bool CheckedRange(size_t offset, size_t length, size_t size) noexcept
		{
			return offset <= size && length <= size - offset;
		}

		template <typename T>
		[[nodiscard]] const T* At(std::span<const std::byte> bytes, size_t offset) noexcept
		{
			return CheckedRange(offset, sizeof(T), bytes.size())
				? reinterpret_cast<const T*>(bytes.data() + offset)
				: nullptr;
		}

		[[nodiscard]] bool SameIdentity(const PeCodeViewIdentity& left, const PeCodeViewIdentity& right) noexcept
		{
			return
				left.machine == right.machine &&
				left.timeDateStamp == right.timeDateStamp &&
				left.sizeOfImage == right.sizeOfImage &&
				InlineIsEqualGUID(left.pdbGuid, right.pdbGuid) &&
				left.pdbAge == right.pdbAge &&
				CompareStringOrdinal(left.pdbName.c_str(), -1, right.pdbName.c_str(), -1, TRUE) == CSTR_EQUAL;
		}

		[[nodiscard]] HRESULT DecodePdbName(
			std::span<const std::byte> record,
			std::wstring& pdbName
		) noexcept
		{
			if (record.size() <= sizeof(RsdsHeader))
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
			}
			const auto nameBytes = record.subspan(sizeof(RsdsHeader));
			const auto terminator = std::ranges::find(nameBytes, std::byte{});
			if (terminator == nameBytes.end())
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
			}
			const size_t length = static_cast<size_t>(terminator - nameBytes.begin());
			if (!length || length > static_cast<size_t>((std::numeric_limits<int>::max)()))
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
			}
			const auto name = reinterpret_cast<const char*>(nameBytes.data());
			const char* basename = name;
			for (size_t index = 0; index < length; index++)
			{
				if (name[index] == '\\' || name[index] == '/')
				{
					basename = name + index + 1;
				}
			}
			const int basenameLength = static_cast<int>(name + length - basename);
			if (!basenameLength)
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
			}
			const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, basename, basenameLength, nullptr, 0);
			if (!wideLength)
			{
				return HRESULT_FROM_WIN32(GetLastError());
			}
			try
			{
				std::wstring decoded(static_cast<size_t>(wideLength), L'\0');
				if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, basename, basenameLength, decoded.data(), wideLength) != wideLength)
				{
					return HRESULT_FROM_WIN32(GetLastError());
				}
				pdbName = std::move(decoded);
			}
			catch (const std::bad_alloc&)
			{
				return E_OUTOFMEMORY;
			}
			return S_OK;
		}

		[[nodiscard]] bool RvaToFileOffset(
			std::span<const std::byte> bytes,
			const IMAGE_FILE_HEADER& fileHeader,
			size_t sectionTableOffset,
			DWORD rva,
			size_t length,
			size_t& fileOffset
		) noexcept
		{
			if (!rva)
			{
				return false;
			}
			for (WORD index = 0; index < fileHeader.NumberOfSections; index++)
			{
				const auto section = At<IMAGE_SECTION_HEADER>(bytes, sectionTableOffset + static_cast<size_t>(index) * sizeof(IMAGE_SECTION_HEADER));
				if (!section)
				{
					return false;
				}
				if (rva < section->VirtualAddress)
				{
					continue;
				}
				const DWORD sectionOffset = rva - section->VirtualAddress;
				if (sectionOffset > section->SizeOfRawData || length > section->SizeOfRawData - sectionOffset)
				{
					continue;
				}
				const size_t candidate = static_cast<size_t>(section->PointerToRawData) + sectionOffset;
				if (!CheckedRange(candidate, length, bytes.size()))
				{
					return false;
				}
				fileOffset = candidate;
				return true;
			}
			return false;
		}

		[[nodiscard]] HRESULT ParseIdentity(
			std::span<const std::byte> bytes,
			bool loadedImage,
			PeCodeViewIdentity& identity
		) noexcept
		{
			const auto dos = At<IMAGE_DOS_HEADER>(bytes, 0);
			if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0)
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			}
			const size_t ntOffset = static_cast<size_t>(dos->e_lfanew);
			const auto signature = At<DWORD>(bytes, ntOffset);
			const auto fileHeader = At<IMAGE_FILE_HEADER>(bytes, ntOffset + sizeof(DWORD));
			if (!signature || *signature != IMAGE_NT_SIGNATURE || !fileHeader)
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			}
			const size_t optionalOffset = ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER);
			if (!CheckedRange(optionalOffset, fileHeader->SizeOfOptionalHeader, bytes.size()))
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			}

			DWORD sizeOfImage{};
			IMAGE_DATA_DIRECTORY debugDirectory{};
			const auto magic = At<WORD>(bytes, optionalOffset);
			if (!magic)
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			}
			if (*magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
			{
				const auto optional = At<IMAGE_OPTIONAL_HEADER64>(bytes, optionalOffset);
				if (!optional || fileHeader->SizeOfOptionalHeader < offsetof(IMAGE_OPTIONAL_HEADER64, DataDirectory) + sizeof(IMAGE_DATA_DIRECTORY) * (IMAGE_DIRECTORY_ENTRY_DEBUG + 1) || optional->NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DEBUG)
				{
					return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
				}
				sizeOfImage = optional->SizeOfImage;
				debugDirectory = optional->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
			}
			else if (*magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
			{
				const auto optional = At<IMAGE_OPTIONAL_HEADER32>(bytes, optionalOffset);
				if (!optional || fileHeader->SizeOfOptionalHeader < offsetof(IMAGE_OPTIONAL_HEADER32, DataDirectory) + sizeof(IMAGE_DATA_DIRECTORY) * (IMAGE_DIRECTORY_ENTRY_DEBUG + 1) || optional->NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_DEBUG)
				{
					return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
				}
				sizeOfImage = optional->SizeOfImage;
				debugDirectory = optional->DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
			}
			else
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			}
			if (!sizeOfImage || !debugDirectory.VirtualAddress || debugDirectory.Size < sizeof(IMAGE_DEBUG_DIRECTORY) || debugDirectory.Size % sizeof(IMAGE_DEBUG_DIRECTORY))
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
			}

			const size_t sectionTableOffset = optionalOffset + fileHeader->SizeOfOptionalHeader;
			if (!CheckedRange(sectionTableOffset, static_cast<size_t>(fileHeader->NumberOfSections) * sizeof(IMAGE_SECTION_HEADER), bytes.size()))
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_EXE_FORMAT);
			}
			size_t debugOffset{};
			if (loadedImage)
			{
				debugOffset = debugDirectory.VirtualAddress;
				if (!CheckedRange(debugOffset, debugDirectory.Size, bytes.size()))
				{
					return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
				}
			}
			else if (!RvaToFileOffset(bytes, *fileHeader, sectionTableOffset, debugDirectory.VirtualAddress, debugDirectory.Size, debugOffset))
			{
				return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
			}

			PeCodeViewIdentity result{};
			result.machine = fileHeader->Machine;
			result.timeDateStamp = fileHeader->TimeDateStamp;
			result.sizeOfImage = sizeOfImage;
			bool found{};
			for (size_t offset = 0; offset < debugDirectory.Size; offset += sizeof(IMAGE_DEBUG_DIRECTORY))
			{
				const auto entry = At<IMAGE_DEBUG_DIRECTORY>(bytes, debugOffset + offset);
				if (!entry || entry->Type != IMAGE_DEBUG_TYPE_CODEVIEW || entry->SizeOfData < sizeof(RsdsHeader) + 1)
				{
					continue;
				}
				size_t recordOffset{};
				if (loadedImage)
				{
					recordOffset = entry->AddressOfRawData;
					if (!CheckedRange(recordOffset, entry->SizeOfData, bytes.size()))
					{
						return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
					}
				}
				else
				{
					recordOffset = entry->PointerToRawData;
					if (!CheckedRange(recordOffset, entry->SizeOfData, bytes.size()))
					{
						return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
					}
				}
				const auto record = bytes.subspan(recordOffset, entry->SizeOfData);
				const auto rsds = At<RsdsHeader>(record, 0);
				if (!rsds || rsds->signature != RsdsSignature)
				{
					continue;
				}
				PeCodeViewIdentity candidate{};
				candidate.machine = result.machine;
				candidate.timeDateStamp = result.timeDateStamp;
				candidate.sizeOfImage = result.sizeOfImage;
				candidate.pdbGuid = rsds->guid;
				candidate.pdbAge = rsds->age;
				const HRESULT nameResult = DecodePdbName(record, candidate.pdbName);
				if (FAILED(nameResult))
				{
					return nameResult;
				}
				if (found && !SameIdentity(result, candidate))
				{
					return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
				}
				result = std::move(candidate);
				found = true;
			}
			if (!found)
			{
				return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
			}
			identity = std::move(result);
			return S_OK;
		}
	}

	HRESULT ReadLoadedPeCodeViewIdentity(HMODULE module, PeCodeViewIdentity& identity) noexcept
	{
		if (!module)
		{
			return E_INVALIDARG;
		}
		MODULEINFO moduleInfo{};
		if (!GetModuleInformation(GetCurrentProcess(), module, &moduleInfo, sizeof(moduleInfo)) || !moduleInfo.lpBaseOfDll || !moduleInfo.SizeOfImage)
		{
			return HRESULT_FROM_WIN32(GetLastError());
		}
		return ParseIdentity(
			std::span{static_cast<const std::byte*>(moduleInfo.lpBaseOfDll), static_cast<size_t>(moduleInfo.SizeOfImage)},
			true,
			identity
		);
	}

	HRESULT ReadFilePeCodeViewIdentity(PCWSTR path, PeCodeViewIdentity& identity) noexcept
	{
		if (!path || !*path)
		{
			return E_INVALIDARG;
		}
		wil::unique_hfile file
		{
			CreateFileW(
				path,
				GENERIC_READ,
				FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr,
				OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL,
				nullptr
			)
		};
		if (!file.is_valid())
		{
			return HRESULT_FROM_WIN32(GetLastError());
		}
		LARGE_INTEGER size{};
		if (!GetFileSizeEx(file.get(), &size))
		{
			return HRESULT_FROM_WIN32(GetLastError());
		}
		if (size.QuadPart <= 0 || static_cast<ULONGLONG>(size.QuadPart) > static_cast<ULONGLONG>((std::numeric_limits<size_t>::max)()))
		{
			return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
		}
		wil::unique_handle mapping
		{
			CreateFileMappingW(file.get(), nullptr, PAGE_READONLY, 0, 0, nullptr)
		};
		if (!mapping)
		{
			return HRESULT_FROM_WIN32(GetLastError());
		}
		wil::unique_mapview_ptr<const std::byte> view
		{
			static_cast<const std::byte*>(MapViewOfFile(mapping.get(), FILE_MAP_READ, 0, 0, 0))
		};
		if (!view)
		{
			return HRESULT_FROM_WIN32(GetLastError());
		}
		return ParseIdentity(std::span{view.get(), static_cast<size_t>(size.QuadPart)}, false, identity);
	}
}
