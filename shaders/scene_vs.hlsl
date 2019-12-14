
cbuffer Constant: register(c0)
{
	matrix world;
	matrix view; 
	matrix proj;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

PSInput vs(float4 position : POSITION, float4 uv : TEXCOORD)
{
	PSInput result;

	result.position = mul(world, position);
	result.position = mul(view, result.position);
	result.position = mul(proj,  result.position);
	result.uv = uv;

	return result;
}

float4 ps(PSInput input) : SV_TARGET
{
	return 1;
}
