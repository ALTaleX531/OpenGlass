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
	// TO-DO: implement pre-w10-2004 specialization
	HRESULT MyCButton_CloneVisualTree(uDWM::CButton* This, uDWM::CButton** clonedVisual, UINT cloneOption);
	
	Projection::Detour<uDWM::Symbol_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Pre_19041, decltype(&MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004)> g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_19041, decltype(&MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004)> g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_CloneVisualTreeForLivePreview_Win10, decltype(&MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win10)> g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win10_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_CloneVisualTreeForLivePreview_Win11, decltype(&MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win11)> g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win11_Org{};
	decltype(&MyCButton_CloneVisualTree) g_CButton_CloneVisualTree_Org{ nullptr };
	decltype(&MyCButton_CloneVisualTree)* g_CButton_CloneVisualTree_Org_Address{ nullptr };
	HookHelper::PointerHook<&MyCButton_CloneVisualTree> g_CButton_CloneVisualTree_Hook;
	
	bool g_windowFramesOnly{ false };
}

bool GlassFrameEnhancer::MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004(uDWM::CTopLevelAtlasedRectsVisual* This, const uDWM::CAtlasedImage* atlasedImage, bool unknown1, bool unknown2, bool unknown3)
{
	const auto partId = atlasedImage->GetPartId();

	bool isButtonPart = (partId == 22);
	bool isSqueegeePart = (partId - 9) <= 8;
	bool isWindowPart = (partId) <= 7;

	// hide window frames in focused live preview window
	if (isWindowPart && g_windowFramesOnly)
	{
		return false;
	}
	if (isSqueegeePart && g_windowFramesOnly)
	{
		return true;
	}

	// hide button image
	if (isButtonPart && g_windowFramesOnly)
	{
		return false;
	}

	// TO-DO: implement pre-w10-2004 specialization to hide window frames in live preview
	// ...

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
	g_windowFramesOnly = windowFramesOnly;
	const auto hr = g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win10_Org(This, windowFramesOnly, unused1, unused2,
																			  clonedWindow);
	g_windowFramesOnly = !windowFramesOnly;

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

// Reimplement button visual clone for Windows 10.
// altalex: probably not the best way, win10 rtm has button visual clone without override virtual function
HRESULT GlassFrameEnhancer::MyCButton_CloneVisualTree(uDWM::CButton* This, uDWM::CButton** clonedVisual, UINT cloneOption)
{
	auto cleanup = wil::scope_exit([clonedVisual]
		{
			if (clonedVisual)
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

	RETURN_IF_FAILED(uDWM::CButton::Create(clonedVisual));
	RETURN_IF_FAILED(This->InitializeVisualTreeClone(*clonedVisual, cloneOption));
	RETURN_IF_FAILED(
		(*clonedVisual)->SetVisualStates(
			This->GetGlyphBitmapArray(),
			This->GetButtonBitmapArray(),
			This->GetGlyphOpacity()
		)
	);

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
	if (Projection::IsVersionInRange(version, uDWM::Symbol_CAtlasedRectsVisual_CloneVisualTree.range()))
	{
		const auto CAtlasedRectsVisual_CloneVisualTree_Org =
			uDWM::Symbol_CAtlasedRectsVisual_CloneVisualTree.get();
		for (auto& vf : std::span{uDWM::CButton::vftable, 20})
		{
			if (vf == CAtlasedRectsVisual_CloneVisualTree_Org)
			{
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
			{&g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004_Org,
			 &MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004, build_before_w10_2004},
			{&g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004_Org,
			 &MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004, !build_before_w10_2004},

			{&g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win10_Org,
			 &MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win10, build_before_or_at_w11_21h2},
			{&g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win11_Org,
			 &MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win11, !build_before_or_at_w11_21h2}},
		true);
}

void GlassFrameEnhancer::Shutdown()
{
	const auto build_before_or_at_w11_21h2 = uDWM::g_versionInfo.build <= os::build_w11_21h2;
	const auto build_before_w10_2004 = uDWM::g_versionInfo.build < os::build_w10_2004;
	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>{
			{&g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004_Org,
			 &MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_Before_W10_2004, build_before_w10_2004},
			{&g_CTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004_Org,
			 &MyCTopLevelAtlasedRectsVisual_ShouldCloneAtlasImage_At_Least_W10_2004, !build_before_w10_2004},

			{&g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win10_Org,
			 &MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win10, build_before_or_at_w11_21h2},
			{&g_CTopLevelWindow_CloneVisualTreeForLivePreview_Win11_Org,
			 &MyCTopLevelWindow_CloneVisualTreeForLivePreview_Win11, !build_before_or_at_w11_21h2}},
		false);

	if (g_CButton_CloneVisualTree_Org)
	{
		HookHelper::GetCurrentHookTransaction().Apply(g_CButton_CloneVisualTree_Hook);
	}
}

void GlassFrameEnhancer::Cleanup()
{
}
