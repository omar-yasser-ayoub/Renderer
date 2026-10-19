Texture2D<float> AOTexture : register(t0, space0);

float4 main(float4 position : SV_Position) : SV_Target
{
    float ao = AOTexture.Load(int3(position.xy, 0));
    return float4(ao, ao, ao, 1.0);
}
