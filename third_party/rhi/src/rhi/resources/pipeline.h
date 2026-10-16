#pragma once

#include "rhi/resources/bindgroup.h"
#include "rhi/resources/shader.h"
#include "rhi/resources/texture.h"

#include <cstdint>
#include <vector>

namespace rhi {

struct PipelineLayoutDesc {
    std::vector<BindGroupLayoutHandle> layouts;
};

struct PipelineLayoutHandle {
    std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
    PipelineLayoutDesc desc{};
};

enum class PrimitiveTopology {
    PointList,
    LineList,
    LineStrip,
    TriangleList,
    TriangleStrip,
};

enum class FillMode {
    Solid,
    Wireframe,
};

enum class CullMode {
    None,
    Front,
    Back,
};

enum class FrontFace {
    CounterClockwise,
    Clockwise,
};

struct VertexAttribute {
    std::uint32_t bufferSlot = 0;
    std::uint32_t location = 0;
    VertexFormat format = VertexFormat::Float32x4;
    std::uint32_t offset = 0;
};

struct VertexBufferLayout {
    std::uint32_t stride = 0;
    std::vector<VertexAttribute> attributes;
};

enum class BlendFactor {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
};

enum class BlendOp {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

enum class ColorWriteMask : std::uint8_t {
    None = 0,
    R = 1 << 0,
    G = 1 << 1,
    B = 1 << 2,
    A = 1 << 3,
    All = R | G | B | A,
};

constexpr ColorWriteMask operator|(ColorWriteMask lhs, ColorWriteMask rhs)
{
    return static_cast<ColorWriteMask>(
        static_cast<std::uint8_t>(lhs) |
        static_cast<std::uint8_t>(rhs));
}

constexpr ColorWriteMask operator&(ColorWriteMask lhs, ColorWriteMask rhs)
{
    return static_cast<ColorWriteMask>(
        static_cast<std::uint8_t>(lhs) &
        static_cast<std::uint8_t>(rhs));
}

struct BlendState {
    bool enabled = false;
    BlendFactor srcColor = BlendFactor::SrcAlpha;
    BlendFactor dstColor = BlendFactor::OneMinusSrcAlpha;
    BlendOp colorOp = BlendOp::Add;
    BlendFactor srcAlpha = BlendFactor::One;
    BlendFactor dstAlpha = BlendFactor::OneMinusSrcAlpha;
    BlendOp alphaOp = BlendOp::Add;
    ColorWriteMask writeMask = ColorWriteMask::All;
};

struct ColorTarget {
    TextureFormat format = TextureFormat::RGBA8Unorm;
    BlendState blend;
};

struct DepthStencilState {
    TextureFormat format = TextureFormat::Unknown;
    bool depthTestEnabled = false;
    bool depthWriteEnabled = false;
    CompareOp depthFunc = CompareOp::Less;
};

struct GraphicsPipelineDesc {
    PipelineLayoutHandle layout;
    ShaderModuleHandle vertexShader;
    ShaderModuleHandle fragmentShader;

    std::vector<VertexBufferLayout> vertexBuffers;
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    FillMode fillMode = FillMode::Solid;
    CullMode cullMode = CullMode::None;
    FrontFace frontFace = FrontFace::CounterClockwise;

    std::vector<ColorTarget> colorTargets;
    DepthStencilState depthStencil;
    SampleCount sampleCount = SampleCount::x1;
};

struct GraphicsPipelineHandle {
    std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
    GraphicsPipelineDesc desc{};
};

struct ComputePipelineDesc {
    PipelineLayoutHandle layout;
    ShaderModuleHandle computeShader;
};

struct ComputePipelineHandle {
    std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
    ComputePipelineDesc desc{};
};

}
