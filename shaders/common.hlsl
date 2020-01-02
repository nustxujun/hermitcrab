struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
	float4 normal: NORMAL0;
	float4 tangent: NORMAL1;
	float4 binormal: NORMAL2;
};

struct LightInfo
{
	float4 pos;
	float4 dir;
	float4 color;
};

cbuffer LightConstants
{
	LightInfo lights[4];
	int numlights;
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
	return half4(result.xxx * tangent.xyz + result.yyy * binormal.xyz + result.zzz * normal.xyz, 0);
}
