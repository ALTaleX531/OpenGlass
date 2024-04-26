#pragma once
#include "CanvasEffect.hpp"

namespace OpenGlass::Win2D
{
	class AlphaMaskEffect : public CanvasEffect
	{
	public:
		AlphaMaskEffect() : CanvasEffect{ CLSID_D2D1AlphaMask } {}
		virtual ~AlphaMaskEffect() = default;

		void SetSource(const wge::IGraphicsEffectSource& source)
		{
			SetInput(0, source);
		}
		void SetSource(const wuc::CompositionEffectSourceParameter& source)
		{
			SetInput(0, source);
		}
		void SetMask(const wge::IGraphicsEffectSource& source)
		{
			SetInput(1, source);
		}
		void SetMask(const wuc::CompositionEffectSourceParameter& source)
		{
			SetInput(1, source);
		}
	};
}