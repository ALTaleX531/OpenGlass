#include "pch.h"
#include "Shared.hpp"
#include "GlassFrameEnhancer.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassFrameEnhancer
{
	bool MyCThemePartPrimitive_ShouldClone(
		uDWM::CThemePartPrimitive* This,
		BYTE cloneOptions
	);
	Projection::Detour<uDWM::Symbol_CThemePartPrimitive_ShouldClone, &MyCThemePartPrimitive_ShouldClone> g_CThemePartPrimitive_ShouldClone_Org{};
}

bool GlassFrameEnhancer::MyCThemePartPrimitive_ShouldClone(
	uDWM::CThemePartPrimitive* This,
	BYTE cloneOptions
)
{
	// TO-DO: implement more specific logic to determine when to clone, instead of just checking the second bit of cloneOptions
	// ...
	if (cloneOptions & 2)
	{
		return true;
	}
	return g_CThemePartPrimitive_ShouldClone_Org(This, cloneOptions);
}

void GlassFrameEnhancer::Update([[maybe_unused]] GlassEngine::UpdateType type)
{
}

void GlassFrameEnhancer::Startup()
{

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CThemePartPrimitive_ShouldClone_Org }
		},
		true
	);
}

void GlassFrameEnhancer::Shutdown()
{
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CThemePartPrimitive_ShouldClone_Org }
		},
		false
	);
}

void GlassFrameEnhancer::Cleanup()
{
}
