struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
	float4 normal: NORMAL0;
	float4 tangent: NORMAL1;
	float4 binormal: NORMAL2;

	float4 worldPos: COLOR0;
};

struct LightInfo
{
	float4 pos;
	float4 dir;
	float4 color;
};

cbuffer CommonConstants
{
	// camera
	float4 campos;
	float4 camdir;

	// lights
	LightInfo lights[4];
	int numlights;

	// directional light
	float3 sundir;
	float4 suncolor;
};

half4 calNormal(
	Texture2D normalmap, 
	sampler linearsampler, 
	half2 uv,
	half4 normal, 
	half4 tangent,
	half4 binormal)
{
	half4 result = normalmap.Sample(linearsampler, uv);
	result.xyz =result.xyz * 2.0f - 1.0f;
	return half4(result.xxx * tangent.xyz + result.yyy * binormal.xyz + result.zzz * normal.xyz, 0);
}
