#pragma once
#include "pch.h"
#include "../OpenGlass/ProjectionHelper.hpp"

namespace OpenGlassTests
{
	using TestFunction = int (*)(int);
	struct FixtureModuleTag;
	extern OpenGlass::Projection::ModuleRegistry* g_activeRegistry;
	extern LONG g_layoutOffsets[8];
	extern bool g_layoutSupported[8];

	int Target(int value);
	int Replacement(int value);
	int Replacement2(int value);
	int InvokeCrossTu(int value);
}

namespace OpenGlass::Projection
{
	template <>
	ModuleRegistry& RegistryFor<OpenGlassTests::FixtureModuleTag>() noexcept;

	template <>
	struct LayoutState<OpenGlassTests::FixtureModuleTag>
	{
		static __forceinline LONG Offset(size_t index) noexcept
		{
			return OpenGlassTests::g_layoutOffsets[index];
		}

		static __forceinline bool IsSupported(size_t index) noexcept
		{
			return OpenGlassTests::g_layoutSupported[index];
		}
	};
}

namespace OpenGlassTests
{
	inline constexpr OpenGlass::Projection::SymbolHandle<FixtureModuleTag, 0, TestFunction> g_symbol{};
}

namespace OpenGlassTests
{
	template <size_t SymbolCount, size_t BindingCount, size_t LayoutCount, size_t CaseCount, size_t SymbolSpecCount = SymbolCount>
	struct RegistryStorage
	{
		std::array<char, 512> strings{};
		std::array<size_t, 8> symbolNameOffsets{};
		std::array<OpenGlass::Projection::Version, 4> versions{};
		std::array<ULONG, 2> knownBuilds{200, 250};
		std::array<OpenGlass::Projection::SymbolSpec, SymbolSpecCount> symbols{};
		std::array<PVOID, SymbolCount> candidates{};
		std::array<PVOID, SymbolCount> resolved{};
		std::array<OpenGlass::Projection::ResolutionState, SymbolCount> resolutionStates{};
		std::array<size_t, SymbolCount> bindingSymbols{};
		std::array<OpenGlass::Projection::BindingSpec, BindingCount> bindings{};
		std::array<OpenGlass::Projection::LayoutSpec, LayoutCount> layouts{};
		std::array<OpenGlass::Projection::LayoutCase, CaseCount> cases{};
		OpenGlass::Projection::ModuleRegistry registry;

		RegistryStorage(OpenGlass::Projection::VersionRange supportedRange = OpenGlass::Projection::all_versions)
			: registry{
				"test", strings.data(), std::span{symbolNameOffsets}, std::span{versions}, std::span{knownBuilds},
				std::span{symbols}, std::span{candidates}, std::span{resolved}, std::span{resolutionStates},
				std::span{bindings}, std::span{layouts}, std::span{cases}, std::span{g_layoutOffsets}.first(LayoutCount),
				std::span{g_layoutSupported}.first(LayoutCount), supportedRange
			}
		{
			for (size_t index = 0; index < SymbolCount; index++)
			{
				bindingSymbols[index] = index;
			}
		}
	};

	template <typename Storage, size_t Size>
	void SetStrings(Storage& storage, const char (&value)[Size])
	{
		static_assert(Size <= storage.strings.size());
		memcpy(storage.strings.data(), value, Size);
	}

	template <typename Storage, size_t Size>
	size_t AddString(Storage& storage, size_t& cursor, const char (&value)[Size])
	{
		const auto offset = cursor;
		memcpy(storage.strings.data() + cursor, value, Size);
		cursor += Size;
		return offset;
	}
}
