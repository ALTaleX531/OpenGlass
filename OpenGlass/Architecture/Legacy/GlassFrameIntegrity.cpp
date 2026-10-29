#include "pch.h"
#include "GlassFrameIntegrity.hpp"
#include "uDwmProjection.hpp"
#include "DetourChains.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassFrameIntegrity
{
	HRESULT MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This);
	RECT* MyCTopLevelWindow_GetBorderRect(const uDWM::CTopLevelWindow* This, RECT* rect, bool screenCoordinates);
	void MyCTopLevelWindow_GetBorderMargins(uDWM::CTopLevelWindow* This, MARGINS* margins);
	void MyCTopLevelWindow_GetFrameMargins(const uDWM::CTopLevelWindow* This, MARGINS* margins);

	DetourChains::TopLevelWindowValidateVisualIntegrityNode g_CTopLevelWindow_ValidateVisual_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_GetBorderRect, &MyCTopLevelWindow_GetBorderRect> g_CTopLevelWindow_GetBorderRect_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_GetBorderMargins, &MyCTopLevelWindow_GetBorderMargins> g_CTopLevelWindow_GetBorderMargins_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_GetFrameMargins, &MyCTopLevelWindow_GetFrameMargins> g_CTopLevelWindow_GetFrameMargins_Org{};

	void CalculateFrameMargins(const uDWM::CTopLevelWindow* window, MARGINS* margins)
	{
		RECT frame;
		window->GetActualWindowRect(&frame, true, true, true);
		const auto& bounds = window->GetData()->GetWindowRect();
		*margins = { frame.left, bounds.right - bounds.left - frame.right,
			frame.top, bounds.bottom - bounds.top - frame.bottom };
	}
}

HRESULT GlassFrameIntegrity::MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This)
{
	const auto data = This->GetData();
	// Notify the live owner, not a cloned tree that borrows its window data.
	if (!data || data->GetWindow() != This)
	{
		return g_CTopLevelWindow_ValidateVisual_Org(This);
	}

	const auto beforeInsideMargins = This->GetFrameInsideMargins();
	RECT before, after;
	This->GetActualWindowRect(&before, false, true, true);
	const auto hr = g_CTopLevelWindow_ValidateVisual_Org(This);
	if (SUCCEEDED(hr))
	{
		This->GetActualWindowRect(&after, false, true, true);
		const auto& insideMargins = This->GetFrameInsideMargins();
		const auto insideMarginsChanged =
			beforeInsideMargins.cxLeftWidth != insideMargins.cxLeftWidth ||
			beforeInsideMargins.cxRightWidth != insideMargins.cxRightWidth ||
			beforeInsideMargins.cyBottomHeight != insideMargins.cyBottomHeight;
		// WindowBorder caches its content clip before atlas sizing. Inside margins
		// can change even when the final rectangle stays the same. Only the side
		// and bottom margins participate in GetBorderRect's atlas exclusion.
		if (
			!EqualRect(&before, &after) ||
			(uDWM::g_versionInfo.build >= os::build_w11_21h2 && insideMarginsChanged)
		)
		{
			LOG_IF_FAILED(data->OnWindowSizeUpdated());
			// The sibling border may already have been visited in this frame.
			if (const auto border = data->GetProjectionBorder(); border)
			{
				LOG_IF_FAILED(border->RenderRecursive());
			}
		}
	}
	return hr;
}

RECT* GlassFrameIntegrity::MyCTopLevelWindow_GetBorderRect(
	const uDWM::CTopLevelWindow* This, RECT* rect, bool screenCoordinates)
{
	// Match the atlas-aware shadow exclusion used by the native legacy branch.
	// Calling GetActualWindowRect here would recurse through its modern-border branch.
	// GetBorderRect uses the native maximized-frame state, independent of appearance overrides.
	const auto maximized = (uDWM::CTopLevelWindow_StateDwordIndex.read(This) & 0x20) != 0;
	auto margins = This->GetFrameOutsideMargins(maximized);
	const auto left = This->GetNCAreaAtlasImage1();
	const auto right = This->GetNCAreaAtlasImage2();
	const auto bottom = This->GetNCAreaAtlasImage3();
	if (left && right && bottom)
	{
		const auto& inside = This->GetFrameInsideMargins();
		margins.cxLeftWidth = std::max<LONG>(margins.cxLeftWidth, inside.cxLeftWidth - left->GetSize().cx);
		margins.cxRightWidth = std::max<LONG>(margins.cxRightWidth, inside.cxRightWidth - right->GetSize().cx);
		margins.cyBottomHeight = std::max<LONG>(margins.cyBottomHeight, inside.cyBottomHeight - bottom->GetSize().cy);
	}
	*rect = This->GetData()->GetWindowRect();
	if (!screenCoordinates)
	{
		OffsetRect(rect, -rect->left, -rect->top);
	}
	rect->left += margins.cxLeftWidth;
	rect->right -= margins.cxRightWidth;
	rect->top += margins.cyTopHeight;
	rect->bottom -= margins.cyBottomHeight;
	return rect;
}

void GlassFrameIntegrity::MyCTopLevelWindow_GetBorderMargins(uDWM::CTopLevelWindow* This, MARGINS* margins)
{
	CalculateFrameMargins(This, margins);
}

void GlassFrameIntegrity::MyCTopLevelWindow_GetFrameMargins(const uDWM::CTopLevelWindow* This, MARGINS* margins)
{
	// InitializeVisualTreeClone freezes these margins; retain that snapshot for later consumers.
	if (This->GetCloneFrameMargins())
	{
		return g_CTopLevelWindow_GetFrameMargins_Org(This, margins);
	}
	CalculateFrameMargins(This, margins);
}

void GlassFrameIntegrity::Update([[maybe_unused]] GlassEngine::UpdateType type)
{
}

void GlassFrameIntegrity::Startup()
{
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CTopLevelWindow_ValidateVisual_Org }
		}
	);

	if (uDWM::g_versionInfo.build < os::build_w11_24h2)
	{
		HookHelper::ApplyInlineHooks(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CTopLevelWindow_GetBorderMargins_Org }
			}
		);
	}
	if (uDWM::g_versionInfo.build >= os::build_w11_21h2)
	{
		HookHelper::ApplyInlineHooks(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CTopLevelWindow_GetBorderRect_Org },
				{ &g_CTopLevelWindow_GetFrameMargins_Org }
			}
		);
	}
}

void GlassFrameIntegrity::Shutdown()
{
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CTopLevelWindow_ValidateVisual_Org }
		}
	);

	if (uDWM::g_versionInfo.build < os::build_w11_24h2)
	{
		HookHelper::ApplyInlineHooks(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CTopLevelWindow_GetBorderMargins_Org }
			}
		);
	}
	if (uDWM::g_versionInfo.build >= os::build_w11_21h2)
	{
		HookHelper::ApplyInlineHooks(
			std::initializer_list<HookHelper::DetourInfo>
			{
				{ &g_CTopLevelWindow_GetBorderRect_Org },
				{ &g_CTopLevelWindow_GetFrameMargins_Org }
			}
		);
	}
}

void GlassFrameIntegrity::Cleanup()
{
}
