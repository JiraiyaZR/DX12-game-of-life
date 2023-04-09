Texture2D output : register(t0);
RWTexture2D<float4> outputRW : register(u0);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
	outputRW[DTid.xy] = output[DTid.xy];
}