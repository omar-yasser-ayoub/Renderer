struct VSOutput
{
    float4 position : SV_Position;
};

VSOutput main(uint id : SV_VertexID)
{
    VSOutput o;
    float2 uv = float2((id << 1u) & 2u, id & 2u);
    o.position = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    return o;
}
