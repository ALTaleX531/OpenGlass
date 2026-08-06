#ifndef BLUR_COMMON_HLSLI
#define BLUR_COMMON_HLSLI

#include "Common.hlsli"

cbuffer BlurVSConstants : register(b0)
{
	float2 viewportSize;
	float2 _vsPadding;
};

cbuffer BlurPSConstants : register(b0)
{
	float4 conv[8];
	float4 afterglow;
	float4 blurBalance;
	float4 color;
	float4 fallback;
};

Texture2D<float4> texture0 : register(t0);
SamplerState sampler0 : register(s0);

struct VS_INPUT
{
	float2 pos : POSITION;
	float2 tex0 : TEXCOORD0;
	float4 tex1 : TEXCOORD1; // (uv0.xy, uv1.xy)
	float4 tex2 : TEXCOORD2; // (uv2.xy, uv3.xy)
	float4 tex3 : TEXCOORD3; // (uv4.xy, uv5.xy)
	float4 tex4 : TEXCOORD4; // (uv6.xy, uv7.xy)
};

struct PS_INPUT
{
	float4 pos : SV_POSITION0;
	float2 tex0 : TEXCOORD0;
	float4 tex1 : TEXCOORD1;
	float4 tex2 : TEXCOORD2;
	float4 tex3 : TEXCOORD3;
	float4 tex4 : TEXCOORD4;
};

static const minfloat3 grayFactor = minfloat3(0.2126, 0.7152, 0.0722);

#endif // BLUR_COMMON_HLSLI
