#include "common.hlsl"

struct VSInput
{
	float2 position : POSITION;
	float2 uv : TEXCOORD;
};

QuadInput vs(VSInput input)
{
	QuadInput result;
	result.position = float4(input.position,0.5,1);
	result.uv = input.uv;

	return result;
}

