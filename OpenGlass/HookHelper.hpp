#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "wil.hpp"
#include <atomic>
#include <chrono>
#include <source_location>

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
		PVOID* m_target{};
		PVOID m_replacement{};
		PVOID m_original{};
		PVOID* m_originalStorage{};
		std::atomic_bool m_installed{};

	protected:
		void Prepare(PVOID* target, PVOID replacement, PVOID* originalStorage) noexcept;

	public:
		[[nodiscard]] bool IsInstalled() const noexcept { return m_installed.load(std::memory_order_acquire); }
	};

	template <typename Owner, typename T> struct PointerHookRundownThunk;

	template <typename Owner, typename R, typename... Args>
	struct PointerHookRundownThunk<Owner, R (*)(Args...)>
	{
		static R Invoke(Args... args)
		{
			auto& rundown = GetHookRundown();
			if (!rundown.TryAcquire())
			{
				return std::invoke(Owner::Original(), std::forward<Args>(args)...);
			}
			const auto release = wil::scope_exit([&rundown]
			{
				rundown.Release();
			});
			return std::invoke(Owner::ReplacementFunction(), std::forward<Args>(args)...);
		}
	};

	template <auto Replacement>
	requires function_pointer<decltype(Replacement)>
	class PointerHook final : public PointerHookBase
	{
		using T = decltype(Replacement);
		inline static T* s_originalStorage{};
		inline static T* s_target{};

	public:
		static T Original() noexcept { return *s_originalStorage; }
		static constexpr T ReplacementFunction() noexcept { return Replacement; }

		void Prepare(T* target, T* originalStorage) noexcept
		{
			FAIL_FAST_IF_FAILED_MSG(!s_target || s_target == target ? S_OK : E_UNEXPECTED, "Lazy hook discovered a different slot");
			s_target = target;
			s_originalStorage = originalStorage;
			PointerHookBase::Prepare(
				reinterpret_cast<PVOID*>(target),
				reinterpret_cast<PVOID>(&PointerHookRundownThunk<PointerHook, T>::Invoke),
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
		void Prepare(HMODULE module, std::span<const ImportDllDetourInfo> imports, bool delayLoad = false) noexcept;
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
		HookMode m_mode;
		bool m_committed{};
		std::array<DetourInfo, 256> m_inlineHooks{};
		std::array<std::string_view, 256> m_inlineGroups{};
		size_t m_inlineHookCount{};
		std::array<PointerHookBase*, 64> m_pointerHooks{};
		size_t m_pointerHookCount{};
		std::array<ImportHook*, 16> m_importHooks{};
		size_t m_importHookCount{};
		std::array<InstructionPatch*, 32> m_instructionPatches{};
		size_t m_instructionPatchCount{};

		void ApplyPointer(PointerHookBase& hook) noexcept;
		void ApplyImport(ImportHook& hook) noexcept;
		void ApplyInstruction(InstructionPatch& patch) noexcept;

	public:
		explicit HookTransaction(HookMode mode) noexcept;
		~HookTransaction() noexcept;
		HookTransaction(const HookTransaction&) = delete;
		HookTransaction& operator=(const HookTransaction&) = delete;

		[[nodiscard]] HookMode Mode() const noexcept { return m_mode; }
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
			FAIL_FAST_IF_FAILED_MSG(s_target == target ? S_OK : E_UNEXPECTED, "Lazy hook discovered a different slot");
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
			transaction.Commit();
			return;
		}
		if (IsInstalled())
		{
			FAIL_FAST_IF_FAILED_MSG(s_target == target ? S_OK : E_UNEXPECTED, "Lazy hook discovered a different slot");
			transaction.Commit();
			return;
		}
		Prepare(target, originalStorage);
		transaction.Apply(*this);
		transaction.Commit();
	}

	struct ImportFunctionDetourInfo : DetourInfo
	{
		LPCSTR importFunction;

		ImportFunctionDetourInfo(LPCSTR function, LPVOID* original, LPVOID detour, bool condition) noexcept :
			DetourInfo{},
			importFunction(condition ? function : nullptr)
		{
			this->original = condition ? original : nullptr;
			this->detour = condition ? detour : nullptr;
		}
	};

	template <auto Replacement, typename T = decltype(Replacement)>
	requires function_pointer<T>
	struct ImportHookRundownThunk;

	template <auto Replacement, typename R, typename... Args>
	struct ImportHookRundownThunk<Replacement, R (*)(Args...)>
	{
		using T = R (*)(Args...);
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
				"Import replacement is bound to multiple original-function stores"
			);
			s_originalStorage = originalStorage;
			return &Invoke;
		}
	};

	template <auto Replacement>
	requires function_pointer<decltype(Replacement)>
	ImportFunctionDetourInfo MakeImportDetour(
		LPCSTR function,
		decltype(Replacement)* originalStorage,
		bool condition = true
	) noexcept
	{
		using T = decltype(Replacement);
		return ImportFunctionDetourInfo
		{
			function,
			reinterpret_cast<LPVOID*>(originalStorage),
			condition ? reinterpret_cast<LPVOID>(ImportHookRundownThunk<Replacement, T>::Bind(originalStorage)) : nullptr,
			condition
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
		bool enable,
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
