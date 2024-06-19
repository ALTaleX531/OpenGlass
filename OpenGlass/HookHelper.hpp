#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "wil.hpp"

namespace OpenGlass::HookHelper
{
	class ThreadSnapshot
	{
		HPSS m_snapshot{ nullptr };
	public:
		ThreadSnapshot(ThreadSnapshot&&) = delete;
		ThreadSnapshot(const ThreadSnapshot&) = delete;
		ThreadSnapshot();
		~ThreadSnapshot();
		void Walk(const std::function<bool(const PSS_THREAD_ENTRY&)>&& callback);
	};
	struct ThreadSafeScope : ThreadSnapshot
	{
		ThreadSafeScope(ThreadSafeScope&&) = delete;
		ThreadSafeScope(const ThreadSafeScope&) = delete;
		ThreadSafeScope()
		{
			Walk([](const PSS_THREAD_ENTRY& threadEntry)
			{ 
				LOG_LAST_ERROR_IF(
					SuspendThread(wil::unique_handle{ OpenThread(THREAD_SUSPEND_RESUME, FALSE, threadEntry.ThreadId) }.get()) == -1
				);
				return true;
			});
		}
		~ThreadSafeScope()
		{
			Walk([](const PSS_THREAD_ENTRY& threadEntry)
			{
				LOG_LAST_ERROR_IF(
					ResumeThread(wil::unique_handle{ OpenThread(THREAD_SUSPEND_RESUME, FALSE, threadEntry.ThreadId) }.get()) == -1
				);
				return true;
			});
		}
	};

	PVOID WritePointerInternal(PVOID* memoryAddress, PVOID value);
	template <typename T1, typename T2> FORCEINLINE T2 WritePointer(T1 address, T2 value) { return reinterpret_cast<T2>(WritePointerInternal(reinterpret_cast<PVOID*>(address), reinterpret_cast<PVOID>(value))); }
	HMODULE GetProcessModule(HANDLE processHandle, std::wstring_view dllPath);

	void WalkIAT(PVOID baseAddress, std::string_view dllName, std::function<bool(PVOID* functionAddress, LPCSTR functionNameOrOrdinal, bool importedByName)> callback);
	void WalkDelayloadIAT(PVOID baseAddress, std::string_view dllName, std::function<bool(HMODULE* moduleHandle, PVOID* functionAddress, LPCSTR functionNameOrOrdinal, bool importedByName)> callback);

	PVOID* GetIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal);
	std::pair<HMODULE*, PVOID*> GetDelayloadIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal, bool resolveAPI = false);
	PVOID WriteIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal, PVOID detourFunction);
	std::pair<HMODULE, PVOID> WriteDelayloadIAT(PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal, PVOID detourFunction, std::optional<HMODULE> newModuleHandle = std::nullopt);
	void ResolveDelayloadIAT(const std::pair<HMODULE*, PVOID*>& info, PVOID baseAddress, std::string_view dllName, LPCSTR targetFunctionNameOrOrdinal);

	template <typename T=PVOID>
	FORCEINLINE T* vtbl_of(void* This)
	{
		return reinterpret_cast<T*>(*reinterpret_cast<PVOID*>(This));
	}

	struct OffsetStorage
	{
		LONGLONG value{ 0 };

		FORCEINLINE bool IsValid() const { return value != 0; }
		template <typename T = PVOID, typename T2 = PVOID>
		FORCEINLINE T To(T2 baseAddress) const { if (baseAddress == 0 || !IsValid()) { return 0; } return reinterpret_cast<T>(RVA_TO_ADDR(baseAddress, value)); }
		template <typename T>
		FORCEINLINE void To(PVOID baseAddress, T& value) const { value = To<T>(baseAddress); }
		template <typename T = PVOID>
		static FORCEINLINE auto From(T baseAddress, T targetAddress) { return OffsetStorage{ LONGLONG(targetAddress) - LONGLONG(baseAddress) }; }
		static FORCEINLINE auto From(PVOID baseAddress, PVOID targetAddress) { if (!baseAddress || !targetAddress) { return OffsetStorage{ 0 }; } return OffsetStorage{ reinterpret_cast<LONGLONG>(targetAddress) - reinterpret_cast<LONGLONG>(baseAddress) }; }
	};

	namespace Detours
	{
		// Call single or multiple Attach/Detach in the callback
		HRESULT Write(const std::function<void()>&& callback);
		// Install an inline hook using Detours
		void Attach(std::string_view dllName, std::string_view funcName, PVOID* realFuncAddr, PVOID hookFuncAddr) noexcept(false);
		void Attach(PVOID* realFuncAddr, PVOID hookFuncAddr) noexcept(false);
		// Uninstall an inline hook using Detours
		void Detach(PVOID* realFuncAddr, PVOID hookFuncAddr) noexcept(false);

		template <typename T>
		FORCEINLINE void Attach(T* org, T detour)
		{
			Attach(reinterpret_cast<PVOID*>(org), reinterpret_cast<PVOID>(detour));
		}
		template <typename T>
		FORCEINLINE void Detach(T* org, T detour)
		{
			Detach(reinterpret_cast<PVOID*>(org), reinterpret_cast<PVOID>(detour));
		}
	}
}