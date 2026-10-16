struct PSInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    return float4(input.uv, 0.0, 1.0);
}
