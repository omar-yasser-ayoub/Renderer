Texture2D<float4> fontTexture : register(t0, space1);
SamplerState fontSampler : register(s1, space1);

struct PSInput
{
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float4 position : SV_Position;
};

float4 main(PSInput input) : SV_Target
{
    return fontTexture.Sample(fontSampler, input.uv) * input.color;
}
