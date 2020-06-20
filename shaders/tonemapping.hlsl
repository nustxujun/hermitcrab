#include "common.hlsl"

Texture2D frame: register(t0);
sampler linearSampler: register(s0);

half3 ACESFilm(half3 x)
{
	half a = 2.51f;
	half b = 0.03f;
	half c = 2.43f;
	half d = 0.59f;
	half e = 0.14f;
	return saturate((x*(a*x + b)) / (x*(c*x + d) + e));
}


half4 ps(QuadInput input):SV_Target
{
	half4 color = frame.Sample(linearSampler, input.uv);

	// do in shading
	color = pow(color, 1.0f / 2.2f);


	color.rgb = ACESFilm(color.rgb);


	return color;
	//return half4(input.uv,0,1);

}


