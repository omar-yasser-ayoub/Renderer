#pragma once

#include <cstdint>
#include <vector>
#include <filesystem>
#include <string>

namespace rhi {

    enum class BindingType
    {
        UniformBuffer,
        StorageBuffer,
        SampledTexture,
        StorageTexture,
        Sampler,
    };

    enum class ShaderStageFlags : std::uint32_t
    {
        None     = 0,
        Vertex   = 1 << 0,
        Fragment = 1 << 1,
        Compute  = 1 << 2,
    };

    constexpr ShaderStageFlags operator|(ShaderStageFlags lhs, ShaderStageFlags rhs)
    {
        return static_cast<ShaderStageFlags>(
            static_cast<std::uint32_t>(lhs) |
            static_cast<std::uint32_t>(rhs));
    }

    constexpr ShaderStageFlags operator&(ShaderStageFlags lhs, ShaderStageFlags rhs)
    {
        return static_cast<ShaderStageFlags>(
            static_cast<std::uint32_t>(lhs) &
            static_cast<std::uint32_t>(rhs));
    }

    constexpr bool HasFlag(ShaderStageFlags value, ShaderStageFlags flag)
    {
        return (static_cast<std::uint32_t>(value) &
                static_cast<std::uint32_t>(flag)) != 0;
    }

    struct ShaderBinding
    {
        uint32_t set;
        uint32_t binding;
        BindingType type;
        uint32_t arraySize;
        ShaderStageFlags stage;
        bool depthSampled = false;
        bool comparison = false;
        // Image is only ever fetched (OpImageFetch), never filtered.
        // Lets WebGPU bind depth-as-data (or any texture) without a
        // filtering sampler requirement. Cleared by any filtering use.
        bool unfilterableSampled = false;
        // True when the image is an array texture (e.g. Texture2DArray).
        // Backends that must declare the view dimension in the layout
        // (WebGPU) need this; Vulkan/DX12 derive it from the bound view.
        bool arrayed = false;
    };

    enum class VertexFormat
    {
        Float32,
        Float32x2,
        Float32x3,
        Float32x4,

        Int32,
        Int32x2,
        Int32x3,
        Int32x4,

        UInt32,
        UInt32x2,
        UInt32x3,
        UInt32x4,
    };

    struct VertexInputAttr
    {
        uint32_t location;
        VertexFormat format;
        // semanticName and index are populated by backends whose shaders
        // carry semantics (DX12); empty when the shader language uses
        // locations instead (SPIRV/WGSL).
        std::string semanticName;
        uint32_t semanticIndex = 0;
    };

    struct ShaderReflection
    {
        std::vector<ShaderBinding> bindings;
        std::vector<VertexInputAttr> vertexInputs;
    };

    struct ShaderModule
    {
        std::vector<uint8_t> bytecode;
        ShaderReflection reflection;
        std::string entryPoint = "main";
    };

    struct ShaderSource
    {
        std::filesystem::path path;
        ShaderStageFlags stage;
        std::string entryPoint;
    };

    struct ShaderModuleHandle
    {
        std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
    };
}
