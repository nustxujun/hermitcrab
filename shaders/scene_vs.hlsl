
cbuffer Constant: register(b0)
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

	result.position = mul(position, world);
	result.position = mul(result.position,view );
	result.position = mul(result.position, proj);
	result.uv = uv;

	return result;
}

float4 ps(PSInput input) : SV_TARGET
{
	return 1;
}
