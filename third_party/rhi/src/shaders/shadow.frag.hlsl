Texture2D<float> ShadowMap : register(t0, space2);
SamplerComparisonState ShadowSampler : register(s1, space2);
Texture2D<float4> Albedo : register(t2, space2);
SamplerState LinearSampler : register(s3, space2);

struct PSIn
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSIn input) : SV_Target
{
    float shadow = ShadowMap.SampleCmp(ShadowSampler, input.uv, 0.5);
    return Albedo.Sample(LinearSampler, input.uv) * shadow;
}
