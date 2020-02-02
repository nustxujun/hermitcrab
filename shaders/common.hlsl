struct QuadInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
	float4 normal: NORMAL0;
	float4 tangent: NORMAL1;
	float4 binormal: NORMAL2;

	float4 worldPos: COLOR0;
	float4 color: COLOR1;
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

half3 calNormal(
	half3 normalmap,
	half3 normal, 
	half3 tangent,
	half3 binormal)
{
	normalmap.xyz = normalmap.xyz * 2.0f - 1.0f;
	return half3(normalmap.xxx * tangent.xyz + normalmap.yyy * binormal.xyz + normalmap.zzz * normal.xyz);
}
