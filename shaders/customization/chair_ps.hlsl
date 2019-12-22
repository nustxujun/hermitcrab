cbuffer PSConstant:register(b0)
{
	float4 ColoBase;
	float4 ColorSeats;
	float4 ColorMetal;
}

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

Texture2D albedo:register(t0);
sampler linearSampler:register(s0);

float4 ps(PSInput input) : SV_TARGET
{
	float4 params = albedo.Sample(linearSampler, input.uv);
	float4 color = lerp(lerp(ColoBase, ColorSeats, params.b), ColorMetal,params.g) * params.r;
	return  color;
}
