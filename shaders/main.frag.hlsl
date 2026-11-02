#define MAX_LIGHTS 64
#define NUM_SHADOWS 64
#define SHADOW_BIAS 0.0025f

static const float  PI = 3.14159265f;

struct PSInput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD1;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
};

// 0 for directional
// 1 for point
// 2 for ambient
struct Light {
    float4 posType; //xyz for first 3 floats, and the type in the last float
    float4 dirRadius; //xyz vector for direction and the last float for radius
    float4 colorIntensity; //rgb for color, last float for intensity
    int    shadowIndex; // index into the shadow map array, -1 = no shadow
    int    _pad0;
    int    _pad1;
    int    _pad2;
};

cbuffer Camera : register(b0, space0)
{
    float4x4 viewProjection;
    float4   cameraPosition;
};

cbuffer Lights : register(b1, space0)
{
    uint lightCount;
    uint _lightPad1;
    uint _lightPad2;
    Light lights[MAX_LIGHTS];
};

cbuffer Shadow : register(b2, space0)
{
    float4x4 lightViewProj[NUM_SHADOWS];
};

cbuffer Material : register(b6, space1)
{
    float4 baseColor;
    float  metallicFactor;
    float  roughnessFactor;
    float2 _pad2;
};

Texture2D<float> AOTexture           : register(t5, space0);

Texture2DArray ShadowMaps            : register(t3, space0);
Texture2D<float4> AlbedoMap          : register(t0, space1);
Texture2D<float4> NormalMap          : register(t1, space1);
Texture2D<float4> RmMap              : register(t2, space1);

SamplerState AlbedoSamp              : register(s3, space1);
SamplerState NormalSamp              : register(s4, space1);
SamplerState RmSamp                  : register(s5, space1);
SamplerComparisonState ShadowSampler : register(s4, space0);

float3 CalculateNormal(PSInput input, float3 normal) {
    // Using screen-space derivatives, calculate the TBN matrix, and from that calculate the final normal position

    // 2 candidate derivates (y and x direction in screenspace) are derived. We know that this derivative = T (du) + B (dv)
    float3 dp1 = ddx(input.worldPos);
    float3 dp2 = ddy(input.worldPos);

    // The derivative in uv in both directions in screenspace
    float2 duv1 = ddx(input.uv);
    float2 duv2 = ddy(input.uv);

    float3 N = normalize(input.normal);

    // derived from Followup: Normal Mapping Without Precomputed Tangents by Christian Schüler, I have no clue how this math works.
    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = rsqrt(max(max(dot(T, T), dot(B, B)), 1e-20));
    float3x3 TBN = float3x3(T * invmax, B * invmax, N);

    // Multiply and return the normal
    return normalize(mul(normal, TBN));
}

float NormalDistribution(float3 N, float3 H, float a) {
    // derived from Trowbridge-Reitz GGX normal distribution
    float a2 = a*a;
    float NdotH = max(dot(N,H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float k)
{
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float k)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx1 = GeometrySchlickGGX(NdotV, k);
    float ggx2 = GeometrySchlickGGX(NdotL, k);

    return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    // Schlick's approximation of the fresnel equation
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float ShadowFactor(int shadowIdx, float3 pos)
{
    if (shadowIdx < 0) return 1.0;

    float4 lightPos = mul(lightViewProj[shadowIdx], float4(pos, 1.0));
    float3 proj = lightPos.xyz / lightPos.w;
    float2 uv = proj.xy * 0.5 + 0.5;

    if (any(uv < 0.0) || any(uv > 1.0) || proj.z > 1.0) return 1.0;

    return ShadowMaps.SampleCmpLevelZero(ShadowSampler, float3(uv, shadowIdx), proj.z - SHADOW_BIAS);
}

float4 main(PSInput input) : SV_Target
{
    // Sample normal
    float3 sampledNormal = NormalMap.Sample(NormalSamp, input.uv).rgb;

    // Sample albedo
    float3 albedo = AlbedoMap.Sample(AlbedoSamp, input.uv).rgb * baseColor.rgb;

    // Sample roughness/metallicness
    float3 rm = RmMap.Sample(RmSamp, input.uv).rgb;

    //Sample AO

    float ao = AOTexture.Load(int3(input.position.xy, 0));

    // Remap from [0,1] to [-1, -1]
    sampledNormal = sampledNormal * 2.0 - 1.0;

    // Calculate the final world-space normal vector accounting for the TBN matrix
    float3 normal = CalculateNormal(input, sampledNormal);

    float3 viewDir = normalize(cameraPosition.xyz - input.worldPos);

    float3 LightOut = 0.0;

    float roughness = max(rm.g * roughnessFactor, 0.04);
    float metallic  = rm.b * metallicFactor;
    float3 F0 = lerp(0.04, albedo, metallic);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float NdotV = max(dot(normal, viewDir), 1e-4);

    for (uint i = 0; i < lightCount; i++) {
        Light light = lights[i];

        float3 L = float3(0.1, 0.1, 0.1);
        float3 radiance = float3(0.1, 0.1, 0.1);

        if (light.posType.w == 2) {
            //ambient light
            LightOut += light.colorIntensity.rgb * light.colorIntensity.w * albedo * (1.0 - metallic) * ao;
            continue;
        }

        if (light.posType.w == 0) {
            //directional light
            L = normalize(-light.dirRadius.xyz);
            radiance = light.colorIntensity.rgb * light.colorIntensity.w;

            radiance *= ShadowFactor(light.shadowIndex, input.worldPos);
        }
        if (light.posType.w == 1) {
            //point light
            float3 nonnormalized = light.posType.xyz - input.worldPos;
            float distance = length(nonnormalized);

            if (distance > light.dirRadius.w) {
                //this point light is too far, don't take its contribution
                continue;
            }
            L = nonnormalized / distance;

            float attenuation = 1.0 / (distance * distance);

            radiance = light.colorIntensity.rgb * light.colorIntensity.w * attenuation;

            //no shadows for now on point lights
        }
        //compute the half vector between the light and view directions

        float3 H = normalize(L + viewDir);

        float NdotL = max(dot(normal, L), 0.0);

        if (NdotL <= 0.0) {
            continue;
        }

        float3 F = fresnelSchlick(max(dot(H, viewDir), 0.0), F0);
        float D = NormalDistribution(normal, H, roughness * roughness);
        float G = GeometrySmith(normal, viewDir, L, k);

        float3 specular = (D * G * F) /
            max(4.0 * NdotV * NdotL, 0.0001);

        float3 kd = (1.0 - F) * (1.0 - metallic);
        float3 diffuse = kd * albedo / PI;

        LightOut += (diffuse + specular) * radiance * NdotL;
    }

    return float4(LightOut, 1.0);
}
