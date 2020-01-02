#include "../common.hlsl"
#include "../pbr.hlsl"

cbuffer PSConstant:register(b0)
{
	float4 ColoBase;
	float4 ColorSeats;
	float4 ColorMetal;
}


Texture2D albedoMap:register(t0);
Texture2D normalMap:register(t1);
sampler linearSampler:register(s0);

float4 ps(PSInput input) : SV_TARGET
{
	float4 params = albedoMap.Sample(linearSampler, input.uv);
	float4 color = lerp(lerp(ColoBase, ColorSeats, params.b), ColorMetal,params.g) * params.r;

	half4 normal = calNormal(normalMap, linearSampler, input.uv,input.normal, input.tangent, input.binormal);

	return  normal;
}
