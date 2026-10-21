#include "pch.h"
#include "TestHarness.hpp"
#include "../OpenGlassGUI/ColorizationPresets.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	void TestColorizationPresets()
	{
		using namespace ColorizationPresets;
		Check(!Get(Family::Vista).empty() && !Get(Family::Windows7).empty());
		Check(CalculateVistaOpacity(0xC4000000) == 77);
		Check(CalculateIntensityAlpha(77) == 0xC4);
		Check(CalculateIntensityAlpha(0) == 0 && CalculateIntensityAlpha(100) == 255);

		const Windows7Parameters sky{0x6B74B8FC, 0x6B74B8FC, 8, 43, 49};
		const Windows7Parameters opaqueSky{0x6B74B8FC, 0x6B74B8FC, 42, 10, 48};
		Check(CalculateWindows7Parameters(sky.color, false) == sky);
		Check(CalculateWindows7Parameters(sky.color, true) == opaqueSky);
		for (const auto& expected : {
			Windows7Parameters{0xA80046AD, 0xA80046AD, 56, 11, 33},
			Windows7Parameters{0x65000000, 0x65000000, 5, 44, 51},
			Windows7Parameters{0x66000000, 0x66000000, 5, 45, 50},
			Windows7Parameters{0xBD000000, 0xBD000000, 70, 0, 30}
		})
		{
			Check(CalculateWindows7Parameters(expected.color, false) == expected);
		}

		// Saved intensity determines a preset match even when Windows changes alpha.
		auto synchronizedSky = sky;
		synchronizedSky.color &= 0x00FFFFFF;
		synchronizedSky.afterglow |= 0xFF000000;
		Check(MatchesWindows7Preset(Windows7.front(), synchronizedSky, 42, false));
		Check(!MatchesWindows7Preset(Windows7.front(), synchronizedSky, 63, false));
		++synchronizedSky.blurBalance;
		Check(!MatchesWindows7Preset(Windows7.front(), synchronizedSky, 42, false));
	}
}
