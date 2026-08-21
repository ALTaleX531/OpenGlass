#include "pch.h"
#include "Shared.hpp"
#include "GlassFrameHandler.hpp"
#include "GlassKernel.hpp"
#include "uDWMProjection.hpp"
#include "dwmcoreProjection.hpp"
#include "GlassEffectBrush.hpp"
#include "GlassReflectionBrush.hpp"
#include "DetourChains.hpp"

using namespace OpenGlass;

namespace OpenGlass::GlassFrameHandler
{
	void MyCGlassColorizationParameters_AdjustWindowColorization(
		uDWM::CGlassColorizationParameters* This,
		const uDWM::GpCC* colorUnused,
		float opacity,
		UINT flag
	);
	HRESULT MyCTopLevelWindow_UpdateClientBlur(uDWM::CTopLevelWindow* This);
	void MyCTopLevelWindow_Destructor(uDWM::CTopLevelWindow* This);

	Projection::Detour<uDWM::Symbol_CGlassColorizationParameters_AdjustWindowColorization, &MyCGlassColorizationParameters_AdjustWindowColorization> g_CGlassColorizationParameters_AdjustWindowColorization_Org{};
	DetourChains::TopLevelWindowUpdateNCAreaBackgroundFrameHandlerNode g_CTopLevelWindow_UpdateNCAreaBackground_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow_UpdateClientBlur, &MyCTopLevelWindow_UpdateClientBlur> g_CTopLevelWindow_UpdateClientBlur_Org{};
	DetourChains::TopLevelWindowValidateVisualFrameHandlerNode g_CTopLevelWindow_ValidateVisual_Org{};
	Projection::Detour<uDWM::Symbol_CTopLevelWindow__CTopLevelWindow, &MyCTopLevelWindow_Destructor> g_CTopLevelWindow_Destructor_Org{};

	HRESULT UpdateReflectionViewport(uDWM::CTopLevelWindow* window);
}

HRESULT GlassFrameHandler::UpdateReflectionViewport(uDWM::CTopLevelWindow* window)
{
	const auto active = window->TreatAsActiveWindow();
	const auto maximized = window->TreatAsMaximized();
	const auto desktop = window->GetTransformParent();
	const auto opacity = GlassKernel::GetAdjustedReflectionIntensity(active, maximized);
	if (
		const auto legacyVisual = window->GetLegacyVisual();
		legacyVisual
	)
	{
		if (
			const auto brush = GlassReflectionBrush::GetOrCreate(window, 0);
			brush &&
			!window->IsOffscreen()
		)
		{
			RETURN_IF_FAILED(
				brush->Update(
					(Shared::g_reflectionPolicy & Shared::ReflectionPolicy::NonClient) ?
					opacity :
					0.f,
					GlassReflectionBrush::CalculateTargetViewport(
						legacyVisual->GetLocalToParentVisualOffset(desktop),
						Shared::g_reflectionParallaxIntensity,
						window->IsRTLMirrored(),
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
	if (
		const auto clientBlurVisual = window->GetClientBlurVisual();
		clientBlurVisual
	)
	{
		if (
			const auto brush = GlassReflectionBrush::GetOrCreate(window, 1);
			brush &&
			!window->IsOffscreen()
		)
		{
			RETURN_IF_FAILED(
				brush->Update(
					(Shared::g_reflectionPolicy & Shared::ReflectionPolicy::NonClient) ?
					opacity :
					0.f,
					GlassReflectionBrush::CalculateTargetViewport(
						clientBlurVisual->GetLocalToParentVisualOffset(desktop),
						Shared::g_reflectionParallaxIntensity,
						window->IsRTLMirrored(),
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
	if (
		const auto accentVisual = window->GetAccent();
		accentVisual
	)
	{
		if (
			const auto brush = GlassReflectionBrush::GetOrCreate(window, 2);
			brush &&
			!window->IsOffscreen()
		)
		{
			RETURN_IF_FAILED(
				brush->Update(
					(Shared::g_reflectionPolicy & Shared::ReflectionPolicy::NonClient) ?
					opacity :
					0.f,
					GlassReflectionBrush::CalculateTargetViewport(
						accentVisual->GetLocalToParentVisualOffset(desktop),
						Shared::g_reflectionParallaxIntensity,
						window->IsRTLMirrored(),
						accentVisual->GetWidth(),
						accentVisual->GetScale()
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

	return S_OK;
}

void GlassFrameHandler::MyCGlassColorizationParameters_AdjustWindowColorization(
	uDWM::CGlassColorizationParameters* This,
	[[maybe_unused]] const uDWM::GpCC* colorUnused,
	[[maybe_unused]] float opacity,
	UINT flag
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

}

HRESULT GlassFrameHandler::MyCTopLevelWindow_UpdateNCAreaBackground(uDWM::CTopLevelWindow* This)
{
	const auto active = This->TreatAsActiveWindow();
	const auto maximized = This->TreatAsMaximized();

	const auto hr = g_CTopLevelWindow_UpdateNCAreaBackground_Org(This);

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
					GlassKernel::g_combinedRgn.get(),
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

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CGlassColorizationParameters_AdjustWindowColorization_Org },
			{ &g_CTopLevelWindow_UpdateNCAreaBackground_Org },
			{ &g_CTopLevelWindow_UpdateClientBlur_Org },

			{ &g_CTopLevelWindow_Destructor_Org },
			{ &g_CTopLevelWindow_ValidateVisual_Org }
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

	HookHelper::ApplyInlineHooks(
		std::initializer_list<HookHelper::DetourInfo>
		{
			{ &g_CGlassColorizationParameters_AdjustWindowColorization_Org },
			{ &g_CTopLevelWindow_UpdateNCAreaBackground_Org },
			{ &g_CTopLevelWindow_UpdateClientBlur_Org },

			{ &g_CTopLevelWindow_Destructor_Org },
			{ &g_CTopLevelWindow_ValidateVisual_Org }
		},
		false
	);

}

void GlassFrameHandler::Cleanup()
{
}
