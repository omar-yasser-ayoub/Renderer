cbuffer Bloom : register(b0, space0)
{
    float2 texelSize; // one texel step along the blur direction
    float  threshold;
    float  knee;
    float  intensity;
    float _pad0;
    float _pad1;
    float _pad2;
};

Texture2D<float4> Source : register(t1, space0);
SamplerState      LinearSamp : register(s2, space0);

static const float weights[5] =
{
    0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216
};

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float3 result = Source.Sample(LinearSamp, uv).rgb * weights[0];

    for (int i = 1; i < 5; ++i) {
        float2 offset = texelSize * float(i);
        result += Source.Sample(LinearSamp, uv + offset).rgb * weights[i];
        result += Source.Sample(LinearSamp, uv - offset).rgb * weights[i];
    }

    return float4(result, 1.0);
}
