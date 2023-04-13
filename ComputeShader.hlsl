#define width 2560
#define height 1440

RWTexture2D<float4> outputRW : register(u0);
RWTexture2D<uint2> oldState : register(u1);
RWTexture2D<uint2> newState : register(u2);



bool IsAlive(uint x, uint y)
{
    /*
        if (x < 0) return false;
    if (x > width - 1)
        return false;
    if (y < 0)
        return false;
    if (y > height-1)
        return false;
*/
    return oldState[uint2(x % width, y % height)].x == 1;
}

[numthreads(256, 1, 1)]
void Hmain( uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID )
{    
    float4 white = float4(1, 1, 1, 1);
    float4 black = float4(0, 0, 0, 1);
    if (oldState[DTid.xy].x == 1)
    {
        outputRW[DTid.xy] = white;
    }
    else
    {
        outputRW[DTid.xy] = black;
    }
    
    
    uint neibor = 0;
    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            if (i == 0 && j == 0)
                continue;
            if (IsAlive(DTid.x + i, DTid.y+j))
                neibor++;
        }
    }
    if (oldState[DTid.xy].x == 0)
    {
        if (neibor == 3)
        {
            newState[DTid.xy] = uint2(1, 0);
        }
    }
    else
    {
        if (neibor == 2 || neibor==3)
        {
            newState[DTid.xy] = uint2(1, 0);
        }
        else
        {
            uint delay = newState[DTid.xy].y >> 1;
            newState[DTid.xy] = uint2(0, delay);
        }
    }
   
}

[numthreads(1, 256, 1)]
void Vmain(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
   oldState[DTid.xy] = newState[DTid.xy];
}