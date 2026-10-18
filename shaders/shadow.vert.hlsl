cbuffer Shadow : register(b0, space0)
{
    float4x4 lightViewProjection;
}

struct VSInput
{
    float3 position : POSITION;
};

float4 main(VSInput input) : SV_POSITION
{
    return mul(lightViewProjection, float4(input.position, 1.0));
}
