#pragma once
#include "CanvasEffect.hpp"

namespace OpenGlass::Win2D
{
	class BlendEffect : public CanvasEffect
	{
	public:
		BlendEffect() : CanvasEffect{ CLSID_D2D1Blend }
		{
			SetBlendMode();
		}
		virtual ~BlendEffect() = default;

		void SetBlendMode(D2D1_BLEND_MODE blendMode = D2D1_BLEND_MODE_MULTIPLY)
		{
			SetProperty(D2D1_BLEND_PROP_MODE, BoxValue(blendMode));
		}
		void SetBackground(const wge::IGraphicsEffectSource& source)
		{
			SetInput(0, source);
		}
		void SetBackground(const wuc::CompositionEffectSourceParameter& source)
		{
			SetInput(0, source);
		}

		void SetForeground(const wge::IGraphicsEffectSource& source)
		{
			SetInput(1, source);
		}
		void SetForeground(const wuc::CompositionEffectSourceParameter& source)
		{
			SetInput(1, source);
		}
	};
}