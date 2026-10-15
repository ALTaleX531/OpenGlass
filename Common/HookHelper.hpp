#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#pragma warning(push)
#pragma warning(disable : 4201 4324 4471)
#include <KNSoft/NDK/NDK.h>
#pragma warning(pop)
#include <wil/resource.h>
#include <wil/result.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <source_location>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace OpenGlass::HookHelper
{
	enum class HookMode
	{
		Install,
		Remove
	};

	class HookRundown final
	{
		static constexpr ULONG64 c_closing = 1ull << 63;
		static constexpr ULONG64 c_countMask = ~c_closing;
		std::atomic<ULONG64> m_state{ c_closing };

	public:
		void Open() noexcept;
		[[nodiscard]] bool TryAcquire() noexcept;
		void Release() noexcept;
		void BeginShutdown() noexcept;
		[[nodiscard]] bool IsClosing() const noexcept;
		void WaitForDrain(std::chrono::milliseconds timeout) noexcept;
	};

	HookRundown& GetHookRundown() noexcept;
	class HookTransaction;
	HookTransaction& GetCurrentHookTransaction() noexcept;

	template <typename T>
	concept function_pointer = std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T>>;

	struct DetourInfo
	{
		LPVOID* original{};
		LPVOID detour{};
		DetourInfo() = default;

		template <function_pointer T> FORCEINLINE DetourInfo(T* p1, T p2, bool condition = true) : original(condition ? reinterpret_cast<LPVOID*>(p1) : nullptr), detour(condition ? reinterpret_cast<LPVOID>(p2) : nullptr) {}
		template <typename Storage>
		requires requires(Storage* storage) { storage->detour_storage(); storage->prepare_detour(); }
		FORCEINLINE DetourInfo(Storage* p1, bool condition = true) :
			original(condition ? reinterpret_cast<LPVOID*>(p1->detour_storage()) : nullptr),
			detour(condition ? reinterpret_cast<LPVOID>(p1->prepare_detour()) : nullptr)
		{
		}
	};
	struct ImportDllDetourInfo;

	class PointerHookBase
	{
		friend class HookTransaction;
	protected:
		PVOID* m_target{};

	private:
		PVOID m_replacement{};
		PVOID* m_originalStorage{};
		std::atomic_bool m_installed{};

	public:
		void Prepare(PVOID* target, PVOID replacement, PVOID* originalStorage) noexcept;
		[[nodiscard]] bool IsInstalled() const noexcept { return m_installed.load(std::memory_order_acquire); }
	};

	template <auto Replacement, typename T = decltype(Replacement)>
	requires function_pointer<T>
	struct HookRundownThunk;

	template <auto Replacement, typename R, typename... Args>
	struct HookRundownThunk<Replacement, R (*)(Args...)>
	{
		using T = R (*)(Args...);
		// Each Replacement binds to one original-function storage across pointer and import hooks.
		inline static T* s_originalStorage{};

		static R Invoke(Args... args)
		{
			auto& rundown = GetHookRundown();
			if (!rundown.TryAcquire())
			{
				return std::invoke(*s_originalStorage, std::forward<Args>(args)...);
			}
			const auto release = wil::scope_exit([&rundown]
			{
				rundown.Release();
			});
			return std::invoke(Replacement, std::forward<Args>(args)...);
		}

		static T Bind(T* originalStorage) noexcept
		{
			FAIL_FAST_IF_FAILED_MSG(
				!s_originalStorage || s_originalStorage == originalStorage ? S_OK : E_UNEXPECTED,
				"Hook replacement is bound to multiple original-function stores"
			);
			s_originalStorage = originalStorage;
			return &Invoke;
		}
	};

	template <auto Replacement>
	requires function_pointer<decltype(Replacement)>
	class PointerHook final : public PointerHookBase
	{
		using T = decltype(Replacement);

	public:
		void Prepare(T* target, T* originalStorage) noexcept
		{
			const auto slot = reinterpret_cast<PVOID*>(target);
			FAIL_FAST_IF_FAILED_MSG(!m_target || m_target == slot ? S_OK : E_UNEXPECTED, "Lazy hook discovered a different slot");
			PointerHookBase::Prepare(
				slot,
				reinterpret_cast<PVOID>(HookRundownThunk<Replacement>::Bind(originalStorage)),
				reinterpret_cast<PVOID*>(originalStorage)
			);
		}

		void AttachOnce(T* target, T* originalStorage) noexcept;
	};

	class ImportHook final
	{
		friend class HookTransaction;
		struct Entry
		{
			PVOID* target;
			PVOID replacement;
			PVOID* originalStorage;
		};
		std::vector<Entry> m_entries;
		bool m_installed{};

	public:
		// Unsupported imports return failure without modifying the IAT; apply only after successful preparation.
		[[nodiscard]] HRESULT Prepare(HMODULE module, std::span<const ImportDllDetourInfo> imports, bool delayLoad = false) noexcept;
		[[nodiscard]] bool IsInstalled() const noexcept { return m_installed; }
	};

	class InstructionPatch final
	{
		friend class HookTransaction;
		uint8_t* m_address{};
		std::vector<uint8_t> m_replacement;
		std::vector<uint8_t> m_original;
		bool m_installed{};

	public:
		void Prepare(uint8_t* address, std::span<const uint8_t> expected, std::span<const uint8_t> replacement) noexcept;
	};

	class HookTransaction final
	{
		struct InlineEntry
		{
			DetourInfo m_detour;
			std::string_view m_group;
		};

		HookMode m_mode;
		bool m_committed{};
		std::vector<InlineEntry> m_inlineHooks;
		std::vector<PointerHookBase*> m_pointerHooks;
		std::vector<ImportHook*> m_importHooks;
		std::vector<InstructionPatch*> m_instructionPatches;

		void ApplyPointer(PointerHookBase& hook) noexcept;
		void ApplyImport(ImportHook& hook) noexcept;
		void ApplyInstruction(InstructionPatch& patch) noexcept;

	public:
		explicit HookTransaction(HookMode mode) noexcept;
		~HookTransaction() noexcept;
		HookTransaction(const HookTransaction&) = delete;
		HookTransaction& operator=(const HookTransaction&) = delete;

		void ApplyInline(std::string_view group, std::span<const DetourInfo> hooks) noexcept;
		void Apply(PointerHookBase& hook) noexcept;
		void Apply(ImportHook& hook) noexcept;
		void Apply(InstructionPatch& patch) noexcept;
		void Commit() noexcept;
	};

	template <auto Replacement>
	requires function_pointer<decltype(Replacement)>
	void PointerHook<Replacement>::AttachOnce(decltype(Replacement)* target, decltype(Replacement)* originalStorage) noexcept
	{
		if (IsInstalled())
		{
			FAIL_FAST_IF_FAILED_MSG(m_target == reinterpret_cast<PVOID*>(target) ? S_OK : E_UNEXPECTED, "Lazy hook discovered a different slot");
			return;
		}

		auto& rundown = GetHookRundown();
		if (!rundown.TryAcquire())
		{
			return;
		}
		const auto release = wil::scope_exit([&rundown]
		{
			rundown.Release();
		});

		// Serializing the transaction closes the race with BeginShutdown(): an
		// install that wins the transaction lock is visible to the subsequent
		// remove inventory, while a losing install observes closing and retires.
		HookTransaction transaction{ HookMode::Install };
		if (rundown.IsClosing())
		{
			return;
		}
		if (IsInstalled())
		{
			FAIL_FAST_IF_FAILED_MSG(m_target == reinterpret_cast<PVOID*>(target) ? S_OK : E_UNEXPECTED, "Lazy hook discovered a different slot");
			return;
		}
		Prepare(target, originalStorage);
		transaction.Apply(*this);
		transaction.Commit();
	}

	struct ImportFunctionDetourInfo
	{
		LPCSTR importFunction{};
		LPVOID* original{};
		LPVOID detour{};
	};

	template <auto Replacement>
	requires function_pointer<decltype(Replacement)>
	ImportFunctionDetourInfo MakeImportDetour(
		LPCSTR function,
		decltype(Replacement)* originalStorage,
		bool condition = true
	) noexcept
	{
		if (!condition)
		{
			return {};
		}
		return ImportFunctionDetourInfo
		{
			function,
			reinterpret_cast<LPVOID*>(originalStorage),
			reinterpret_cast<LPVOID>(HookRundownThunk<Replacement>::Bind(originalStorage))
		};
	}

	struct ImportDllDetourInfo
	{
		LPCSTR importDllName;
		std::span<const ImportFunctionDetourInfo> functionsToDetour;
	};

	constexpr uint16_t c_patwc = 0xFFFFu;

	HMODULE GetRemoteModuleBase(DWORD processId, LPCWSTR moduleName);

	FORCEINLINE auto Unprotect(std::span<uint8_t> views)
	{
		FAIL_FAST_IF_FAILED_MSG(!views.empty() ? S_OK : E_INVALIDARG, "Cannot change protection of an empty range");
		DWORD oldProtect{ 0 };
		FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(
			VirtualProtect(
				views.data(),
				views.size_bytes(),
				PAGE_EXECUTE_READWRITE,
				&oldProtect
			),
			"Unable to make hook target %p writable",
			views.data()
		);
		return wil::scope_exit([=]
		{
			auto unused = 0ul;
			FAIL_FAST_IF_WIN32_BOOL_FALSE_MSG(
				VirtualProtect(
					views.data(),
					views.size_bytes(),
					oldProtect,
					&unused
				),
				"Unable to restore hook target protection at %p",
				views.data()
			);
		});
	}
	const uint8_t* FindPattern(std::span<const uint8_t> base, std::span<const uint16_t> pat);
	void ApplyInlineHooks(
		std::span<const DetourInfo> functionsToDetour,
		const std::source_location& location = std::source_location::current()
	);

	template <typename T = PVOID>
	FORCEINLINE T* get_vftable_from(const void* This)
	{
		return reinterpret_cast<T*>(*reinterpret_cast<PVOID*>(const_cast<void*>(This)));
	}
	template <typename T = LPCVOID>
	FORCEINLINE T& get_vftable_reference_from(const void* This)
	{
		return *reinterpret_cast<T*>(const_cast<void*>(This));
	}
}
