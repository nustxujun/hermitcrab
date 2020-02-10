#include "common.hlsl"

QuadInput vs(uint id:SV_VertexID)
{
	QuadInput o;
	float x = float( id % 2);
	float y = float( id/2 % 2) ;
	o.position = float4(x * 2.0f - 1.0f,1.0f - y * 2.0f ,0,1);
	o.uv = float2(x,y);
	return o;
}
