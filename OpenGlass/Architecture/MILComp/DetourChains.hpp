#pragma once
#include "ProjectionHelper.hpp"
#include "uDwmProjection.hpp"

namespace OpenGlass::GlassFrameDemodernizer
{
	HRESULT MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This);
	HRESULT MyCTopLevelWindow_UpdateNCAreaBackground(uDWM::CTopLevelWindow* This);
}

namespace OpenGlass::GlassFrameHandler
{
	HRESULT MyCTopLevelWindow_ValidateVisual(uDWM::CTopLevelWindow* This);
	HRESULT MyCTopLevelWindow_UpdateNCAreaBackground(uDWM::CTopLevelWindow* This);
}

namespace OpenGlass::DetourChains
{
	// Replacements are listed in invocation order, not component startup order.
	using TopLevelWindowValidateVisual = Projection::ChainDetour<
		uDWM::Symbol_CTopLevelWindow_ValidateVisual,
		&GlassFrameDemodernizer::MyCTopLevelWindow_ValidateVisual,
		&GlassFrameHandler::MyCTopLevelWindow_ValidateVisual
	>;
	using TopLevelWindowValidateVisualDemodernizerNode = TopLevelWindowValidateVisual::Node<0>;
	using TopLevelWindowValidateVisualFrameHandlerNode = TopLevelWindowValidateVisual::Node<1>;

	using TopLevelWindowUpdateNCAreaBackground = Projection::ChainDetour<
		uDWM::Symbol_CTopLevelWindow_UpdateNCAreaBackground,
		&GlassFrameDemodernizer::MyCTopLevelWindow_UpdateNCAreaBackground,
		&GlassFrameHandler::MyCTopLevelWindow_UpdateNCAreaBackground
	>;
	using TopLevelWindowUpdateNCAreaBackgroundDemodernizerNode = TopLevelWindowUpdateNCAreaBackground::Node<0>;
	using TopLevelWindowUpdateNCAreaBackgroundFrameHandlerNode = TopLevelWindowUpdateNCAreaBackground::Node<1>;
}
