#pragma once
#include "CanvasEffect.hpp"

namespace OpenGlass::Win2D
{
	class CrossFadeEffect : public CanvasEffect
	{
	public:
		CrossFadeEffect() : CanvasEffect{ CLSID_D2D1CrossFade }
		{
			m_namedProperties.emplace_back(
				L"Weight",
				D2D1_CROSSFADE_PROP_WEIGHT,
				NamedProperty::GRAPHICS_EFFECT_PROPERTY_MAPPING::GRAPHICS_EFFECT_PROPERTY_MAPPING_DIRECT
			);
			SetWeight();
		}
		virtual ~CrossFadeEffect() = default;

		void SetWeight(float weight = 0.5f)
		{
			SetProperty(D2D1_CROSSFADE_PROP_WEIGHT, BoxValue(weight));
		}
		void SetDestination(const wge::IGraphicsEffectSource& source)
		{
			SetInput(1, source);
		}
		void SetSource(const wge::IGraphicsEffectSource& source)
		{
			SetInput(0, source);
		}

		void SetDestination(const wuc::CompositionEffectSourceParameter& source)
		{
			SetInput(1, source);
		}
		void SetSource(const wuc::CompositionEffectSourceParameter& source)
		{
			SetInput(0, source);
		}
	};
}