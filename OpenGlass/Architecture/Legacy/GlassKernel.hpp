#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "GlassEngine.hpp"

namespace OpenGlass::GlassKernel
{
	inline const uDWM::CTopLevelWindow* g_window{ nullptr };
	inline RECT g_roundedBounds{};
	inline int g_diameter{ 0 };
	inline std::optional<bool> g_redirectFirstCreateRectRgnCall{};

	class AlphaChannelReinterpreter
	{
		std::bitset<16> m_flag{};
	public:
		bool GetIsValid() const
		{
			return m_flag.test(0);
		}
		bool GetIsActive() const
		{
			return m_flag.test(1);
		}
		bool GetIsMaximized() const
		{
			return m_flag.test(2);
		}
		void SetIsActive(bool value)
		{
			m_flag.set(1, value);
		}
		void SetIsMaximized(bool value)
		{
			m_flag.set(2, value);
		}

		float ToFloat() const
		{
			const auto value = ((m_flag.to_ulong() << 8) | 0x3F000001);
			return *reinterpret_cast<float const*>(&value);
		}
		AlphaChannelReinterpreter(float value)
		{
			if ((*reinterpret_cast<DWORD const*>(&value) & 0xFF0000FF) == 0x3F000001)
			{
				m_flag = (*reinterpret_cast<DWORD const*>(&value) & 0x00FFFF00) >> 8;
				m_flag.set(0);
			}
		}
		AlphaChannelReinterpreter(
			bool active,
			bool maximized
		)
		{
			SetIsActive(active);
			SetIsMaximized(maximized);
		}
	};

	void RedrawAllTopLevelWindow(bool deepRedraw);
	float GetBlurRadius();

	struct CRealizedGlassColorizationParameters
	{
		D2D1_COLOR_F color;
		D2D1_COLOR_F afterglow;
		float colorBalance;
		float afterglowBalance;
		float blurBalance;

		D2D1_COLOR_F GetEffectivescRGBBlendColor(float sdrBoost) const;
	};
	float GetColorizationOpacity(bool active, bool maximized);
	D2D1_COLOR_F GetBaseColor(bool opaque, bool maximized);
	D2D1_COLOR_F GetSourceColor(bool active);
	CRealizedGlassColorizationParameters RealizeWindowColorization(
		const D2D1_COLOR_F& baseColor,
		const D2D1_COLOR_F& srcColor,
		float colorizationOpacity,
		bool opaque,
		bool livePreview
	);
	float GetAdjustedReflectionIntensity(bool active, bool maximized);

	bool IsCurrentCVIFullyTransparent();

	void Update(GlassEngine::UpdateType type);
	void Startup();
	void Shutdown();
}
