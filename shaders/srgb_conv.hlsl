
Buffer<float4> input;
RWBuffer<float4> output;

cbuffer Constants
{
	uint width;
};

[numthreads(1,1,1)]
void main(uint GI : SV_GroupIndex, uint3 DTid : SV_DispatchThreadID)
{
	uint index = DTid.y * width + DTid.x;
	output[index] = pow(input[index], 2.2f);
}