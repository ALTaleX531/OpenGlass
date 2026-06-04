#include "pch.h"
#include "Shared.hpp"
#include "GlassFrameHandler.hpp"
#include "GlassKernel.hpp"
#include "uDWMProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "ReflectionVisual.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassFrameHandler
{
	HRESULT MyCGlassColorizationParameters_AdjustWindowColorization(
		uDWM::CGlassColorizationParameters* This,
		[[maybe_unused]] uDWM::GpCC* colorUnused,
		[[maybe_unused]] float opacity,
		BYTE flag
	);

	HRESULT MyCChannel_CombinedGeometryUpdate(
		dwmcore::CChannel* This,
		UINT handleId,
		D2D1_COMBINE_MODE mode,
		UINT geometry1HandleId,
		UINT geometry2HandleId
	);
	void MyCLegacyNonClientBackground_ClearAll(uDWM::CLegacyNonClientBackground* This);
	bool MyCLegacyNonClientBackground_HasSomethingToRender(uDWM::CLegacyNonClientBackground* This);
	HRESULT MyCLegacyNonClientBackground_SetCaptionRect(uDWM::CLegacyNonClientBackground* This, LPCRECT rc);
	HRESULT MyCLegacyNonClientBackground_SetBorderRects(uDWM::CLegacyNonClientBackground* This, LPCRECT rc1, LPCRECT rc2);
	HRESULT MyCLegacyNonClientBackground_SetCaptionColor(uDWM::CLegacyNonClientBackground* This, const D2D1_COLOR_F& color);
	void MyCLegacyNonClientBackground_Destructor(uDWM::CLegacyNonClientBackground* This);

	bool MyCRectangleVisual_SetRect(uDWM::CRectangleVisual* This, const D2D1_RECT_F& rc);
	HRESULT MyCTopLevelWindow_UpdateClientBlur(uDWM::CTopLevelWindow* This);
	HRESULT MyCTopLevelWindow_UpdateNCAreaBackground(uDWM::CTopLevelWindow* This);
	bool MyCTopLevelWindow_EdgeBorderMustBeOpaque(uDWM::CTopLevelWindow* This); 

	HRESULT MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This);
	void MyCTopLevelWindow_Destructor(uDWM::CTopLevelWindow* This);

	decltype(&MyCGlassColorizationParameters_AdjustWindowColorization) g_CGlassColorizationParameters_AdjustWindowColorization_Org{ nullptr };

	decltype(&MyCChannel_CombinedGeometryUpdate) g_CChannel_CombinedGeometryUpdate_Org{ nullptr };
	decltype(&MyCLegacyNonClientBackground_ClearAll) g_CLegacyNonClientBackground_ClearAll_Org{ nullptr };
	decltype(&MyCLegacyNonClientBackground_HasSomethingToRender) g_CLegacyNonClientBackground_HasSomethingToRender_Org{ nullptr };
	decltype(&MyCLegacyNonClientBackground_SetCaptionRect) g_CLegacyNonClientBackground_SetCaptionRect_Org{ nullptr };
	decltype(&MyCLegacyNonClientBackground_SetBorderRects) g_CLegacyNonClientBackground_SetBorderRects_Org{ nullptr };
	decltype(&MyCLegacyNonClientBackground_SetCaptionColor) g_CLegacyNonClientBackground_SetCaptionColor_Org{ nullptr };
	decltype(&MyCLegacyNonClientBackground_Destructor) g_CLegacyNonClientBackground_Destructor_Org{ nullptr };

	decltype(&MyCRectangleVisual_SetRect) g_CRectangleVisual_SetRect_Org{ nullptr };
	decltype(&MyCTopLevelWindow_UpdateClientBlur) g_CTopLevelWindow_UpdateClientBlur_Org{ nullptr };
	decltype(&MyCTopLevelWindow_UpdateNCAreaBackground) g_CTopLevelWindow_UpdateNCAreaBackground_Org{ nullptr };
	decltype(&MyCTopLevelWindow_EdgeBorderMustBeOpaque) g_CTopLevelWindow_EdgeBorderMustBeOpaque_Org{ nullptr };

	decltype(&MyCTopLevelWindow_ValidateVisual) g_CTopLevelWindow_ValidateVisual_Org{ nullptr };
	decltype(&MyCTopLevelWindow_Destructor) g_CTopLevelWindow_Destructor_Org{ nullptr };

	std::unordered_map<uDWM::CLegacyNonClientBackground*, winrt::com_ptr<uDWM::CSolidRectangleVisual>> g_effectVisualMap{};
	winrt::com_ptr<uDWM::CSolidRectangleVisual> GetOrCreateEffectVisual(uDWM::CLegacyNonClientBackground* background, bool createIfNecessary)
	{
		if (createIfNecessary)
		{
			auto& effectVisual = g_effectVisualMap[background];
			if (!effectVisual)
			{
				THROW_IF_FAILED(
					uDWM::CSolidRectangleVisual::Create(
						effectVisual.put()
					)
				);
				THROW_IF_FAILED(
					background->GetVisualCollection()->InsertRelative(
						effectVisual.get(),
						nullptr,
						false,
						true
					)
				);
			}

			return effectVisual;
		}

		const auto it = g_effectVisualMap.find(background);
		return it == g_effectVisualMap.end() ? nullptr : it->second;
	}
	void RemoveEffectVisual(uDWM::CLegacyNonClientBackground* background)
	{
		if (const auto it = g_effectVisualMap.find(background); it != g_effectVisualMap.end())
		{
			const auto& effectVisual = it->second;
			const auto parent = effectVisual->GetTransformParent();
			if (parent)
			{
				THROW_IF_FAILED(parent->GetVisualCollection()->Remove(effectVisual.get()));
			}
			g_effectVisualMap.erase(it);
		}
	}
	void RemoveAllEffectVisuals()
	{
		for (const auto& [background, effectVisual] : g_effectVisualMap)
		{
			const auto parent = effectVisual->GetTransformParent();
			if (parent)
			{
				THROW_IF_FAILED(parent->GetVisualCollection()->Remove(effectVisual.get()));
			}
		}
		g_effectVisualMap.clear();
	}

	std::unordered_map<uDWM::CTopLevelWindow*, std::array<winrt::com_ptr<CReflectionVisual>, 2>> g_reflectionVisualMap{};
	winrt::com_ptr<CReflectionVisual> GetOrCreateReflectionVisual(uDWM::CTopLevelWindow* window, int index, uDWM::CRectangleVisual* effectVisual, bool createIfNecessary)
	{
		if (createIfNecessary)
		{
			auto& reflectionVisual = g_reflectionVisualMap[window][index];
			if (!reflectionVisual)
			{
				THROW_IF_FAILED(
					CReflectionVisual::Create(
						reflectionVisual.put()
					)
				);
				THROW_IF_FAILED(
					effectVisual->GetVisualCollection()->InsertRelative(
						reflectionVisual.get(),
						nullptr,
						false,
						true
					)
				);
			}

			return reflectionVisual;
		}

		const auto it = g_reflectionVisualMap.find(window);
		return it == g_reflectionVisualMap.end() ? nullptr : it->second[index];
	}
	void RemoveReflectionVisual(uDWM::CTopLevelWindow* window, int index)
	{
		if (const auto it = g_reflectionVisualMap.find(window); it != g_reflectionVisualMap.end())
		{
			if (index >= 0)
			{
				auto& reflectionVisual = it->second[index];
				if (reflectionVisual)
				{
					const auto parent = reflectionVisual->GetTransformParent();
					if (parent)
					{
						THROW_IF_FAILED(parent->GetVisualCollection()->Remove(reflectionVisual.get()));
					}
					reflectionVisual = nullptr;
				}
			}
			else
			{
				for (auto& reflectionVisual : it->second)
				{
					if (reflectionVisual)
					{
						const auto parent = reflectionVisual->GetTransformParent();
						if (parent)
						{
							THROW_IF_FAILED(parent->GetVisualCollection()->Remove(reflectionVisual.get()));
						}
						reflectionVisual = nullptr;
					}
				}
				g_reflectionVisualMap.erase(it);
			}
		}
	}
	void RemoveAllReflectionVisuals()
	{
		for (const auto& [window, reflectionVisuals] : g_reflectionVisualMap)
		{
			for (const auto& reflectionVisual : reflectionVisuals)
			{
				if (reflectionVisual)
				{
					const auto parent = reflectionVisual->GetTransformParent();
					if (parent)
					{
						THROW_IF_FAILED(parent->GetVisualCollection()->Remove(reflectionVisual.get()));
					}
				}
			}
		}
		g_reflectionVisualMap.clear();
	}

	HRESULT UpdateReflectionViewport(uDWM::CTopLevelWindow* window);

	RECT g_innerBorderRect{};
	RECT g_outerBorderRect{};
}

HRESULT GlassFrameHandler::UpdateReflectionViewport(uDWM::CTopLevelWindow* window)
{
	const auto opacity =
		Shared::g_reflectionPolicy & Shared::ReflectionPolicy::NonClient ?
		GlassKernel::GetAdjustedReflectionIntensity(
			window->TreatAsActiveWindow(),
			window->TreatAsMaximized()
		) :
		0.f;
	const auto windowOffset = window->GetOffset();
	if (windowOffset.x == -32000 && windowOffset.y == -32000)
	{
		return S_OK;
	}

	if (
		const auto legacyVisual = window->GetLegacyVisual();
		legacyVisual
	)
	{
		const auto effectVisual = GetOrCreateEffectVisual(legacyVisual, false);
		if (
			const auto reflectionVisual = GetOrCreateReflectionVisual(window, 0, nullptr, false);
			reflectionVisual
		)
		{
			RETURN_IF_FAILED(reflectionVisual->UpdateSurface(GlassKernel::GetOrCreateReflectionSurface()));

			RETURN_IF_FAILED(
				reflectionVisual->UpdateViewport(
					reflectionVisual->GetLocalToParentVisualOffset(window->GetTransformParent()),
					Shared::g_reflectionParallaxIntensity,
					window->IsRTLMirrored(),
					effectVisual->GetSize().cx,
					window->GetScale()
				)
			);
			
			RETURN_IF_FAILED(
				reflectionVisual->UpdateOpacity(
					opacity
				)
			);
		}
	}
	if (
		const auto clientBlurVisual = window->GetClientBlurVisual();
		clientBlurVisual
	)
	{
		if (
			const auto reflectionVisual = GetOrCreateReflectionVisual(window, 1, nullptr, false);
			reflectionVisual
		)
		{
			RETURN_IF_FAILED(reflectionVisual->UpdateSurface(GlassKernel::GetOrCreateReflectionSurface()));

			RETURN_IF_FAILED(
				reflectionVisual->UpdateViewport(
					reflectionVisual->GetLocalToParentVisualOffset(window->GetTransformParent()),
					Shared::g_reflectionParallaxIntensity,
					window->IsRTLMirrored(),
					clientBlurVisual->GetSize().cx,
					window->GetScale()
				)
			);
			
			RETURN_IF_FAILED(
				reflectionVisual->UpdateOpacity(
					opacity
				)
			);
		}
	}

	return S_OK;
}

HRESULT GlassFrameHandler::MyCGlassColorizationParameters_AdjustWindowColorization(
	uDWM::CGlassColorizationParameters* This,
	[[maybe_unused]] uDWM::GpCC* colorUnused,
	[[maybe_unused]] float opacity,
	BYTE flag
)
{
	const auto active = (flag & 1) != 0;
	const auto maximized = GlassKernel::g_window ? GlassKernel::g_window->TreatAsMaximized() : false;

	This->color = Color::ToArgb(
		Color::scRGBTosRGB(
			GlassKernel::RealizeWindowColorization(
				GlassKernel::GetBaseColor(Shared::IsTransparencyDisabled(), maximized),
				GlassKernel::GetSourceColor(active),
				GlassKernel::GetColorizationOpacity(active, maximized),
				Shared::IsTransparencyDisabled(),
				false
			).GetEffectivescRGBBlendColor(0.f),
			0.f
		)
	);
	This->afterglow = 0;
	This->colorBalance = 100;
	This->afterglowBalance = 0;
	This->blurBalance = 0;
	This->windowColorization = TRUE;
	This->glassAttribute = 0;

	return S_OK;
}


HRESULT GlassFrameHandler::MyCChannel_CombinedGeometryUpdate(
	dwmcore::CChannel* This,
	UINT handleId,
	D2D1_COMBINE_MODE mode,
	UINT geometry1HandleId,
	UINT geometry2HandleId
)
{
	if (GlassKernel::g_window)
	{
		std::bitset<2> windowStatus{};
		windowStatus.set(0, GlassKernel::g_window->TreatAsActiveWindow());
		windowStatus.set(1, GlassKernel::g_window->TreatAsMaximized());

		mode = static_cast<D2D1_COMBINE_MODE>(windowStatus.to_ulong());
	}

	return g_CChannel_CombinedGeometryUpdate_Org(This, handleId, mode, geometry1HandleId, geometry2HandleId);
}
void GlassFrameHandler::MyCLegacyNonClientBackground_ClearAll(uDWM::CLegacyNonClientBackground* This)
{
	RemoveReflectionVisual(uDWM::TryGetWindowFromVisual(This), 0);
	RemoveEffectVisual(This);
	return g_CLegacyNonClientBackground_ClearAll_Org(This);
}
bool GlassFrameHandler::MyCLegacyNonClientBackground_HasSomethingToRender(uDWM::CLegacyNonClientBackground* This)
{
	return g_effectVisualMap.find(This) != g_effectVisualMap.end();
}
HRESULT GlassFrameHandler::MyCLegacyNonClientBackground_SetCaptionRect(uDWM::CLegacyNonClientBackground* This, LPCRECT rc)
{
	const auto window = uDWM::TryGetWindowFromVisual(This);
	if (!IsRectEmpty(&g_outerBorderRect))
	{
		const auto effectVisual = GetOrCreateEffectVisual(This, true);
		const auto reflectionVisual = GetOrCreateReflectionVisual(window, 0, effectVisual.get(), true);

		wil::unique_hrgn windowRgn{ nullptr };
		wil::unique_hrgn clientRgn{ nullptr };

		const auto diameter = Shared::g_roundRectRadius * 2;
		// HACK: there is a fxxking gdi bug introduced in windows 11 25h2 that causes CreateRoundRectRgn to shrink the region by 1 pixel in both dimensions,
		// which is exactly the case for our border region when rounded corners are enabled,
		// causing our effect visual to be clipped by 1 pixel on the right and bottom sides.
		// As a workaround, we can simply add 1 pixel to the width and height when creating the rounded region,
		// which does not cause any visual issue but can avoid the GDI bug. 
		windowRgn.reset(
			diameter ?
			CreateRoundRectRgn(
				g_outerBorderRect.left,
				g_outerBorderRect.top,
				g_outerBorderRect.right + 1,
				g_outerBorderRect.bottom + 1,
				diameter,
				diameter
			) :
			CreateRectRgn(
				g_outerBorderRect.left,
				g_outerBorderRect.top,
				g_outerBorderRect.right,
				g_outerBorderRect.bottom
			)
		);
		RETURN_LAST_ERROR_IF_NULL(windowRgn);

		bool isSheetOfGlass = g_innerBorderRect.left == 0 && g_innerBorderRect.top == 0 && g_innerBorderRect.right == 0 && g_innerBorderRect.bottom == 0;
		clientRgn.reset(
			isSheetOfGlass ?
			CreateRectRgn(0, 0, 0, 0) :
			CreateRectRgn(
				rc->left,
				rc->bottom,
				rc->right,
				g_innerBorderRect.bottom
			)
		);
		RETURN_LAST_ERROR_IF_NULL(clientRgn);

		RETURN_IF_WIN32_BOOL_FALSE(CombineRgn(windowRgn.get(), windowRgn.get(), clientRgn.get(), RGN_DIFF));

		RECT rgnBox{};
		RETURN_LAST_ERROR_IF(GetRgnBox(windowRgn.get(), &rgnBox) == NULLREGION);
		POINT offset{ 0, 0 };
		SIZE size{ rgnBox.right, rgnBox.bottom };

		// NOTE: you MUST use SetRect otherwise the entire visual will be invisible and you will have higher posibility to encounter mysterious compositor bug.
		D2D1_RECT_F rect
		{
			static_cast<float>(0.f),
			static_cast<float>(0.f),
			static_cast<float>(rgnBox.right),
			static_cast<float>(rgnBox.bottom)
		};
		effectVisual->SetRect(rect);
		//effectVisual->SetOffset(&offset);

		winrt::com_ptr<uDWM::CRgnGeometryProxy> regionGeometry{};
		RETURN_IF_FAILED(uDWM::ResourceHelper::CreateGeometryFromHRGN(windowRgn.get(), regionGeometry.put()));

		winrt::com_ptr<uDWM::CCombinedGeometryProxy> combinedGeometry{};
		RETURN_IF_FAILED(uDWM::ResourceHelper::CreateCombinedGeometry(regionGeometry.get(), nullptr, D2D1_COMBINE_MODE_INTERSECT, combinedGeometry.put()));
		RETURN_IF_FAILED(effectVisual->UpdateClip(combinedGeometry.get()));

		reflectionVisual->SetSize(&size);

		RETURN_IF_FAILED(reflectionVisual->UpdateSurface(GlassKernel::GetOrCreateReflectionSurface()));

		RETURN_IF_FAILED(
			reflectionVisual->UpdateViewport(
				reflectionVisual->GetLocalToParentVisualOffset(window->GetTransformParent()),
				Shared::g_reflectionParallaxIntensity,
				window->IsRTLMirrored(),
				size.cx,
				window->GetScale()
			)
		);

		RETURN_IF_FAILED(
			reflectionVisual->UpdateOpacity(
				Shared::g_reflectionPolicy & Shared::ReflectionPolicy::NonClient ?
				GlassKernel::GetAdjustedReflectionIntensity(
					window->TreatAsActiveWindow(),
					window->TreatAsMaximized()
				) :
				0.f
			)
		);
	}
	else
	{
		RemoveReflectionVisual(window, 0);
		RemoveEffectVisual(This);
	}

	RECT emptyRect{};
	return g_CLegacyNonClientBackground_SetCaptionRect_Org(This, &emptyRect);
}
HRESULT GlassFrameHandler::MyCLegacyNonClientBackground_SetBorderRects(uDWM::CLegacyNonClientBackground* This, LPCRECT rc1, LPCRECT rc2)
{
	// patch microsoft's terrible code
	// IsSheetOfGlass == true && IsHighContrastMode == true
	// 
	/*bool isSheetOfGlass = rc1->left == 0 && rc1->top == 0 && rc1->right == 0 && rc1->bottom == 0;
	if (isSheetOfGlass)
	{
		const auto center = (rc2->left + rc2->right) / 2;
		const auto middle = (rc2->top + rc2->bottom) / 2;
		const_cast<LPRECT>(rc1)->left = center;
		const_cast<LPRECT>(rc1)->right = center;
		const_cast<LPRECT>(rc1)->top = middle;
		const_cast<LPRECT>(rc1)->bottom = middle;
	}*/

	g_innerBorderRect = *rc1;
	g_outerBorderRect = *rc2;

	RECT emptyRect{};
	return g_CLegacyNonClientBackground_SetBorderRects_Org(This, &emptyRect, &emptyRect);
}
HRESULT GlassFrameHandler::MyCLegacyNonClientBackground_SetCaptionColor(uDWM::CLegacyNonClientBackground* This, const D2D1_COLOR_F& color)
{
	const auto effectVisual = GetOrCreateEffectVisual(This, false);
	if (effectVisual)
	{
		RETURN_IF_FAILED(effectVisual->UpdateColor(color));
	}

	return g_CLegacyNonClientBackground_SetCaptionColor_Org(This, color);
}
void GlassFrameHandler::MyCLegacyNonClientBackground_Destructor(uDWM::CLegacyNonClientBackground* This)
{
	const auto window = uDWM::TryGetWindowFromVisual(This);
	RemoveReflectionVisual(window, 0);
	RemoveEffectVisual(This);
	return g_CLegacyNonClientBackground_Destructor_Org(This);
}

bool GlassFrameHandler::MyCRectangleVisual_SetRect(uDWM::CRectangleVisual* This, const D2D1_RECT_F& rc)
{
	const auto window = uDWM::TryGetWindowFromVisual(This);
	if (
		!window ||
		window->GetClientBlurVisual() != This
	)
	{
		return g_CRectangleVisual_SetRect_Org(This, rc);
	}

	const auto reflectionVisual = GetOrCreateReflectionVisual(window, 1, This, true);

	SIZE size{ static_cast<LONG>(rc.right), static_cast<LONG>(rc.bottom) };
	reflectionVisual->SetSize(&size);

	RETURN_IF_FAILED(reflectionVisual->UpdateSurface(GlassKernel::GetOrCreateReflectionSurface()));
	RETURN_IF_FAILED(
		reflectionVisual->UpdateViewport(
			reflectionVisual->GetLocalToParentVisualOffset(window->GetTransformParent()),
			Shared::g_reflectionParallaxIntensity,
			window->IsRTLMirrored(),
			size.cx,
			window->GetScale()
		)
	);

	RETURN_IF_FAILED(
		reflectionVisual->UpdateOpacity(
			Shared::g_reflectionPolicy & Shared::ReflectionPolicy::NonClient ?
			GlassKernel::GetAdjustedReflectionIntensity(
				window->TreatAsActiveWindow(),
				window->TreatAsMaximized()
			) :
			0.f
		)
	);

	// HACK: SetRect is not SetViewbox, 
	// microsoft's code mistakenly applies the offset to the rect,
	// which causes the blur to be misaligned when DwmExtendFrameIntoClientArea is used.
	auto adjustedRect = rc;
	adjustedRect.left = adjustedRect.top = 0;
	return g_CRectangleVisual_SetRect_Org(This, adjustedRect);
}
HRESULT GlassFrameHandler::MyCTopLevelWindow_UpdateClientBlur(uDWM::CTopLevelWindow* This)
{
	const auto hr = g_CTopLevelWindow_UpdateClientBlur_Org(This);

	if (const auto clientBlurVisual = This->GetClientBlurVisual(); !clientBlurVisual)
	{
		RemoveReflectionVisual(This, 1);
	}
	return hr;
}

HRESULT GlassFrameHandler::MyCTopLevelWindow_UpdateNCAreaBackground(uDWM::CTopLevelWindow* This)
{
	// HACK: UpdateClientBlur always checks current dirty flags to decide whether to update blur or not,
	// in some cases (e.g. when window is maximized/activated) the window states can be changed without UpdateClientBlur being called,
	// which causes the glass to not update and thus look wrong.
	// Forcing an UpdateClientBlur call here seems to fix the issue.
	This->OnBlurBehindUpdated();
	return g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);
}

bool GlassFrameHandler::MyCTopLevelWindow_EdgeBorderMustBeOpaque(uDWM::CTopLevelWindow* This)
{
	// When the window is inactive and EdgeBorderMustBeOpaque is false,
	// the border rect computed in CTopLevelWindow::UpdateNCAreaGeometry shrinks to zero (border visual removed, only caption remains).
	// This shrinks the visual's SetRect to caption size, which causes the occlusion pass to pass because
	// COcclusionContext::PreSubgraph contains a size threshold optimization at 0x180069838:
	//   comiss xmm10, 74752.0   ; visual area vs threshold
	//   jb    skip_check         ; area < 74752 -> bypass the stricter occlusion checks
	// When SetRect is set to the full window rect, the visual area exceeds 74752 px^2,
	// triggering additional validation that disables occlusion for the subtree.

	if (Shared::g_dontDeflateInactiveFrameGeometry)
	{
		return true;
	}

	return g_CTopLevelWindow_EdgeBorderMustBeOpaque_Org(This);
}

HRESULT GlassFrameHandler::MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This)
{
	auto data = This->GetData();
	if (!data)
	{
		return g_CTopLevelWindow_ValidateVisual_Org(This);
	}

	GlassKernel::g_window = This;
	const auto hr = g_CTopLevelWindow_ValidateVisual_Org(This);
	LOG_IF_FAILED(UpdateReflectionViewport(This));
	GlassKernel::g_window = nullptr;
	return hr;
}

void GlassFrameHandler::MyCTopLevelWindow_Destructor(uDWM::CTopLevelWindow* This)
{
	RemoveReflectionVisual(This, -1);
	return g_CTopLevelWindow_Destructor_Org(This);
}

void GlassFrameHandler::Update([[maybe_unused]] GlassEngine::UpdateType type)
{
}

void GlassFrameHandler::Startup()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_GlassFrameHandler))
	{
		return;
	}

	uDWM::g_projectionArray.ApplyToVariable("CGlassColorizationParameters::AdjustWindowColorization", g_CGlassColorizationParameters_AdjustWindowColorization_Org);
	uDWM::g_projectionArray.ApplyToVariable("ResourceHelper::CreateGeometryFromHRGN", g_ResourceHelper_CreateGeometryFromHRGN_Org);
	uDWM::g_projectionArray.ApplyToVariable("CTopLevelWindow::UpdateNCAreaBackground", g_CTopLevelWindow_UpdateNCAreaBackground_Org);
	uDWM::g_projectionArray.ApplyToVariable("CTopLevelWindow::UpdateClientBlur", g_CTopLevelWindow_UpdateClientBlur_Org);
	uDWM::g_projectionArray.ApplyToVariable("CTopLevelWindow::ValidateVisual", g_CTopLevelWindow_ValidateVisual_Org);
	uDWM::g_projectionArray.ApplyToVariable("CTopLevelWindow::~CTopLevelWindow", g_CTopLevelWindow_Destructor_Org);

	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CGlassColorizationParameters_AdjustWindowColorization_Org, &MyCGlassColorizationParameters_AdjustWindowColorization },
			{ &g_ResourceHelper_CreateGeometryFromHRGN_Org, &MyResourceHelper_CreateGeometryFromHRGN },
			{ &g_CTopLevelWindow_UpdateNCAreaBackground_Org, &MyCTopLevelWindow_UpdateNCAreaBackground },
			{ &g_CTopLevelWindow_UpdateClientBlur_Org, &MyCTopLevelWindow_UpdateClientBlur },

			{ &g_CTopLevelWindow_Destructor_Org, &MyCTopLevelWindow_Destructor },
			{ &g_CTopLevelWindow_ValidateVisual_Org, &MyCTopLevelWindow_ValidateVisual }
		},
		true
	);
}

void GlassFrameHandler::Shutdown()
{
	if (Shared::g_disabledHooks.test(Shared::DisabledHooks_GlassFrameHandler))
	{
		return;
	}

	HookHelper::PatchFunctions(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CGlassColorizationParameters_AdjustWindowColorization_Org, &MyCGlassColorizationParameters_AdjustWindowColorization },
			{ &g_ResourceHelper_CreateGeometryFromHRGN_Org, &MyResourceHelper_CreateGeometryFromHRGN },
			{ &g_CTopLevelWindow_UpdateNCAreaBackground_Org, &MyCTopLevelWindow_UpdateNCAreaBackground },
			{ &g_CTopLevelWindow_UpdateClientBlur_Org, &MyCTopLevelWindow_UpdateClientBlur },

			{ &g_CTopLevelWindow_Destructor_Org, &MyCTopLevelWindow_Destructor },
			{ &g_CTopLevelWindow_ValidateVisual_Org, &MyCTopLevelWindow_ValidateVisual }
		},
		false
	);

	SwitchToThread();

	g_combinedRgn.reset();
}
