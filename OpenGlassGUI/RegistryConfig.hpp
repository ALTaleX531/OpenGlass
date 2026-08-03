#pragma once
#include "pch.h"
#include "SettingsCatalog.hpp"

namespace OpenGlass
{
	class RegistryConfig
	{
	public:
		enum class Mode
		{
			Canonical,
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

		HRESULT DeleteValue(const std::wstring& valueName);
		[[nodiscard]] bool HasValue(const std::wstring& valueName) const;
		[[nodiscard]] bool HasKey() const;

		[[nodiscard]] Mode GetMode() const noexcept { return m_mode; }
		[[nodiscard]] Settings::Scope ScopeFor(const std::wstring& valueName) const noexcept;

	private:
		wil::unique_hkey OpenKey(const std::wstring& valueName, bool readOnly) const;
		[[nodiscard]] std::pair<HKEY, std::wstring> GetLocation(const std::wstring& valueName) const;
		
		Mode m_mode;
		std::wstring m_userSid;
	};
}
