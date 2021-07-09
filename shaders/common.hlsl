struct QuadInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
	float3 normal: NORMAL0;
	float3 tangent: NORMAL1;
	float3 binormal: NORMAL2;

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
	float3 sundir; // directional light dir
	
	float4 suncolor;// directional light color

	float deltatime;
	float time;
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

// [ Jimenez et al. 2016, "Practical Realtime Strategies for Accurate Indirect Occlusion" ]
float3 AOMultiBounce(float3 BaseColor, float AO)
{
	float3 a = 2.0404 * BaseColor - 0.3324;
	float3 b = -4.7951 * BaseColor + 0.6417;
	float3 c = 2.7552 * BaseColor + 0.6903;
	return max(AO, ((AO * a + b) * AO + c) * AO);
}