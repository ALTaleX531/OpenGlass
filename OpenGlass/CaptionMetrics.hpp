#pragma once
#include "framework.hpp"

namespace OpenGlass::CaptionMetrics
{
	enum CaptionButtons : UINT
	{
		Disabled = 0,
		WindowsVista,
		Windows7,
		Windows8
	};

	SIZE CalculateButtonSize(CaptionButtons style, int cySize, int buttonType);
}
