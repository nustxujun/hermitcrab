#include "common.hlsl"

Texture2D tex: register(t0);
sampler linearSampler: register(s0);

half4 ps(QuadInput input):SV_Target
{
	return tex.Sample(linearSampler, input.uv);
	//return float4(input.uv,0,1);
}


