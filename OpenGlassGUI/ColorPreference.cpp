#include "pch.h"
#include "ColorPreference.hpp"
#include "DwmSettings.hpp"
#include "ColorPolicy.hpp"
#include "PreviewJournal.hpp"
#include "ShellColorRefresh.hpp"

namespace OpenGlass
{
	namespace
	{
		constexpr auto AccentKey = LR"(Software\Microsoft\Windows\CurrentVersion\Explorer\Accent)";
		constexpr auto DesktopKey = LR"(Control Panel\Desktop)";
		RegistryConfig::RawValue ReadRaw(HKEY user, const wchar_t* path, const wchar_t* name)
		{
			wil::unique_hkey key;
			THROW_IF_WIN32_ERROR(RegOpenKeyExW(user, path, 0, KEY_QUERY_VALUE, key.put()));
			RegistryConfig::RawValue value;
			DWORD size{};
			auto status = RegQueryValueExW(key.get(), name, nullptr, &value.type, nullptr, &size);
			if (status == ERROR_FILE_NOT_FOUND) return {};
			THROW_IF_WIN32_ERROR(status);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), size > 4096);
			value.bytes.resize(std::max<DWORD>(size, 1));
			THROW_IF_WIN32_ERROR(RegQueryValueExW(key.get(), name, nullptr, &value.type, value.bytes.data(), &size));
			value.bytes.resize(size);
			value.present = true;
			return value;
		}
		void WriteRaw(HKEY key, const wchar_t* name, const RegistryConfig::RawValue& value)
		{
			const auto status = value.present
				? RegSetValueExW(key, name, 0, value.type, value.bytes.data(), static_cast<DWORD>(value.bytes.size()))
				: RegDeleteValueW(key, name);
			if (value.present || status != ERROR_FILE_NOT_FOUND) THROW_IF_WIN32_ERROR(status);
		}
		RegistryConfig::RawValue EncodeAccent(DWORD rgb)
		{
			// Both Windows Accent values use opaque COLORREF, unlike DWM Colorization ARGB.
			const DWORD color = RGB((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255) | 0xFF000000;
			RegistryConfig::RawValue value{ true, REG_DWORD, std::vector<BYTE>(sizeof(color)) };
			memcpy(value.bytes.data(), &color, sizeof(color));
			return value;
		}
		ColorPreference::Snapshot ChoiceOnly(ColorPreference::Snapshot value)
		{
			value.accent.reset();
			value.dwmAccent.reset();
			return value;
		}
		ColorPreference::Snapshot AccentOnly(const ColorPreference::Snapshot& value)
		{
			ColorPreference::Snapshot result;
			result.applyChoice = false;
			result.accent = value.accent;
			return result;
		}
		ColorPreference::Snapshot DwmAccentOnly(const ColorPreference::Snapshot& value)
		{
			ColorPreference::Snapshot result;
			result.applyChoice = false;
			result.dwmAccent = value.dwmAccent;
			return result;
		}
		std::optional<DWORD> ReadDword(HKEY user, const wchar_t* path, const wchar_t* name)
		{
			DWORD value{}, size = sizeof(value);
			const auto status = RegGetValueW(user, path, name, RRF_RT_REG_DWORD, nullptr, &value, &size);
			if (status == ERROR_FILE_NOT_FOUND || status == ERROR_PATH_NOT_FOUND) return {};
			THROW_IF_WIN32_ERROR(status);
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), size != sizeof(value));
			return value;
		}
		DWORD ReadColor(HKEY user)
		{
			const auto color = ReadDword(user, DwmSettings::g_keyPath, L"ColorizationColor");
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), !color);
			return *color & 0xFFFFFF;
		}
		// Check the actual token identity, including alternate-credential elevation.
		bool TokenMatches(HANDLE token, const std::wstring& sid)
		{
			DWORD size{};
			GetTokenInformation(token, TokenUser, nullptr, 0, &size);
			std::vector<BYTE> buffer(size);
			THROW_IF_WIN32_BOOL_FALSE(GetTokenInformation(token, TokenUser, buffer.data(), size, &size));
			wil::unique_hlocal_string text;
			THROW_IF_WIN32_BOOL_FALSE(ConvertSidToStringSidW(static_cast<PSID>(reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid), text.put()));
			return !_wcsicmp(text.get(), sid.c_str());
		}

		wil::unique_handle GetUserToken(const std::wstring& sid)
		{
			wil::unique_handle token;
			THROW_IF_WIN32_BOOL_FALSE(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, token.put()));
			if (!TokenMatches(token.get(), sid))
			{
				DWORD pid{}, session{}, currentSession{};
				GetWindowThreadProcessId(GetShellWindow(), &pid);
				THROW_HR_IF(E_ACCESSDENIED, !pid);
				THROW_IF_WIN32_BOOL_FALSE(ProcessIdToSessionId(pid, &session));
				THROW_IF_WIN32_BOOL_FALSE(ProcessIdToSessionId(GetCurrentProcessId(), &currentSession));
				THROW_HR_IF(E_ACCESSDENIED, session != currentSession);
				wil::unique_handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
				THROW_LAST_ERROR_IF(!process);
				THROW_IF_WIN32_BOOL_FALSE(OpenProcessToken(process.get(), TOKEN_QUERY | TOKEN_DUPLICATE, token.put()));
				THROW_HR_IF(E_ACCESSDENIED, !TokenMatches(token.get(), sid));
			}
			wil::unique_handle duplicate;
			THROW_IF_WIN32_BOOL_FALSE(DuplicateTokenEx(token.get(), TOKEN_QUERY | TOKEN_IMPERSONATE,
				nullptr, SecurityImpersonation, TokenImpersonation, duplicate.put()));
			return duplicate;
		}

		struct UserScope
		{
			wil::unique_handle previous;
			explicit UserScope(HANDLE token)
			{
				if (!OpenThreadToken(GetCurrentThread(), TOKEN_IMPERSONATE | TOKEN_QUERY, TRUE, previous.put()))
				{
					THROW_LAST_ERROR_IF(GetLastError() != ERROR_NO_TOKEN);
				}
				THROW_IF_WIN32_BOOL_FALSE(SetThreadToken(nullptr, token));
			}
			~UserScope()
			{
				FAIL_FAST_IF_WIN32_BOOL_FALSE(SetThreadToken(nullptr, previous.get()));
			}
		};

		class WindowsColorBackend final : public ColorPreference::Backend
		{
			wil::unique_hmodule shell;
			wil::unique_hkey userKey, desktopKey, accentKey, dwmKey;
			wil::unique_handle token;
			std::wstring userSid;
			HWND shellWindow{};
			bool synchronizeAutomaticAccent{};
		public:
			HRESULT Capture(const std::wstring& sid, ColorPreference::Snapshot& snapshot) noexcept override
			try
			{
				RETURN_HR_IF(E_INVALIDARG, sid.empty());
				if (!token)
				{
					token = GetUserToken(sid);
					userSid = sid;
				}
				RETURN_HR_IF(E_ACCESSDENIED, userSid != sid);
				if (!userKey) RETURN_IF_WIN32_ERROR(RegOpenKeyExW(HKEY_USERS, sid.c_str(), 0, KEY_READ, userKey.put()));
				ColorPreference::Snapshot captured;
				if (!ColorPolicy::IsAccentSyncBlocked(RegGetValueW))
					captured.accent = ReadRaw(userKey.get(), AccentKey, L"AccentColorMenu");
				captured.dwmAccent = ReadRaw(userKey.get(), DwmSettings::g_keyPath, L"AccentColor");
				captured.automatic = ReadDword(userKey.get(), DesktopKey, L"AutoColorization").value_or(0);
				if (!captured.IsAutomatic()) captured.rgb = ReadColor(userKey.get());
				snapshot = captured;
				return S_OK;
			}
			CATCH_RETURN()

			HRESULT Prepare(const ColorPreference::Snapshot& choice) noexcept override
			try
			{
				RETURN_HR_IF(E_UNEXPECTED, !token || !userKey);
				synchronizeAutomaticAccent = choice.RestoresAutomatic() && !ColorPolicy::IsAccentSyncBlocked(RegGetValueW);
				// Recovery uses recorded write targets, independent of current policy.
				if (choice.accent || synchronizeAutomaticAccent)
					RETURN_IF_WIN32_ERROR(RegOpenKeyExW(userKey.get(), AccentKey, 0, KEY_SET_VALUE, accentKey.put()));
				if (choice.dwmAccent || choice.applyChoice)
				{
					RETURN_IF_WIN32_ERROR(RegOpenKeyExW(userKey.get(), DwmSettings::g_keyPath, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, dwmKey.put()));
				}
				if (!choice.applyChoice) return S_OK;
				// Existing Desktop key and access are required before any state mutation.
				RETURN_IF_WIN32_ERROR(RegOpenKeyExW(userKey.get(), DesktopKey, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, desktopKey.put()));
				if (!choice.IsAutomatic())
				{
					RETURN_HR_IF(E_INVALIDARG, !choice.rgb || *choice.rgb > 0xFFFFFF);
					RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA),
						!ReadDword(dwmKey.get(), nullptr, L"ColorizationColor") || !ReadDword(dwmKey.get(), nullptr, L"ColorizationAfterglow"));
					return S_OK;
				}
				if (!shell) shell.reset(LoadLibraryExW(L"shell32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
				RETURN_LAST_ERROR_IF(!shell);
				// Retain the shell capability check even though delivery is synchronous.
				RETURN_HR_IF(E_NOINTERFACE, !GetProcAddress(shell.get(), MAKEINTRESOURCEA(901)));
				DWORD pid{}, session{}, currentSession{};
				shellWindow = GetShellWindow();
				GetWindowThreadProcessId(shellWindow, &pid);
				RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_NOT_READY), !pid);
				RETURN_IF_WIN32_BOOL_FALSE(ProcessIdToSessionId(pid, &session));
				RETURN_IF_WIN32_BOOL_FALSE(ProcessIdToSessionId(GetCurrentProcessId(), &currentSession));
				RETURN_HR_IF(E_ACCESSDENIED, session != currentSession);
				wil::unique_handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
				RETURN_LAST_ERROR_IF(!process);
				wil::unique_handle shellToken;
				RETURN_IF_WIN32_BOOL_FALSE(OpenProcessToken(process.get(), TOKEN_QUERY, shellToken.put()));
				RETURN_HR_IF(E_ACCESSDENIED, !TokenMatches(shellToken.get(), userSid));
				return S_OK;
			}
			CATCH_RETURN()

			HRESULT Apply(const ColorPreference::Snapshot& choice, ColorPreference::Snapshot* applied) noexcept override
			try
			{
				RETURN_IF_FAILED(Prepare(choice));
				UserScope scope(token.get());
				auto result = choice;
				if (choice.applyChoice)
				{
					const DWORD mode = choice.automatic;
					RETURN_IF_WIN32_ERROR(RegSetValueExW(desktopKey.get(), L"AutoColorization", 0, REG_DWORD,
						reinterpret_cast<const BYTE*>(&mode), sizeof(mode)));
				}
				if (choice.RestoresAutomatic())
				{
					RETURN_IF_FAILED(ShellColorRefresh::Request(shellWindow));
					// The shell may update glass RGB while its same-preference shortcut
					// leaves both Accent values stale. Use the completed result, not old RGB.
					const auto accent = EncodeAccent(ReadColor(userKey.get()));
					result = ChoiceOnly(choice);
					if (synchronizeAutomaticAccent) result.accent = accent;
					result.dwmAccent = accent;
				}
				if (result.accent && ReadRaw(userKey.get(), AccentKey, L"AccentColorMenu") != *result.accent)
					WriteRaw(accentKey.get(), L"AccentColorMenu", *result.accent);
				// Publish AccentColorMenu first, then the persistent colors consumed by DWM/OpenGlass.
				// The caller journals both base values independently before this operation.
				if (choice.applyChoice && !choice.IsAutomatic())
				{
					for (const auto name : { L"ColorizationColor", L"ColorizationAfterglow" })
					{
						const auto before = ReadDword(dwmKey.get(), nullptr, name);
						RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), !before);
						const DWORD color = (*before & 0xFF000000) | *choice.rgb;
						if (color != *before) RETURN_IF_WIN32_ERROR(RegSetValueExW(dwmKey.get(), name, 0, REG_DWORD,
							reinterpret_cast<const BYTE*>(&color), sizeof(color)));
					}
				}
				if (result.dwmAccent && ReadRaw(userKey.get(), DwmSettings::g_keyPath, L"AccentColor") != *result.dwmAccent)
					WriteRaw(dwmKey.get(), L"AccentColor", *result.dwmAccent);
				if (applied) *applied = std::move(result);
				return S_OK;
			}
			CATCH_RETURN()
		};
	}

	struct ColorPreference::State
	{
		std::unique_ptr<Backend> backend;
		PreviewJournal<int, Snapshot> journal;
		explicit State(std::unique_ptr<Backend> value) : backend(std::move(value)) {}
	};
	ColorPreference::ColorPreference() : ColorPreference(std::make_unique<WindowsColorBackend>()) {}
	ColorPreference::ColorPreference(std::unique_ptr<Backend> backend) : m_state(std::make_unique<State>(std::move(backend))) {}
	ColorPreference::~ColorPreference() = default;

	HRESULT ColorPreference::Capture(const std::wstring& userSid, Snapshot& snapshot) noexcept
	{
		return m_state->backend->Capture(userSid, snapshot);
	}
	HRESULT ColorPreference::ReadAutoColorization(const std::wstring& userSid, bool& enabled) noexcept
	try
	{
		RETURN_HR_IF(E_INVALIDARG, userSid.empty());
		wil::unique_hkey user;
		RETURN_IF_WIN32_ERROR(RegOpenKeyExW(HKEY_USERS, userSid.c_str(), 0, KEY_READ, user.put()));
		enabled = ReadDword(user.get(), DesktopKey, L"AutoColorization").value_or(0) != 0;
		return S_OK;
	}
	CATCH_RETURN()
	HRESULT ColorPreference::ReadRgb(const std::wstring& userSid, DWORD& rgb) noexcept
	try
	{
		RETURN_HR_IF(E_INVALIDARG, userSid.empty());
		wil::unique_hkey user;
		RETURN_IF_WIN32_ERROR(RegOpenKeyExW(HKEY_USERS, userSid.c_str(), 0, KEY_READ, user.put()));
		rgb = ReadColor(user.get());
		return S_OK;
	}
	CATCH_RETURN()

	HRESULT ColorPreference::Apply(const std::wstring& userSid, std::optional<DWORD> argb,
		const std::function<void(const Snapshot&)>& beforeWrite) noexcept
	try
	{
		Snapshot before;
		RETURN_IF_FAILED(Capture(userSid, before));
		Snapshot desired{ argb ? 0u : 1u, argb ? std::optional<DWORD>(*argb & 0xFFFFFF) : std::nullopt };
		if (argb && before.accent && *before.accent != EncodeAccent(*argb)) desired.accent = EncodeAccent(*argb);
		if (argb && !desired.accent) before.accent.reset();
		if (argb && before.dwmAccent && *before.dwmAccent != EncodeAccent(*argb)) desired.dwmAccent = EncodeAccent(*argb);
		// Automatic may replace a Manual AccentColor through the shell. An existing
		// Automatic value is derived state and is recovered by recomputation instead.
		if (argb ? !desired.dwmAccent : before.IsAutomatic()) before.dwmAccent.reset();
		// Check both forward and recovery capabilities before any mutation.
		RETURN_IF_FAILED(m_state->backend->Prepare(desired));
		RETURN_IF_FAILED(m_state->backend->Prepare(before));
		if (beforeWrite)
		{
			auto recovery = before;
			if (recovery.RestoresAutomatic()) recovery.dwmAccent.reset();
			beforeWrite(recovery);
		}
		auto& journal = m_state->journal;
		journal.Begin();
		journal.Touch(0, [&](int) { return ChoiceOnly(before); });
		// The shell request may change Accent before its final RGB is known. Keep
		// operation backups first; reconciliation removes unchanged targets.
		if (before.accent) journal.Touch(1, [&](int) { return AccentOnly(before); });
		if (before.dwmAccent) journal.Touch(2, [&](int) { return DwmAccentOnly(before); });
		Snapshot applied;
		RETURN_IF_FAILED(m_state->backend->Apply(desired, &applied));
		// Track our choice and write targets, never subsequent asynchronous output.
		journal.Reconcile([&](int key)
		{
			return key == 0 ? ChoiceOnly(applied) : key == 1
				? AccentOnly(applied.accent ? applied : before) : DwmAccentOnly(applied);
		});
		const auto baseline = Baseline();
		if (!argb && before.IsAutomatic())
		{
			// Even an unchanged Automatic choice has shell side effects. A later
			// failure in this attempt must recompute them, without making Save dirty.
			journal.Touch(0, [&](int) { return ChoiceOnly(before); });
			if (!baseline || !baseline->applyChoice) journal.DiscardBaseline(0);
		}
		if (!argb && (!baseline || !baseline->applyChoice || baseline->RestoresAutomatic()))
		{
			journal.DiscardBaseline(1);
			journal.DiscardBaseline(2);
		}
		return S_OK;
	}
	CATCH_RETURN()

	HRESULT ColorPreference::RollbackAttempt() noexcept
	{
		if (!m_state->journal.IsAttemptActive()) return S_OK;
		const auto result = RestoreChanges(true);
		m_state->journal.RollbackAttempt([](int, const auto&) { return true; }, SUCCEEDED(result));
		return result;
	}
	HRESULT ColorPreference::RecoverSnapshot(const Snapshot& snapshot) noexcept
	{
		// Check the complete recovery before restoring its independent raw values.
		RETURN_IF_FAILED(m_state->backend->Prepare(snapshot));
		if (snapshot.RestoresAutomatic() && snapshot.accent)
		{
			// Match in-session Revert: restore the recorded preference before asking
			// the shell to recompute Automatic. It is not a new color selection.
			RETURN_IF_FAILED(m_state->backend->Apply(AccentOnly(snapshot)));
			return m_state->backend->Apply(ChoiceOnly(snapshot));
		}
		return m_state->backend->Apply(snapshot);
	}
	std::optional<ColorPreference::Snapshot> ColorPreference::Baseline(bool attempt) const
	{
		std::optional<Snapshot> result;
		m_state->journal.VisitRestore(attempt, [&](int key, const Snapshot& value)
		{
			if (!result) result = value;
			else if (key == 1) result->accent = value.accent;
			else if (key == 2) result->dwmAccent = value.dwmAccent;
		});
		if (result && result->RestoresAutomatic()) result->dwmAccent.reset();
		return result;
	}
	HRESULT ColorPreference::RestoreChanges(bool attempt) noexcept
	{
		HRESULT result = S_OK;
		bool restoreAutomatic{};
		m_state->journal.VisitRestore(attempt, [&](int key, const Snapshot& value)
		{
			if (key == 0) restoreAutomatic = value.RestoresAutomatic();
		});
		// Restore the Windows preference first; raw DWM Accent follows Manual RGB.
		for (const auto target : { 1, 0, 2 }) m_state->journal.VisitRestore(attempt, [&](int key, const Snapshot& value)
		{
			if (key != target) return;
			if (key == 2 && restoreAutomatic) return;
			const auto status = m_state->backend->Apply(value);
			if (FAILED(status)) result = status;
		});
		return result;
	}
	void ColorPreference::CommitAttempt() noexcept { m_state->journal.CommitAttempt(); }
	HRESULT ColorPreference::Revert() noexcept { return RestoreChanges(false); }
	void ColorPreference::Accept() noexcept { m_state->journal.Accept(); }
	bool ColorPreference::IsDirty() const noexcept { return m_state->journal.IsDirty(); }
	bool ColorPreference::IsAttemptActive() const noexcept { return m_state->journal.IsAttemptActive(); }
}
