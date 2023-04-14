#define width 2560
#define height 1440

SamplerState Texsampler : register(s1);
Texture2D colorTex : register(t1);
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
    if (oldState[DTid.xy].x == 1)
    {
        float loc = (float) oldState[DTid.xy].y / 255.0f;
        float4 sampledColor = colorTex.SampleLevel(Texsampler, float2(loc, 0.5f), 0);
        outputRW[DTid.xy] = float4(1/113,1/28,1/145,1);
    }
    else
    {
        float loc = (float) oldState[DTid.xy].y / 255.0f;
        float4 sampledColor = colorTex.SampleLevel(Texsampler, float2(loc, 0.5f), 0);
        outputRW[DTid.xy] = sampledColor;
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
        else
        {
            uint delay = oldState[DTid.xy].y;
            if (delay == 255)
            {
                delay = 255;
            }
            else
            {
                delay++;
            }
            newState[DTid.xy] = uint2(0, delay);
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
            newState[DTid.xy] = uint2(0, 1);
        }
    }
}

[numthreads(1, 256, 1)]
void Vmain(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
   oldState[DTid.xy] = newState[DTid.xy];
}