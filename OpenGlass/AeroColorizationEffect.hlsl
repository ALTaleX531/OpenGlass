#define D2D_INPUT_COUNT 1
#define D2D_INPUT0_SIMPLE
#include <d2d1effecthelpers.hlsli>
#include "Common.hlsli"

cbuffer constants : register(b0)
{
	minfloat4 afterglow;
	minfloat4 blurBalance;
	minfloat4 color;
	minfloat4 fallback;
};

static const minfloat3 grayFactor = minfloat3(0.2126, 0.7152, 0.0722);

D2D_PS_ENTRY(AeroColorizationEffect)
{
	minfloat4 background = minfloat4(D2DGetInput(0));
	minfloat coverage = saturate(background.a);
	minfloat luminance = dot(background.rgb, grayFactor);
	minfloat3 result = minfloat3(blurBalance.xxx) * background.rgb;
	result = luminance * minfloat3(afterglow.xyz) + result;
	result = coverage * minfloat3(color.xyz) + result;

	minfloat4 fallbackColor = Premultiply(fallback);
	result += minfloat(1.0f - coverage) * fallbackColor.rgb;
	return minfloat4(
		result,
		coverage + minfloat(1.0f - coverage) * fallbackColor.a
	);
}
