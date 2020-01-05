#include "../common.hlsl"
#include "../pbr.hlsl"


cbuffer PSConstant
{
	float4 ColorTop;
	float4 ColorBottom;

}


Texture2D albedoMap:register(t0);
Texture2D normalMap:register(t1);
sampler linearSampler:register(s0);

half4 ps(PSInput input) : SV_TARGET
{
	float4 params = albedoMap.Sample(linearSampler, input.uv);

	half4 albedo = lerp(ColorTop, ColorBottom, params.g) * params.r;

	half4 normal = calNormal(normalMap.Sample(linearSampler, input.uv), input.normal, input.tangent, input.binormal);

	half roughness = lerp(0.1086,0.3115,params.g);
	half3 color = directBRDF(roughness, params.g, F0_DEFAULT, albedo, normal, -sundir, campos - input.worldPos);

	return  half4(color,1) * suncolor;
}
