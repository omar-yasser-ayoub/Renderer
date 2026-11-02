cbuffer Camera : register(b0, space0)
{
    float4x4 viewProjection;
    float4   cameraPosition;
};

cbuffer Object : register(b0, space2)
{
    float4x4 model;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD1;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput o;
    float4 world = mul(model, float4(input.position, 1.0));
    o.worldPos = world.xyz;
    o.position = mul(viewProjection, world);
    o.normal   = mul((float3x3)model, input.normal);
    o.uv       = input.uv;
    return o;
}
