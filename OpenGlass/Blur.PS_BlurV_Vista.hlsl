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
	
	return minfloat4(
		minfloat3(color.xyz) + minfloat(1.0f - color.a) * background.xyz,
		minfloat(color.a) + minfloat(1.0f - color.a) * background.a
	);
}
