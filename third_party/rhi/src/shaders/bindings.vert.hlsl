cbuffer Camera : register(b0)
{
    float4x4 viewProjection;
};

StructuredBuffer<float4> Positions : register(t1, space1);

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(viewProjection, float4(input.position, 1.0));
    output.uv = input.uv + Positions[0].xy;
    return output;
}
