SamplerState Texsampler : register(s1);
Texture2D colorTex : register(t1);
RWTexture2D<float4> outputRW : register(u0);
RWTexture2D<uint2> oldState : register(u1);
RWTexture2D<uint2> newState : register(u2);

cbuffer cb : register(b0)
{
    uint width;
    uint height;
    float board;
    uint rule;
}

//判断二维平面上cell的死活
bool IsAlive(uint x, uint y)
{
    // 通过注释不同的条件语句实现不同的边界情况
    if (x < 0)
        return false;
    if (x > width - 1)
        return false;
    if (y < 0)
        return false;
    if (y > height - 1)
        return false;
    return oldState[uint2(x % width, y % height)].x == 1;
}

// 底部一维元胞自动机死活判断
bool IsAlive1D(uint x, uint y)
{
    // 边界外细胞死活等同于边界处
    if (x < 0)
        return oldState[uint2(x + 1, y)].x == 1;
    if (x > width - 1)
        return oldState[uint2(x - 1, y)].x == 1;
    return oldState[uint2(x, y)].x == 1;
}

// 更新死活状态
void update(uint neibor, uint2 loc)
{
    if (oldState[loc].x == 0)
    {
        if (neibor == 3)
        {
            newState[loc] = uint2(1, 0);
        }
        else
        {
            //延迟用于控制其呈现不同颜色
            uint delay = oldState[loc].y;
            if (delay == 9364)
            {
                delay = 9364;
            }
            else
            {
                delay++;
            }
            newState[loc] = uint2(0, delay);
        }
    }
    else
    {
        if (neibor == 2 || neibor == 3)
        {
            newState[loc] = uint2(1, 0);
        }
        else
        {
            newState[loc] = uint2(0, 1);
        }
    }
}

[numthreads(256, 1, 1)]
void CSmain(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    // 展示上一帧计算的图像
    float loc = (float) oldState[DTid.xy].y / 9363.0f;
    float4 sampledColor = colorTex.SampleLevel(Texsampler, float2(loc, 0.5f), 0);
    outputRW[DTid.xy] = sampledColor;
    
    // 边界上方的应用b23规则
    if (DTid.y < board * height)
    {
        uint neibor = 0;
        for (int i = -1; i <= 1; i++)
        {
            for (int j = -1; j <= 1; j++)
            {
                if (i == 0 && j == 0)
                    continue;
                if (IsAlive(DTid.x + i, DTid.y + j))
                    neibor++;
            }
        }
        update(neibor, DTid.xy);
    }
    // 最底部应用一维规则
    else if (DTid.y == height - 1)
    {
        uint ruleList[8];
        for (uint j = 0; j < 8; j++)
        {
            ruleList[j] = (rule >> j) & 0x01;
        }
            
        uint3 exponent = uint3(1, 2, 4);
        uint index = 0;
        for (int i = -1; i <= 1; i++)
        {
            index += IsAlive1D(DTid.x + i, DTid.y) * exponent[i + 1];
        }
        uint life = ruleList[index];
        update(life * 3, DTid.xy);
    }
    //其余的部分通过上移得到
    else
    {
        newState[DTid.xy] = oldState[uint2(DTid.x, DTid.y + 1)];
    }
    
}

[numthreads(256, 1, 1)]
void stateUpdate(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    // 更新状态
    oldState[DTid.xy] = newState[DTid.xy];
}