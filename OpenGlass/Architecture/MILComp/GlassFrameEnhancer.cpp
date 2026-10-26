#include "pch.h"
#include "Shared.hpp"
#include "GlassFrameEnhancer.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassFrameEnhancer
{
	bool MyCThemePartPrimitive_ShouldClone(
		uDWM::CThemePartPrimitive* This,
		UINT cloneOptions
	);
	Projection::Detour<uDWM::Symbol_CThemePartPrimitive_ShouldClone, &MyCThemePartPrimitive_ShouldClone> g_CThemePartPrimitive_ShouldClone_Org{};
	HRESULT MyCThemePartPrimitive_ClonePrimitive(uDWM::CThemePartPrimitive* This, uDWM::CPrimitive** primitive, UINT cloneOptions);
	Projection::Detour<uDWM::Symbol_CThemePartPrimitive_ClonePrimitive, &MyCThemePartPrimitive_ClonePrimitive> g_CThemePartPrimitive_ClonePrimitive_Org{};
}

HRESULT GlassFrameEnhancer::MyCThemePartPrimitive_ClonePrimitive(uDWM::CThemePartPrimitive* This, uDWM::CPrimitive** primitive, UINT cloneOptions)
{
	const auto partId = This->GetPartId();
	if ((cloneOptions & 10) == 10 && partId >= 9 && partId <= 17)
	{
		// ThemePart::Generate suppresses squeegees even in a glass-only clone.
		// A native nine-grid copy retains the atlas, opacity, margins and layout,
		// and draws those pixels without changing the source theme primitive.
		return This->CNineGridImagePrimitive::ClonePrimitive(primitive, cloneOptions);
	}
	return g_CThemePartPrimitive_ClonePrimitive_Org(This, primitive, cloneOptions);
}

bool GlassFrameEnhancer::MyCThemePartPrimitive_ShouldClone(
	uDWM::CThemePartPrimitive* This,
	UINT cloneOptions
)
{
	// Ordinary LivePreview clones retain the frame, but glass-only clones must
	// keep the native squeegee/shadow selection instead of copying the NC frame.
	if ((cloneOptions & 2) && !(cloneOptions & 8))
	{
		const auto partId = This->GetPartId();
		if (partId <= 7)
		{
			return true;
		}
		if (partId >= 9 && partId <= 17)
		{
			return false;
		}
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
			{ &g_CThemePartPrimitive_ShouldClone_Org },
			{ &g_CThemePartPrimitive_ClonePrimitive_Org }
		}
	);
}

void GlassFrameEnhancer::Shutdown()
{
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CThemePartPrimitive_ShouldClone_Org },
			{ &g_CThemePartPrimitive_ClonePrimitive_Org }
		}
	);
}

void GlassFrameEnhancer::Cleanup()
{
}
