#include "pch.h"
#include "AccentOverrider.hpp"
#include "GlassInfra.hpp"
#include "GlassKernel.hpp"
#include "ReflectionVisual.hpp"
#include "uDwmProjection.hpp"
#include "Shared.hpp"

using namespace OpenGlass;
namespace OpenGlass::AccentOverrider
{
	extern "C" bool MyCAccent__IsAcrylicBlurEnabledAndAllowed(const uDWM::CAccent* This);
	extern "C" bool MyCAccent__IsAcrylicBlurEnabledAndAllowed_Impl(const uDWM::CAccent* This);
	HRESULT MyCAccent__UpdateSolidFill(uDWM::CAccent* This, uDWM::CNineGridVisual* visual, ULONG color, const RECT& rect, float opacity);
	void MyCAccent__CleanupNonStaticsResources(uDWM::CAccent* This);
	HRESULT MyCAccent_CloneVisualTree(uDWM::CAccent* This, uDWM::CVisual** clone, UINT options);

	Projection::CustomDispatchDetour<uDWM::Symbol_CAccent__IsAcrylicBlurEnabledAndAllowed, &MyCAccent__IsAcrylicBlurEnabledAndAllowed> g_CAccent__IsAcrylicBlurEnabledAndAllowed_Org{};
	Projection::Detour<uDWM::Symbol_CAccent__UpdateSolidFill, &MyCAccent__UpdateSolidFill> g_CAccent__UpdateSolidFill_Org{};
	Projection::Detour<uDWM::Symbol_CAccent__CleanupNonStaticsResources, &MyCAccent__CleanupNonStaticsResources> g_CAccent__CleanupNonStaticsResources_Org{};
	Projection::Detour<uDWM::Symbol_CAccent_CloneVisualTree, &MyCAccent_CloneVisualTree> g_CAccent_CloneVisualTree_Org{};

	struct CAccentEffect
	{
		winrt::com_ptr<uDWM::CSolidRectangleVisual> visual;
		winrt::com_ptr<uDWM::CRgnGeometryProxy> geometry;
		std::bitset<2> status;
	};
	std::unordered_map<uDWM::CAccent*, std::vector<CAccentEffect>> g_effectVisuals{};

	uDWM::CTopLevelWindow* GetAccentOverrideWindow(const uDWM::CAccent* accent)
	{
		if (!Shared::g_overrideAccent)
		{
			return nullptr;
		}
		const auto window = uDWM::TryGetWindowFromVisual(accent);
		const auto data = window ? window->GetData() : nullptr;
		return data && UxMitigation::AllowsAccentOverride(data->GetHwnd(), Shared::g_uxMitigationFlags) ? window : nullptr;
	}

	void DetachEffect(uDWM::CSolidRectangleVisual* effect)
	{
		CReflectionVisual::RemoveChildren(effect);
		if (const auto parent = effect->GetTransformParent())
		{
			FAIL_FAST_IF_FAILED_MSG(parent->GetVisualCollection()->Remove(effect), "Unable to remove an accent effect visual");
		}
	}

	void RemoveAllEffects()
	{
		for (const auto& [accent, effects] : g_effectVisuals)
		{
			for (const auto& effect : effects)
			{
				DetachEffect(effect.visual.get());
			}
		}
		g_effectVisuals.clear();
	}

	HRESULT UpdateEffect(uDWM::CAccent* accent, uDWM::CNineGridVisual* background, uDWM::CTopLevelWindow* window, float opacity)
	{
		auto& effects = g_effectVisuals[accent];
		auto it = std::ranges::find_if(effects, [background](const auto& effect)
		{
			return effect.visual->GetTransformParent() == background;
		});
		if (it == effects.end())
		{
			winrt::com_ptr<uDWM::CSolidRectangleVisual> effect;
			RETURN_IF_FAILED(uDWM::CSolidRectangleVisual::Create(effect.put()));
			RETURN_IF_FAILED(effect->UpdateColor(D2D1::ColorF(D2D1::ColorF::White)));
			RETURN_IF_FAILED(background->GetVisualCollection()->InsertRelative(effect.get(), nullptr, false, true));
			effects.push_back({ std::move(effect) });
			it = std::prev(effects.end());
		}
		const auto effect = it->visual.get();
		const auto& rect = background->GetRect();
		const auto width = wil::rect_width(rect);
		const auto height = wil::rect_height(rect);
		effect->SetRect(D2D1::RectF(0.f, 0.f, width, height));
		effect->SetOpacity(opacity);

		// The native brush's hollow center does not clip children.
		winrt::com_ptr<abi::ICompositionBrush> brush;
		RETURN_IF_FAILED(background->GetBrush(brush.put()));
		winrt::com_ptr<abi::ICompositionNineGridBrush> nineGrid;
		RETURN_IF_FAILED(brush->QueryInterface(nineGrid.put()));
		boolean hollow{};
		RETURN_IF_FAILED(nineGrid->get_IsCenterHollow(&hollow));
		wil::unique_hrgn region{ CreateRectRgn(0, 0, static_cast<int>(width), static_cast<int>(height)) };
		RETURN_LAST_ERROR_IF_NULL(region);
		if (hollow)
		{
			float left, top, right, bottom;
			RETURN_IF_FAILED(nineGrid->get_LeftInset(&left));
			RETURN_IF_FAILED(nineGrid->get_TopInset(&top));
			RETURN_IF_FAILED(nineGrid->get_RightInset(&right));
			RETURN_IF_FAILED(nineGrid->get_BottomInset(&bottom));
			wil::unique_hrgn hole{ CreateRectRgn(static_cast<int>(left), static_cast<int>(top), static_cast<int>(width - right), static_cast<int>(height - bottom)) };
			RETURN_LAST_ERROR_IF_NULL(hole);
			RETURN_LAST_ERROR_IF(CombineRgn(region.get(), region.get(), hole.get(), RGN_DIFF) == ERROR);
		}
		winrt::com_ptr<uDWM::CRgnGeometryProxy> geometry;
		RETURN_IF_FAILED(uDWM::ResourceHelper::CreateGeometryFromHRGN(region.get(), geometry.put()));
		it->geometry = std::move(geometry);
		const auto status = GlassKernel::GetWindowStatus(window);
		RETURN_IF_FAILED(GlassInfra::UpdateGlassMarker(effect, it->geometry.get(), status));
		it->status = status;
		// Glass-only captures receive reflection at the preview presentation layer.
		if (const auto preview = uDWM::CDesktopManager::GetInstance()->GetLivePreview();
			preview && window->GetTransformParent() == preview->GetGlassRoot())
		{
			return S_OK;
		}
		winrt::com_ptr<CReflectionVisual> reflection;
		RETURN_IF_FAILED(CReflectionVisual::GetOrCreate(window, CReflectionVisual::Slot::Accent, effect, reflection.put()));
		return reflection->UpdateForWindow(window, CReflectionVisual::Slot::Accent);
	}
}

HRESULT AccentOverrider::MyCAccent_CloneVisualTree(uDWM::CAccent* This, uDWM::CVisual** clone, UINT options)
{
	constexpr UINT glassOnlyCloneOption = 1u << 3;
	const auto& sourcePolicy = This->GetAccentPolicy();
	HRESULT result;
	if ((options & glassOnlyCloneOption) && g_effectVisuals.contains(This) &&
		(sourcePolicy.AccentState == 3 || sourcePolicy.AccentState == 4))
	{
		// Native cloning replays Accent policy without honoring the glass-only
		// option. Keep the atlas flags, but do not rebuild its client background.
		auto& policy = This->GetAccentPolicy();
		const auto previousPolicy = policy;
		const auto restore = wil::scope_exit([&policy, previousPolicy] { policy = previousPolicy; });
		policy.AccentState = 2;
		policy.dwGradientColor = 0;
		policy.dwAnimationId = 0;
		result = g_CAccent_CloneVisualTree_Org(This, clone, options);
	}
	else
	{
		result = g_CAccent_CloneVisualTree_Org(This, clone, options);
	}
	if (SUCCEEDED(result))
	{
		// UpdateAccentPolicy already dirtied the clone; retain its native visibility gate.
		static_cast<uDWM::CAccent*>(*clone)->GetParentVisible() = This->GetParentVisible();
	}
	return result;
}

bool AccentOverrider::MyCAccent__IsAcrylicBlurEnabledAndAllowed_Impl(const uDWM::CAccent* This)
{
	return g_CAccent__IsAcrylicBlurEnabledAndAllowed_Org.Dispatch([](const uDWM::CAccent* accent)
	{
		return !GetAccentOverrideWindow(accent) && g_CAccent__IsAcrylicBlurEnabledAndAllowed_Org(accent);
	}, This);
}

HRESULT AccentOverrider::MyCAccent__UpdateSolidFill(uDWM::CAccent* This, uDWM::CNineGridVisual* visual, ULONG color, const RECT& rect, float opacity)
{
	// EnsureBackgroundVisual creates the primary before the transition layer.
	// During creation the corresponding native pointer has not been assigned yet.
	const auto primary = This->GetBackgroundVisual();
	const auto& policy = !primary || primary == visual ? This->GetAccentPolicy() : This->GetTransitionPolicy();
	const auto window = (policy.AccentState == 3 || policy.AccentState == 4) ? GetAccentOverrideWindow(This) : nullptr;
	const auto override = window != nullptr;
	if (override)
	{
		// Match Legacy's native maximized-colorization subscription. The next
		// application blur-attribute update may replace this bit as usual.
		window->GetData()->EnableTransitionOnMaximized();
	}
	if (const auto it = g_effectVisuals.find(This); it != g_effectVisuals.end())
	{
		// Native policy changes and EndTransition destroy old nine-grid layers.
		std::erase_if(it->second, [visual, override](const auto& effect)
		{
			const auto parent = effect.visual->GetTransformParent();
			if (!parent || (!override && parent == visual))
			{
				DetachEffect(effect.visual.get());
				return true;
			}
			return false;
		});
		if (it->second.empty())
		{
			g_effectVisuals.erase(it);
		}
	}
	// Keep the native brush graph, insets, rectangle and ForceRender behavior.
	// Only its fill is transparent while the child supplies OpenGlass rendering.
	RETURN_IF_FAILED(g_CAccent__UpdateSolidFill_Org(This, visual, color, rect, override ? 0.f : opacity));
	return override ? UpdateEffect(This, visual, window, opacity) : S_OK;
}

void AccentOverrider::MyCAccent__CleanupNonStaticsResources(uDWM::CAccent* This)
{
	if (const auto it = g_effectVisuals.find(This); it != g_effectVisuals.end())
	{
		for (const auto& effect : it->second)
		{
			DetachEffect(effect.visual.get());
		}
		g_effectVisuals.erase(it);
	}
	return g_CAccent__CleanupNonStaticsResources_Org(This);
}

HRESULT AccentOverrider::UpdateMarkers(uDWM::CTopLevelWindow* window)
{
	if (const auto it = g_effectVisuals.find(window->GetAccentVisual()); it != g_effectVisuals.end())
	{
		const auto status = GlassKernel::GetWindowStatus(window);
		for (auto& effect : it->second)
		{
			// Policy changes can retire the old layer before its next fill update.
			if (effect.visual->GetTransformParent() && effect.status != status)
			{
				RETURN_IF_FAILED(GlassInfra::UpdateGlassMarker(effect.visual.get(), effect.geometry.get(), status));
				effect.status = status;
			}
		}
	}
	return S_OK;
}

void AccentOverrider::Update(GlassEngine::UpdateType type)
{
	if (type & GlassEngine::UpdateType::Backdrop)
	{
		Shared::g_overrideAccent = static_cast<bool>(GlassEngine::GetDwordFromRegistry(L"GlassOverrideAccent"));
		for (auto it = g_effectVisuals.begin(); it != g_effectVisuals.end();)
		{
			if (!GetAccentOverrideWindow(it->first))
			{
				for (const auto& effect : it->second)
				{
					DetachEffect(effect.visual.get());
				}
				it = g_effectVisuals.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
}

void AccentOverrider::Startup()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_AccentOverrider))
	{
		return;
	}
	HookHelper::ApplyInlineHooks(std::initializer_list<HookHelper::DetourInfo>
	{
		{ &g_CAccent__IsAcrylicBlurEnabledAndAllowed_Org },
		{ &g_CAccent__UpdateSolidFill_Org },
		{ &g_CAccent__CleanupNonStaticsResources_Org },
		{ &g_CAccent_CloneVisualTree_Org }
	});
}

void AccentOverrider::Shutdown()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_AccentOverrider))
	{
		return;
	}
	HookHelper::ApplyInlineHooks(std::initializer_list<HookHelper::DetourInfo>
	{
		{ &g_CAccent__IsAcrylicBlurEnabledAndAllowed_Org },
		{ &g_CAccent__UpdateSolidFill_Org },
		{ &g_CAccent__CleanupNonStaticsResources_Org },
		{ &g_CAccent_CloneVisualTree_Org }
	});
}

void AccentOverrider::Cleanup()
{
	RemoveAllEffects();
}
