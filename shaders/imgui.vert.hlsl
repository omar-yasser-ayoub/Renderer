struct ImGuiTransform
{
    float2 scale;
    float2 offset;
};

cbuffer TransformBuf : register(b0, space0)
{
    ImGuiTransform transform;
}

struct VSInput
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
    uint col : COLOR0;
};

struct VSOutput
{
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float4 position : SV_Position;
};

VSOutput main(VSInput input)
{
    VSOutput output;

    output.position.x = input.pos.x * transform.scale.x + transform.offset.x;
    output.position.y = input.pos.y * transform.scale.y + transform.offset.y;
    output.position.z = 0.0;
    output.position.w = 1.0;
    output.uv = input.uv;
    output.color = float4(
        (input.col & 0xFF) / 255.0,
        ((input.col >> 8) & 0xFF) / 255.0,
        ((input.col >> 16) & 0xFF) / 255.0,
        ((input.col >> 24) & 0xFF) / 255.0);

    return output;
}
