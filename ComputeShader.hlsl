#define blocksize 256
#define width 1280
#define height 720
#define cacheSize 258

struct lifecell {
	uint state;
	uint delay;
};

groupshared uint2 sharedState[cacheSize];

RWTexture2D<float4> outputRW : register(u0);
RWTexture2D<uint2> oldState : register(u1);
RWTexture2D<uint2> newState : register(u2);

[numthreads(256, 1, 1)]
void Hmain( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID )
{
    if (GTid.x < 1)
    {
		// Clamp out of bound samples that occur at image borders.
        uint x = (DTid.x - 1) % width;
        sharedState[GTid.x] = oldState[uint2(x, DTid.y)];
    }
    if (GTid.x >= blocksize - 1)
    {
		// Clamp out of bound samples that occur at image borders.
        uint x = (DTid.x + 1) % width;
        sharedState[GTid.x + 2] = oldState[uint2(x, DTid.y)];
    }

	// Clamp out of bound samples that occur at image borders.
    uint x = DTid.x % width;
    uint y = DTid.y % height;
    sharedState[GTid.x + 1] = oldState[uint2(x, y)];

	// Wait for all threads to finish.
    GroupMemoryBarrierWithGroupSync();

    uint2 neibor = uint2(0, 0);
	uint k = GTid.x + 1;
    neibor = sharedState[k - 1] + sharedState[k] + sharedState[k + 1];
    newState[DTid.xy] = neibor;
}

[numthreads(1, 256, 1)]
void Vmain(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    if (GTid.y < 1)
    {
		// Clamp out of bound samples that occur at image borders.
        uint y = (DTid.y - 1) % height;
        sharedState[GTid.y] = newState[uint2(DTid.x, y)];
    }
    if (GTid.y >= blocksize - 1)
    {
		// Clamp out of bound samples that occur at image borders.
        uint y = (DTid.y + 1) % height;
        sharedState[GTid.y + 2] = newState[uint2(DTid.x, y)];
    }
	
	// Clamp out of bound samples that occur at image borders.
    uint x = DTid.x % width;
    uint y = DTid.y % height;
    sharedState[GTid.y + 1] = newState[uint2(x, y)];


	// Wait for all threads to finish.
    GroupMemoryBarrierWithGroupSync();
    
    newState[DTid.xy] = oldState[DTid.xy];
    
    uint2 neibor = uint2(0, 0);
	
    for (int i = -1; i <= 1; ++i)
    {
        int k = GTid.y + 1 + i;
		
        neibor += sharedState[k];
    }
    neibor -= newState[DTid.xy];
    
    if (neibor.x == 2 || neibor.x == 3)
    {
        oldState[DTid.xy] = uint2(1, 1);
        outputRW[DTid.xy] = float4(1, 1, 1, 1);
    }
    else
    {
        oldState[DTid.xy] = uint2(0, 0);
        outputRW[DTid.xy] = float4(0, 0, 0, 1);
    }
    
}