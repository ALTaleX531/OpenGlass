#pragma once
#include "resource.h"
#include "uDWMProjection.hpp"
#include "Shared.hpp"

namespace OpenGlass::GlassReflectionBrush
{
	D2D1_RECT_F CalculateTargetViewport(
		const POINT& offset,
		float parallaxIntensity = 0.f,
		bool mirrored = false,
		LONG width = 0,
		const DWM::MilSizeD& scale = { 1.0, 1.0 }
	);
	
	winrt::com_ptr<uDWM::CImageLegacyMilBrushProxy> GetOrCreate(
		void* owner,
		unsigned int slot,
		bool createIfNecessary = false
	);
	void Remove(void* owner);
	void RemoveAll();
}
