Texture2D<float4> Albedo : register(t0, space2);
SamplerState LinearSampler : register(s1, space2);
RWTexture2D<float4> Output : register(u2, space3);

float4 main() : SV_Target
{
    float4 color = Albedo.Sample(LinearSampler, float2(0.5, 0.5));
    Output[uint2(0, 0)] = color;
    return color;
}
