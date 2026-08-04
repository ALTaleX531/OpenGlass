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
