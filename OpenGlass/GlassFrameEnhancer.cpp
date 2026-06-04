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
	decltype(&MyCThemePartPrimitive_ShouldClone) g_CThemePartPrimitive_ShouldClone_Org{ nullptr };
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
	uDWM::g_projectionArray.ApplyToVariable("CThemePartPrimitive::ShouldClone", g_CThemePartPrimitive_ShouldClone_Org);

	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CThemePartPrimitive_ShouldClone_Org, &MyCThemePartPrimitive_ShouldClone }
		},
		true
	);
}

void GlassFrameEnhancer::Shutdown()
{
	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CThemePartPrimitive_ShouldClone_Org, &MyCThemePartPrimitive_ShouldClone }
		},
		false
	);
}
