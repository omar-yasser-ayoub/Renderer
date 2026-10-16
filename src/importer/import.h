#pragma once

#include "rhi/resources/buffer.h"
#include "rhi/resources/sampler.h"
#include "rhi/resources/texture.h"
#include <fastgltf/core.hpp>
#include <fastgltf/parser.hpp>
#include <glm/glm.hpp>
#include "rhi/rhi.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct Submesh {
    uint32_t firstIndex;
    uint32_t indexCount;
    int32_t baseVertex;
    uint32_t materialIdx;
};

struct Material {
    glm::vec4 baseColor = glm::vec4(1.0f);
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    uint32_t albedoTextureIdx = 0xFFFFFFFF;
    uint32_t albedoSamplerIdx = 0xFFFFFFFF;
    uint32_t normalTextureIdx = 0xFFFFFFFF;
    uint32_t normalSamplerIdx = 0xFFFFFFFF;
    uint32_t rmTextureIdx = 0xFFFFFFFF;
    uint32_t rmSamplerIdx = 0xFFFFFFFF;
};

struct Drawable {
    uint32_t submeshIdx;
    glm::mat4 transform;
};

struct ModelData {
    rhi::BufferHandle vertexBuffer;
    rhi::BufferHandle indexBuffer;
    std::vector<Submesh> primitivesList;
    std::vector<rhi::TextureViewHandle> textures;
    std::vector<rhi::SamplerHandle> samplers;
    std::vector<Material> materials;
    std::vector<Drawable> drawables;
};

ModelData loadModel(RHI& rhi, const std::string& path);
