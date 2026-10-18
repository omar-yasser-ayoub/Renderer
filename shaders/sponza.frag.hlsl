Texture2D<float4> AlbedoMap : register(t0, space1);
Texture2D<float4> NormalMap : register(t1, space1);
Texture2D<float4> RmMap     : register(t2, space1);

SamplerState AlbedoSamp : register(s3, space1);
SamplerState NormalSamp : register(s4, space1);
SamplerState RmSamp     : register(s5, space1);

cbuffer Camera : register(b0, space0)
{
    float4x4 viewProjection;
    float4   cameraPosition;
};

// 0 for directional
// 1 for point
// 2 for ambient
struct Light {
    float4 posType; //xyz for first 3 floats, and the type in the last float
    float4 dirRadius; //xyz vector for direction and the last float for radius
    float4 colorIntensity; //rgb for color, last float for intensity
};

#define MAX_LIGHTS 64
cbuffer Lights : register(b1, space0)
{
    uint lightCount;
    uint3 _pad;
    Light lights[MAX_LIGHTS];
};

cbuffer Material : register(b6, space1)
{
    float4 baseColor;
    float  metallicFactor;
    float  roughnessFactor;
    float2 _pad2;
};

static const float  PI              = 3.14159265f;

struct PSInput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD1;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

float3 PerturbNormal(PSInput input, float3 N)
{
    float3 dp1 = ddx(input.worldPos);
    float3 dp2 = ddy(input.worldPos);
    float2 duv1 = ddx(input.uv);
    float2 duv2 = ddy(input.uv);

    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    float3x3 TBN = float3x3(T * invmax, B * invmax, N);

    float3 n = NormalMap.Sample(NormalSamp, input.uv).xyz * 2.0f - 1.0f;
    return normalize(mul(n, TBN));
}

float DistributionGGX(float NdotH, float rough)
{
    float a  = rough * rough;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-4f);
}

float GeometrySchlick(float NdotX, float rough)
{
    float r = rough + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotX / (NdotX * (1.0f - k) + k);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float4 main(PSInput input) : SV_Target
{
    float4 albedoSample = AlbedoMap.Sample(AlbedoSamp, input.uv);
    float4 rmSample     = RmMap.Sample(RmSamp, input.uv);

    float3 albedo    = albedoSample.rgb * baseColor.rgb;
    float  metallic  = saturate(rmSample.b * metallicFactor);
    float  roughness = clamp(rmSample.g * roughnessFactor, 0.04f, 1.0f);

    float3 N = normalize(input.normal);
    float3 V = normalize(cameraPosition.xyz - input.worldPos);
    N = PerturbNormal(input, N);

    float3 lit = 0;
    for (uint i = 0; i < lightCount; ++i) {
        Light light = lights[i];

        if (lights[i].posType.w == 2) {
            //Ambient light
            lit += light.colorIntensity.xyz * light.colorIntensity.w;
            continue;
        }

        float3 L;
        float atten = 1.0f;
        if (lights[i].posType.w == 1) {
            //Point light
            float3 toLight = light.posType.xyz - input.worldPos;
            float dist = length(toLight);
            L = toLight / max(dist, 1e-4f);
            atten = saturate(1.0f - dist / light.dirRadius.w);
            atten *= atten;
        } else {
            //Directional light
            L = normalize(-light.dirRadius.xyz);
        }

        float3 H = normalize(V + L);
        float3 radiance = light.colorIntensity.xyz * light.colorIntensity.w;

        float NdotL = saturate(dot(N, L));
        float NdotV = saturate(dot(N, V)) + 1e-4f;
        float NdotH = saturate(dot(N, H));
        float VdotH = saturate(dot(V, H));

        float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
        float  D  = DistributionGGX(NdotH, roughness);
        float  G  = GeometrySchlick(NdotV, roughness) * GeometrySchlick(NdotL, roughness);
        float3 F  = FresnelSchlick(VdotH, F0);

        float3 specular = (D * G * F) / max(4.0f * NdotV * NdotL, 1e-4f);
        float3 kD       = (1.0f - F) * (1.0f - metallic);
        float3 diffuse  = kD * albedo / PI;

        lit += (diffuse + specular) * radiance * NdotL * atten;
    }

    lit += albedo * 0.03f;
    lit = lit / (lit + 1.0f);
    lit = pow(lit, 1.0f / 2.2f);
    return float4(lit, albedoSample.a * baseColor.a);
}
