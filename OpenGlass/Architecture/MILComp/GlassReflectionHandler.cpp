#include "pch.h"
#include "GlassReflectionHandler.hpp"
#include "uDwmProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "Shared.hpp"
#include "GlassInfra.hpp"
#include "ReflectionVisual.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassReflectionHandler
{
	HRESULT MyCLivePreview__FadeOutToGlass(uDWM::CLivePreview* This);
	HRESULT MyCLivePreview__UpdateResources(uDWM::CLivePreview* This);
	Projection::Detour<uDWM::Symbol_CLivePreview__FadeOutToGlass, &MyCLivePreview__FadeOutToGlass> g_CLivePreview__FadeOutToGlass_Org{};
	Projection::Detour<uDWM::Symbol_CLivePreview__UpdateResources, &MyCLivePreview__UpdateResources> g_CLivePreview__UpdateResources_Org{};

	HRESULT UpdateReflection(uDWM::CLivePreview* preview, const uDWM::LivePreviewResource& resource)
	{
		const auto parent = resource.glassVisual;
		auto reflection = CReflectionVisual::Find(parent);
		if (!(Shared::g_reflectionPolicy & Shared::ReflectionPolicy::LivePreview))
		{
			return reflection ? reflection->UpdateOpacity(0.f) : S_OK;
		}
		winrt::com_ptr<CReflectionVisual> created;
		if (!reflection)
		{
			RETURN_IF_FAILED(CReflectionVisual::Create(created.put()));
			reflection = created.get();
			RETURN_IF_FAILED(parent->GetVisualCollection()->InsertRelative(reflection, nullptr, false, true));
		}
		wil::unique_hrgn region{ CreateRectRgn(0, 0, 0, 0) };
		RETURN_LAST_ERROR_IF_NULL(region);
		for (const auto& visual : preview->GetWindows())
		{
			const auto window = visual.data->GetWindow();
			if (!window || !visual.data->HasLivePreviewGlass())
			{
				continue;
			}
			RECT rect;
			window->GetActualWindowRect(&rect, false, true, true);
			const auto diameter = Shared::g_roundRectRadius * 2;
			wil::unique_hrgn windowRegion
			{
				diameter ? CreateRoundRectRgn(rect.left, rect.top, rect.right + 1, rect.bottom + 1, diameter, diameter) :
				CreateRectRgnIndirect(&rect)
			};
			RETURN_LAST_ERROR_IF_NULL(windowRegion);
			RETURN_LAST_ERROR_IF(CombineRgn(region.get(), region.get(), windowRegion.get(), RGN_OR) == ERROR);
		}
		wil::unique_hrgn monitorRegion{ CreateRectRgnIndirect(&resource.monitorRect) };
		RETURN_LAST_ERROR_IF_NULL(monitorRegion);
		RETURN_LAST_ERROR_IF(CombineRgn(region.get(), region.get(), monitorRegion.get(), RGN_AND) == ERROR);
		// CRectangleVisual's composition offset includes its rectangle origin.
		RETURN_LAST_ERROR_IF(OffsetRgn(region.get(), -resource.glassRect.left, -resource.glassRect.top) == ERROR);
		winrt::com_ptr<uDWM::CRgnGeometryProxy> geometry;
		RETURN_IF_FAILED(uDWM::ResourceHelper::CreateGeometryFromHRGN(region.get(), geometry.put()));
		RETURN_IF_FAILED(reflection->GetVisualProxy()->SetClip(geometry.get()));
		const SIZE size{ wil::rect_width(resource.glassRect), wil::rect_height(resource.glassRect) };
		reflection->SetSize(&size);
		RETURN_IF_FAILED(reflection->UpdateSurface(GlassInfra::GetOrCreateReflectionSurface()));
		auto offset = preview->GetLocalToParentVisualOffset();
		offset.x += resource.glassRect.left;
		offset.y += resource.glassRect.top;
		RETURN_IF_FAILED(reflection->UpdateViewport(offset));
		return reflection->UpdateOpacity(1.f);
	}
}

HRESULT GlassReflectionHandler::MyCLivePreview__FadeOutToGlass(uDWM::CLivePreview* This)
{
	// Let uDWM insert the preview and create ordinary clones before capturing them.
	// Reversals retain its timeline's current opacity and existing clone inventory.
	RETURN_IF_FAILED(g_CLivePreview__FadeOutToGlass_Org(This));
	return This->UpdateResources();
}

HRESULT GlassReflectionHandler::MyCLivePreview__UpdateResources(uDWM::CLivePreview* This)
{
	const auto glassRoot = This->GetGlassRoot();
	// Match the native ordinary-clone traversal and retain native record ownership.
	for (auto& visual : This->GetWindows() | std::views::reverse)
	{
		const auto window = visual.data->GetWindow();
		if (!visual.glassClone && window && visual.data->HasLivePreviewGlass())
		{
			winrt::com_ptr<uDWM::CTopLevelWindow> clone;
			RETURN_IF_FAILED(window->CloneVisualTreeForLivePreview(true, clone.put()));
			RETURN_IF_FAILED(glassRoot->GetVisualCollection()->InsertRelative(clone.get(), nullptr, false, true));
			visual.glassClone = clone.detach();
		}
	}
	glassRoot->RenderRecursive();
	// Reset retains its visual surface. A frozen capture cannot follow new bounds
	// when the source visual is unchanged, so each resource update needs fresh captures.
	for (auto& resource : This->GetResources())
	{
		resource.normalBrush = {};
		resource.glassBrush = {};
	}
	RETURN_IF_FAILED(g_CLivePreview__UpdateResources_Org(This));

	const auto opacity = This->GetTimeline()->GetCurrentOpacity();
	for (const auto& resource : This->GetResources())
	{
		// Enumeration copies a temporary LivePreviewResource into the native vector;
		// its destructor removes both visuals. Attach only after enumeration finishes.
		if (const auto normal = resource.normalVisual)
		{
			normal->SetOpacity(opacity);
			if (!normal->GetTransformParent())
			{
				RETURN_IF_FAILED(This->GetVisualCollection()->InsertRelative(normal, nullptr, false, true));
			}
		}
		if (const auto glass = resource.glassVisual)
		{
			glass->SetOpacity(1.0 - opacity);
			if (!glass->GetTransformParent())
			{
				RETURN_IF_FAILED(This->GetVisualCollection()->InsertRelative(glass, nullptr, false, true));
			}
			RETURN_IF_FAILED(UpdateReflection(This, resource));
		}
	}
	return S_OK;
}

void GlassReflectionHandler::Update(GlassEngine::UpdateType type)
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_GlassReflectionHandler) ||
		!(type & (GlassEngine::UpdateType::Theme | GlassEngine::UpdateType::Backdrop)))
	{
		return;
	}
	if (const auto preview = uDWM::CDesktopManager::GetInstance()->GetLivePreview())
	{
		for (const auto& resource : preview->GetResources())
		{
			if (resource.glassVisual)
			{
				LOG_IF_FAILED(UpdateReflection(preview, resource));
			}
		}
	}
}

void GlassReflectionHandler::Startup()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_GlassReflectionHandler))
	{
		return;
	}
	HookHelper::ApplyInlineHooks(std::initializer_list<HookHelper::DetourInfo>
	{
		{ &g_CLivePreview__FadeOutToGlass_Org },
		{ &g_CLivePreview__UpdateResources_Org }
	});
}

void GlassReflectionHandler::Shutdown()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_GlassReflectionHandler))
	{
		return;
	}
	HookHelper::ApplyInlineHooks(std::initializer_list<HookHelper::DetourInfo>
	{
		{ &g_CLivePreview__FadeOutToGlass_Org },
		{ &g_CLivePreview__UpdateResources_Org }
	});
}

void GlassReflectionHandler::Cleanup()
{
}
