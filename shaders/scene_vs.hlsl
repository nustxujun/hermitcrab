#include "common.hlsl"
cbuffer VSConstant
{
	matrix world;
	matrix view; 
	matrix proj;
};

struct VSInput
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
	float3 normal: NORMAL0;

	float3 tangent: NORMAL1;
	float3 binormal: NORMAL2;
};

PSInput vs(VSInput input)
{
	PSInput result;

	float4 worldpos = mul(float4(input.position,1), world);
	result.worldPos = worldpos;

	float4 viewpos = mul(worldpos,view );
	result.position = mul(viewpos, proj);
	result.uv = input.uv;


	result.normal = mul(float4(input.normal, 0), world);
	result.tangent = mul(float4(input.tangent, 0), world);
	result.binormal = mul(float4(input.binormal, 0), world);
	return result;
}

