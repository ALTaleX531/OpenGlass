#pragma once
#include "uDWMProjection.hpp"

namespace OpenGlass::GlassEffectBrush
{
	winrt::com_ptr<uDWM::CSolidColorLegacyMilBrushProxy> GetOrCreate(
		uDWM::CTopLevelWindow* window,
		bool createIfNecessary = false
	);
	void Remove(uDWM::CTopLevelWindow* window);
	void RemoveAll();
}
