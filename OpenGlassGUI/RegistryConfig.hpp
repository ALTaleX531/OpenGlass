#pragma once
#include <Windows.h>
#include <wil/resource.h>
#include <string>
#include <vector>
#include "SettingsCatalog.hpp"

namespace OpenGlass
{
	class RegistryConfig
	{
	public:
		struct RawValue
		{
			bool present{};
			DWORD type{};
			std::vector<BYTE> bytes;
			bool operator==(const RawValue&) const = default;
		};
		[[nodiscard]] RawValue ReadRaw(const std::wstring& name) const;
		HRESULT WriteRaw(const std::wstring& name, const RawValue& value);
		enum class Mode
		{
			User,
			Machine
		};

		RegistryConfig(Mode mode, std::wstring userSid = {});

		[[nodiscard]] DWORD GetDword(const std::wstring& valueName, DWORD defaultValue) const;
		HRESULT SetDword(const std::wstring& valueName, DWORD value);
		[[nodiscard]] bool TryGetDword(const std::wstring& valueName, DWORD& value) const;

		[[nodiscard]] std::wstring GetString(const std::wstring& valueName, const std::wstring& defaultValue) const;
		HRESULT SetString(const std::wstring& valueName, const std::wstring& value);
		[[nodiscard]] bool TryGetString(const std::wstring& valueName, std::wstring& value) const;

		HRESULT CheckDeleteAccess() const;
		HRESULT DeleteValue(const std::wstring& valueName);

		const std::wstring& UserSid() const noexcept { return m_userSid; }
		[[nodiscard]] Mode GetMode() const noexcept { return m_mode; }

	private:
		wil::unique_hkey OpenKey(bool readOnly, PCWSTR valueName) const;
		[[nodiscard]] std::pair<HKEY, std::wstring> GetLocation(PCWSTR valueName = nullptr) const;

		Mode m_mode;
		std::wstring m_userSid;
	};
}
