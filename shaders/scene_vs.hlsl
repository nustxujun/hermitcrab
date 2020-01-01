
cbuffer VSConstant: register(b0)
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
};

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
	float4 normal: NORMAL0;
};

PSInput vs(VSInput input)
{
	PSInput result;

	float4 worldpos = mul(float4(input.position,1), world);
	float4 viewpos = mul(worldpos,view );
	result.position = mul(viewpos, proj);
	result.uv = input.uv;


	result.normal = mul(float4(input.normal, 0), world);
	return result;
}

