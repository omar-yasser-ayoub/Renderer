#pragma once
#include "rhi/resources/buffer.h"
#include "rhi/resources/sampler.h"
#include "rhi/resources/shader.h"
#include "rhi/resources/texture.h"
#include <vector>
namespace rhi {
    struct BindGroupEntry
    {
        uint32_t binding;
        BufferHandle buffer;
        TextureViewHandle textureView;
        SamplerHandle sampler;
    };

    struct BindGroupLayoutDesc {
        std::vector<ShaderBinding> shaderBindings;
    };

    inline std::vector<BindGroupLayoutDesc> BuildBindGroupLayoutDescs(
        const std::vector<ShaderReflection>& reflections)
    {
        std::vector<BindGroupLayoutDesc> layouts;
        for (const auto& reflection : reflections)
        {
            for (const auto& binding : reflection.bindings)
            {
                BindGroupLayoutDesc* target = nullptr;
                for (auto& layout : layouts)
                {
                    if (!layout.shaderBindings.empty() &&
                        layout.shaderBindings[0].set == binding.set)
                    {
                        target = &layout;
                        break;
                    }
                }
                if (target == nullptr)
                {
                    layouts.push_back(BindGroupLayoutDesc{});
                    target = &layouts.back();
                }
                bool merged = false;
                for (auto& existing : target->shaderBindings)
                {
                    if (existing.binding == binding.binding)
                    {
                        existing.stage = existing.stage | binding.stage;
                        existing.depthSampled = existing.depthSampled || binding.depthSampled;
                        existing.comparison = existing.comparison || binding.comparison;
                        // Unfilterable only survives if NO stage filters it.
                        existing.unfilterableSampled =
                            existing.unfilterableSampled && binding.unfilterableSampled;
                        existing.arrayed = existing.arrayed || binding.arrayed;
                        merged = true;
                        break;
                    }
                }
                if (!merged)
                    target->shaderBindings.push_back(binding);
            }
        }
        return layouts;
    }

    struct BindGroupLayoutHandle
    {
        std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
        BindGroupLayoutDesc desc;
    };

    struct BindGroupDesc
    {
        BindGroupLayoutHandle layout;
        std::vector<BindGroupEntry> entries;
    };

    struct BindGroupHandle
    {
        std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
        BindGroupDesc desc;
    };

}
