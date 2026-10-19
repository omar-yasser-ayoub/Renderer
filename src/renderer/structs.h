#include "rhi/resources/texture.h"
#include <cstdint>
#include <glm/glm.hpp>

struct MaterialUBO {
    glm::vec4 baseColor;
    float metallicFactor;
    float roughnessFactor;
    float _pad[2];
};

struct CameraUBO {
    glm::mat4 viewProjMatrix;
    glm::vec4 position;
};

struct ModelUBO {
    glm::mat4 modelMatrix;
};

struct ShadowPrepassUBO {
    glm::mat4 lightViewProjMatrix;
};

struct AOUBO {
    glm::mat4 invProjection;
    glm::vec2 resolution;
    float radius;
    float strength;
};

constexpr uint32_t MAX_SHADOWS = 64;

struct ShadowData {
    glm::mat4 lightViewProj[MAX_SHADOWS];
};

constexpr uint32_t MAX_LIGHTS = 64;

struct Light {
    glm::vec4 posType;        // xyz = world position (point), w = type
    glm::vec4 dirRadius;      // xyz = direction (directional), w = range (point)
    glm::vec4 colorIntensity; // rgb = color, w = intensity
    int shadowIndex = -1;     // index into ShadowData::lightViewProj, -1 = no shadow
    int _pad[3] = {0, 0, 0};
};

struct LightData {
    uint32_t lightCount;
    uint32_t _pad[3];
    Light lights[MAX_LIGHTS];
};
