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

HRESULT GlassFrameHandler::MyCTopLevelWindow_UpdateNCAreaBackground(uDWM::CTopLevelWindow* This)
{
	const auto active = This->TreatAsActiveWindow();
	const auto maximized = This->TreatAsMaximized();

	HRESULT hr{ S_OK };
	GlassKernel::g_redirectFirstCreateRectRgnCall = true;
	g_combinedRgn.reset(CreateRectRgn(0, 0, 0, 0));
	const auto combinedRgnScope = wil::scope_exit([] { GlassKernel::g_redirectFirstCreateRectRgnCall = std::nullopt; g_combinedRgn.reset(); });
	hr = g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);

	auto effectBrush = GlassEffectBrush::GetOrCreate(This);

	if (
		const auto legacyVisual = This->GetLegacyVisual();
		legacyVisual &&
		SUCCEEDED(legacyVisual->_ValidateVisual()) &&
		!legacyVisual->IsEmpty()
	)
	{
		const auto reflectionBrush = GlassReflectionBrush::GetOrCreate(
			This,
			0,
			true
		);
		if (!effectBrush)
		{
			effectBrush = GlassEffectBrush::GetOrCreate(This, true);

			if (Shared::g_overrideAccent)
			{
				if (
					const auto accentVisual = This->GetAccent();
					accentVisual
				)
				{
					accentVisual->SetDirtyFlags(0x1000);
				}
			}
		}

		winrt::com_ptr<uDWM::CRgnGeometryProxy> captionGeometry{ nullptr };
		winrt::com_ptr<uDWM::CBaseLegacyMilBrushProxy> brush{ nullptr };
		{
			const auto instruction = reinterpret_cast<uDWM::CDrawGeometryInstruction*>(legacyVisual->GetInstructions().views().front());
			brush.copy_from(instruction->GetBrush());
			captionGeometry.copy_from(reinterpret_cast<uDWM::CRgnGeometryProxy*>(instruction->GetGeometry()));
			RETURN_IF_FAILED(
				uDWM::ResourceHelper::CreateGeometryFromHRGN(
					g_combinedRgn.get(),
					reinterpret_cast<uDWM::CRgnGeometryProxy**>(&captionGeometry)
				)
			);
		}

		if (brush.get() != effectBrush.get())
		{
			RETURN_IF_FAILED(legacyVisual->ClearInstructions());
			winrt::com_ptr<uDWM::CDrawGeometryInstruction> instruction{};
			RETURN_IF_FAILED(
				uDWM::CDrawGeometryInstruction::Create(
					effectBrush.get(),
					captionGeometry.get(),
					instruction.put()
				)
			);
			RETURN_IF_FAILED(legacyVisual->AddInstruction(instruction.get()));
			RETURN_IF_FAILED(
				uDWM::CDrawGeometryInstruction::Create(
					reflectionBrush.get(),
					captionGeometry.get(),
					instruction.put()
				)
			);
			RETURN_IF_FAILED(legacyVisual->AddInstruction(instruction.get()));
		}

		if (!This->IsOffscreen())
		{
			RETURN_IF_FAILED(
				reflectionBrush->Update(
					(Shared::g_reflectionPolicy & Shared::ReflectionPolicy::NonClient) ?
					GlassKernel::GetAdjustedReflectionIntensity(active, maximized) :
					0.f,
					GlassReflectionBrush::CalculateTargetViewport(
						legacyVisual->GetLocalToParentVisualOffset(This->GetTransformParent()),
						Shared::g_reflectionParallaxIntensity,
						This->IsRTLMirrored(),
						legacyVisual->GetWidth(),
						legacyVisual->GetScale()
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
		}
	}

	// if fTransitionOnMaximized is specified,
	// we should also update the brush even the nonclient area is empty
	if (effectBrush)
	{
		auto color = This->GetCaptionColorizationParameters()->getArgbcolor();
		color.a = GlassKernel::AlphaChannelReinterpreter(active, maximized).ToFloat();
		RETURN_IF_FAILED(effectBrush->Update(1.0, color));
	}
	else if (Shared::g_overrideAccent)
	{
		if (
			const auto accentVisual = This->GetAccent();
			accentVisual
		)
		{
			accentVisual->SetDirtyFlags(0x1000);
		}
	}

	return hr;
}

HRESULT GlassFrameHandler::MyCTopLevelWindow_UpdateClientBlur(uDWM::CTopLevelWindow* This)
{
	const auto hr = g_CTopLevelWindow_UpdateClientBlur_Org(This);

	const auto active = This->TreatAsActiveWindow();
	const auto maximized = This->TreatAsMaximized();

	auto effectBrush = GlassEffectBrush::GetOrCreate(This);

	if (
		const auto clientBlurVisual = This->GetClientBlurVisual();
		clientBlurVisual &&
		!clientBlurVisual->IsEmpty()
	)
	{
		const auto reflectionBrush = GlassReflectionBrush::GetOrCreate(
			This,
			1,
			true
		);
		if (!effectBrush)
		{
			effectBrush = GlassEffectBrush::GetOrCreate(This, true);

			if (Shared::g_overrideAccent)
			{
				if (
					const auto accentVisual = This->GetAccent();
					accentVisual
				)
				{
					accentVisual->SetDirtyFlags(0x1000);
				}
			}
		}

		winrt::com_ptr<uDWM::CRgnGeometryProxy> clientBlurGeometry{ nullptr };
		winrt::com_ptr<uDWM::CBaseLegacyMilBrushProxy> brush{ nullptr };
		{
			const auto instruction = reinterpret_cast<uDWM::CDrawGeometryInstruction*>(clientBlurVisual->GetInstructions().views().front());
			brush.copy_from(instruction->GetBrush());
			clientBlurGeometry.copy_from(reinterpret_cast<uDWM::CRgnGeometryProxy*>(instruction->GetGeometry()));
		}

		if (brush.get() != effectBrush.get())
		{
			RETURN_IF_FAILED(clientBlurVisual->ClearInstructions());
			winrt::com_ptr<uDWM::CDrawGeometryInstruction> instruction{};
			RETURN_IF_FAILED(
				uDWM::CDrawGeometryInstruction::Create(
					effectBrush.get(),
					clientBlurGeometry.get(),
					instruction.put()
				)
			);
			RETURN_IF_FAILED(clientBlurVisual->AddInstruction(instruction.get()));
			RETURN_IF_FAILED(
				uDWM::CDrawGeometryInstruction::Create(
					reflectionBrush.get(),
					clientBlurGeometry.get(),
					instruction.put()
				)
			);
			RETURN_IF_FAILED(clientBlurVisual->AddInstruction(instruction.get()));
		}

		if (!This->IsOffscreen())
		{
			RETURN_IF_FAILED(
				reflectionBrush->Update(
					(Shared::g_reflectionPolicy & Shared::ReflectionPolicy::NonClient) ?
					GlassKernel::GetAdjustedReflectionIntensity(active, maximized) :
					0.f,
					GlassReflectionBrush::CalculateTargetViewport(
						clientBlurVisual->GetLocalToParentVisualOffset(This->GetTransformParent()),
						Shared::g_reflectionParallaxIntensity,
						This->IsRTLMirrored(),
						clientBlurVisual->GetWidth(),
						clientBlurVisual->GetScale()
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
		}
	}

	// if fTransitionOnMaximized is specified,
	// we should also update the brush even the nonclient area is empty
	if (effectBrush)
	{
		auto color = This->GetCaptionColorizationParameters()->getArgbcolor();
		color.a = GlassKernel::AlphaChannelReinterpreter(active, maximized).ToFloat();
		RETURN_IF_FAILED(effectBrush->Update(1.0, color));
	}
	else if (Shared::g_overrideAccent)
	{
		if (
			const auto accentVisual = This->GetAccent();
			accentVisual
		)
		{
			accentVisual->SetDirtyFlags(0x1000);
		}
	}

	return hr;
}

HRESULT GlassFrameHandler::MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This)
{
	auto data = This->GetData();
	if (!data)
	{
		return g_CTopLevelWindow_ValidateVisual_Org(This);
	}

	GlassKernel::g_window = This;
	const auto updateReflectionBeforeLeave = wil::scope_exit([This]
	{
		GlassKernel::g_window = nullptr;
		LOG_IF_FAILED(UpdateReflectionViewport(This));
	});
	
	return g_CTopLevelWindow_ValidateVisual_Org(This);
}

void GlassFrameHandler::MyCTopLevelWindow_Destructor(uDWM::CTopLevelWindow* This)
{
	GlassReflectionBrush::Remove(This);
	GlassEffectBrush::Remove(This);
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
