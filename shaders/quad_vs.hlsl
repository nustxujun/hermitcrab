#include "common.hlsl"

struct VSInput
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
};

QuadInput vs(VSInput input)
{
	QuadInput result;
	result.position = float4(input.position,1);
	result.uv = input.uv;

	return result;
}

