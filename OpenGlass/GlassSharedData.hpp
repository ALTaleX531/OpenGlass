#pragma once
#include "framework.hpp"
#include "cpprt.hpp"

namespace OpenGlass::GlassSharedData
{
	inline void* g_LastTopLevelWindow = nullptr;
	inline bool g_disableOnBattery{ true };
	inline bool g_overrideAccent{ false };
	inline bool g_batteryMode{ false };
	inline bool g_transparencyEnabled{ true };
	inline float g_ColorizationAfterglowBalance = 0.43f;
	inline float g_ColorizationBlurBalance = 0.49f;
	inline float g_ColorizationColorBalance = 0.08f;
	inline D2D_COLOR_F g_ColorizationColor{ 116.0f / 255.0f, 184.0f / 255.0f, 252.0f / 255.0f,1.0f };

	FORCEINLINE bool IsBackdropAllowed()
	{
		if (g_batteryMode && g_disableOnBattery)
		{
			return false;
		}
		if (!g_transparencyEnabled)
		{
			return false;
		}

		return true;
	}
}