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
