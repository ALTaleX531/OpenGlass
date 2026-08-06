#include "Blur.Common.hlsli"

minfloat4 main(PS_INPUT input) : SV_Target0
{
	minfloat4 result = 0;
	
	result += minfloat4(conv[0].x * texture0.Sample(sampler0, input.tex1.xy));
	result += minfloat4(conv[1].x * texture0.Sample(sampler0, input.tex1.zw));
	result += minfloat4(conv[2].x * texture0.Sample(sampler0, input.tex2.xy));
	result += minfloat4(conv[3].x * texture0.Sample(sampler0, input.tex2.zw));
	result += minfloat4(conv[4].x * texture0.Sample(sampler0, input.tex3.xy));
	result += minfloat4(conv[5].x * texture0.Sample(sampler0, input.tex3.zw));
	result += minfloat4(conv[6].x * texture0.Sample(sampler0, input.tex4.xy));
	result += minfloat4(conv[7].x * texture0.Sample(sampler0, input.tex4.zw));

	return result;
}
