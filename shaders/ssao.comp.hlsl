cbuffer GTAO : register(b0, space0)
{
    float4x4 invProjection;
    float2 resolution;
    float radius;
    float strength;
};

Texture2D<float> DepthTex : register(t1, space0);
RWTexture2D<float> AOOut : register(u2, space0);

float3 reconstructViewPos(uint2 px)
{
    float depth = DepthTex.Load(int3(px, 0));
    float2 uv = (float2(px) + 0.5) / resolution;
    float2 ndc = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

    float4 viewPos = mul(
        invProjection,
        float4(ndc, depth, 1.0)
    );

    return viewPos.xyz / viewPos.w;
}

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint2 px = id.xy;
    if (px.x >= resolution.x || px.y >= resolution.y)
        return;

    float depth = DepthTex.Load(int3(px, 0));

    if (depth >= 1.0) {
        // was never set a value, occlusion value must be 1.0
        AOOut[px] = 1.0;
        return;
    }

    float3 pos = reconstructViewPos(px);

    // normals from neighbouring view-space positions
    uint2 maxPx = uint2(resolution) - 1u;
    uint2 plPx = uint2(max(px.x, 1u) - 1u, px.y);
    uint2 prPx = uint2(min(px.x + 1u, maxPx.x), px.y);
    uint2 puPx = uint2(px.x, max(px.y, 1u) - 1u);
    uint2 pdPx = uint2(px.x, min(px.y + 1u, maxPx.y));

    float dl = DepthTex.Load(int3(plPx, 0));
    float dr = DepthTex.Load(int3(prPx, 0));
    float du = DepthTex.Load(int3(puPx, 0));
    float dd = DepthTex.Load(int3(pdPx, 0));

    bool validL = dl < 1.0;
    bool validR = dr < 1.0;
    bool validU = du < 1.0;
    bool validD = dd < 1.0;

    // Use one-sided differences where a neighbour is sky so silhouettes don't
    // poison the normal.
    float3 n = float3(0, 0, 1);
    if ((validL || validR) && (validU || validD))
    {
        float3 dx = (validL && validR)
            ? reconstructViewPos(prPx) - reconstructViewPos(plPx)
            : (validR ? reconstructViewPos(prPx) - pos : pos - reconstructViewPos(plPx));
        float3 dy = (validU && validD)
            ? reconstructViewPos(pdPx) - reconstructViewPos(puPx)
            : (validD ? reconstructViewPos(pdPx) - pos : pos - reconstructViewPos(puPx));

        float3 candidate = cross(dx, dy);
        float len = length(candidate);
        if (dot(dx, dx) > 1e-12 && dot(dy, dy) > 1e-12 && len > 1e-6)
            n = candidate / len;
    }

    if (dot(n, -pos) < 0.0) n = -n; // view-space camera is at the origin

    float3 T = normalize(cross(n, abs(n.y) < 0.99 ? float3(0, 1, 0) : float3(1, 0, 0)));
    float3 B = cross(n, T);

    float2 projScale = float2(1.0 / invProjection[0][0],
    1.0 / invProjection[1][1]);

    const int DIRS  = 8;
    const int STEPS = 6;
    float bias = 0.02 * max(1.0, -pos.z * 0.1);
    float hash = frac(sin(dot(float2(px), float2(12.9898, 78.233))) * 43758.5453);

    float occlusion = 0.0;

    for (int d = 0; d < DIRS; ++d)
    {
        float a = (float(d) + hash) / DIRS * 6.2831853;
        float3 dir = cos(a) * T + sin(a) * B;

        for (int s = 1; s <= STEPS; ++s)
        {
            float t  = float(s) / STEPS;
            float3 sp = pos + dir * (radius * t);

            // project the view-space sample point to screen
            float2 sndc = float2(sp.x * projScale.x, sp.y * projScale.y) / -sp.z;
            float2 suv  = float2(sndc.x * 0.5 + 0.5, 0.5 - sndc.y * 0.5);
            if (any(suv < 0.0) || any(suv >= 1.0)) continue;

            uint2 spx = min(uint2(suv * resolution), maxPx);
            float sd = DepthTex.Load(int3(spx, 0));
            if (sd >= 1.0) continue;

            float4 sv = mul(invProjection, float4(sndc, sd, 1.0));
            float3 spos = sv.xyz / sv.w;

            // view space looks down -Z, so closer means more negative
            float dz = sp.z - spos.z;
            if (dz > bias && dz < radius * 2.0)
                occlusion += 1.0 - t; // weight nearer samples more
        }
    }

    occlusion /= DIRS * STEPS;
    AOOut[px] = saturate(1.0 - occlusion * strength);
}
