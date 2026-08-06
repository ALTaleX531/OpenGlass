#include "Blur.Common.hlsli"

minfloat4 main(PS_INPUT input) : SV_Target0
{
	minfloat4 background = 0;

	background += minfloat4(conv[0].x * texture0.Sample(sampler0, input.tex1.xy));
	background += minfloat4(conv[1].x * texture0.Sample(sampler0, input.tex1.zw));
	background += minfloat4(conv[2].x * texture0.Sample(sampler0, input.tex2.xy));
	background += minfloat4(conv[3].x * texture0.Sample(sampler0, input.tex2.zw));
	background += minfloat4(conv[4].x * texture0.Sample(sampler0, input.tex3.xy));
	background += minfloat4(conv[5].x * texture0.Sample(sampler0, input.tex3.zw));
	background += minfloat4(conv[6].x * texture0.Sample(sampler0, input.tex4.xy));
	background += minfloat4(conv[7].x * texture0.Sample(sampler0, input.tex4.zw));

	minfloat coverage = saturate(background.a);
	minfloat luminance = dot(background.rgb, grayFactor);
	minfloat3 result = minfloat3(blurBalance.xxx) * background.rgb;
	result = luminance * minfloat3(afterglow.xyz) + result;
	result = coverage * minfloat3(color.xyz) + result;

	minfloat4 fallbackColor = Premultiply(minfloat4(fallback));
	result += minfloat(1.0f - coverage) * fallbackColor.rgb;
	return minfloat4(
		result,
		coverage + minfloat(1.0f - coverage) * fallbackColor.a
	);
}
