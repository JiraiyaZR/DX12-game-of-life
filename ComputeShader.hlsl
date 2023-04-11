#define blocksize 64
#define width 1280
#define height 720
#define cacheSize 80

struct lifecell {
	uint state;
	uint delay;
};

groupshared lifecell sharedHState[8][10];
groupshared lifecell sharedVState[10][8];

RWTexture2D<float4> outputRW : register(u0);
RWStructuredBuffer<lifecell> oldState: register(u1);
RWStructuredBuffer<lifecell> newState: register(u2);

[numthreads(8, 8, 1)]
void main( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID )
{
    /*
	uint xl = max(DTid.x - 1, 0);
	uint yl = max(DTid.y - 1, 0);
	uint xr = min(DTid.x + 1, width - 1);
	uint yr = min(DTid.y + 1, height - 1);

    // 按行存储
	sharedHState[GTid.x + 1][GTid.y] = oldState[DTid.x + DTid.y * width];
	if (GTid.x == 0) {
		sharedHState[GTid.x][GTid.y] = oldState[xl + DTid.y * width];
	}
	if (GTid.x == 7) {
		sharedHState[GTid.x + 2][GTid.y] = oldState[xr + DTid.y * width];
	}
    AllMemoryBarrierWithGroupSync();
	
    for (int i = -1; i <= 1; i++)
    {
        newState[DTid.x + DTid.y * width].state += sharedHState[GTid.x + i + 1][GTid.y].state;
    }
	
    newState[DTid.x + DTid.y * width].state -= sharedHState[GTid.x + 1][GTid.y].state;

    AllMemoryBarrierWithGroupSync();
	
    // 按列存储
    sharedVState[GTid.x][GTid.y + 1] = newState[DTid.x + DTid.y * width];
	
    if (GTid.y == 0)
    {
        sharedVState[GTid.x][GTid.y] = newState[DTid.x + yl * width];
    }
    if (GTid.y == 7)
    {
        sharedVState[GTid.x][GTid.y + 2] = newState[DTid.x + yr * width];
    }
    AllMemoryBarrierWithGroupSync();
    
    for (int j = -1; j <= 1; j++)
    {
        newState[DTid.x + DTid.y * width].state += sharedVState[GTid.x][GTid.y + i + 1].state;
    }
    
    bool s1 = newState[DTid.x + DTid.y * width].state & 2;
    bool s2 = newState[DTid.x + DTid.y * width].state & 3;
    newState[DTid.x + DTid.y * width].state = s1 | s2;

    if (newState[DTid.x + DTid.y * width].state == 0)
    {
        outputRW[DTid.xy] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    else
    {
        outputRW[DTid.xy] = float4(1.0f, 1.0f, 1.0f, 1.0f);
    }*/
    if (newState[DTid.x*height/2 + DTid.y].state == 0)
    {
        outputRW[DTid.xy] = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    else
    {
        outputRW[DTid.xy] = float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
}