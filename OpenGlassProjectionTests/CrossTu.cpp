#include "pch.h"
#include "ProjectionFixture.hpp"

namespace OpenGlassProjectionTests
{
	OpenGlass::Projection::ModuleRegistry* g_activeRegistry{};

	int InvokeCrossTu(int value)
	{
		return g_symbol(value);
	}
}

template <>
OpenGlass::Projection::ModuleRegistry&
OpenGlass::Projection::RegistryFor<OpenGlassProjectionTests::FixtureModuleTag>() noexcept
{
	return *OpenGlassProjectionTests::g_activeRegistry;
}
