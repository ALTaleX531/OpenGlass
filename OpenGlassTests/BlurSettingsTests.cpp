#include "pch.h"
#include "TestHarness.hpp"
#include "BlurSettings.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestBlurSettings()
	{
		Check(BlurSettings::DecodeBlurAmount(0) == 0.f);
		Check(BlurSettings::DecodeBlurAmount(BlurSettings::DefaultEncodedDeviation) == 9.f);
		Check(BlurSettings::DecodeBlurAmount(100) == 30.f);
		Check(BlurSettings::DecodeBlurAmount(UINT32_MAX) == BlurSettings::MaximumBlurAmount);
		Check(BlurSettings::DecodeGuiBlurAmount(UINT32_MAX) == BlurSettings::GuiMaximumBlurAmount);
		Check(BlurSettings::EncodeGuiBlurAmount(-1) == 0);
		Check(BlurSettings::EncodeGuiBlurAmount(9) == BlurSettings::DefaultEncodedDeviation);
		Check(BlurSettings::EncodeGuiBlurAmount(31) == 100);
		for (int blurAmount = BlurSettings::GuiMinimumBlurAmount; blurAmount <= BlurSettings::GuiMaximumBlurAmount; ++blurAmount)
		{
			Check(
				BlurSettings::DecodeGuiBlurAmount(
					BlurSettings::EncodeGuiBlurAmount(blurAmount)
				) == blurAmount
			);
		}
		Check(BlurSettings::Direct3DStandardDeviation == 3.f);
	}
}
