#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "wil.hpp"

namespace OpenGlass::HookHelper
{
	template <typename T>
	concept function_pointer = std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T>>;

	struct DetourInfo
	{
		LPVOID* original;
		LPVOID detour;

		template <function_pointer T> FORCEINLINE DetourInfo(T* p1, T p2, bool condition = true) : original(condition ? reinterpret_cast<LPVOID*>(p1) : nullptr), detour(condition ? reinterpret_cast<LPVOID>(p2) : nullptr) {}
	};

	struct ImportFunctionDetourInfo : DetourInfo
	{
		LPCSTR importFunction;

		template <function_pointer T> FORCEINLINE ImportFunctionDetourInfo(LPCSTR f, T* p1, T p2, bool condition = true) : DetourInfo(p1, p2, condition), importFunction(condition ? f : nullptr) {}
	};

	struct ImportDllDetourInfo
	{
		LPCSTR importDllName;
		std::span<const ImportFunctionDetourInfo> functionsToDetour;
	};

	constexpr uint16_t c_patwc = 0xFFFFu;

	HMODULE GetRemoteModuleBase(DWORD processId, LPCWSTR moduleName);

	FORCEINLINE auto Unprotect(std::span<uint8_t> views)
	{
		THROW_HR_IF(E_INVALIDARG, views.empty());
		DWORD oldProtect{ 0 };
		THROW_IF_WIN32_BOOL_FALSE(
			VirtualProtect(
				views.data(),
				views.size_bytes(),
				PAGE_EXECUTE_READWRITE,
				&oldProtect
			)
		);
		return wil::scope_exit([=]
		{
			auto unused = 0ul;
			THROW_IF_WIN32_BOOL_FALSE(
				VirtualProtect(
					views.data(),
					views.size_bytes(),
					oldProtect,
					&unused
				)
			);
		});
	}
	const uint8_t* FindPattern(std::span<const uint8_t> base, std::span<const uint16_t> pat);
	void PatchPointer(void** from, void* to, void** original = nullptr);
	template <function_pointer T> FORCEINLINE void PatchPointerT(T* from, T to, T* original = nullptr){ PatchPointer(reinterpret_cast<void**>(from), reinterpret_cast<void*>(to), reinterpret_cast<void**>(original)); }
	void PatchInstructions(uint8_t* base, std::span<const uint8_t> data);
	void PatchIAT(
		HMODULE moduleHandle,
		std::span<const ImportDllDetourInfo> dllsToDetour
	);
	void PatchDelayloadIAT(
		HMODULE moduleHandle,
		std::span<const ImportDllDetourInfo> dllsToDetour
	);
	void PatchFunctions(std::span<const DetourInfo> functionsToDetour, bool enable);

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
