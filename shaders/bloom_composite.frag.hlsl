cbuffer Bloom : register(b0, space0)
{
    float2 texelSize; // 1 / render resolution
    float  threshold;
    float  knee;
    float  intensity;
    float _pad0;
    float _pad1;
    float _pad2;
};

Texture2D<float4> SceneColor : register(t1, space0);
Texture2D<float4> BloomTex   : register(t2, space0);
SamplerState      LinearSamp : register(s3, space0);

float4 main(float4 position : SV_Position) : SV_Target
{
    float2 uv = position.xy * texelSize;

    float3 scene = SceneColor.Sample(LinearSamp, uv).rgb;
    float3 bloom = BloomTex.Sample(LinearSamp, uv).rgb;

    float3 c = scene + bloom * intensity;
    c = c / (c + 1.0);
    c = pow(c, 1.0 / 2.2);

    return float4(c, 1.0);
}
