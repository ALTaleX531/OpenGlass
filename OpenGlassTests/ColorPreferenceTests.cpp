#include "pch.h"
#include "TestHarness.hpp"
#include "../OpenGlassGUI/ColorPreference.hpp"
#include "../OpenGlassGUI/EffectiveConfiguration.hpp"
#include "../OpenGlassGUI/PreviewJournal.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestAutomaticColorPreview()
	{
		using Snapshot = ColorPreference::Snapshot;
		struct Backend final : ColorPreference::Backend
		{
			Snapshot live;
			DWORD wallpaper{ 0x123456 }, derived{};
			bool reject{}, fail{}, failAfterAccent{}, failAfterDwmAccent{}, shellWritesAccent{ true };
			unsigned writes{}, refreshes{};
			HRESULT Capture(const std::wstring&, Snapshot& value) noexcept override { value = live; return S_OK; }
			HRESULT Prepare(const Snapshot&) noexcept override { return reject ? E_NOINTERFACE : S_OK; }
			HRESULT Apply(const Snapshot& value, Snapshot* applied) noexcept override
			{
				++writes;
				if (value.applyChoice) live.automatic = value.automatic; // Failure may follow a partial mutation.
				if (fail) return E_FAIL;
				if (value.accent && !value.RestoresAutomatic()) live.accent = value.accent;
				if (value.applyChoice)
				{
					live.rgb = value.rgb;
					if (value.IsAutomatic()) { ++refreshes; derived = wallpaper; }
					else derived = *value.rgb;
				}
				if (value.RestoresAutomatic() && live.accent && shellWritesAccent)
					live.accent = RegistryConfig::RawValue{ true, REG_DWORD,
						{ static_cast<BYTE>(wallpaper >> 16), static_cast<BYTE>(wallpaper >> 8), static_cast<BYTE>(wallpaper), 0xFF } };
				if (failAfterAccent) return E_FAIL;
				if (value.RestoresAutomatic() && live.dwmAccent)
					live.dwmAccent = RegistryConfig::RawValue{ true, REG_DWORD,
						{ static_cast<BYTE>(wallpaper >> 16), static_cast<BYTE>(wallpaper >> 8), static_cast<BYTE>(wallpaper), 0xFF } };
				else if (value.dwmAccent) live.dwmAccent = value.dwmAccent;
				if (failAfterDwmAccent) return E_FAIL;
				if (applied)
				{
					*applied = value;
					if (value.RestoresAutomatic())
					{
						applied->accent = live.accent;
						applied->dwmAccent = live.dwmAccent;
					}
				}
				return S_OK;
			}
		};
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			ColorPreference fresh(std::move(backend));
			const Snapshot persisted{ DWORD{0}, DWORD{0xABCDEF} };
			Check(SUCCEEDED(fresh.RecoverSnapshot(persisted)) && live->writes == 1 && live->live == persisted && !fresh.IsDirty());
			live->reject = true;
			Check(FAILED(fresh.RecoverSnapshot(persisted)) && live->writes == 1);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 0u, 0x445566u };
			Snapshot persisted{ 1u, std::nullopt };
			persisted.accent = RegistryConfig::RawValue{ true, REG_BINARY, { 1, 2, 3 } };
			ColorPreference preview(std::move(backend));
			// An interrupted Manual edit may have an Automatic baseline and an
			// independent raw Accent backup. Restore it before recomputing Automatic.
			live->reject = true;
			Check(FAILED(preview.RecoverSnapshot(persisted)) && live->writes == 0 && !live->live.IsAutomatic());
			live->reject = false;
			live->fail = true;
			Check(FAILED(preview.RecoverSnapshot(persisted)) && live->refreshes == 0 && !live->live.IsAutomatic());
			live->fail = false;
			Check(SUCCEEDED(preview.RecoverSnapshot(persisted)) && live->live.IsAutomatic() && live->refreshes == 1
				&& live->live.accent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } });
		}
		for (const auto& raw : { RegistryConfig::RawValue{}, RegistryConfig::RawValue{ true, REG_BINARY, { 1, 2, 3 } } })
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			Snapshot original{ 0u, 0x112233u }; original.accent = raw;
			live->live = original;
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566)) && live->live.accent == raw);
			preview.CommitAttempt();
			Check(!preview.Baseline()->accent); // Manual preview never owns AccentColorMenu.
			Snapshot recovery;
			Check(SUCCEEDED(preview.SaveAccent(L"user", 0x445566, [&](const Snapshot& before) { recovery = before; })));
			Check(!recovery.applyChoice && recovery.accent == raw && !recovery.dwmAccent);
			Check(live->live.accent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x44, 0x55, 0x66, 0xFF } });
			preview.CommitAttempt();
			Check(SUCCEEDED(preview.RecoverSnapshot(recovery)) && live->live.accent == raw && live->live.rgb == DWORD{0x445566});
			Check(SUCCEEDED(preview.Revert()) && live->live == original);
			preview.Accept();
			Check(SUCCEEDED(preview.Apply(L"user", 0x778899))); preview.CommitAttempt();
			const auto manualPreview = live->live;
			live->reject = true;
			Check(FAILED(preview.SaveAccent(L"user", 0x778899)) && live->live == manualPreview);
			live->reject = false;
			live->failAfterAccent = true;
			Check(FAILED(preview.SaveAccent(L"user", 0x778899)) && live->live.accent != raw);
			live->failAfterAccent = false;
			Check(SUCCEEDED(preview.RollbackAttempt()) && live->live == manualPreview && preview.IsDirty());
			Check(SUCCEEDED(preview.SaveAccent(L"user", 0x778899))); preview.CommitAttempt();
			preview.Accept();
			Check(!preview.IsDirty() && live->live.rgb == DWORD{0x778899});
			const auto writes = live->writes;
			Check(SUCCEEDED(preview.SaveAccent(L"user", 0x778899, [](const Snapshot&) { Check(false); })));
			Check(live->writes == writes && !preview.IsDirty() && !preview.IsAttemptActive());
		}

		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 0u, 0x445566u };
			live->live.accent = RegistryConfig::RawValue{ true, REG_DWORD, { 0x11, 0x22, 0x33, 0xFF } };
			ColorPreference preview(std::move(backend));
			bool recordedAccent{};
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233, [&](const Snapshot& before) { recordedAccent = before.accent.has_value(); })));
			Check(!recordedAccent && !preview.Baseline()->accent); // Manual preview never writes or backs up Accent.
			preview.CommitAttempt();
			const RegistryConfig::RawValue external{ true, REG_DWORD, { 7, 8, 9, 0xFF } };
			live->live.accent = external;
			Check(SUCCEEDED(preview.Revert()) && live->live.rgb == DWORD{0x445566} && live->live.accent == external);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 0u, 0x112233u };
			live->live.accent = RegistryConfig::RawValue{};
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233)) && !preview.IsDirty()); preview.CommitAttempt();
			Check(SUCCEEDED(preview.SaveAccent(L"user", 0x112233))); preview.CommitAttempt();
			Check(preview.IsDirty() && !preview.Baseline()->applyChoice && preview.Baseline()->accent);
			live->live.automatic = 1; // Untouched mode changed externally during an Accent-only preview.
			Check(SUCCEEDED(preview.Revert()) && live->live.automatic == 1 && !live->live.accent->present);
		}
		for (const auto& raw : { RegistryConfig::RawValue{}, RegistryConfig::RawValue{ true, REG_BINARY, { 1, 2, 3 } },
			RegistryConfig::RawValue{ true, REG_DWORD, { 0x11, 0x22, 0x33, 0x77 } } })
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			Snapshot original{ 0u, 0x112233u }; original.dwmAccent = raw;
			live->live = original; // DWM AccentColor has its own preview and recovery identity.
			ColorPreference preview(std::move(backend));
			Snapshot persisted;
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566, [&](const Snapshot& before) { persisted = before; })));
			Check(!live->live.accent && live->live.dwmAccent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x44, 0x55, 0x66, 0xFF } });
			Check(persisted.dwmAccent == raw); preview.CommitAttempt();
			const auto first = live->live;
			live->failAfterDwmAccent = true;
			Check(FAILED(preview.Apply(L"user", 0x778899)));
			live->failAfterDwmAccent = false;
			Check(SUCCEEDED(preview.RollbackAttempt()) && live->live == first && preview.IsDirty());
			// Automatic may replace AccentColor before a later step fails; preserve the Manual attempt baseline.
			live->failAfterDwmAccent = true;
			Check(FAILED(preview.Apply(L"user", std::nullopt)) && live->live.IsAutomatic()
				&& live->live.dwmAccent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } });
			live->failAfterDwmAccent = false;
			Check(SUCCEEDED(preview.RollbackAttempt()) && live->live == first && preview.IsDirty());
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt))); preview.CommitAttempt();
			Check(live->live.dwmAccent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } });
			Check(SUCCEEDED(preview.Revert()) && live->live == original); // Includes Manual -> Automatic.
			Check(SUCCEEDED(preview.RecoverSnapshot(persisted)) && live->live == original);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 0u, 0x112233u };
			const RegistryConfig::RawValue raw{ true, REG_DWORD, { 0x11, 0x22, 0x33, 0xFF } };
			live->live.dwmAccent = raw;
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233, [&](const Snapshot& before) { Check(!before.dwmAccent); })));
			Check(!preview.IsDirty()); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233)) && !preview.IsDirty()); preview.CommitAttempt();
			// An equal, untouched DWM Accent survives Revert of a different Manual RGB.
			live->live.rgb = 0x778899;
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233))); preview.CommitAttempt();
			Check(!preview.Baseline()->dwmAccent);
			const RegistryConfig::RawValue external{ true, REG_BINARY, { 7, 8, 9 } };
			live->live.dwmAccent = external;
			Check(SUCCEEDED(preview.Revert()) && live->live.dwmAccent == external);
			preview.Accept();
			// A DWM Accent-only correction must not own mode or Manual RGB.
			Check(SUCCEEDED(preview.Apply(L"user", 0x778899))); preview.CommitAttempt();
			Check(!preview.Baseline()->applyChoice && preview.Baseline()->dwmAccent == external);
			live->live.automatic = 1;
			live->fail = true;
			Check(FAILED(preview.Revert()) && preview.IsDirty());
			live->fail = false;
			Check(SUCCEEDED(preview.Revert()) && live->live.automatic == 1 && live->live.dwmAccent == external);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 1u, std::nullopt };
			live->live.dwmAccent = RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } };
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566, [](const Snapshot& before)
			{
				Check(before.RestoresAutomatic() && !before.dwmAccent); // Durable recovery must recompute it.
			}))); preview.CommitAttempt();
			Check(!preview.Baseline()->dwmAccent);
			Check(SUCCEEDED(preview.Apply(L"user", 0x778899))); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt)) && !preview.IsDirty()); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
			live->wallpaper = 0xABCDEF;
			Check(SUCCEEDED(preview.Revert()) && live->live.IsAutomatic()
				&& live->live.dwmAccent == RegistryConfig::RawValue{ true, REG_DWORD, { 0xAB, 0xCD, 0xEF, 0xFF } });
			preview.Accept();
			Check(SUCCEEDED(preview.Apply(L"user", 0x112233)));
			Check(SUCCEEDED(preview.RollbackAttempt()) && !preview.IsDirty() && live->live.IsAutomatic()
				&& live->live.dwmAccent == RegistryConfig::RawValue{ true, REG_DWORD, { 0xAB, 0xCD, 0xEF, 0xFF } });
		}
		for (const auto& raw : { RegistryConfig::RawValue{}, RegistryConfig::RawValue{ true, REG_BINARY, { 7, 8, 9 } },
			RegistryConfig::RawValue{ true, REG_DWORD, { 0x44, 0x55, 0x66, 0xFF } } })
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			Snapshot original{ 0u, 0x445566u };
			original.accent = raw; original.dwmAccent = raw;
			live->live = original;
			ColorPreference preview(std::move(backend));
			Snapshot recovery;
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt, [&](const Snapshot& before) { recovery = before; })));
			Check(live->live.accent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } }
				&& live->live.dwmAccent == live->live.accent);
			preview.CommitAttempt();
			Check(preview.Baseline()->accent == raw && preview.Baseline()->dwmAccent == raw);
			Check(SUCCEEDED(preview.Revert()) && live->live == original);
			Check(SUCCEEDED(preview.RecoverSnapshot(recovery)) && live->live == original);
			preview.Accept();
			live->failAfterAccent = true;
			Check(FAILED(preview.Apply(L"user", std::nullopt)) && live->live.accent != raw && live->live.dwmAccent == raw);
			live->failAfterAccent = false;
			Check(SUCCEEDED(preview.RollbackAttempt()) && live->live == original && !preview.IsDirty());
			// A correction failure retains both raw values for retry, including absence/type.
			live->failAfterDwmAccent = true;
			Check(FAILED(preview.Apply(L"user", std::nullopt)) && preview.IsDirty());
			Check(FAILED(preview.RollbackAttempt()) && preview.IsDirty());
			live->failAfterDwmAccent = false;
			Check(SUCCEEDED(preview.Revert()) && live->live == original);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 0u, 0x445566u };
			live->live.accent = RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } };
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt)) && !preview.Baseline()->accent);
			preview.CommitAttempt();
			const RegistryConfig::RawValue external{ true, REG_BINARY, { 9, 8, 7 } };
			live->live.accent = external;
			Check(SUCCEEDED(preview.Revert()) && live->live.accent == external); // Equal target remains unowned.
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 1u, std::nullopt };
			live->live.accent = RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } };
			live->live.dwmAccent = live->live.accent;
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
			live->wallpaper = 0xABCDEF;
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt)) && !preview.IsDirty()); preview.CommitAttempt();
			Check(live->live.accent == RegistryConfig::RawValue{ true, REG_DWORD, { 0xAB, 0xCD, 0xEF, 0xFF } }
				&& live->live.dwmAccent == live->live.accent); // Automatic owns derived Accent state.
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			live->live = Snapshot{ 1u, std::nullopt };
			live->live.accent = RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } };
			live->live.dwmAccent = live->live.accent;
			ColorPreference preview(std::move(backend));
			live->wallpaper = 0xABCDEF;
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt)) && !preview.IsDirty());
			// A later preset/Override step can fail after a successful same-mode refresh.
			Check(preview.Baseline(true) && preview.Baseline(true)->RestoresAutomatic());
			live->wallpaper = 0x778899;
			Check(SUCCEEDED(preview.RollbackAttempt()) && !preview.IsDirty() && live->refreshes == 2
				&& live->live.accent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x77, 0x88, 0x99, 0xFF } }
				&& live->live.dwmAccent == live->live.accent);
		}
		{
			auto backend = std::make_unique<Backend>(); auto* live = backend.get();
			const RegistryConfig::RawValue original{ true, REG_BINARY, { 1, 2, 3 } };
			live->live = Snapshot{ 0u, 0x445566u }; live->live.accent = original;
			live->shellWritesAccent = false;
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt))); preview.CommitAttempt();
			Check(live->live.accent == original && !preview.Baseline()->accent);
			Check(SUCCEEDED(preview.SaveAccent(L"user", live->derived))); preview.CommitAttempt();
			Check(live->live.IsAutomatic() && live->live.accent == RegistryConfig::RawValue{ true, REG_DWORD, { 0x12, 0x34, 0x56, 0xFF } });
			Check(SUCCEEDED(preview.Revert()) && live->live.accent == original && live->live.rgb == DWORD{0x445566});
		}
		for (const DWORD mode : { 0u, 1u, 2u })
		{
			auto backend = std::make_unique<Backend>();
			auto& live = *backend;
			const Snapshot original{ mode, mode ? std::nullopt : std::optional<DWORD>{0x112233} };
			live.live = original;
			ColorPreference preview(std::move(backend));
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt)));
			Check(preview.IsAttemptActive()); // Automatic may refresh RGB without changing its mode.
			Check(preview.IsDirty() == (original != Snapshot{ 1u, std::nullopt }));
			Check(SUCCEEDED(preview.RollbackAttempt()) && live.live == original && !preview.IsDirty());
			Check(!preview.IsAttemptActive());
			Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", std::nullopt))); preview.CommitAttempt();
			Check(SUCCEEDED(preview.Apply(L"user", 0))); preview.CommitAttempt();
			live.wallpaper = 0x778899;
			Check(SUCCEEDED(preview.Revert()) && live.live == original);
			if (original.IsAutomatic()) Check(live.derived == live.wallpaper); // Recompute current wallpaper.
			preview.Accept(); Check(!preview.IsDirty());
		}
		auto backend = std::make_unique<Backend>();
		auto& live = *backend;
		const Snapshot original{ 0u, 0x112233u };
		live.live = original;
		ColorPreference preview(std::move(backend));
		Check(SUCCEEDED(preview.Apply(L"user", 0xFF112233)) && !preview.IsDirty()); preview.CommitAttempt();
		Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
		const auto a = live.live;
		live.reject = true;
		const auto writes = live.writes;
		Check(FAILED(preview.Apply(L"user", std::nullopt)) && writes == live.writes);
		Check(!preview.IsAttemptActive()); // Capability rejection happens before color mutation.
		Check(SUCCEEDED(preview.RollbackAttempt()) && live.live == a && preview.IsDirty());
		live.reject = false; live.fail = true;
		Check(FAILED(preview.Apply(L"user", std::nullopt)));
		live.fail = false;
		Check(SUCCEEDED(preview.RollbackAttempt()) && live.live == a && preview.IsDirty());
		Check(SUCCEEDED(preview.Apply(L"user", 0x778899))); preview.CommitAttempt();
		// Untracked Windows output never becomes part of the GUI checkpoint.
		live.derived = 0xABCDEF;
		Check(preview.IsDirty());
		live.fail = true;
		Check(FAILED(preview.Revert()) && preview.IsDirty());
		live.fail = false;
		Check(SUCCEEDED(preview.Revert()) && live.live == original); preview.Accept();
		Check(SUCCEEDED(preview.Apply(L"user", 0x445566))); preview.CommitAttempt();
		Check(SUCCEEDED(preview.Apply(L"user", *original.rgb)) && !preview.IsDirty()); preview.CommitAttempt();
		Check(SUCCEEDED(preview.Apply(L"user", std::nullopt))); preview.CommitAttempt(); preview.Accept();
		live.wallpaper = 0xABCDEF; live.derived = live.wallpaper;
		Check(!preview.IsDirty());
		Check(SUCCEEDED(preview.Apply(L"user", 0))); preview.CommitAttempt();
		live.wallpaper = 0xFEDCBA;
		Check(SUCCEEDED(preview.Revert()) && live.live.IsAutomatic() && live.derived == 0xFEDCBA); preview.Accept();
		// Failed recovery must keep a retryable baseline even after attempt rollback.
		live.fail = true;
		Check(FAILED(preview.Apply(L"user", 0x123456)));
		Check(FAILED(preview.RollbackAttempt()) && preview.IsDirty());
		live.fail = false;
		Check(SUCCEEDED(preview.Revert()) && live.live.IsAutomatic()); preview.Accept();
	}
}
