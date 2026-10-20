#include "import.h"
#include "fastgltf/types.hpp"
#include "fastgltf/util.hpp"
#include "rhi/resources/buffer.h"
#include "rhi/resources/sampler.h"
#include "rhi/resources/texture.h"
#include <cstddef>
#include <variant>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "fastgltf/glm_element_traits.hpp"


ModelData loadModel(RHI& rhi, const std::string& path)
{
    ModelData res;

    fastgltf::Parser parser;

#if defined(FASTGLTF_HAS_MEMORY_MAPPED_FILE)
    auto file = fastgltf::MappedGltfFile::FromPath(path);
#else
    auto file = fastgltf::GltfDataBuffer::FromPath(path);
#endif
    if (!file) {
        return res;
    }

    auto parentPath = std::filesystem::path(path).parent_path();

    auto parsed = parser.loadGltf(file.get(),
        parentPath,
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages);

    if (!parsed) {
        return res;
    }

    auto& asset = parsed.get();

    const auto& images = asset.images;
    const auto& samplers = asset.samplers;
    const auto& materials = asset.materials;
    const auto& meshes = asset.meshes;

    std::vector<rhi::TextureViewHandle> textureViewList;
    std::vector<rhi::SamplerHandle> samplerList;
    std::vector<Material> materialList;
    std::vector<Submesh> primitivesList;
    std::vector<Drawable> drawableList;
    std::vector<uint32_t> indexList;
    std::vector<Vertex> vertList;

    for (const auto& sampler : samplers) {
        rhi::SamplerDesc desc;

        if (sampler.magFilter.has_value()) {
            switch (sampler.magFilter.value()) {
                case fastgltf::Filter::Linear:
                    desc.magFilter = rhi::Filter::Linear;
                    break;
                case fastgltf::Filter::Nearest:
                    desc.magFilter = rhi::Filter::Nearest;
                    break;
                case fastgltf::Filter::NearestMipMapLinear:
                    desc.magFilter = rhi::Filter::Nearest;
                    desc.mipFilter = rhi::MipmapFilter::Linear;
                    break;
                case fastgltf::Filter::NearestMipMapNearest:
                    desc.magFilter = rhi::Filter::Nearest;
                    desc.mipFilter = rhi::MipmapFilter::Nearest;
                    break;
                case fastgltf::Filter::LinearMipMapLinear:
                    desc.magFilter = rhi::Filter::Linear;
                    desc.mipFilter = rhi::MipmapFilter::Linear;
                    break;
                case fastgltf::Filter::LinearMipMapNearest:
                    desc.magFilter = rhi::Filter::Linear;
                    desc.mipFilter = rhi::MipmapFilter::Nearest;
                    break;
            }
        }

        if (sampler.minFilter.has_value()) {
            switch (sampler.minFilter.value()) {
                case fastgltf::Filter::Linear:
                    desc.minFilter = rhi::Filter::Linear;
                    break;
                case fastgltf::Filter::Nearest:
                    desc.minFilter = rhi::Filter::Nearest;
                    break;
                case fastgltf::Filter::NearestMipMapLinear:
                    desc.minFilter = rhi::Filter::Nearest;
                    desc.mipFilter = rhi::MipmapFilter::Linear;
                    break;
                case fastgltf::Filter::NearestMipMapNearest:
                    desc.minFilter = rhi::Filter::Nearest;
                    desc.mipFilter = rhi::MipmapFilter::Nearest;
                    break;
                case fastgltf::Filter::LinearMipMapLinear:
                    desc.minFilter = rhi::Filter::Linear;
                    desc.mipFilter = rhi::MipmapFilter::Linear;
                    break;
                case fastgltf::Filter::LinearMipMapNearest:
                    desc.minFilter = rhi::Filter::Linear;
                    desc.mipFilter = rhi::MipmapFilter::Nearest;
                    break;
            }
        } else {
            desc.minFilter = rhi::Filter::Linear;
        }

        switch (sampler.wrapS) {
            case fastgltf::Wrap::Repeat:
                desc.addressU = rhi::AddressMode::Repeat;
                break;
            case fastgltf::Wrap::ClampToEdge:
                desc.addressU = rhi::AddressMode::ClampToEdge;
                break;
            case fastgltf::Wrap::MirroredRepeat:
                desc.addressU = rhi::AddressMode::MirrorRepeat;
                break;
            default:
                desc.addressU = rhi::AddressMode::Repeat;
        }

        switch (sampler.wrapT) {
            case fastgltf::Wrap::Repeat:
                desc.addressV = rhi::AddressMode::Repeat;
                break;
            case fastgltf::Wrap::ClampToEdge:
                desc.addressV = rhi::AddressMode::ClampToEdge;
                break;
            case fastgltf::Wrap::MirroredRepeat:
                desc.addressV = rhi::AddressMode::MirrorRepeat;
                break;
            default:
                desc.addressV = rhi::AddressMode::Repeat;
        }

        rhi::SamplerHandle samplerHandle = rhi.CreateSampler(desc);
        samplerList.push_back(samplerHandle);
    }

    // we do a prepass to determine each image's usage to know if we should make it into srgb (for albedo textures) or linear (for normal/rm textures)
    std::vector<bool> imageIsSrgb(images.size(), false);

    for (const auto& material : materials) {
        auto markTexture = [&](const auto& textureInfo, bool srgb) {
            if (!textureInfo.has_value())
                return;

            const auto& texture = asset.textures[textureInfo->textureIndex];

            if (texture.imageIndex.has_value())
                imageIsSrgb[texture.imageIndex.value()] = imageIsSrgb[texture.imageIndex.value()] || srgb;
        };

        markTexture(material.pbrData.baseColorTexture, true);
        markTexture(material.emissiveTexture, true);

        markTexture(material.normalTexture, false);
        markTexture(material.pbrData.metallicRoughnessTexture, false);
        markTexture(material.occlusionTexture, false);
    }

    for (size_t i = 0; i < images.size(); ++i) {

        const std::byte* bytes = nullptr; size_t len = 0;

        std::visit(fastgltf::visitor{
          [&](auto&) {},
          [&](const fastgltf::sources::Array& a)    { bytes = a.bytes.data(); len = a.bytes.size(); },
          [&](const fastgltf::sources::ByteView& b) { bytes = b.bytes.data(); len = b.bytes.size(); },
        }, images[i].data);

        if (!bytes) {
            textureViewList.push_back({});
            continue;
        }

        int width = 0, height = 0;
        auto* picture = stbi_load_from_memory((const stbi_uc*)bytes, (int)len, &width, &height, nullptr, 4);
        if (!picture) {
            textureViewList.push_back({});
            continue;
        }

        rhi::TextureDesc desc;
        desc.width = width;
        desc.height = height;
        desc.format = imageIsSrgb[i]
            ? rhi::TextureFormat::RGBA8UnormSrgb
            : rhi::TextureFormat::RGBA8Unorm;
        desc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst;
        auto texture = rhi.CreateTexture(desc);

        rhi.UploadTexture(texture, 0, picture, (uint32_t)width, (uint32_t)height);
        stbi_image_free(picture);

        rhi::TextureViewDesc viewDesc;
        viewDesc.format = desc.format;
        textureViewList.push_back(rhi.CreateTextureView(viewDesc, texture));
    }

    for (const auto& material : materials) {
        Material mat;
        const auto& pbrData = material.pbrData;
        const auto tempColor = pbrData.baseColorFactor;
        mat.baseColor = glm::vec4(tempColor.x(), tempColor.y(), tempColor.z(), tempColor.w());
        mat.metallicFactor = pbrData.metallicFactor;
        mat.roughnessFactor = pbrData.roughnessFactor;
        if (pbrData.baseColorTexture.has_value()) {
          auto& tex = asset.textures[material.pbrData.baseColorTexture->textureIndex];
          if (tex.imageIndex.has_value())
            mat.albedoTextureIdx = (uint32_t)tex.imageIndex.value();
          if (tex.samplerIndex.has_value())
            mat.albedoSamplerIdx = (uint32_t)tex.samplerIndex.value();
        }
        if (material.normalTexture.has_value()) {
          auto& tex = asset.textures[material.normalTexture->textureIndex];
          if (tex.imageIndex.has_value())
            mat.normalTextureIdx = (uint32_t)tex.imageIndex.value();
          if (tex.samplerIndex.has_value())
            mat.normalSamplerIdx = (uint32_t)tex.samplerIndex.value();
        }
        if (material.pbrData.metallicRoughnessTexture.has_value()) {
          auto& tex = asset.textures[material.pbrData.metallicRoughnessTexture->textureIndex];
          if (tex.imageIndex.has_value())
            mat.rmTextureIdx = (uint32_t)tex.imageIndex.value();
          if (tex.samplerIndex.has_value())
            mat.rmSamplerIdx = (uint32_t)tex.samplerIndex.value();
        }

        materialList.push_back(mat);
    }

    const uint32_t defaultMatIdx = (uint32_t)materialList.size();
    materialList.push_back(Material{});

    std::vector<std::pair<uint32_t, uint32_t>> meshList;
    meshList.reserve(meshes.size());

    for (size_t mi = 0; mi < meshes.size(); ++mi) {

        const auto& mesh = meshes[mi];
        const uint32_t rangeStart = (uint32_t)primitivesList.size();

        for (const auto& primitive: mesh.primitives) {
            if (primitive.type != fastgltf::PrimitiveType::Triangles) continue;
            Submesh outMesh;
            outMesh.materialIdx = (uint32_t)primitive.materialIndex.value_or(defaultMatIdx);
            outMesh.baseVertex = (int32_t)vertList.size();
            outMesh.firstIndex = (uint32_t)indexList.size();

            auto positionIt = primitive.findAttribute("POSITION");
            if (positionIt == primitive.attributes.end())
                continue;

            const auto& posAccessor = asset.accessors[positionIt->accessorIndex];

            std::vector<glm::vec3> positions(posAccessor.count);
            std::vector<glm::vec3> normals(posAccessor.count, glm::vec3(0.0f));
            std::vector<glm::vec2> uvs(posAccessor.count, glm::vec2(0.0f));

            fastgltf::copyFromAccessor<glm::vec3>(
                asset, posAccessor, positions.data()
            );

            auto normalIt = primitive.findAttribute("NORMAL");
            if (normalIt != primitive.attributes.end()) {
                const auto& accessor = asset.accessors[normalIt->accessorIndex];

                fastgltf::copyFromAccessor<glm::vec3>(
                    asset, accessor, normals.data()
                );
            }

            auto uvIt = primitive.findAttribute("TEXCOORD_0");
            if (uvIt != primitive.attributes.end()) {
                const auto& accessor = asset.accessors[uvIt->accessorIndex];

                fastgltf::copyFromAccessor<glm::vec2>(
                    asset, accessor, uvs.data()
                );
            }

            for (size_t i = 0; i < positions.size(); ++i)
                vertList.push_back(Vertex{ positions[i], normals[i], uvs[i] });

            if (primitive.indicesAccessor.has_value()) {
                const auto& idxAcc = asset.accessors[primitive.indicesAccessor.value()];
                const size_t start = indexList.size();
                indexList.resize(start + idxAcc.count);
                fastgltf::copyFromAccessor<std::uint32_t>(asset, idxAcc, indexList.data() + start);
            } else {
                for (uint32_t i = 0; i < (uint32_t)positions.size(); ++i)
                    indexList.push_back(i);
            }

            outMesh.indexCount = (uint32_t)indexList.size() - outMesh.firstIndex;
            primitivesList.push_back(outMesh);
        }
        meshList.emplace_back(rangeStart, (uint32_t)primitivesList.size() - rangeStart);
    }

    // with our vertex and index lists, we can create their buffers!
    rhi::BufferDesc vertDesc;
    vertDesc.size = vertList.size() * sizeof(Vertex);
    vertDesc.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst;
    vertDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
    rhi::BufferDesc indexDesc;
    indexDesc.size = indexList.size() * sizeof(uint32_t);
    indexDesc.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst;
    indexDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;

    rhi::BufferHandle vertBuffer = rhi.CreateBuffer(vertDesc);
    rhi::BufferHandle indexBuffer = rhi.CreateBuffer(indexDesc);

    rhi.WriteBuffer(vertBuffer, vertList.data(), vertList.size() * sizeof(Vertex));
    rhi.WriteBuffer(indexBuffer, indexList.data(), indexList.size() * sizeof(uint32_t));

    if (!asset.scenes.empty()) {
        const size_t sceneIdx = asset.defaultScene.value_or(0);
        fastgltf::iterateSceneNodes(asset, sceneIdx, fastgltf::math::fmat4x4(),
            [&](fastgltf::Node& node, const fastgltf::math::fmat4x4& world) {
                if (!node.meshIndex.has_value() || node.meshIndex.value() >= meshList.size())
                    return;
                glm::mat4 transform(1.0f);
                for (int c = 0; c < 4; ++c)
                    for (int r = 0; r < 4; ++r)
                        transform[c][r] = world[c][r];
                const auto [first, count] = meshList[node.meshIndex.value()];
                for (uint32_t j = 0; j < count; ++j)
                    drawableList.push_back(Drawable{ first + j, transform });
            });
    }

    res.materials = materialList;
    res.textures = textureViewList;
    res.samplers = samplerList;
    res.primitivesList = primitivesList;
    res.drawables = drawableList;
    res.vertexBuffer = vertBuffer;
    res.indexBuffer = indexBuffer;

    return res;
}
