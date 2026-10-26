#include "pch.h"
#include "Shared.hpp"
#include "GlassFrameEnhancer.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassFrameEnhancer
{
	bool MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004(uDWM::CTopLevelAtlasedRectsVisual* This, const uDWM::CAtlasedImage* atlasedImage, bool unknown1, bool unknown2, bool unknown3);
	bool MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004(uDWM::CTopLevelAtlasedRectsVisual* This, const uDWM::CAtlasedImage* atlasedImage, UINT cloneOptions);
	HRESULT MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win10(uDWM::CTopLevelWindow* This, bool windowFramesOnly, bool unused1, bool unused2, uDWM::CTopLevelWindow** clonedWindow);
	HRESULT MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win11(uDWM::CTopLevelWindow* This, bool windowFramesOnly, uDWM::CTopLevelWindow** clonedWindow);
	HRESULT MyCButton_CloneVisualTree_Pre19041(uDWM::CButton* This, uDWM::CButton** clonedVisual, bool option1, bool option2, bool option3);
	HRESULT MyCButton_CloneVisualTree(uDWM::CButton* This, uDWM::CButton** clonedVisual, UINT cloneOption);
	HRESULT CloneButton(uDWM::CButton* source, uDWM::CButton** clonedVisual, UINT cloneOption);
	
	Projection::Detour<uDWM::Symbol_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Pre_19041, &MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004> g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_19041, &MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004> g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_CloneVisualTreeForLivePreview_Win10, &MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win10> g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win10_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_CloneVisualTreeForLivePreview_Win11, &MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win11> g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win11_Org{};
	decltype(&MyCButton_CloneVisualTree) g_CButton_CloneVisualTree_Org{ nullptr };
	decltype(&MyCButton_CloneVisualTree)* g_CButton_CloneVisualTree_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCButton_CloneVisualTree> g_CButton_CloneVisualTree_Hook;
	decltype(&MyCButton_CloneVisualTree_Pre19041) g_CButton_CloneVisualTree_Pre19041_Org{};
	decltype(&MyCButton_CloneVisualTree_Pre19041)* g_CButton_CloneVisualTree_Pre19041_Org_Address{};
	HookHelper::PointerHook<&MyCButton_CloneVisualTree_Pre19041> g_CButton_CloneVisualTree_Pre19041_Hook;
	
	bool g_windowFramesOnly{ false };
	bool g_livePreviewClone{};
}

bool GlassFrameEnhancer::IsGlassOnlyLivePreviewClone()
{
	return g_livePreviewClone && g_windowFramesOnly;
}

bool GlassFrameEnhancer::MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004(uDWM::CTopLevelAtlasedRectsVisual* This, const uDWM::CAtlasedImage* atlasedImage, bool unknown1, bool unknown2, bool unknown3)
{
	const auto partId = atlasedImage->GetPartId();

	bool isButtonPart = (partId == 22);
	bool isSqueegeePart = (partId - 9) <= 8;
	bool isWindowPart = (partId) <= 7;

	// Button clones rebuild their own glyph-bearing atlas images.
	if (isButtonPart)
	{
		return false;
	}

	// Nested CVisual clones reset the three booleans to false/false/true.
	// Keep the outer LivePreview scope distinct from ordinary snapshots.
	if (g_livePreviewClone)
	{
		if (isSqueegeePart)
		{
			return g_windowFramesOnly;
		}
		if (isWindowPart)
		{
			return !g_windowFramesOnly;
		}
	}

	return g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004_Org(This, atlasedImage, unknown1, unknown2, unknown3);
}
bool GlassFrameEnhancer::MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004(uDWM::CTopLevelAtlasedRectsVisual* This, const uDWM::CAtlasedImage* atlasedImage, UINT cloneOptions)
{
	const auto partId = atlasedImage->GetPartId();

	bool isButtonPart = (partId == 22);
	bool isSqueegeePart = (partId - 9) <= 8;
	bool isWindowPart = (partId) <= 7;

	// hide button image
	if (isButtonPart)
	{
		return false;
	}

	// (cloneOptions == 4) => clone by window snapshot
	// should fix square corners close/minimize animation while using theme with rounded borders
	if (
		(isSqueegeePart && (cloneOptions == 4)) ||
		(isSqueegeePart && (cloneOptions != 4) && !g_windowFramesOnly)
	)
	{
		return false;
	}

	// show window frames in focused live preview window
	if (isWindowPart && (cloneOptions != 4) && !g_windowFramesOnly)
	{
		return true;
	}

	return g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004_Org(This, atlasedImage, cloneOptions);
}

HRESULT GlassFrameEnhancer::MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win10(uDWM::CTopLevelWindow* This,
																				  bool windowFramesOnly, bool unused1,
																				  bool unused2,
																				  uDWM::CTopLevelWindow** clonedWindow)
{
	const auto previousClone = std::exchange(g_livePreviewClone, true);
	const auto restoreClone = wil::scope_exit([previousClone] { g_livePreviewClone = previousClone; });
	const auto previousFramesOnly = g_windowFramesOnly;
	g_windowFramesOnly = windowFramesOnly;
	const auto hr = g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win10_Org(This, windowFramesOnly, unused1, unused2,
																			  clonedWindow);
	g_windowFramesOnly = uDWM::g_versionInfo.build < os::build_w10_2004 ? previousFramesOnly : !windowFramesOnly;

	for (int i = 0; i < 4; i++)
	{
		if (auto button = This->GetButton(i))
		{
			// HACK: we need to allow the cloning of buttons after this function for close/minimize
			if (uDWM::g_versionInfo.build < os::build_w11_21h2)
			{
				button->SetExcludeSubtree(false);
			}
		}
	}

	return hr;
}

HRESULT GlassFrameEnhancer::MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win11(uDWM::CTopLevelWindow* This,
																				  bool windowFramesOnly,
																				  uDWM::CTopLevelWindow** clonedWindow)
{
	g_windowFramesOnly = windowFramesOnly;
	const auto hr = g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win11_Org(This, windowFramesOnly, clonedWindow);
	g_windowFramesOnly = !windowFramesOnly;

	return hr;
}

// Retain CButton glyph/glow state instead of the native base atlas clone.
HRESULT GlassFrameEnhancer::MyCButton_CloneVisualTree(uDWM::CButton* This, uDWM::CButton** clonedVisual, UINT cloneOption)
{
	return CloneButton(This, clonedVisual, cloneOption);
}

HRESULT GlassFrameEnhancer::MyCButton_CloneVisualTree_Pre19041(uDWM::CButton* This, uDWM::CButton** clonedVisual,
	[[maybe_unused]] bool option1, [[maybe_unused]] bool option2, [[maybe_unused]] bool option3)
{
	// CButton inherits the unfiltered atlas clone. Its base CVisual clone
	// always receives false/false/true, independently of these atlas options.
	return CloneButton(This, clonedVisual, 0);
}

HRESULT GlassFrameEnhancer::CloneButton(uDWM::CButton* This, uDWM::CButton** clonedVisual, UINT cloneOption)
{
	*clonedVisual = nullptr;
	auto cleanup = wil::scope_exit([clonedVisual]
	{
		if (*clonedVisual)
		{
			(*clonedVisual)->Release();
			*clonedVisual = nullptr;
		}
	});

	// CButton::CancelCrossfade
	if (This->GetTimeline())
	{
		*This->GetButtonState() |= 0x40u;
		This->SetDirtyFlags(0x10000);
		RETURN_IF_FAILED(This->RenderRecursive());
	}

	if (uDWM::g_versionInfo.build < os::build_w10_1903)
	{
		// 17763 inlines this factory in EnsureNonClientAreaButton: 416 bytes
		// from the process heap, then the native constructor and initializer.
		const auto memory = HeapAlloc(GetProcessHeap(), 0, 416);
		RETURN_HR_IF(E_OUTOFMEMORY, !memory);
		*clonedVisual = uDWM::CButton::Construct_Pre18362(static_cast<uDWM::CButton*>(memory));
		RETURN_IF_FAILED((*clonedVisual)->Initialize_Pre18362(This->GetVisualProxy()->GetProxy()->GetChannel()));
	}
	else
	{
		RETURN_IF_FAILED(uDWM::CButton::Create(clonedVisual));
	}
	if (uDWM::g_versionInfo.build < os::build_w10_2004)
	{
		RETURN_IF_FAILED(This->CloneVisualTree_Pre19041(reinterpret_cast<uDWM::CVisual**>(clonedVisual), false, false, true));
		if (const auto margins = This->GetClipMargins())
		{
			RETURN_IF_FAILED((*clonedVisual)->SetClipMargins(margins));
		}
		RETURN_IF_FAILED(
			(*clonedVisual)->SetVisualStates_Win10(
				This->GetButtonBitmapArray(), This->GetGlyphBitmapArray(),
				This->GetGlowBitmap(), This->GetGlyphOpacity()
			)
		);
	}
	else
	{
		RETURN_IF_FAILED(This->InitializeVisualTreeClone(*clonedVisual, cloneOption));
		RETURN_IF_FAILED(
			(*clonedVisual)->SetVisualStates(
				This->GetButtonBitmapArray(), This->GetGlyphBitmapArray(), This->GetGlyphOpacity()
			)
		);
	}

	*(*clonedVisual)->GetButtonState() = *This->GetButtonState();
	cleanup.release();

	return S_OK;
}

void GlassFrameEnhancer::Update([[maybe_unused]] GlassEngine::UpdateType type)
{
}

void GlassFrameEnhancer::Startup()
{
	const Projection::Version version{uDWM::g_versionInfo.build, uDWM::g_versionInfo.revision};
	const auto before19041 = uDWM::g_versionInfo.build < os::build_w10_2004;
	if (before19041 || Projection::IsVersionInRange(version, uDWM::Symbol_CAtlasedRectsVisual_CloneVisualTree.range()))
	{
		const auto CAtlasedRectsVisual_CloneVisualTree_Org =
			before19041 ? Util::force_cast_from(uDWM::Symbol_CAtlasedRectsVisual_CloneVisualTree_Pre19041.get()) :
			Util::force_cast_from(uDWM::Symbol_CAtlasedRectsVisual_CloneVisualTree.get());
		for (auto& vf : std::span{uDWM::CButton::vftable, 20})
		{
			if (vf == CAtlasedRectsVisual_CloneVisualTree_Org)
			{
				if (before19041)
				{
					g_CButton_CloneVisualTree_Pre19041_Org_Address = reinterpret_cast<decltype(g_CButton_CloneVisualTree_Pre19041_Org_Address)>(&vf);
					g_CButton_CloneVisualTree_Pre19041_Hook.Prepare(g_CButton_CloneVisualTree_Pre19041_Org_Address, &g_CButton_CloneVisualTree_Pre19041_Org);
					HookHelper::GetCurrentHookTransaction().Apply(g_CButton_CloneVisualTree_Pre19041_Hook);
					continue;
				}
				g_CButton_CloneVisualTree_Org_Address =
					reinterpret_cast<decltype(g_CButton_CloneVisualTree_Org_Address)>(&vf);
				g_CButton_CloneVisualTree_Hook.Prepare(g_CButton_CloneVisualTree_Org_Address, &g_CButton_CloneVisualTree_Org);
				HookHelper::GetCurrentHookTransaction().Apply(g_CButton_CloneVisualTree_Hook);
			}
		}
	}

	const auto build_before_or_at_w11_21h2 = uDWM::g_versionInfo.build <= os::build_w11_21h2;
	const auto build_before_w10_2004 = uDWM::g_versionInfo.build < os::build_w10_2004;
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>{
			{&g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004_Org, build_before_w10_2004},
			{&g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004_Org, !build_before_w10_2004},

			{&g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win10_Org, build_before_or_at_w11_21h2},
			{&g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win11_Org, !build_before_or_at_w11_21h2}});
}

void GlassFrameEnhancer::Shutdown()
{
	const auto build_before_or_at_w11_21h2 = uDWM::g_versionInfo.build <= os::build_w11_21h2;
	const auto build_before_w10_2004 = uDWM::g_versionInfo.build < os::build_w10_2004;
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>{
			{&g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004_Org, build_before_w10_2004},
			{&g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004_Org, !build_before_w10_2004},

			{&g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win10_Org, build_before_or_at_w11_21h2},
			{&g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win11_Org, !build_before_or_at_w11_21h2}});

	if (g_CButton_CloneVisualTree_Org)
	{
		HookHelper::GetCurrentHookTransaction().Apply(g_CButton_CloneVisualTree_Hook);
	}
	if (g_CButton_CloneVisualTree_Pre19041_Org)
	{
		HookHelper::GetCurrentHookTransaction().Apply(g_CButton_CloneVisualTree_Pre19041_Hook);
	}
}

void GlassFrameEnhancer::Cleanup()
{
}
