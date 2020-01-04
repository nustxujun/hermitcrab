#include "common.hlsl"

Texture2D frame: register(t0);
sampler linearSampler: register(s0);

half4 ps(QuadInput input):SV_Target
{
	half4 color = frame.Sample(linearSampler, input.uv);
	return pow(color, 1.0f / 2.2f);
	//return half4(color.xyz, 1);
	//return color ;
	//return half4(input.uv,0,1);
}


