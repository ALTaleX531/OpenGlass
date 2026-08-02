#pragma once
#include "pch.h"
#include "../OpenGlass/ProjectionHelper.hpp"

namespace OpenGlassProjectionTests
{
	using TestFunction = int (*)(int);
	struct FixtureModuleTag;
	extern OpenGlass::Projection::ModuleRegistry* g_activeRegistry;
	extern LONG g_layoutOffsets[8];
	extern bool g_layoutSupported[8];

	int Target(int value);
	int Replacement(int value);
	int InvokeCrossTu(int value);
}

namespace OpenGlass::Projection
{
	template <>
	ModuleRegistry& RegistryFor<OpenGlassProjectionTests::FixtureModuleTag>() noexcept;

	template <>
	struct LayoutState<OpenGlassProjectionTests::FixtureModuleTag>
	{
		static __forceinline LONG Offset(size_t index) noexcept
		{
			return OpenGlassProjectionTests::g_layoutOffsets[index];
		}

		static __forceinline bool IsSupported(size_t index) noexcept
		{
			return OpenGlassProjectionTests::g_layoutSupported[index];
		}
	};
}

namespace OpenGlassProjectionTests
{
	inline constexpr OpenGlass::Projection::SymbolHandle<FixtureModuleTag, 0, TestFunction> g_symbol{};
}
