#include "HookHelper.hpp"
#include <KNSoft/SlimDetours/SlimDetours.h>
#include <DbgHelp.h>
#include <TlHelp32.h>
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <string>
#define FORMAT_IMPORT_FUNCTION(function) (IS_INTRESOURCE(function) ? ("#" + std::to_string(reinterpret_cast<ULONG_PTR>(function))).c_str() : function)
#pragma comment(lib, "KNSoft.SlimDetours.lib")
#pragma comment(lib, "DbgHelp.lib")
#pragma comment(lib, "onecore.lib")

using namespace OpenGlass;

namespace
{
	HookHelper::HookRundown g_hookRundown;
	SRWLOCK g_transactionLock{ SRWLOCK_INIT };
	thread_local HookHelper::HookTransaction* g_currentTransaction{};

	PVOID ApplyPointerMutation(PVOID* target, PVOID replacement) noexcept
	{
		const auto unprotectedScope = HookHelper::Unprotect({ reinterpret_cast<uint8_t*>(target), sizeof(*target) });
		return InterlockedExchangePointer(target, replacement);
	}

	void ApplyInstructionMutation(uint8_t* address, std::span<const uint8_t> bytes) noexcept
	{
		const auto unprotectedScope = HookHelper::Unprotect({ address, bytes.size_bytes() });
		memcpy_s(address, bytes.size_bytes(), bytes.data(), bytes.size_bytes());
		FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(FlushInstructionCache(GetCurrentProcess(), address, bytes.size_bytes()), "Unable to flush patched instructions at %p", address);
	}
}

HookHelper::HookRundown& HookHelper::GetHookRundown() noexcept
{
	return g_hookRundown;
}

HookHelper::HookTransaction& HookHelper::GetCurrentHookTransaction() noexcept
{
	FAIL_FAST_IF_FAILED_MSG(g_currentTransaction ? S_OK : E_UNEXPECTED, "No hook transaction is active");
	return *g_currentTransaction;
}

void HookHelper::HookRundown::Open() noexcept
{
	const auto previous = m_state.exchange(0, std::memory_order_acq_rel);
	FAIL_FAST_IF_FAILED_MSG(previous & c_countMask ? E_UNEXPECTED : S_OK, "Hook rundown reopened with active calls");
}

bool HookHelper::HookRundown::TryAcquire() noexcept
{
	auto state = m_state.load(std::memory_order_relaxed);
	for (;;)
	{
		if (state & c_closing)
		{
			return false;
		}
		FAIL_FAST_IF_FAILED_MSG((state & c_countMask) == c_countMask ? E_UNEXPECTED : S_OK, "Hook rundown counter overflow");
		if (m_state.compare_exchange_weak(state, state + 1, std::memory_order_acquire, std::memory_order_relaxed))
		{
			return true;
		}
	}
}

void HookHelper::HookRundown::Release() noexcept
{
	const auto previous = m_state.fetch_sub(1, std::memory_order_release);
	FAIL_FAST_IF_FAILED_MSG((previous & c_countMask) ? S_OK : E_UNEXPECTED, "Unbalanced hook rundown release");
	if ((previous & c_closing) && (previous & c_countMask) == 1)
	{
		WakeByAddressAll(&m_state);
	}
}

void HookHelper::HookRundown::BeginShutdown() noexcept
{
	m_state.fetch_or(c_closing, std::memory_order_acq_rel);
	WakeByAddressAll(&m_state);
}

bool HookHelper::HookRundown::IsClosing() const noexcept
{
	return !!(m_state.load(std::memory_order_acquire) & c_closing);
}

void HookHelper::HookRundown::WaitForDrain(std::chrono::milliseconds timeout) noexcept
{
	const auto deadline = GetTickCount64() + static_cast<ULONGLONG>(timeout.count());
	for (;;)
	{
		const auto state = m_state.load(std::memory_order_acquire);
		if (!(state & c_countMask))
		{
			return;
		}

		const auto now = GetTickCount64();
		FAIL_FAST_IF_FAILED_MSG(now < deadline ? S_OK : HRESULT_FROM_WIN32(ERROR_TIMEOUT), "Hook rundown did not drain within %llu ms", static_cast<unsigned long long>(timeout.count()));
		const DWORD remaining = static_cast<DWORD>(std::min<ULONGLONG>(deadline - now, MAXDWORD));
		WaitOnAddress(&m_state, const_cast<ULONG64*>(&state), sizeof(state), remaining);
	}
}

void HookHelper::PointerHookBase::Prepare(PVOID* target, PVOID replacement, PVOID* originalStorage) noexcept
{
	FAIL_FAST_IF_FAILED_MSG(target && replacement && originalStorage ? S_OK : E_INVALIDARG, "Invalid pointer hook");
	FAIL_FAST_IF_FAILED_MSG(!m_installed.load(std::memory_order_acquire) ? S_OK : E_UNEXPECTED, "Cannot prepare an installed pointer hook");
	m_target = target;
	m_replacement = replacement;
	m_originalStorage = originalStorage;
}

HRESULT HookHelper::ImportHook::Prepare(HMODULE module, std::span<const ImportDllDetourInfo> imports, bool delayLoad) noexcept
{
	struct RequestedImport
	{
		LPCSTR dllName;
		const ImportFunctionDetourInfo* function;
		size_t matchCount{};
	};

	FAIL_FAST_IF_FAILED_MSG(!m_installed ? S_OK : E_UNEXPECTED, "Cannot prepare installed import hooks");
	m_entries.clear();
	RETURN_HR_IF_MSG(HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND), !module, "Import hook module is not loaded");
	std::vector<RequestedImport> requestedImports;
	for (const auto& dll : imports)
	{
		FAIL_FAST_IF_NULL_MSG(dll.importDllName, "Import hook inventory contains an empty DLL name");
		for (const auto& function : dll.functionsToDetour)
		{
			if (!function.importFunction)
			{
				continue;
			}
			FAIL_FAST_IF_FAILED_MSG(function.original && function.detour ? S_OK : E_INVALIDARG, "Incomplete requested import hook %hs!%hs", dll.importDllName, FORMAT_IMPORT_FUNCTION(function.importFunction));
			requestedImports.push_back({ dll.importDllName, &function });
		}
	}
	FAIL_FAST_IF_FAILED_MSG(!requestedImports.empty() ? S_OK : E_INVALIDARG, "Import hook inventory contains no active entries");

	ULONG size{};
	const auto directory = delayLoad ? IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT : IMAGE_DIRECTORY_ENTRY_IMPORT;
	auto directoryData = ImageDirectoryEntryToDataEx(module, TRUE, static_cast<USHORT>(directory), &size, nullptr);
	RETURN_HR_IF_MSG(E_NOINTERFACE, !directoryData, "Unable to read import directory %lu for %p", directory, module);

	auto appendMatches = [&](LPCSTR dllName, DWORD thunkRva, DWORD nameThunkRva) -> HRESULT
	{
		if (std::none_of(requestedImports.begin(), requestedImports.end(), [dllName](const RequestedImport& entry) { return !_stricmp(entry.dllName, dllName); }))
		{
			return S_OK;
		}

		RETURN_HR_IF_MSG(E_NOINTERFACE, !thunkRva || !nameThunkRva, "Missing import table for %hs in %p", dllName, module);
		auto thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(RVA_TO_ADDR(module, thunkRva));
		auto nameThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(RVA_TO_ADDR(module, nameThunkRva));
		while (thunk->u1.Function)
		{
			const bool byName = !IMAGE_SNAP_BY_ORDINAL(nameThunk->u1.Ordinal);
			const auto importName = byName ?
				reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(RVA_TO_ADDR(module, nameThunk->u1.AddressOfData))->Name :
				MAKEINTRESOURCEA(IMAGE_ORDINAL(nameThunk->u1.Ordinal));
			for (auto& requested : requestedImports)
			{
				if (_stricmp(requested.dllName, dllName))
				{
					continue;
				}
				const auto& function = *requested.function;
				const bool matches = byName ?
					(!IS_INTRESOURCE(function.importFunction) && !strcmp(function.importFunction, importName)) :
					(function.importFunction == importName);
				if (matches)
				{
					++requested.matchCount;
					RETURN_HR_IF_MSG(E_NOINTERFACE, requested.matchCount != 1, "Import hook %hs!%hs matched multiple slots", requested.dllName, FORMAT_IMPORT_FUNCTION(function.importFunction));
					const auto target = reinterpret_cast<PVOID*>(&thunk->u1.Function);
					FAIL_FAST_IF_FAILED_MSG(
						std::none_of(m_entries.begin(), m_entries.end(), [target](const Entry& entry) { return entry.target == target; }) ? S_OK : E_UNEXPECTED,
						"Duplicate import hook slot %p",
						target
					);
					m_entries.push_back({ target, function.detour, function.original });
				}
			}
			++thunk;
			++nameThunk;
		}
		return S_OK;
	};

	if (delayLoad)
	{
		// Known defect: delay imports are not resolved before patching, so the
		// saved original may still be a delay thunk. Calling that thunk after
		// installation can recurse into the hook via ResolveDelayLoadedAPI.
		// Current callers happen to avoid this: DWM hooks rely on imports being
		// resolved before installation; the theme extension never calls saved
		// delay-IAT originals (its normal-IAT CoCreateInstance hook does forward).
		// This is a caller assumption, not a guarantee provided by ImportHook.
		for (auto descriptor = static_cast<PIMAGE_DELAYLOAD_DESCRIPTOR>(directoryData); descriptor->DllNameRVA; ++descriptor)
		{
			RETURN_HR_IF_MSG(E_NOINTERFACE, descriptor->Attributes.RvaBased != 1, "Unsupported VA-based delay import directory in %p", module);
			RETURN_IF_FAILED(appendMatches(
				reinterpret_cast<LPCSTR>(RVA_TO_ADDR(module, descriptor->DllNameRVA)),
				descriptor->ImportAddressTableRVA,
				descriptor->ImportNameTableRVA
			));
		}
	}
	else
	{
		for (auto descriptor = static_cast<PIMAGE_IMPORT_DESCRIPTOR>(directoryData); descriptor->Name; ++descriptor)
		{
			RETURN_IF_FAILED(appendMatches(
				reinterpret_cast<LPCSTR>(RVA_TO_ADDR(module, descriptor->Name)),
				descriptor->FirstThunk,
				descriptor->OriginalFirstThunk
			));
		}
	}
	for (const auto& requested : requestedImports)
	{
		RETURN_HR_IF_MSG(E_NOINTERFACE, requested.matchCount != 1, "Requested import hook %hs!%hs was not found in %p", requested.dllName, FORMAT_IMPORT_FUNCTION(requested.function->importFunction), module);
	}
	return S_OK;
}

void HookHelper::InstructionPatch::Prepare(
	uint8_t* address,
	std::span<const uint8_t> expected,
	std::span<const uint8_t> replacement
) noexcept
{
	FAIL_FAST_IF_FAILED_MSG(address && !expected.empty() && expected.size() == replacement.size() ? S_OK : E_INVALIDARG, "Invalid instruction patch");
	FAIL_FAST_IF_FAILED_MSG(!m_installed ? S_OK : E_UNEXPECTED, "Cannot prepare an installed instruction patch");
	FAIL_FAST_IF_FAILED_MSG(std::equal(expected.begin(), expected.end(), address) ? S_OK : E_UNEXPECTED, "Instruction bytes do not match the expected original at %p", address);
	m_address = address;
	m_replacement.assign(replacement.begin(), replacement.end());
	m_original.assign(expected.begin(), expected.end());
}

HookHelper::HookTransaction::HookTransaction(HookMode mode) noexcept : m_mode(mode)
{
	FAIL_FAST_IF_FAILED_MSG(!g_currentTransaction ? S_OK : E_UNEXPECTED, "Nested hook transactions are not supported");
	AcquireSRWLockExclusive(&g_transactionLock);
	// This is the preparation phase of the logical transaction. SlimDetours begins
	// in Commit(), after every hook target and instruction pattern is resolved, so
	// preparation never calls COM or the loader while DWM threads are suspended.
	g_currentTransaction = this;
}

HookHelper::HookTransaction::~HookTransaction() noexcept
{
	FAIL_FAST_IF_FAILED_MSG(
		m_committed ||
		(m_inlineHooks.empty() && m_pointerHooks.empty() && m_importHooks.empty() && m_instructionPatches.empty()) ? S_OK : E_UNEXPECTED,
		"Non-empty hook transaction left without Commit"
	);
	g_currentTransaction = nullptr;
	ReleaseSRWLockExclusive(&g_transactionLock);
}

void HookHelper::HookTransaction::ApplyInline(std::string_view group, std::span<const DetourInfo> hooks) noexcept
{
	FAIL_FAST_IF_FAILED_MSG(!m_committed ? S_OK : E_UNEXPECTED, "Cannot modify a committed hook transaction");
	for (const auto& hook : hooks)
	{
		if (!hook.original && !hook.detour)
		{
			continue;
		}
		FAIL_FAST_IF_FAILED_MSG(hook.original && hook.detour ? S_OK : E_INVALIDARG, "Incomplete hook %.*hs", static_cast<int>(group.size()), group.data());
		const auto duplicate = std::find_if(
			m_inlineHooks.begin(),
			m_inlineHooks.end(),
			[&hook](const InlineEntry& queued) { return queued.m_detour.original == hook.original; }
		);
		if (duplicate != m_inlineHooks.end())
		{
			FAIL_FAST_IF_FAILED_MSG(duplicate->m_detour.detour == hook.detour ? S_OK : E_UNEXPECTED, "Conflicting inline hooks target the same storage in %.*hs", static_cast<int>(group.size()), group.data());
			continue;
		}
		m_inlineHooks.push_back({ hook, group });
	}
}

void HookHelper::HookTransaction::ApplyPointer(PointerHookBase& hook) noexcept
{
	FAIL_FAST_IF_FAILED_MSG(hook.m_target && hook.m_replacement ? S_OK : E_UNEXPECTED, "Pointer hook was not prepared");
	FAIL_FAST_IF_FAILED_MSG(hook.IsInstalled() != (m_mode == HookMode::Install) ? S_OK : E_UNEXPECTED, "Pointer hook state does not match transaction mode");
	if (m_mode == HookMode::Install)
	{
		*hook.m_originalStorage = ApplyPointerMutation(hook.m_target, hook.m_replacement);
	}
	else
	{
		FAIL_FAST_IF_FAILED_MSG(*hook.m_target == hook.m_replacement ? S_OK : E_UNEXPECTED, "Pointer hook ownership conflict at %p", hook.m_target);
		ApplyPointerMutation(hook.m_target, *hook.m_originalStorage);
	}
	hook.m_installed.store(m_mode == HookMode::Install, std::memory_order_release);
}

void HookHelper::HookTransaction::Apply(PointerHookBase& hook) noexcept
{
	m_pointerHooks.push_back(&hook);
}

void HookHelper::HookTransaction::Apply(ImportHook& hook) noexcept
{
	m_importHooks.push_back(&hook);
}

void HookHelper::HookTransaction::ApplyImport(ImportHook& hook) noexcept
{
	FAIL_FAST_IF_FAILED_MSG((m_mode == HookMode::Install ? !hook.m_installed : hook.m_installed) ? S_OK : E_UNEXPECTED, "Import hook state does not match transaction mode");
	for (auto& entry : hook.m_entries)
	{
		if (m_mode == HookMode::Install)
		{
			*entry.originalStorage = ApplyPointerMutation(entry.target, entry.replacement);
		}
		else
		{
			FAIL_FAST_IF_FAILED_MSG(*entry.target == entry.replacement ? S_OK : E_UNEXPECTED, "Import hook ownership conflict at %p", entry.target);
			ApplyPointerMutation(entry.target, *entry.originalStorage);
		}
	}
	hook.m_installed = m_mode == HookMode::Install;
}

void HookHelper::HookTransaction::Apply(InstructionPatch& patch) noexcept
{
	m_instructionPatches.push_back(&patch);
}

void HookHelper::HookTransaction::ApplyInstruction(InstructionPatch& patch) noexcept
{
	FAIL_FAST_IF_FAILED_MSG(patch.m_address && !patch.m_original.empty() ? S_OK : E_UNEXPECTED, "Instruction patch was not prepared");
	FAIL_FAST_IF_FAILED_MSG((m_mode == HookMode::Install ? !patch.m_installed : patch.m_installed) ? S_OK : E_UNEXPECTED, "Instruction patch state does not match transaction mode at %p", patch.m_address);
	const auto expected = m_mode == HookMode::Install ? std::span<const uint8_t>{ patch.m_original } : std::span<const uint8_t>{ patch.m_replacement };
	const auto replacement = m_mode == HookMode::Install ? std::span<const uint8_t>{ patch.m_replacement } : std::span<const uint8_t>{ patch.m_original };
	FAIL_FAST_IF_FAILED_MSG(std::equal(expected.begin(), expected.end(), patch.m_address) ? S_OK : E_UNEXPECTED, "Instruction patch ownership conflict at %p", patch.m_address);
	ApplyInstructionMutation(patch.m_address, replacement);
	patch.m_installed = m_mode == HookMode::Install;
}

void HookHelper::HookTransaction::Commit() noexcept
{
	FAIL_FAST_IF_FAILED_MSG(!m_committed ? S_OK : E_UNEXPECTED, "Hook transaction was committed twice");
	FAIL_FAST_IF_FAILED_MSG(SlimDetoursTransactionBegin(), "Unable to begin the %hs hook transaction", m_mode == HookMode::Install ? "install" : "remove");
	for (size_t index = 0; index < m_inlineHooks.size(); ++index)
	{
		const auto& [hook, group] = m_inlineHooks[index];
		const auto hr = m_mode == HookMode::Install ? SlimDetoursAttach(hook.original, hook.detour) : SlimDetoursDetach(hook.original, hook.detour);
		FAIL_FAST_IF_FAILED_MSG(hr, "Unable to %hs hook %.*hs[%zu] at %p", m_mode == HookMode::Install ? "install" : "remove", static_cast<int>(group.size()), group.data(), index, *hook.original);
	}
	for (auto hook : m_pointerHooks)
	{
		ApplyPointer(*hook);
	}
	for (auto hook : m_importHooks)
	{
		ApplyImport(*hook);
	}
	for (auto patch : m_instructionPatches)
	{
		ApplyInstruction(*patch);
	}
	FAIL_FAST_IF_FAILED_MSG(SlimDetoursTransactionCommit(), "Unable to commit the %hs hook transaction", m_mode == HookMode::Install ? "install" : "remove");
	m_committed = true;
}

HMODULE HookHelper::GetRemoteModuleBase(DWORD processId, LPCWSTR moduleName)
{
	wil::unique_hfile snapshot{};
	for (;;)
	{
		snapshot.reset(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId));
		if (snapshot.is_valid())
		{
			break;
		}

		const auto error = GetLastError();
		if (error != ERROR_BAD_LENGTH)
		{
			LOG_WIN32(error);
			return nullptr;
		}
		Sleep(5);
	}

	MODULEENTRY32W me{ sizeof(me) };
	if (!Module32FirstW(snapshot.get(), &me))
	{
		LOG_WIN32(GetLastError());
		return nullptr;
	}

	do
	{
		if (!_wcsicmp(me.szModule, moduleName) || !_wcsicmp(me.szExePath, moduleName))
		{
			return me.hModule;
		}
	} while (Module32NextW(snapshot.get(), &me));

	return nullptr;
}

const uint8_t* HookHelper::FindPattern(std::span<const uint8_t> base, std::span<const uint16_t> pat)
{
	if (base.empty() || pat.empty() || pat.size() > base.size())
	{
		return nullptr;
	}

	const uint8_t* first = base.data();
	const uint8_t* last = first + base.size();

	const auto it = std::search(first, last, pat.begin(), pat.end(), [](uint8_t mem, uint16_t p)
	{
		return p == c_patwc || mem == static_cast<uint8_t>(p);
	});

	return (it == last) ? nullptr : it;
}

void HookHelper::ApplyInlineHooks(
	std::span<const DetourInfo> functionsToDetour,
	const std::source_location& location
)
{
	GetCurrentHookTransaction().ApplyInline(location.function_name(), functionsToDetour);
}
