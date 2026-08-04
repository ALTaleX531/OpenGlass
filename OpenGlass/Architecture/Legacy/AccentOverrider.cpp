#include "pch.h"
#include "AccentOverrider.hpp"
#include "GlassKernel.hpp"
#include "uDWMProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "Shared.hpp"
#include "GlassEffectBrush.hpp"
#include "GlassReflectionBrush.hpp"

using namespace OpenGlass;
namespace OpenGlass::AccentOverrider
{
	HRESULT MyCAccent_UpdateAccentPolicy(
		uDWM::CAccent* This,
		LPCRECT rect,
		uDWM::ACCENT_POLICY* policy,
		uDWM::CBaseGeometryProxy* geometry
	);
	HRESULT MyCAccent__UpdateSolidFill(
		uDWM::CAccent* This,
		uDWM::CRenderDataVisual* visual,
		UINT color,
		const D2D1_RECT_F& rect,
		float opacity
	);
	void MyCAccent_SetClipRegion(
		uDWM::CAccent* This,
		uDWM::CBaseGeometryProxy* geometry
	);
	void MyCAccent_Destructor(
		uDWM::CAccent* This
	);
	Projection::Detour<uDWM::Symbol_CAccent_UpdateAccentPolicy, decltype(&MyCAccent_UpdateAccentPolicy)> g_CAccent_UpdateAccentPolicy_Org{};
	Projection::Detour<uDWM::Symbol_CAccent__UpdateSolidFill, decltype(&MyCAccent__UpdateSolidFill)> g_CAccent__UpdateSolidFill_Org{};
	Projection::Detour<uDWM::Symbol_CAccent_SetClipRegion, decltype(&MyCAccent_SetClipRegion)> g_CAccent_SetClipRegion_Org{};
	Projection::Detour<uDWM::Symbol_CAccent__CAccent, decltype(&MyCAccent_Destructor)> g_CAccent_Destructor_Org{};

	struct CAccentInfo
	{
		winrt::com_ptr<uDWM::CBaseGeometryProxy> clipRegion;
		winrt::com_ptr<uDWM::CRgnGeometryProxy> drawRegion;
		winrt::com_ptr<uDWM::CRenderDataVisual> visual;
	};
	std::unordered_map<uDWM::CAccent*, CAccentInfo> g_accentClipRegions{};
}

HRESULT AccentOverrider::MyCAccent_UpdateAccentPolicy(
	uDWM::CAccent* This,
	LPCRECT rect,
	uDWM::ACCENT_POLICY* policy,
	uDWM::CBaseGeometryProxy* geometry
)
{
	if (!Shared::g_overrideAccent)
	{
		return g_CAccent_UpdateAccentPolicy_Org(This, rect, policy, geometry);
	}

	if (
		policy->AccentState != 1 &&
		policy->AccentState != 3 &&
		policy->AccentState != 4
	)
	{
		return g_CAccent_UpdateAccentPolicy_Org(This, rect, policy, geometry);
	}

	auto accentPolicy = *policy;
	accentPolicy.AccentState = 1;
	accentPolicy.dwGradientColor = 0;
	if (const auto it = g_accentClipRegions.find(This); it != g_accentClipRegions.end() && it->second.clipRegion)
	{
		// remove accent border atlas
		accentPolicy.AccentFlags &= ~((1 << 5) | (1 << 6) | (1 << 7) | (1 << 8));
	}

	return g_CAccent_UpdateAccentPolicy_Org(This, rect, &accentPolicy, geometry);
}
HRESULT AccentOverrider::MyCAccent__UpdateSolidFill(
	uDWM::CAccent* This,
	uDWM::CRenderDataVisual* visual,
	UINT color,
	const D2D1_RECT_F& rect,
	float opacity
)
{
	const auto result = g_CAccent__UpdateSolidFill_Org(
		This,
		visual,
		color,
		rect,
		opacity
	);

	if (!Shared::g_overrideAccent)
	{
		return result;
	}

	const auto& accentPolicy = This->GetAccentPolicy();
	if (accentPolicy.AccentState == 2)
	{
		return result;
	}

	const auto window = uDWM::TryGetWindowFromVisual(This);
	if (!window)
	{
		visual->ClearInstructions();
		return result;
	}

	auto data = window->GetData();
	if (!data)
	{
		visual->ClearInstructions();
		return result;
	}

	auto& accentInfo = g_accentClipRegions[This];
	accentInfo.visual.copy_from(visual);

	// indicates window should transition on maximized, so we can receive colorization updates
	// this make permanent change to the attribute during its lifetime, until a update to the attribute is made
	// is there any better way to implement it?
	data->GetClientBlurAttributeReference() |= 8;

	auto effectBrush = GlassEffectBrush::GetOrCreate(window);
	const auto maximized = window->TreatAsMaximized(data);

	if (!visual->GetInstructions().views().empty())
	{
		winrt::com_ptr<uDWM::CDrawGeometryInstruction> instruction{ nullptr };
		{
			const auto drawRect = RectF::ToRectL(reinterpret_cast<uDWM::CSolidRectangleInstruction*>(visual->GetInstructions().views().front())->GetRectangle());
			RETURN_IF_FAILED(
				uDWM::ResourceHelper::CreateGeometryFromHRGN(
					wil::unique_hrgn{ CreateRectRgnIndirect(&drawRect) }.get(),
					accentInfo.drawRegion.put()
				)
			);
			if (!effectBrush)
			{
				RETURN_IF_FAILED(
					uDWM::CDesktopManager::GetInstance()->GetCompositor()->CreateSolidColorLegacyMilBrushProxy(
						effectBrush.put()
					)
				);
				auto glassColor = Color::sRGBToscRGB(
					GlassKernel::RealizeWindowColorization(
						GlassKernel::GetBaseColor(Shared::IsTransparencyDisabled(), maximized),
						GlassKernel::GetSourceColor(true),
						GlassKernel::GetColorizationOpacity(true, maximized),
						Shared::IsTransparencyDisabled(),
						false
					).GetEffectivescRGBBlendColor(0.f),
					0.f
				);
				glassColor.a = GlassKernel::AlphaChannelReinterpreter(true, maximized).ToFloat();
				RETURN_IF_FAILED(effectBrush->Update(1.0, glassColor));
			}

			RETURN_IF_FAILED(
				uDWM::CDrawGeometryInstruction::Create(
					effectBrush.get(),
					accentInfo.drawRegion.get(),
					instruction.put()
				)
			);
			RETURN_IF_FAILED(visual->ClearInstructions());
			RETURN_IF_FAILED(visual->AddInstruction(instruction.get()));
		}

		if (
			const auto brush = GlassReflectionBrush::GetOrCreate(
				window,
				2,
				true
			);
			brush
		)
		{
			RETURN_IF_FAILED(
				brush->Update(
					(Shared::g_reflectionPolicy & Shared::ReflectionPolicy::NonClient) ?
					GlassKernel::GetAdjustedReflectionIntensity(true, maximized) :
					0.f,
					GlassReflectionBrush::CalculateTargetViewport(
						visual->GetLocalToParentVisualOffset(window->GetTransformParent()),
						Shared::g_reflectionParallaxIntensity,
						window->IsRTLMirrored(),
						window->GetWidth(),
						visual->GetScale()
					),
					D2D1::RectF(),
					nullptr,
					DWM::MilBrushMappingMode::Absolute,
					DWM::MilBrushMappingMode::Absolute,
					nullptr,
					nullptr,
					DWM::MilStretch::None,
					DWM::MilTileMode::Extend,
					DWM::MilHorizontalAlignment::Left,
					DWM::MilVerticalAlignment::Top,
					nullptr
				)
			);
			RETURN_IF_FAILED(
				uDWM::CDrawGeometryInstruction::Create(
					brush.get(),
					accentInfo.drawRegion.get(),
					instruction.put()
				)
			);
			RETURN_IF_FAILED(visual->AddInstruction(instruction.get()));
		}
	}

	if (
		accentPolicy.IsClipRegionEffective() &&
		accentInfo.clipRegion
	)
	{
		for (auto& instruction : accentInfo.visual->GetInstructions().views())
		{
			auto& originalGeometry = reinterpret_cast<uDWM::CDrawGeometryInstruction*>(instruction)->GetGeometry();
			if (originalGeometry)
			{
				originalGeometry->Release();
				originalGeometry = nullptr;
			}
			if (accentInfo.clipRegion)
			{
				winrt::com_ptr<uDWM::CCombinedGeometryProxy> combinedGeometry{ nullptr };
				RETURN_IF_FAILED(
					uDWM::ResourceHelper::CreateCombinedGeometry(
						accentInfo.clipRegion.get(),
						accentInfo.drawRegion.get(),
						D2D1_COMBINE_MODE_INTERSECT,
						combinedGeometry.put()
					)
				);
				originalGeometry = combinedGeometry.detach();
			}
			else
			{
				originalGeometry = accentInfo.drawRegion.get();
				originalGeometry->AddRef();
			}
		}
	}

	return result;
}
void AccentOverrider::MyCAccent_SetClipRegion(
	uDWM::CAccent* This,
	uDWM::CBaseGeometryProxy* geometry
)
{
	if (!Shared::g_overrideAccent)
	{
		return g_CAccent_SetClipRegion_Org(This, geometry);
	}

	const auto& accentPolicy = This->GetAccentPolicy();
	if (accentPolicy.AccentState == 2)
	{
		return g_CAccent_SetClipRegion_Org(This, geometry);
	}

	const auto window = uDWM::TryGetWindowFromVisual(This);
	if (!window)
	{
		return g_CAccent_SetClipRegion_Org(This, geometry);
	}

	auto data = window->GetData();
	if (!data)
	{
		return g_CAccent_SetClipRegion_Org(This, geometry);
	}

	auto& accentInfo = g_accentClipRegions[This];
	accentInfo.clipRegion.copy_from(geometry);

	if (
		accentPolicy.IsClipRegionEffective() &&
		accentInfo.visual &&
		accentInfo.drawRegion
	)
	{
		for (auto& instruction : accentInfo.visual->GetInstructions().views())
		{
			auto& originalGeometry = reinterpret_cast<uDWM::CDrawGeometryInstruction*>(instruction)->GetGeometry();
			if (originalGeometry)
			{
				originalGeometry->Release();
				originalGeometry = nullptr;
			}
			if (accentInfo.clipRegion)
			{
				winrt::com_ptr<uDWM::CCombinedGeometryProxy> combinedGeometry{ nullptr };
				THROW_IF_FAILED(
					uDWM::ResourceHelper::CreateCombinedGeometry(
						accentInfo.clipRegion.get(),
						accentInfo.drawRegion.get(),
						D2D1_COMBINE_MODE_INTERSECT,
						combinedGeometry.put()
					)
				);
				originalGeometry = combinedGeometry.detach();
			}
			else
			{
				originalGeometry = accentInfo.drawRegion.get();
				originalGeometry->AddRef();
			}
			THROW_IF_FAILED(accentInfo.visual->UpdateRenderData());
		}
	}
	return g_CAccent_SetClipRegion_Org(This, nullptr);
}
void AccentOverrider::MyCAccent_Destructor(
	uDWM::CAccent* This
)
{
	g_accentClipRegions.erase(This);
	return g_CAccent_Destructor_Org(This);
}
void AccentOverrider::Update(GlassEngine::UpdateType type)
{
	if (type & GlassEngine::UpdateType::Backdrop)
	{
		Shared::g_overrideAccent = static_cast<bool>(GlassEngine::GetDwordFromRegistry(L"GlassOverrideAccent"));
		if (!Shared::g_overrideAccent)
		{
			g_accentClipRegions.clear();
		}
	}
}

void AccentOverrider::Startup()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_AccentOverrider))
	{
		return;
	}

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CAccent_UpdateAccentPolicy_Org, &MyCAccent_UpdateAccentPolicy },
			{ &g_CAccent__UpdateSolidFill_Org, &MyCAccent__UpdateSolidFill },
			{ &g_CAccent_SetClipRegion_Org, &MyCAccent_SetClipRegion },
			{ &g_CAccent_Destructor_Org, &MyCAccent_Destructor },
		},
		true
	);
}

void AccentOverrider::Shutdown()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_AccentOverrider))
	{
		return;
	}

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CAccent_UpdateAccentPolicy_Org, &MyCAccent_UpdateAccentPolicy },
			{ &g_CAccent__UpdateSolidFill_Org, &MyCAccent__UpdateSolidFill },
			{ &g_CAccent_SetClipRegion_Org, &MyCAccent_SetClipRegion },
			{ &g_CAccent_Destructor_Org, &MyCAccent_Destructor },
		},
		false
	);

}

void AccentOverrider::Cleanup()
{
	g_accentClipRegions.clear();
}
