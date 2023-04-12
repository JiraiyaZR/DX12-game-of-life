#define blocksize 256
#define width 1280
#define height 720
#define cacheSize 258

struct lifecell {
	uint state;
	uint delay;
};

groupshared lifecell sharedState[cacheSize];

RWTexture2D<float4> outputRW : register(u0);
RWStructuredBuffer<lifecell> oldState: register(u1);
RWStructuredBuffer<lifecell> newState: register(u2);

[numthreads(256, 1, 1)]
void Hmain( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID )
{
    if (GTid.x < 1)
    {
		// Clamp out of bound samples that occur at image borders.
        int x = max(DTid.x - 1, 0);
        sharedState[GTid.x] = oldState[x * height + GTid.y];
    }
    if (GTid.x >= blocksize - 1)
    {
		// Clamp out of bound samples that occur at image borders.
        int x = min(DTid.x + 1, width - 1);
        sharedState[GTid.x + 2] = oldState[x * height + DTid.y];
    }

	// Clamp out of bound samples that occur at image borders.
    int x = min(DTid.x, width - 1);
    int y = min(DTid.y, height - 1);
    sharedState[GTid.x + 1] = oldState[x * height + y];

	// Wait for all threads to finish.
    GroupMemoryBarrierWithGroupSync();

    int neibor = 0;
	int k = GTid.x + 1;
    neibor = sharedState[k - 1].state + sharedState[k + 1].state;
    newState[DTid.x * height + DTid.y].state = neibor;
}

[numthreads(1, 256, 1)]
void Vmain(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    if (GTid.y < 1)
    {
		// Clamp out of bound samples that occur at image borders.
        int y = max(DTid.y - 1, 0);
        sharedState[GTid.y] = newState[DTid.x * height + y];
    }
    if (GTid.y >= blocksize - 1)
    {
		// Clamp out of bound samples that occur at image borders.
        int y = min(DTid.y + 1, height - 1);
        sharedState[GTid.y + 2] = newState[DTid.x * height + y];
    }
	
	// Clamp out of bound samples that occur at image borders.
    int x = min(DTid.x, width - 1);
    int y = min(DTid.y, height - 1);
    sharedState[GTid.y + 1] = newState[x * height + y];


	// Wait for all threads to finish.
    GroupMemoryBarrierWithGroupSync();

    int neibor = 0;
	
    for (int i = -1; i <= 1; ++i)
    {
        int k = GTid.y + 1 + i;
		
        neibor += sharedState[k].state;
    }
    
    if (neibor == 2 || neibor == 3)
    {
        oldState[DTid.x * height + DTid.y].state = 1;
        outputRW[DTid.xy] = float4(1, 1, 1, 1);
    }
    else
    {
        oldState[DTid.x * height + DTid.y].state = 0;
        outputRW[DTid.xy] = float4(0, 0, 0, 1);
    }
    
}