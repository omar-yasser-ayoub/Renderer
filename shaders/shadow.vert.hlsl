cbuffer Shadow : register(b0, space0)
{
    float4x4 lightViewProjection;
}

cbuffer Object : register(b0, space1)
{
    float4x4 model;
};

struct VSInput
{
    float3 position : POSITION;
};

float4 main(VSInput input) : SV_POSITION
{
    float4 world = mul(model, float4(input.position, 1.0));
    return mul(lightViewProjection, world);
}
