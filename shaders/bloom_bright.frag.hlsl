cbuffer Bloom : register(b0, space0)
{
    float2 texelSize;
    float  threshold;
    float  knee;
    float  intensity;
    float _pad0;
    float _pad1;
    float _pad2;
};

Texture2D<float4> SceneColor : register(t1, space0);
SamplerState      LinearSamp : register(s2, space0);

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
    float3 c = SceneColor.Sample(LinearSamp, uv).rgb;

    float brightness = max(c.r, max(c.g, c.b));
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-4);
    float contribution = max(soft, brightness - threshold) / max(brightness, 1e-4);

    return float4(c * contribution, 1.0);
}
