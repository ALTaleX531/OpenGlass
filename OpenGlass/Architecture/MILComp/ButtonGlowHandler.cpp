#include "pch.h"
#include "ButtonGlowHandler.hpp"
#include "uDwmProjection.hpp"

using namespace OpenGlass;
namespace OpenGlass::ButtonGlowHandler
{
	enum class GlowType
	{
		MinMax,
		Close,
		ToolClose
	};

	std::array<int, 3> g_glowPartIds{ 93, 92, 94 };
	std::array<winrt::com_ptr<uDWM::CBitmapSource>, 3> g_glowBitmaps{};
	std::unordered_map<uDWM::CVisual*, GlowType> g_buttonTypes;
	// Retain images, never their parents. After rundown closes, native parent
	// destruction can bypass our destructor hook before Cleanup detaches images.
	std::unordered_map<uDWM::CVisual*, winrt::com_ptr<uDWM::CImage>> g_buttonGlows;

	HRESULT MyCTopLevelWindow_CreateGlyphsFromAtlas(HTHEME theme);
	HRESULT MyCTopLevelWindow_UpdateButtonVisuals(uDWM::CTopLevelWindow* This, const uDWM::CTopLevelWindow::WindowFrame* frame);
	void MyCButton_DrawStateW(uDWM::CButton* This, uDWM::CButtonVisual* visual, UINT state);
	HRESULT MyCButton_InitializeVisualTreeClone(uDWM::CButton* This, uDWM::CButton* cloned, UINT options);
	HRESULT MyCImage_InitializeVisualTreeClone(uDWM::CImage* This, uDWM::CImage* cloned, UINT options);
	void MyCContainerVisual_Destructor(uDWM::CContainerVisual* This);

	Projection::Detour<uDWM::Symbol_CTopLevelWindow_CreateGlyphsFromAtlas, &MyCTopLevelWindow_CreateGlyphsFromAtlas> g_CTopLevelWindow_CreateGlyphsFromAtlas_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_UpdateButtonVisuals, &MyCTopLevelWindow_UpdateButtonVisuals> g_CTopLevelWindow_UpdateButtonVisuals_Org{};
	Projection::Detour<uDWM::Symbol_CButton_DrawStateW, &MyCButton_DrawStateW> g_CButton_DrawStateW_Org{};
	Projection::Detour<uDWM::Symbol_CButton_InitializeVisualTreeClone, &MyCButton_InitializeVisualTreeClone> g_CButton_InitializeVisualTreeClone_Org{};
	Projection::Detour<uDWM::Symbol_CImage_InitializeVisualTreeClone, &MyCImage_InitializeVisualTreeClone> g_CImage_InitializeVisualTreeClone_Org{};
	Projection::Detour<uDWM::Symbol_CContainerVisual_Destructor, &MyCContainerVisual_Destructor> g_CContainerVisual_Destructor_Org{};

	HRESULT UpdateGlow(uDWM::CButton* button, uDWM::CButtonVisual* visual, UINT state);
}

HRESULT ButtonGlowHandler::MyCTopLevelWindow_CreateGlyphsFromAtlas(HTHEME theme)
{
	for (size_t i = 0; i < g_glowBitmaps.size(); ++i)
	{
		auto& bitmap = g_glowBitmaps[i];
		bitmap = nullptr;
		MARGINS margins{};
		if (SUCCEEDED(uDWM::CTopLevelWindow::CreateBitmapFromAtlas(theme, g_glowPartIds[i], &margins, bitmap.put())))
		{
			// CImage expands its rectangle by these destination outsets. Native
			// atlas creation already supplies the independent source slice margins.
			bitmap->GetOutsetMargins() = margins;
		}
	}
	return g_CTopLevelWindow_CreateGlyphsFromAtlas_Org(theme);
}

HRESULT ButtonGlowHandler::MyCTopLevelWindow_UpdateButtonVisuals(uDWM::CTopLevelWindow* This, const uDWM::CTopLevelWindow::WindowFrame* frame)
{
	const auto hr = g_CTopLevelWindow_UpdateButtonVisuals_Org(This, frame);
	if (SUCCEEDED(hr))
	{
		for (int i = 0; i != 4; ++i)
		{
			if (const auto button = This->GetButton(i))
			{
				g_buttonTypes[button] = i != 3 ? GlowType::MinMax :
					(This->HasThinRenderedBorder() ? GlowType::ToolClose : GlowType::Close);
			}
		}
	}
	return hr;
}

HRESULT ButtonGlowHandler::UpdateGlow(uDWM::CButton* button, uDWM::CButtonVisual* visual, UINT state) try
{
	const auto type = g_buttonTypes.find(button);
	const auto bitmap = state >= 1 && state <= 2 && type != g_buttonTypes.end() ?
		g_glowBitmaps[static_cast<size_t>(type->second)].get() : nullptr;
	const auto existing = g_buttonGlows.find(visual);
	if (!bitmap)
	{
		if (existing != g_buttonGlows.end())
		{
			auto image = std::move(existing->second);
			g_buttonGlows.erase(existing);
			RETURN_IF_FAILED(visual->GetVisualCollection()->Remove(image.get()));
		}
		return S_OK;
	}

	if (existing == g_buttonGlows.end())
	{
		winrt::com_ptr<uDWM::CImage> image;
		RETURN_IF_FAILED(uDWM::CImage::Create(image.put()));
		image->SetBitmapSource(bitmap);
		image->SetInsetFromParent({});
		RETURN_IF_FAILED(visual->GetVisualCollection()->InsertRelative(image.get(), nullptr, false, true));
		g_buttonGlows.emplace(visual, std::move(image));
	}
	else
	{
		existing->second->SetBitmapSource(bitmap);
	}
	// The glow extends beyond the button, just like Legacy's atlas image.
	// Keep it above neighboring buttons while retaining native hit testing.
	RETURN_IF_FAILED(button->MoveToFront(false));
	return S_OK;
}
CATCH_RETURN()

void ButtonGlowHandler::MyCButton_DrawStateW(uDWM::CButton* This, uDWM::CButtonVisual* visual, UINT state)
{
	g_CButton_DrawStateW_Org(This, visual, state);
	LOG_IF_FAILED(UpdateGlow(This, visual, state));
}

HRESULT ButtonGlowHandler::MyCButton_InitializeVisualTreeClone(uDWM::CButton* This, uDWM::CButton* cloned, UINT options)
{
	const auto hr = g_CButton_InitializeVisualTreeClone_Org(This, cloned, options);
	if (SUCCEEDED(hr))
	{
		if (const auto type = g_buttonTypes.find(This); type != g_buttonTypes.end())
		{
			g_buttonTypes[cloned] = type->second;
		}
	}
	return hr;
}

HRESULT ButtonGlowHandler::MyCImage_InitializeVisualTreeClone(uDWM::CImage* This, uDWM::CImage* cloned, UINT options)
{
	// One synchronous native clone call: nested image hooks return the matching
	// glow here before its owner is added to the retained-image map.
	struct GlowCloneContext
	{
		uDWM::CImage* source{};
		winrt::com_ptr<uDWM::CImage> image;
	};
	// Native cloning re-enters this hook synchronously for each child. Share
	// the active context across those calls and restore it when unwinding.
	static GlowCloneContext* g_glowCloneContext{};
	GlowCloneContext cloneContext;
	if (const auto glow = g_buttonGlows.find(This); glow != g_buttonGlows.end())
	{
		cloneContext.source = glow->second.get();
	}
	const auto parentContext = g_glowCloneContext;
	if (cloneContext.source)
	{
		g_glowCloneContext = &cloneContext;
	}
	const auto restore = wil::scope_exit([parentContext]
	{
		g_glowCloneContext = parentContext;
	});

	const auto hr = g_CImage_InitializeVisualTreeClone_Org(This, cloned, options);
	if (SUCCEEDED(hr))
	{
		if (parentContext && parentContext->source == This)
		{
			parentContext->image.copy_from(cloned);
		}
		if (cloneContext.image)
		{
			// Native cloning attached this exact child. Retain it only when the
			// owner succeeded; failed owners leave no cleanup pointer behind.
			g_buttonGlows.emplace(cloned, std::move(cloneContext.image));
		}
	}
	return hr;
}

void ButtonGlowHandler::MyCContainerVisual_Destructor(uDWM::CContainerVisual* This)
{
	g_buttonTypes.erase(This);
	g_buttonGlows.erase(This);
	g_CContainerVisual_Destructor_Org(This);
}

void ButtonGlowHandler::Update([[maybe_unused]] GlassEngine::UpdateType type)
{
	g_glowPartIds = {
		static_cast<int>(GlassEngine::GetDwordFromRegistry(L"MINMAXBUTTONGLOWid", 93)),
		static_cast<int>(GlassEngine::GetDwordFromRegistry(L"CLOSEBUTTONGLOWid", 92)),
		static_cast<int>(GlassEngine::GetDwordFromRegistry(L"TOOLCLOSEBUTTONGLOWid", 94))
	};
}

void ButtonGlowHandler::Startup()
{
	HookHelper::ApplyInlineHooks(std::initializer_list<HookHelper::DetourInfo>
	{
		{ &g_CTopLevelWindow_CreateGlyphsFromAtlas_Org },
		{ &g_CTopLevelWindow_UpdateButtonVisuals_Org },
		{ &g_CButton_DrawStateW_Org },
		{ &g_CButton_InitializeVisualTreeClone_Org },
		{ &g_CImage_InitializeVisualTreeClone_Org },
		{ &g_CContainerVisual_Destructor_Org }
	});
}

void ButtonGlowHandler::Shutdown()
{
	HookHelper::ApplyInlineHooks(std::initializer_list<HookHelper::DetourInfo>
	{
		{ &g_CTopLevelWindow_CreateGlyphsFromAtlas_Org },
		{ &g_CTopLevelWindow_UpdateButtonVisuals_Org },
		{ &g_CButton_DrawStateW_Org },
		{ &g_CButton_InitializeVisualTreeClone_Org },
		{ &g_CImage_InitializeVisualTreeClone_Org },
		{ &g_CContainerVisual_Destructor_Org }
	});
}

void ButtonGlowHandler::Cleanup()
{
	for (const auto& [owner, image] : g_buttonGlows)
	{
		if (const auto parent = image->GetTransformParent())
		{
			FAIL_FAST_IF_FAILED_MSG(parent->GetVisualCollection()->Remove(image.get()), "Unable to remove a button glow during shutdown");
		}
	}
	g_buttonGlows.clear();
	g_buttonTypes.clear();
	g_glowBitmaps = {};
}
