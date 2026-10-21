#include "pch.h"
#include "ProjectionFixture.hpp"

namespace OpenGlassTests
{
	OpenGlass::Projection::ModuleRegistry* g_activeRegistry{};

	int InvokeCrossTu(int value)
	{
		return g_symbol(value);
	}
}

template <>
OpenGlass::Projection::ModuleRegistry&
OpenGlass::Projection::RegistryFor<OpenGlassTests::FixtureModuleTag>() noexcept
{
	return *OpenGlassTests::g_activeRegistry;
}

LONG OpenGlassTests::g_layoutOffsets[8]{};
bool OpenGlassTests::g_layoutSupported[8]{};

int OpenGlassTests::Target(int value)
{
	return value + 10;
}

int OpenGlassTests::Replacement(int value)
{
	return value + 20;
}

int OpenGlassTests::Replacement2(int value)
{
	return value + 30;
}
