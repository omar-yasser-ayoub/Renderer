// NOTE: single-channel float (R32Float), not float4. R32Float is one of the
// few formats supporting read_write storage access on all backends
// (D3D12 typed UAV loads exclude RGBA8). RWTexture is always read_write.
RWTexture2D<float> Output : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    Output[dispatchThreadId.xy] = 1.0;
}
