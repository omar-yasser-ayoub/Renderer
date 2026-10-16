#include <gtest/gtest.h>
#include "rhi/rhi.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif




#if defined(RHI_VULKAN)
    using RHIImpl = VulkanRHI;
#elif defined(RHI_DX12)
    using RHIImpl = DX12RHI;
#elif defined(RHI_WEBGPU)
    using RHIImpl = WebGPURHI;
#else
    #error "No RHI backend selected"
#endif

class RHITest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(rhi.Initialize()) << "Failed to initialize RHI";
    }

    void TearDown() override
    {
        rhi.Shutdown();
    }

    RHIImpl rhi;
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(RHITest, InitializeSuccessfully)
{
    // Test is implicit: SetUp() will fail if Initialize() returns false
    EXPECT_TRUE(true);
}

TEST_F(RHITest, ReportsCapabilities)
{
    const auto& caps = rhi.GetCapabilities();
    EXPECT_FALSE(caps.deviceName.empty());
}

// ============================================================================
// Sampler Creation Tests
// ============================================================================

TEST_F(RHITest, CreateDefaultSampler)
{
    rhi::SamplerDesc desc;
    auto handle = rhi.CreateSampler(desc);

    EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF) << "Sampler handle should be valid";
}

TEST_F(RHITest, CreateMultipleSamplers)
{
    rhi::SamplerDesc desc;

    auto handle1 = rhi.CreateSampler(desc);
    auto handle2 = rhi.CreateSampler(desc);
    auto handle3 = rhi.CreateSampler(desc);

    EXPECT_NE(handle1.id, 0xFFFFFFFFFFFFFFFF);
    EXPECT_NE(handle2.id, 0xFFFFFFFFFFFFFFFF);
    EXPECT_NE(handle3.id, 0xFFFFFFFFFFFFFFFF);

    // Handles should be different
    EXPECT_NE(handle1.id, handle2.id);
    EXPECT_NE(handle2.id, handle3.id);
}

TEST_F(RHITest, CreateSamplerWithNearestFiltering)
{
    rhi::SamplerDesc desc;
    desc.minFilter = rhi::Filter::Nearest;
    desc.magFilter = rhi::Filter::Nearest;
    desc.mipFilter = rhi::MipmapFilter::Nearest;

    auto handle = rhi.CreateSampler(desc);
    EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF);
}

TEST_F(RHITest, CreateSamplerWithLinearFiltering)
{
    rhi::SamplerDesc desc;
    desc.minFilter = rhi::Filter::Linear;
    desc.magFilter = rhi::Filter::Linear;
    desc.mipFilter = rhi::MipmapFilter::Linear;

    auto handle = rhi.CreateSampler(desc);
    EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF);
}

TEST_F(RHITest, CreateSamplerWithDifferentAddressModes)
{
    rhi::SamplerDesc desc;

    // Test Repeat
    desc.addressU = rhi::AddressMode::Repeat;
    desc.addressV = rhi::AddressMode::Repeat;
    desc.addressW = rhi::AddressMode::Repeat;
    auto handle1 = rhi.CreateSampler(desc);
    EXPECT_NE(handle1.id, 0xFFFFFFFFFFFFFFFF);

    // Test ClampToEdge
    desc.addressU = rhi::AddressMode::ClampToEdge;
    desc.addressV = rhi::AddressMode::ClampToEdge;
    desc.addressW = rhi::AddressMode::ClampToEdge;
    auto handle2 = rhi.CreateSampler(desc);
    EXPECT_NE(handle2.id, 0xFFFFFFFFFFFFFFFF);

    // Test MirrorRepeat
    desc.addressU = rhi::AddressMode::MirrorRepeat;
    desc.addressV = rhi::AddressMode::MirrorRepeat;
    desc.addressW = rhi::AddressMode::MirrorRepeat;
    auto handle3 = rhi.CreateSampler(desc);
    EXPECT_NE(handle3.id, 0xFFFFFFFFFFFFFFFF);
}

TEST_F(RHITest, CreateSamplerWithAnisotropy)
{
    rhi::SamplerDesc desc;
    desc.anisotropyEnabled = true;
    desc.maxAnisotropy = 16;

    auto handle = rhi.CreateSampler(desc);
    EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF);
}

TEST_F(RHITest, CreateSamplerWithComparison)
{
    rhi::SamplerDesc desc;
    desc.compareEnabled = true;
    desc.compareOp = rhi::CompareOp::Less;

    auto handle = rhi.CreateSampler(desc);
    EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF);
}

TEST_F(RHITest, CreateSamplerWithDifferentComparisons)
{
    rhi::SamplerDesc desc;
    desc.compareEnabled = true;

    rhi::CompareOp ops[] = {
        rhi::CompareOp::Never,
        rhi::CompareOp::Less,
        rhi::CompareOp::Equal,
        rhi::CompareOp::LessEqual,
        rhi::CompareOp::Greater,
        rhi::CompareOp::NotEqual,
        rhi::CompareOp::GreaterEqual,
        rhi::CompareOp::Always,
    };

    for (auto op : ops)
    {
        desc.compareOp = op;
        auto handle = rhi.CreateSampler(desc);
        EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF) << "Failed for CompareOp: " << static_cast<int>(op);
    }
}

TEST_F(RHITest, CreateSamplerWithBorderColors)
{
    rhi::SamplerDesc desc;
    desc.addressU = rhi::AddressMode::ClampToBorder;
    desc.addressV = rhi::AddressMode::ClampToBorder;
    desc.addressW = rhi::AddressMode::ClampToBorder;

    rhi::BorderColor colors[] = {
        rhi::BorderColor::TransparentBlack,
        rhi::BorderColor::OpaqueBlack,
        rhi::BorderColor::OpaqueWhite,
    };

    for (auto color : colors)
    {
        desc.borderColor = color;
        auto handle = rhi.CreateSampler(desc);
        EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF) << "Failed for BorderColor: " << static_cast<int>(color);
    }
}

TEST_F(RHITest, CreateSamplerWithLODBias)
{
    rhi::SamplerDesc desc;
    desc.mipLodBias = 2.0f;

    auto handle = rhi.CreateSampler(desc);
    EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF);
}

TEST_F(RHITest, CreateSamplerWithLODRange)
{
    rhi::SamplerDesc desc;
    desc.minLOD = 0.0f;
    desc.maxLOD = 10.0f;

    auto handle = rhi.CreateSampler(desc);
    EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF);
}

TEST_F(RHITest, DestroySampler)
{
    rhi::SamplerDesc desc;
    auto handle = rhi.CreateSampler(desc);
    EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF);

    // Should not crash
    rhi.DestroySampler(handle);
}

TEST_F(RHITest, CreateAndDestroyMultipleSamplers)
{
    rhi::SamplerDesc desc;

    auto handle1 = rhi.CreateSampler(desc);
    auto handle2 = rhi.CreateSampler(desc);
    auto handle3 = rhi.CreateSampler(desc);

    EXPECT_NE(handle1.id, 0xFFFFFFFFFFFFFFFF);
    EXPECT_NE(handle2.id, 0xFFFFFFFFFFFFFFFF);
    EXPECT_NE(handle3.id, 0xFFFFFFFFFFFFFFFF);

    // Destroy in reverse order
    rhi.DestroySampler(handle3);
    rhi.DestroySampler(handle2);
    rhi.DestroySampler(handle1);
}

TEST_F(RHITest, ReusesSamplerSlotAfterDestroy)
{
    rhi::SamplerDesc desc;

    auto handle = rhi.CreateSampler(desc);
    ASSERT_NE(handle.id, 0xFFFFFFFFFFFFFFFF);
    rhi.DestroySampler(handle);

    auto reused = rhi.CreateSampler(desc);
    ASSERT_NE(reused.id, 0xFFFFFFFFFFFFFFFF);
    EXPECT_EQ(reused.id, handle.id);
    rhi.DestroySampler(reused);
}

TEST_F(RHITest, ReusesFirstSamplerSlotAfterDestroy)
{
    rhi::SamplerDesc desc;

    auto handle1 = rhi.CreateSampler(desc);
    auto handle2 = rhi.CreateSampler(desc);
    auto handle3 = rhi.CreateSampler(desc);
    ASSERT_NE(handle1.id, 0xFFFFFFFFFFFFFFFF);
    ASSERT_NE(handle2.id, 0xFFFFFFFFFFFFFFFF);
    ASSERT_NE(handle3.id, 0xFFFFFFFFFFFFFFFF);

    rhi.DestroySampler(handle1);

    auto handle4 = rhi.CreateSampler(desc);
    ASSERT_NE(handle4.id, 0xFFFFFFFFFFFFFFFF);
    EXPECT_EQ(handle4.id, handle1.id);

    rhi.DestroySampler(handle4);
    rhi.DestroySampler(handle3);
    rhi.DestroySampler(handle2);
}

// ============================================================================
// Buffer Tests
// ============================================================================

namespace
{
constexpr std::uint64_t InvalidHandle = 0xFFFFFFFFFFFFFFFF;

rhi::BufferDesc MakeBufferDesc(
    std::uint64_t size,
    rhi::MemoryUsage memoryUsage,
    rhi::BufferUsage usage = rhi::BufferUsage::CopySrc)
{
    rhi::BufferDesc desc;
    desc.size = size;
    desc.memoryUsage = memoryUsage;
    desc.usage = usage;
    return desc;
}
}

TEST_F(RHITest, CreateBuffer)
{
    auto handle = rhi.CreateBuffer(MakeBufferDesc(
        256, rhi::MemoryUsage::CpuToGpu));

    EXPECT_NE(handle.id, InvalidHandle);
    rhi.DestroyBuffer(handle);
}

TEST_F(RHITest, CreateMultipleBuffers)
{
    std::vector<rhi::BufferHandle> handles;
    for (int i = 0; i < 3; ++i) {
        auto handle = rhi.CreateBuffer(MakeBufferDesc(
            256, rhi::MemoryUsage::CpuToGpu));
        ASSERT_NE(handle.id, InvalidHandle);
        handles.push_back(handle);
    }

    EXPECT_NE(handles[0].id, handles[1].id);
    EXPECT_NE(handles[1].id, handles[2].id);

    for (auto handle : handles) {
        rhi.DestroyBuffer(handle);
    }
}

TEST_F(RHITest, RejectsZeroSizedBuffer)
{
    auto handle = rhi.CreateBuffer(MakeBufferDesc(
        0, rhi::MemoryUsage::CpuToGpu));

    EXPECT_EQ(handle.id, InvalidHandle);
}

TEST_F(RHITest, MapAndWriteCpuToGpuBuffer)
{
    constexpr std::size_t BufferSize = 64;
    constexpr std::uint32_t Value = 0x12345678;

    auto handle = rhi.CreateBuffer(MakeBufferDesc(
        BufferSize,
        rhi::MemoryUsage::CpuToGpu));
    ASSERT_NE(handle.id, InvalidHandle);

    auto* mapped = static_cast<std::uint32_t*>(rhi.MapBuffer(handle));
    ASSERT_NE(mapped, nullptr);
    mapped[0] = Value;
    rhi.UnmapBuffer(handle);

    std::uint32_t written = 0;
    rhi.WriteBuffer(handle, &Value, sizeof(Value));

    mapped = static_cast<std::uint32_t*>(rhi.MapBuffer(handle));
    ASSERT_NE(mapped, nullptr);
    std::memcpy(&written, mapped, sizeof(written));
    rhi.UnmapBuffer(handle);

    EXPECT_EQ(written, Value);
    rhi.DestroyBuffer(handle);
}

TEST_F(RHITest, WriteBufferWithOffset)
{
    constexpr std::size_t BufferSize = 64;
    constexpr std::size_t Offset = 16;
    constexpr std::array<std::uint8_t, 4> Data = { 1, 2, 3, 4 };

    auto handle = rhi.CreateBuffer(MakeBufferDesc(
        BufferSize, rhi::MemoryUsage::CpuToGpu));
    ASSERT_NE(handle.id, InvalidHandle);

    rhi.WriteBuffer(handle, Data.data(), Data.size(), Offset);

    auto* mapped = static_cast<std::uint8_t*>(rhi.MapBuffer(handle));
    ASSERT_NE(mapped, nullptr);
    EXPECT_EQ(std::memcmp(mapped + Offset, Data.data(), Data.size()), 0);
    rhi.UnmapBuffer(handle);
    rhi.DestroyBuffer(handle);
}

TEST_F(RHITest, RejectsOutOfBoundsBufferWrite)
{
    constexpr std::array<std::uint8_t, 8> Data = {};
    auto handle = rhi.CreateBuffer(MakeBufferDesc(
        16, rhi::MemoryUsage::CpuToGpu));
    ASSERT_NE(handle.id, InvalidHandle);

    // The operation should be rejected without crashing.
    rhi.WriteBuffer(handle, Data.data(), Data.size(), 12);

    rhi.DestroyBuffer(handle);
}

TEST_F(RHITest, GpuOnlyBufferCannotBeMapped)
{
    auto handle = rhi.CreateBuffer(MakeBufferDesc(
        256,
        rhi::MemoryUsage::GpuOnly,
        rhi::BufferUsage::Vertex));
    ASSERT_NE(handle.id, InvalidHandle);

    EXPECT_EQ(rhi.MapBuffer(handle), nullptr);
    rhi.UnmapBuffer(handle);
    rhi.DestroyBuffer(handle);
}

TEST_F(RHITest, GpuToCpuBufferCanBeMapped)
{
    auto handle = rhi.CreateBuffer(MakeBufferDesc(
        256,
        rhi::MemoryUsage::GpuToCpu,
        rhi::BufferUsage::CopyDst));
    ASSERT_NE(handle.id, InvalidHandle);

    // Note: older Dawn D3D12 had a quirk where MapAsync(Read) reported
    // success but GetMappedRange returned null. Fixed in current Dawn.
    EXPECT_NE(rhi.MapBuffer(handle), nullptr);
    rhi.UnmapBuffer(handle);
    rhi.DestroyBuffer(handle);
}

TEST_F(RHITest, DestroyBuffer)
{
    auto handle = rhi.CreateBuffer(MakeBufferDesc(
        256, rhi::MemoryUsage::CpuToGpu));
    ASSERT_NE(handle.id, InvalidHandle);

    rhi.DestroyBuffer(handle);
    EXPECT_EQ(rhi.MapBuffer(handle), nullptr);

    // Destroying the same handle again should not crash.
    rhi.DestroyBuffer(handle);
}

TEST_F(RHITest, CreateAndDestroyMultipleBuffers)
{
    std::vector<rhi::BufferHandle> handles;
    for (int i = 0; i < 16; ++i) {
        auto handle = rhi.CreateBuffer(MakeBufferDesc(
            128 + i, rhi::MemoryUsage::CpuToGpu));
        ASSERT_NE(handle.id, InvalidHandle);
        handles.push_back(handle);
    }

    for (auto it = handles.rbegin(); it != handles.rend(); ++it) {
        rhi.DestroyBuffer(*it);
    }
}

TEST_F(RHITest, ReusesBufferSlotAfterDestroy)
{
    auto handle = rhi.CreateBuffer(MakeBufferDesc(
        256, rhi::MemoryUsage::CpuToGpu));
    ASSERT_NE(handle.id, InvalidHandle);
    rhi.DestroyBuffer(handle);

    auto reused = rhi.CreateBuffer(MakeBufferDesc(
        256, rhi::MemoryUsage::CpuToGpu));
    ASSERT_NE(reused.id, InvalidHandle);
    EXPECT_EQ(reused.id, handle.id);
    rhi.DestroyBuffer(reused);
}

TEST_F(RHITest, ReusesFirstBufferSlotAfterDestroy)
{
    auto handle1 = rhi.CreateBuffer(MakeBufferDesc(
        256, rhi::MemoryUsage::CpuToGpu));
    auto handle2 = rhi.CreateBuffer(MakeBufferDesc(
        256, rhi::MemoryUsage::CpuToGpu));
    auto handle3 = rhi.CreateBuffer(MakeBufferDesc(
        256, rhi::MemoryUsage::CpuToGpu));
    ASSERT_NE(handle1.id, InvalidHandle);
    ASSERT_NE(handle2.id, InvalidHandle);
    ASSERT_NE(handle3.id, InvalidHandle);

    rhi.DestroyBuffer(handle1);

    auto handle4 = rhi.CreateBuffer(MakeBufferDesc(
        256, rhi::MemoryUsage::CpuToGpu));
    ASSERT_NE(handle4.id, InvalidHandle);
    EXPECT_EQ(handle4.id, handle1.id);

    rhi.DestroyBuffer(handle4);
    rhi.DestroyBuffer(handle3);
    rhi.DestroyBuffer(handle2);
}


#if defined(RHI_DX12) || defined(RHI_VULKAN) || defined(RHI_WEBGPU)
// ============================================================================
// Backend Shader Module Tests
// ============================================================================

namespace
{
std::filesystem::path ShaderPath(const char* filename)
{
#ifdef RHI_SHADER_DIR
    return std::filesystem::path(RHI_SHADER_DIR) / filename;
#else
    return std::filesystem::path("src/shaders") / filename;
#endif
}

rhi::ShaderSource MakeShaderSource(
    const char* filename,
    rhi::ShaderStageFlags stage)
{
    rhi::ShaderSource source;
    source.path = ShaderPath(filename);
    source.stage = stage;
    source.entryPoint = "main";
    return source;
}

const rhi::ShaderBinding* FindBinding(
    const rhi::ShaderReflection& reflection,
    std::uint32_t set,
    std::uint32_t binding,
    rhi::BindingType type)
{
    for (const auto& candidate : reflection.bindings) {
        if (candidate.set == set &&
            candidate.binding == binding &&
            candidate.type == type) {
            return &candidate;
        }
    }
    return nullptr;
}
}

TEST_F(RHITest, CompileVertexShaderModuleFromShaderAsset)
{
    const auto source = MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex);
    ASSERT_TRUE(std::filesystem::exists(source.path)) << source.path.string();

    const auto handle = rhi.CompileShader(source);
    const auto module = rhi.GetShaderModule(handle);

    ASSERT_TRUE(module.has_value());
    ASSERT_FALSE(module->bytecode.empty());
    ASSERT_GE(module->bytecode.size(), 4u);
    EXPECT_EQ(module->entryPoint, "main");
#if defined(RHI_DX12)
    EXPECT_EQ(module->bytecode[0], static_cast<std::uint8_t>('D'));
    EXPECT_EQ(module->bytecode[1], static_cast<std::uint8_t>('X'));
    EXPECT_EQ(module->bytecode[2], static_cast<std::uint8_t>('B'));
    EXPECT_EQ(module->bytecode[3], static_cast<std::uint8_t>('C'));
#elif defined(RHI_VULKAN)
    const auto* spirv = reinterpret_cast<const std::uint32_t*>(module->bytecode.data());
    EXPECT_EQ(spirv[0], 0x07230203u);
#else
    const std::string wgsl(module->bytecode.begin(), module->bytecode.end());
    EXPECT_NE(wgsl.find("@vertex"), std::string::npos);
#endif

    ASSERT_EQ(module->reflection.bindings.size(), 0u);
    ASSERT_EQ(module->reflection.vertexInputs.size(), 2u);

    const auto& position = module->reflection.vertexInputs[0];
    EXPECT_EQ(position.location, 0u);
    EXPECT_EQ(position.format, rhi::VertexFormat::Float32x3);

    const auto& uv = module->reflection.vertexInputs[1];
    EXPECT_EQ(uv.location, 1u);
    EXPECT_EQ(uv.format, rhi::VertexFormat::Float32x2);
}

TEST_F(RHITest, CompileBindingVertexShaderModuleFromShaderAsset)
{
    const auto source = MakeShaderSource(
        "bindings.vert.hlsl",
        rhi::ShaderStageFlags::Vertex);
    ASSERT_TRUE(std::filesystem::exists(source.path)) << source.path.string();

    const auto handle = rhi.CompileShader(source);
    const auto module = rhi.GetShaderModule(handle);
    ASSERT_TRUE(module.has_value());
    ASSERT_FALSE(module->bytecode.empty());
    ASSERT_EQ(module->reflection.bindings.size(), 2u);
    ASSERT_EQ(module->reflection.vertexInputs.size(), 2u);

    const auto* camera = FindBinding(
        module->reflection, 0u, 0u, rhi::BindingType::UniformBuffer);
    ASSERT_NE(camera, nullptr);
    EXPECT_EQ(camera->arraySize, 1u);
    EXPECT_EQ(camera->stage, rhi::ShaderStageFlags::Vertex);

    const auto* positions = FindBinding(
        module->reflection, 1u, 1u, rhi::BindingType::StorageBuffer);
    ASSERT_NE(positions, nullptr);
    EXPECT_EQ(positions->arraySize, 1u);
    EXPECT_EQ(positions->stage, rhi::ShaderStageFlags::Vertex);
}

TEST_F(RHITest, CompileBindingFragmentShaderModuleFromShaderAsset)
{
    const auto source = MakeShaderSource(
        "bindings.frag.hlsl",
        rhi::ShaderStageFlags::Fragment);
    ASSERT_TRUE(std::filesystem::exists(source.path)) << source.path.string();

    const auto handle = rhi.CompileShader(source);
    const auto module = rhi.GetShaderModule(handle);
    ASSERT_TRUE(module.has_value());
    ASSERT_FALSE(module->bytecode.empty());
    ASSERT_EQ(module->reflection.bindings.size(), 3u);
    EXPECT_TRUE(module->reflection.vertexInputs.empty());

    const auto* albedo = FindBinding(
        module->reflection, 2u, 0u, rhi::BindingType::SampledTexture);
    ASSERT_NE(albedo, nullptr);
    EXPECT_EQ(albedo->arraySize, 1u);
    EXPECT_EQ(albedo->stage, rhi::ShaderStageFlags::Fragment);

    const auto* sampler = FindBinding(
        module->reflection, 2u, 1u, rhi::BindingType::Sampler);
    ASSERT_NE(sampler, nullptr);
    EXPECT_EQ(sampler->arraySize, 1u);
    EXPECT_EQ(sampler->stage, rhi::ShaderStageFlags::Fragment);

    const auto* output = FindBinding(
        module->reflection, 3u, 2u, rhi::BindingType::StorageTexture);
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(output->arraySize, 1u);
    EXPECT_EQ(output->stage, rhi::ShaderStageFlags::Fragment);
}

TEST_F(RHITest, CompileFragmentShaderModuleFromShaderAsset)
{
    const auto source = MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment);
    ASSERT_TRUE(std::filesystem::exists(source.path)) << source.path.string();

    const auto handle = rhi.CompileShader(source);
    const auto module = rhi.GetShaderModule(handle);
    ASSERT_TRUE(module.has_value());

    ASSERT_FALSE(module->bytecode.empty());
    ASSERT_GE(module->bytecode.size(), 4u);
#if defined(RHI_DX12)
    EXPECT_EQ(module->bytecode[0], static_cast<std::uint8_t>('D'));
    EXPECT_EQ(module->bytecode[1], static_cast<std::uint8_t>('X'));
    EXPECT_EQ(module->bytecode[2], static_cast<std::uint8_t>('B'));
    EXPECT_EQ(module->bytecode[3], static_cast<std::uint8_t>('C'));
#elif defined(RHI_VULKAN)
    const auto* spirv = reinterpret_cast<const std::uint32_t*>(module->bytecode.data());
    EXPECT_EQ(spirv[0], 0x07230203u);
#else
    const std::string wgsl(module->bytecode.begin(), module->bytecode.end());
    EXPECT_NE(wgsl.find("@fragment"), std::string::npos);
#endif
    EXPECT_TRUE(module->reflection.bindings.empty());
    EXPECT_TRUE(module->reflection.vertexInputs.empty());
}

TEST_F(RHITest, DestroyAndReuseShaderModule)
{
    const auto handle = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    ASSERT_TRUE(rhi.GetShaderModule(handle).has_value());

    rhi.DestroyShaderModule(handle);
    EXPECT_FALSE(rhi.GetShaderModule(handle).has_value());

    // Double destroy is safe.
    rhi.DestroyShaderModule(handle);

    const auto reused = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    ASSERT_TRUE(rhi.GetShaderModule(reused).has_value());
    EXPECT_EQ(reused.id, handle.id);
    rhi.DestroyShaderModule(reused);
}

TEST_F(RHITest, BuildBindGroupLayoutDescsFromReflection)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "shadow.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    const auto vsModule = rhi.GetShaderModule(vs);
    const auto fsModule = rhi.GetShaderModule(fs);
    ASSERT_TRUE(vsModule.has_value());
    ASSERT_TRUE(fsModule.has_value());

    const auto descs = rhi::BuildBindGroupLayoutDescs(
        { vsModule->reflection, fsModule->reflection });
    ASSERT_EQ(descs.size(), 1u);
    ASSERT_EQ(descs[0].shaderBindings.size(), 4u);

    const auto layout = rhi.CreateBindGroupLayout(descs[0]);
    EXPECT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);
    rhi.DestroyBindGroupLayout(layout);
    rhi.DestroyShaderModule(vs);
    rhi.DestroyShaderModule(fs);
}
#endif

#if defined(RHI_WEBGPU)
TEST_F(RHITest, ReflectionMarksDepthComparisonSampling)
{
    const auto handle = rhi.CompileShader(MakeShaderSource(
        "shadow.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    const auto module = rhi.GetShaderModule(handle);
    ASSERT_TRUE(module.has_value());
    ASSERT_EQ(module->reflection.bindings.size(), 4u);

    const auto* shadow = FindBinding(
        module->reflection, 2u, 0u, rhi::BindingType::SampledTexture);
    ASSERT_NE(shadow, nullptr);
    EXPECT_TRUE(shadow->depthSampled);
    EXPECT_FALSE(shadow->comparison);

    const auto* shadowSampler = FindBinding(
        module->reflection, 2u, 1u, rhi::BindingType::Sampler);
    ASSERT_NE(shadowSampler, nullptr);
    EXPECT_TRUE(shadowSampler->comparison);
    EXPECT_FALSE(shadowSampler->depthSampled);

    const auto* albedo = FindBinding(
        module->reflection, 2u, 2u, rhi::BindingType::SampledTexture);
    ASSERT_NE(albedo, nullptr);
    EXPECT_FALSE(albedo->depthSampled);

    const auto* linear = FindBinding(
        module->reflection, 2u, 3u, rhi::BindingType::Sampler);
    ASSERT_NE(linear, nullptr);
    EXPECT_FALSE(linear->comparison);

    rhi.DestroyShaderModule(handle);
}
#endif

#if defined(RHI_VULKAN) || defined(RHI_WEBGPU)
TEST_F(RHITest, ReflectionLeavesRegularSamplingUnmarked)
{
    const auto handle = rhi.CompileShader(MakeShaderSource(
        "bindings.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    const auto module = rhi.GetShaderModule(handle);
    ASSERT_TRUE(module.has_value());

    for (const auto& binding : module->reflection.bindings)
    {
        EXPECT_FALSE(binding.depthSampled);
        EXPECT_FALSE(binding.comparison);
    }

    rhi.DestroyShaderModule(handle);
}
#endif

#if defined(RHI_VULKAN) || defined(RHI_WEBGPU) || defined(RHI_DX12)
// ============================================================================
// Bind Group Tests
// ============================================================================

namespace
{
rhi::BindGroupLayoutDesc MakeUniformBindGroupLayoutDesc()
{
    rhi::BindGroupLayoutDesc desc;
    desc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    return desc;
}
}

TEST_F(RHITest, CreateBindGroupLayout)
{
    const auto desc = MakeUniformBindGroupLayoutDesc();
    const auto layout = rhi.CreateBindGroupLayout(desc);

    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);
    ASSERT_EQ(layout.desc.shaderBindings.size(), 1u);
    EXPECT_EQ(layout.desc.shaderBindings[0].set, 0u);
    EXPECT_EQ(layout.desc.shaderBindings[0].binding, 0u);
    EXPECT_EQ(layout.desc.shaderBindings[0].type, rhi::BindingType::UniformBuffer);
}

TEST_F(RHITest, RejectsEmptyBindGroupLayout)
{
    rhi::BindGroupLayoutDesc desc;
    const auto layout = rhi.CreateBindGroupLayout(desc);
    EXPECT_EQ(layout.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, RejectsBindGroupLayoutWithMixedSets)
{
    rhi::BindGroupLayoutDesc desc = MakeUniformBindGroupLayoutDesc();
    desc.shaderBindings.push_back({
        1,
        1,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});

    const auto layout = rhi.CreateBindGroupLayout(desc);
    EXPECT_EQ(layout.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, RejectsBindGroupLayoutWithDuplicateBinding)
{
    rhi::BindGroupLayoutDesc desc = MakeUniformBindGroupLayoutDesc();
    // Same binding number, different type: still a duplicate.
    desc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::Sampler,
        1,
        rhi::ShaderStageFlags::Vertex});

    const auto layout = rhi.CreateBindGroupLayout(desc);
    EXPECT_EQ(layout.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, DestroyAndReuseBindGroupLayout)
{
    const auto handle = rhi.CreateBindGroupLayout(MakeUniformBindGroupLayoutDesc());
    ASSERT_NE(handle.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyBindGroupLayout(handle);

    // Double destroy is safe.
    rhi.DestroyBindGroupLayout(handle);

    const auto reused = rhi.CreateBindGroupLayout(MakeUniformBindGroupLayoutDesc());
    ASSERT_NE(reused.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_EQ(reused.id, handle.id);
    rhi.DestroyBindGroupLayout(reused);
}

TEST_F(RHITest, CreateBindGroupWithBuffer)
{
    const auto layout = rhi.CreateBindGroupLayout(MakeUniformBindGroupLayoutDesc());
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    // Bind-only, so device-local: no backend allows CPU-visible bindable buffers.
    const auto buffer = rhi.CreateBuffer(MakeBufferDesc(
        256,
        rhi::MemoryUsage::GpuOnly,
        rhi::BufferUsage::Uniform));
    ASSERT_NE(buffer.id, InvalidHandle);

    rhi::BindGroupDesc desc;
    desc.layout = layout;
    desc.entries.push_back({ 0, buffer, {}, {} });

    const auto bindGroup = rhi.CreateBindGroup(desc);
    ASSERT_NE(bindGroup.id, 0xFFFFFFFFFFFFFFFFull);
    ASSERT_EQ(bindGroup.desc.layout.id, layout.id);
    ASSERT_EQ(bindGroup.desc.entries.size(), 1u);
    EXPECT_EQ(bindGroup.desc.entries[0].binding, 0u);

    rhi.DestroyBuffer(buffer);
}

TEST_F(RHITest, RejectsEmptyBindGroup)
{
    const auto layout = rhi.CreateBindGroupLayout(MakeUniformBindGroupLayoutDesc());
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::BindGroupDesc desc;
    desc.layout = layout;
    const auto bindGroup = rhi.CreateBindGroup(desc);
    EXPECT_EQ(bindGroup.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, RejectsBindGroupWithInvalidLayout)
{
    rhi::BindGroupDesc desc;
    desc.layout.id = 0xFFFFFFFFFFFFFFFFull;
    desc.entries.push_back({ 0, {}, {}, {} });

    const auto bindGroup = rhi.CreateBindGroup(desc);
    EXPECT_EQ(bindGroup.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, DestroyAndReuseBindGroup)
{
    const auto layout = rhi.CreateBindGroupLayout(MakeUniformBindGroupLayoutDesc());
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    // Bind-only, so device-local: no backend allows CPU-visible bindable buffers.
    const auto buffer = rhi.CreateBuffer(MakeBufferDesc(
        256,
        rhi::MemoryUsage::GpuOnly,
        rhi::BufferUsage::Uniform));
    ASSERT_NE(buffer.id, InvalidHandle);

    rhi::BindGroupDesc desc;
    desc.layout = layout;
    desc.entries.push_back({ 0, buffer, {}, {} });

    const auto handle = rhi.CreateBindGroup(desc);
    ASSERT_NE(handle.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyBindGroup(handle);

    // Double destroy is safe.
    rhi.DestroyBindGroup(handle);

    const auto reused = rhi.CreateBindGroup(desc);
    ASSERT_NE(reused.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_EQ(reused.id, handle.id);

    rhi.DestroyBindGroup(reused);
    rhi.DestroyBuffer(buffer);
    rhi.DestroyBindGroupLayout(layout);
}
#endif

// ============================================================================
// Texture Tests
// ============================================================================

namespace
{
constexpr std::uint64_t InvalidTextureHandle = 0xFFFFFFFFFFFFFFFFull;

#if defined(RHI_VULKAN) || defined(RHI_WEBGPU) || defined(RHI_DX12)
#define RHI_HAS_TEXTURE_VIEWS 1
#endif

rhi::TextureDesc MakeTextureDesc(
    std::uint32_t width = 4,
    std::uint32_t height = 4,
    rhi::TextureFormat format = rhi::TextureFormat::RGBA8Unorm,
    rhi::TextureUsage usage = rhi::TextureUsage::Sampled |
                               rhi::TextureUsage::CopyDst)
{
    rhi::TextureDesc desc;
    desc.width = width;
    desc.height = height;
    desc.format = format;
    desc.usage = usage;
    return desc;
}
}

TEST_F(RHITest, CreateTexture)
{
    auto handle = rhi.CreateTexture(MakeTextureDesc());

    EXPECT_NE(handle.id, InvalidTextureHandle);
    rhi.DestroyTexture(handle);
}

TEST_F(RHITest, CreateMultipleTextures)
{
    std::vector<rhi::TextureHandle> handles;
    for (int i = 0; i < 3; ++i) {
        auto handle = rhi.CreateTexture(MakeTextureDesc(4 + i, 4 + i));
        ASSERT_NE(handle.id, InvalidTextureHandle);
        handles.push_back(handle);
    }

    EXPECT_NE(handles[0].id, handles[1].id);
    EXPECT_NE(handles[1].id, handles[2].id);

    for (auto handle : handles) {
        rhi.DestroyTexture(handle);
    }
}

TEST_F(RHITest, ReusesTextureSlotAfterDestroy)
{
    auto handle = rhi.CreateTexture(MakeTextureDesc());
    ASSERT_NE(handle.id, InvalidTextureHandle);
    rhi.DestroyTexture(handle);

    auto reused = rhi.CreateTexture(MakeTextureDesc());
    ASSERT_NE(reused.id, InvalidTextureHandle);
    EXPECT_EQ(reused.id, handle.id);
    rhi.DestroyTexture(reused);
}

TEST_F(RHITest, ReusesFirstTextureSlotAfterDestroy)
{
    auto handle1 = rhi.CreateTexture(MakeTextureDesc(4, 4));
    auto handle2 = rhi.CreateTexture(MakeTextureDesc(5, 5));
    auto handle3 = rhi.CreateTexture(MakeTextureDesc(6, 6));
    ASSERT_NE(handle1.id, InvalidTextureHandle);
    ASSERT_NE(handle2.id, InvalidTextureHandle);
    ASSERT_NE(handle3.id, InvalidTextureHandle);

    rhi.DestroyTexture(handle1);

    auto handle4 = rhi.CreateTexture(MakeTextureDesc(4, 4));
    ASSERT_NE(handle4.id, InvalidTextureHandle);
    EXPECT_EQ(handle4.id, handle1.id);

    rhi.DestroyTexture(handle4);
    rhi.DestroyTexture(handle3);
    rhi.DestroyTexture(handle2);
}

TEST_F(RHITest, CreateTextureWithDifferentDimensionsAndFormats)
{
    struct TextureCase {
        rhi::TextureDimension dimension;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t depth;
        rhi::TextureFormat format;
    };

    const TextureCase cases[] = {
        { rhi::TextureDimension::Tex1D, 16, 1, 1, rhi::TextureFormat::R8Unorm },
        { rhi::TextureDimension::Tex2D, 8, 8, 1, rhi::TextureFormat::RGBA8Unorm },
        { rhi::TextureDimension::Tex3D, 4, 4, 4, rhi::TextureFormat::RGBA16Float },
    };

    for (const auto& testCase : cases) {
        auto desc = MakeTextureDesc(
            testCase.width,
            testCase.height,
            testCase.format,
            rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage |
                rhi::TextureUsage::CopySrc | rhi::TextureUsage::CopyDst);
        desc.dimension = testCase.dimension;
        desc.depth = testCase.depth;

        auto handle = rhi.CreateTexture(desc);
#if defined(RHI_WEBGPU)
        // R8Unorm is not storage-compatible in WebGPU, so Storage usage is
        // rejected for that case (the backend logs the supported formats).
        if (testCase.format == rhi::TextureFormat::R8Unorm) {
            EXPECT_EQ(handle.id, InvalidTextureHandle)
                << "R8Unorm with Storage usage should be rejected on WebGPU";
            continue;
        }
#endif
        ASSERT_NE(handle.id, InvalidTextureHandle)
            << "Failed for texture dimension/format case";
        rhi.DestroyTexture(handle);
    }
}

TEST_F(RHITest, CreateTextureWithMipLevelsAndSampleCount)
{
    auto desc = MakeTextureDesc(
        16,
        16,
        rhi::TextureFormat::RGBA8Unorm,
        rhi::TextureUsage::Sampled | rhi::TextureUsage::RenderTarget);
    desc.mipLevels = 4;
    desc.sampleCount = rhi::SampleCount::x1;

    auto handle = rhi.CreateTexture(desc);
    ASSERT_NE(handle.id, InvalidTextureHandle);

    rhi.DestroyTexture(handle);
}

#if defined(RHI_HAS_TEXTURE_VIEWS)
TEST_F(RHITest, CreateTextureView)
{
    auto texture = rhi.CreateTexture(MakeTextureDesc());
    ASSERT_NE(texture.id, InvalidTextureHandle);

    rhi::TextureViewDesc viewDesc;
    auto view = rhi.CreateTextureView(viewDesc, texture);

    EXPECT_NE(view.id, InvalidTextureHandle);
    rhi.DestroyTextureView(view);
    rhi.DestroyTexture(texture);
}

TEST_F(RHITest, CreateTextureViewsForMipLevelsAndArrayLayers)
{
    auto textureDesc = MakeTextureDesc();
    textureDesc.mipLevels = 3;
    textureDesc.arrayLayers = 2;

    auto texture = rhi.CreateTexture(textureDesc);
    ASSERT_NE(texture.id, InvalidTextureHandle);

    rhi::TextureViewDesc viewDesc;
    viewDesc.baseMipLevel = 1;
    viewDesc.baseArrayLayer = 1;
    viewDesc.mipLevels = 1;
    viewDesc.arrayLayers = 1;

    auto view = rhi.CreateTextureView(viewDesc, texture);
    ASSERT_NE(view.id, InvalidTextureHandle);

    rhi.DestroyTextureView(view);
    rhi.DestroyTexture(texture);
}

TEST_F(RHITest, CreateAndDestroyMultipleTextureViews)
{
    auto texture = rhi.CreateTexture(MakeTextureDesc());
    ASSERT_NE(texture.id, InvalidTextureHandle);

    rhi::TextureViewDesc viewDesc;
    auto view1 = rhi.CreateTextureView(viewDesc, texture);
    auto view2 = rhi.CreateTextureView(viewDesc, texture);
    ASSERT_NE(view1.id, InvalidTextureHandle);
    ASSERT_NE(view2.id, InvalidTextureHandle);
    EXPECT_NE(view1.id, view2.id);

    rhi.DestroyTextureView(view2);
    rhi.DestroyTextureView(view1);
    rhi.DestroyTexture(texture);
}

TEST_F(RHITest, DestroyTextureAndTextureViewAreIdempotent)
{
    auto texture = rhi.CreateTexture(MakeTextureDesc());
    ASSERT_NE(texture.id, InvalidTextureHandle);

    rhi::TextureViewDesc viewDesc;
    auto view = rhi.CreateTextureView(viewDesc, texture);
    ASSERT_NE(view.id, InvalidTextureHandle);

    rhi.DestroyTextureView(view);
    rhi.DestroyTextureView(view);
    rhi.DestroyTexture(texture);
    rhi.DestroyTexture(texture);
}

TEST_F(RHITest, ReusesTextureViewSlotAfterDestroy)
{
    auto texture = rhi.CreateTexture(MakeTextureDesc());
    ASSERT_NE(texture.id, InvalidTextureHandle);

    rhi::TextureViewDesc viewDesc;
    auto view = rhi.CreateTextureView(viewDesc, texture);
    ASSERT_NE(view.id, InvalidTextureHandle);
    rhi.DestroyTextureView(view);

    auto reused = rhi.CreateTextureView(viewDesc, texture);
    ASSERT_NE(reused.id, InvalidTextureHandle);
    EXPECT_EQ(reused.id, view.id);

    rhi.DestroyTextureView(reused);
    rhi.DestroyTexture(texture);
}

#endif

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(RHITest, CreateManySimpleSamplers)
{
    rhi::SamplerDesc desc;
    std::vector<rhi::SamplerHandle> handles;

    // Create 100 samplers
    for (int i = 0; i < 100; ++i)
    {
        auto handle = rhi.CreateSampler(desc);
        EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF) << "Failed at sampler " << i;
        handles.push_back(handle);
    }

    // All handles should be unique
    for (size_t i = 0; i < handles.size(); ++i)
    {
        for (size_t j = i + 1; j < handles.size(); ++j)
        {
            EXPECT_NE(handles[i].id, handles[j].id);
        }
    }
}

TEST_F(RHITest, CreateComplexSamplerCombinations)
{
    // Test various combinations of parameters
    rhi::SamplerDesc desc;

    rhi::Filter filters[] = { rhi::Filter::Nearest, rhi::Filter::Linear };
    rhi::MipmapFilter mipFilters[] = { rhi::MipmapFilter::Nearest, rhi::MipmapFilter::Linear };
    rhi::AddressMode addressModes[] = {
        rhi::AddressMode::Repeat,
        rhi::AddressMode::ClampToEdge,
        rhi::AddressMode::MirrorRepeat,
    };

    int count = 0;
    for (auto minF : filters)
    {
        for (auto magF : filters)
        {
            for (auto mipF : mipFilters)
            {
                for (auto addr : addressModes)
                {
                    desc.minFilter = minF;
                    desc.magFilter = magF;
                    desc.mipFilter = mipF;
                    desc.addressU = addr;
                    desc.addressV = addr;
                    desc.addressW = addr;

                    auto handle = rhi.CreateSampler(desc);
                    EXPECT_NE(handle.id, 0xFFFFFFFFFFFFFFFF) << "Failed at combination " << count;
                    count++;
                }
            }
        }
    }
}

#if defined(RHI_WEBGPU)
// ============================================================================
// Pipeline Layout + Graphics Pipeline Tests
// ============================================================================

namespace
{
rhi::GraphicsPipelineDesc MakeTrianglePipelineDesc(
    rhi::PipelineLayoutHandle layout,
    rhi::ShaderModuleHandle vs,
    rhi::ShaderModuleHandle fs)
{
    rhi::GraphicsPipelineDesc desc;
    desc.layout = layout;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;

    rhi::VertexBufferLayout buffer;
    buffer.stride = 20;
    buffer.attributes.push_back({ 0, 0, rhi::VertexFormat::Float32x3, 0 });
    buffer.attributes.push_back({ 0, 1, rhi::VertexFormat::Float32x2, 12 });
    desc.vertexBuffers.push_back(buffer);

    rhi::ColorTarget target;
    target.format = rhi::TextureFormat::RGBA8Unorm;
    desc.colorTargets.push_back(target);
    return desc;
}
}

TEST_F(RHITest, CreatePipelineLayout)
{
    rhi::BindGroupLayoutDesc bindDesc;
    bindDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto bindLayout = rhi.CreateBindGroupLayout(bindDesc);
    ASSERT_NE(bindLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc desc;
    desc.layouts.push_back(bindLayout);
    const auto layout = rhi.CreatePipelineLayout(desc);

    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);
    ASSERT_EQ(layout.desc.layouts.size(), 1u);
    EXPECT_EQ(layout.desc.layouts[0].id, bindLayout.id);
}

TEST_F(RHITest, AcceptsEmptyPipelineLayout)
{
    rhi::PipelineLayoutDesc desc;
    const auto layout = rhi.CreatePipelineLayout(desc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_TRUE(layout.desc.layouts.empty());
    rhi.DestroyPipelineLayout(layout);
}

TEST_F(RHITest, RejectsPipelineLayoutWithInvalidLayout)
{
    rhi::PipelineLayoutDesc desc;
    rhi::BindGroupLayoutHandle bad;
    bad.id = 0xFFFFFFFFFFFFFFFFull;
    desc.layouts.push_back(bad);
    const auto layout = rhi.CreatePipelineLayout(desc);
    EXPECT_EQ(layout.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, CreateTriangleGraphicsPipeline)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    const auto pipeline = rhi.CreateGraphicsPipeline(
        MakeTrianglePipelineDesc(layout, vs, fs));
    EXPECT_NE(pipeline.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, RejectsGraphicsPipelineWithWireframe)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    auto desc = MakeTrianglePipelineDesc(layout, vs, fs);
    desc.fillMode = rhi::FillMode::Wireframe;
    const auto pipeline = rhi.CreateGraphicsPipeline(desc);
    EXPECT_EQ(pipeline.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, RejectsGraphicsPipelineWithoutTargets)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    auto desc = MakeTrianglePipelineDesc(layout, vs, fs);
    desc.colorTargets.clear();
    const auto pipeline = rhi.CreateGraphicsPipeline(desc);
    EXPECT_EQ(pipeline.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, DestroyAndReusePipelineLayout)
{
    rhi::BindGroupLayoutDesc bindDesc;
    bindDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto bindLayout = rhi.CreateBindGroupLayout(bindDesc);
    ASSERT_NE(bindLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc desc;
    desc.layouts.push_back(bindLayout);
    const auto handle = rhi.CreatePipelineLayout(desc);
    ASSERT_NE(handle.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyPipelineLayout(handle);

    // Double destroy is safe.
    rhi.DestroyPipelineLayout(handle);

    const auto reused = rhi.CreatePipelineLayout(desc);
    ASSERT_NE(reused.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_EQ(reused.id, handle.id);

    rhi.DestroyPipelineLayout(reused);
    rhi.DestroyBindGroupLayout(bindLayout);
}

TEST_F(RHITest, DestroyAndReusePipeline)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    const auto handle = rhi.CreateGraphicsPipeline(
        MakeTrianglePipelineDesc(layout, vs, fs));
    ASSERT_NE(handle.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyPipeline(handle);

    // Double destroy is safe.
    rhi.DestroyPipeline(handle);

    const auto reused = rhi.CreateGraphicsPipeline(
        MakeTrianglePipelineDesc(layout, vs, fs));
    ASSERT_NE(reused.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_EQ(reused.id, handle.id);

    rhi.DestroyPipeline(reused);
    rhi.DestroyPipelineLayout(layout);
    rhi.DestroyBindGroupLayout(uniformLayout);
    rhi.DestroyShaderModule(vs);
    rhi.DestroyShaderModule(fs);
}

TEST_F(RHITest, CreatePipelineWithBlendDepthAndCull)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    auto desc = MakeTrianglePipelineDesc(layout, vs, fs);
    desc.cullMode = rhi::CullMode::Back;
    desc.depthStencil.format = rhi::TextureFormat::Depth32Float;
    desc.depthStencil.depthTestEnabled = true;
    desc.depthStencil.depthWriteEnabled = true;
    desc.colorTargets[0].blend.enabled = true;
    const auto pipeline = rhi.CreateGraphicsPipeline(desc);
    EXPECT_NE(pipeline.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyPipeline(pipeline);
    rhi.DestroyPipelineLayout(layout);
    rhi.DestroyBindGroupLayout(uniformLayout);
    rhi.DestroyShaderModule(vs);
    rhi.DestroyShaderModule(fs);
}
#endif

#if defined(RHI_VULKAN)
// ============================================================================
// Pipeline Layout + Graphics Pipeline Tests (Vulkan)
// ============================================================================

namespace
{
rhi::GraphicsPipelineDesc MakeTrianglePipelineDesc(
    rhi::PipelineLayoutHandle layout,
    rhi::ShaderModuleHandle vs,
    rhi::ShaderModuleHandle fs)
{
    rhi::GraphicsPipelineDesc desc;
    desc.layout = layout;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;

    rhi::VertexBufferLayout buffer;
    buffer.stride = 20;
    buffer.attributes.push_back({ 0, 0, rhi::VertexFormat::Float32x3, 0 });
    buffer.attributes.push_back({ 0, 1, rhi::VertexFormat::Float32x2, 12 });
    desc.vertexBuffers.push_back(buffer);

    rhi::ColorTarget target;
    target.format = rhi::TextureFormat::RGBA8Unorm;
    desc.colorTargets.push_back(target);
    return desc;
}
}

TEST_F(RHITest, CreatePipelineLayout)
{
    rhi::BindGroupLayoutDesc bindDesc;
    bindDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto bindLayout = rhi.CreateBindGroupLayout(bindDesc);
    ASSERT_NE(bindLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc desc;
    desc.layouts.push_back(bindLayout);
    const auto layout = rhi.CreatePipelineLayout(desc);

    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);
    ASSERT_EQ(layout.desc.layouts.size(), 1u);
    EXPECT_EQ(layout.desc.layouts[0].id, bindLayout.id);
}

TEST_F(RHITest, AcceptsEmptyPipelineLayout)
{
    rhi::PipelineLayoutDesc desc;
    const auto layout = rhi.CreatePipelineLayout(desc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_TRUE(layout.desc.layouts.empty());
    rhi.DestroyPipelineLayout(layout);
}

TEST_F(RHITest, RejectsPipelineLayoutWithInvalidLayout)
{
    rhi::PipelineLayoutDesc desc;
    rhi::BindGroupLayoutHandle bad;
    bad.id = 0xFFFFFFFFFFFFFFFFull;
    desc.layouts.push_back(bad);
    const auto layout = rhi.CreatePipelineLayout(desc);
    EXPECT_EQ(layout.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, CreateTriangleGraphicsPipeline)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    const auto pipeline = rhi.CreateGraphicsPipeline(
        MakeTrianglePipelineDesc(layout, vs, fs));
    EXPECT_NE(pipeline.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, RejectsGraphicsPipelineWithWireframe)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    auto desc = MakeTrianglePipelineDesc(layout, vs, fs);
    desc.fillMode = rhi::FillMode::Wireframe;
    const auto pipeline = rhi.CreateGraphicsPipeline(desc);
    EXPECT_EQ(pipeline.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, RejectsGraphicsPipelineWithoutTargets)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    auto desc = MakeTrianglePipelineDesc(layout, vs, fs);
    desc.colorTargets.clear();
    const auto pipeline = rhi.CreateGraphicsPipeline(desc);
    EXPECT_EQ(pipeline.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, DestroyAndReusePipelineLayout)
{
    rhi::BindGroupLayoutDesc bindDesc;
    bindDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto bindLayout = rhi.CreateBindGroupLayout(bindDesc);
    ASSERT_NE(bindLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc desc;
    desc.layouts.push_back(bindLayout);
    const auto handle = rhi.CreatePipelineLayout(desc);
    ASSERT_NE(handle.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyPipelineLayout(handle);

    // Double destroy is safe.
    rhi.DestroyPipelineLayout(handle);

    const auto reused = rhi.CreatePipelineLayout(desc);
    ASSERT_NE(reused.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_EQ(reused.id, handle.id);

    rhi.DestroyPipelineLayout(reused);
    rhi.DestroyBindGroupLayout(bindLayout);
}

TEST_F(RHITest, DestroyAndReusePipeline)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    const auto handle = rhi.CreateGraphicsPipeline(
        MakeTrianglePipelineDesc(layout, vs, fs));
    ASSERT_NE(handle.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyPipeline(handle);

    // Double destroy is safe.
    rhi.DestroyPipeline(handle);

    const auto reused = rhi.CreateGraphicsPipeline(
        MakeTrianglePipelineDesc(layout, vs, fs));
    ASSERT_NE(reused.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_EQ(reused.id, handle.id);

    rhi.DestroyPipeline(reused);
    rhi.DestroyPipelineLayout(layout);
    rhi.DestroyBindGroupLayout(uniformLayout);
    rhi.DestroyShaderModule(vs);
    rhi.DestroyShaderModule(fs);
}

TEST_F(RHITest, CreatePipelineWithBlendDepthAndCull)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    auto desc = MakeTrianglePipelineDesc(layout, vs, fs);
    desc.cullMode = rhi::CullMode::Back;
    desc.depthStencil.format = rhi::TextureFormat::Depth32Float;
    desc.depthStencil.depthTestEnabled = true;
    desc.depthStencil.depthWriteEnabled = true;
    desc.colorTargets[0].blend.enabled = true;
    const auto pipeline = rhi.CreateGraphicsPipeline(desc);
    EXPECT_NE(pipeline.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyPipeline(pipeline);
    rhi.DestroyPipelineLayout(layout);
    rhi.DestroyBindGroupLayout(uniformLayout);
    rhi.DestroyShaderModule(vs);
    rhi.DestroyShaderModule(fs);
}
#endif

#if defined(RHI_DX12)
// ============================================================================
// Pipeline Layout + Graphics Pipeline Tests (DX12)
// ============================================================================

namespace
{
rhi::GraphicsPipelineDesc MakeTrianglePipelineDesc(
    rhi::PipelineLayoutHandle layout,
    rhi::ShaderModuleHandle vs,
    rhi::ShaderModuleHandle fs)
{
    rhi::GraphicsPipelineDesc desc;
    desc.layout = layout;
    desc.vertexShader = vs;
    desc.fragmentShader = fs;

    rhi::VertexBufferLayout buffer;
    buffer.stride = 20;
    buffer.attributes.push_back({ 0, 0, rhi::VertexFormat::Float32x3, 0 });
    buffer.attributes.push_back({ 0, 1, rhi::VertexFormat::Float32x2, 12 });
    desc.vertexBuffers.push_back(buffer);

    rhi::ColorTarget target;
    target.format = rhi::TextureFormat::RGBA8Unorm;
    desc.colorTargets.push_back(target);
    return desc;
}
}

TEST_F(RHITest, CreatePipelineLayout)
{
    rhi::BindGroupLayoutDesc bindDesc;
    bindDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto bindLayout = rhi.CreateBindGroupLayout(bindDesc);
    ASSERT_NE(bindLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc desc;
    desc.layouts.push_back(bindLayout);
    const auto layout = rhi.CreatePipelineLayout(desc);

    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);
    ASSERT_EQ(layout.desc.layouts.size(), 1u);
    EXPECT_EQ(layout.desc.layouts[0].id, bindLayout.id);
}

TEST_F(RHITest, AcceptsEmptyPipelineLayout)
{
    rhi::PipelineLayoutDesc desc;
    const auto layout = rhi.CreatePipelineLayout(desc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_TRUE(layout.desc.layouts.empty());
    rhi.DestroyPipelineLayout(layout);
}

TEST_F(RHITest, RejectsPipelineLayoutWithInvalidLayout)
{
    rhi::PipelineLayoutDesc desc;
    rhi::BindGroupLayoutHandle bad;
    bad.id = 0xFFFFFFFFFFFFFFFFull;
    desc.layouts.push_back(bad);
    const auto layout = rhi.CreatePipelineLayout(desc);
    EXPECT_EQ(layout.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, CreateTriangleGraphicsPipeline)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    const auto pipeline = rhi.CreateGraphicsPipeline(
        MakeTrianglePipelineDesc(layout, vs, fs));
    EXPECT_NE(pipeline.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, AcceptsGraphicsPipelineWithWireframe)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    auto desc = MakeTrianglePipelineDesc(layout, vs, fs);
    desc.fillMode = rhi::FillMode::Wireframe;
    const auto pipeline = rhi.CreateGraphicsPipeline(desc);
    EXPECT_NE(pipeline.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, RejectsGraphicsPipelineWithoutTargets)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    auto desc = MakeTrianglePipelineDesc(layout, vs, fs);
    desc.colorTargets.clear();
    const auto pipeline = rhi.CreateGraphicsPipeline(desc);
    EXPECT_EQ(pipeline.id, 0xFFFFFFFFFFFFFFFFull);
}

TEST_F(RHITest, DestroyAndReusePipelineLayout)
{
    rhi::BindGroupLayoutDesc bindDesc;
    bindDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto bindLayout = rhi.CreateBindGroupLayout(bindDesc);
    ASSERT_NE(bindLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc desc;
    desc.layouts.push_back(bindLayout);
    const auto handle = rhi.CreatePipelineLayout(desc);
    ASSERT_NE(handle.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyPipelineLayout(handle);

    // Double destroy is safe.
    rhi.DestroyPipelineLayout(handle);

    const auto reused = rhi.CreatePipelineLayout(desc);
    ASSERT_NE(reused.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_EQ(reused.id, handle.id);

    rhi.DestroyPipelineLayout(reused);
    rhi.DestroyBindGroupLayout(bindLayout);
}

TEST_F(RHITest, DestroyAndReusePipeline)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    const auto handle = rhi.CreateGraphicsPipeline(
        MakeTrianglePipelineDesc(layout, vs, fs));
    ASSERT_NE(handle.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyPipeline(handle);

    // Double destroy is safe.
    rhi.DestroyPipeline(handle);

    const auto reused = rhi.CreateGraphicsPipeline(
        MakeTrianglePipelineDesc(layout, vs, fs));
    ASSERT_NE(reused.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_EQ(reused.id, handle.id);

    rhi.DestroyPipeline(reused);
    rhi.DestroyPipelineLayout(layout);
    rhi.DestroyBindGroupLayout(uniformLayout);
    rhi.DestroyShaderModule(vs);
    rhi.DestroyShaderModule(fs);
}

TEST_F(RHITest, CreatePipelineWithBlendDepthAndCull)
{
    const auto vs = rhi.CompileShader(MakeShaderSource(
        "basic.vert.hlsl",
        rhi::ShaderStageFlags::Vertex));
    const auto fs = rhi.CompileShader(MakeShaderSource(
        "basic.frag.hlsl",
        rhi::ShaderStageFlags::Fragment));
    ASSERT_TRUE(rhi.GetShaderModule(vs).has_value());
    ASSERT_TRUE(rhi.GetShaderModule(fs).has_value());

    rhi::BindGroupLayoutDesc uniformDesc;
    uniformDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::UniformBuffer,
        1,
        rhi::ShaderStageFlags::Vertex});
    const auto uniformLayout = rhi.CreateBindGroupLayout(uniformDesc);
    ASSERT_NE(uniformLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(uniformLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    auto desc = MakeTrianglePipelineDesc(layout, vs, fs);
    desc.cullMode = rhi::CullMode::Back;
    desc.depthStencil.format = rhi::TextureFormat::Depth32Float;
    desc.depthStencil.depthTestEnabled = true;
    desc.depthStencil.depthWriteEnabled = true;
    desc.colorTargets[0].blend.enabled = true;
    const auto pipeline = rhi.CreateGraphicsPipeline(desc);
    EXPECT_NE(pipeline.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyPipeline(pipeline);
    rhi.DestroyPipelineLayout(layout);
    rhi.DestroyBindGroupLayout(uniformLayout);
    rhi.DestroyShaderModule(vs);
    rhi.DestroyShaderModule(fs);
}
#endif

#if defined(RHI_DX12) || defined(RHI_VULKAN) || defined(RHI_WEBGPU)
// ============================================================================
// CpuToGpu vertex buffer write path. Vulkan and DirectX 12 map these
// directly; WebGPU stages through a throwaway upload buffer, so mapping one
// is rejected and writes go through instead.
// ============================================================================

TEST_F(RHITest, WriteThenMapCpuToGpuVertexBuffer)
{
    constexpr std::uint64_t kSize = 64;
    std::uint8_t pattern[kSize];
    for (std::uint64_t i = 0; i < kSize; ++i) {
        pattern[i] = static_cast<std::uint8_t>(i * 5 + 3);
    }

    rhi::BufferDesc desc;
    desc.size = kSize;
    desc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
    desc.usage = rhi::BufferUsage::Vertex;
    auto buffer = rhi.CreateBuffer(desc);
    ASSERT_NE(buffer.id, InvalidHandle);

    // Must not crash on any backend.
    rhi.WriteBuffer(buffer, pattern, sizeof(pattern));

#if defined(RHI_WEBGPU)
    EXPECT_EQ(rhi.MapBuffer(buffer), nullptr);
#else
    auto* mapped = static_cast<std::uint8_t*>(rhi.MapBuffer(buffer));
    ASSERT_NE(mapped, nullptr);
    EXPECT_EQ(std::memcmp(mapped, pattern, kSize), 0);
    rhi.UnmapBuffer(buffer);
#endif

    rhi.DestroyBuffer(buffer);
}

// ============================================================================
// Compute Pipeline Tests (all backends)
// ============================================================================

namespace
{
rhi::ShaderSource MakeComputeShaderSource()
{
    rhi::ShaderSource source;
#ifdef RHI_SHADER_DIR
    source.path = std::filesystem::path(RHI_SHADER_DIR) / "basic.comp.hlsl";
#else
    source.path = std::filesystem::path("src/shaders") / "basic.comp.hlsl";
#endif
    source.stage = rhi::ShaderStageFlags::Compute;
    source.entryPoint = "main";
    return source;
}
}

TEST_F(RHITest, CreateComputePipeline)
{
    const auto cs = rhi.CompileShader(MakeComputeShaderSource());
    ASSERT_TRUE(rhi.GetShaderModule(cs).has_value());

    // basic.comp.hlsl writes a storage texture at set 0, binding 0.
    rhi::BindGroupLayoutDesc storageDesc;
    storageDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::StorageTexture,
        1,
        rhi::ShaderStageFlags::Compute});
    const auto storageLayout = rhi.CreateBindGroupLayout(storageDesc);
    ASSERT_NE(storageLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(storageLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::ComputePipelineDesc desc;
    desc.layout = layout;
    desc.computeShader = cs;
    const auto pipeline = rhi.CreateComputePipeline(desc);
    EXPECT_NE(pipeline.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyComputePipeline(pipeline);
    rhi.DestroyPipelineLayout(layout);
    rhi.DestroyBindGroupLayout(storageLayout);
    rhi.DestroyShaderModule(cs);
}

TEST_F(RHITest, RejectsComputePipelineWithInvalidLayout)
{
    const auto cs = rhi.CompileShader(MakeComputeShaderSource());
    ASSERT_TRUE(rhi.GetShaderModule(cs).has_value());

    rhi::ComputePipelineDesc desc;
    desc.layout.id = 0xFFFFFFFFFFFFFFFFull;
    desc.computeShader = cs;
    const auto pipeline = rhi.CreateComputePipeline(desc);
    EXPECT_EQ(pipeline.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyShaderModule(cs);
}

TEST_F(RHITest, RejectsComputePipelineWithInvalidShader)
{
    rhi::PipelineLayoutDesc layoutDesc;
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::ComputePipelineDesc desc;
    desc.layout = layout;
    const auto pipeline = rhi.CreateComputePipeline(desc);
    EXPECT_EQ(pipeline.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyPipelineLayout(layout);
}

TEST_F(RHITest, DestroyAndReuseComputePipeline)
{
    const auto cs = rhi.CompileShader(MakeComputeShaderSource());
    ASSERT_TRUE(rhi.GetShaderModule(cs).has_value());

    // basic.comp.hlsl writes a storage texture at set 0, binding 0.
    rhi::BindGroupLayoutDesc storageDesc;
    storageDesc.shaderBindings.push_back({
        0,
        0,
        rhi::BindingType::StorageTexture,
        1,
        rhi::ShaderStageFlags::Compute});
    const auto storageLayout = rhi.CreateBindGroupLayout(storageDesc);
    ASSERT_NE(storageLayout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::PipelineLayoutDesc layoutDesc;
    layoutDesc.layouts.push_back(storageLayout);
    const auto layout = rhi.CreatePipelineLayout(layoutDesc);
    ASSERT_NE(layout.id, 0xFFFFFFFFFFFFFFFFull);

    rhi::ComputePipelineDesc desc;
    desc.layout = layout;
    desc.computeShader = cs;
    const auto handle = rhi.CreateComputePipeline(desc);
    ASSERT_NE(handle.id, 0xFFFFFFFFFFFFFFFFull);

    rhi.DestroyComputePipeline(handle);

    // Double destroy is safe.
    rhi.DestroyComputePipeline(handle);

    const auto reused = rhi.CreateComputePipeline(desc);
    ASSERT_NE(reused.id, 0xFFFFFFFFFFFFFFFFull);
    EXPECT_EQ(reused.id, handle.id);

    rhi.DestroyComputePipeline(reused);
    rhi.DestroyPipelineLayout(layout);
    rhi.DestroyBindGroupLayout(storageLayout);
    rhi.DestroyShaderModule(cs);
}
#endif


// ============================================================================
// Hidden native windows for surface/swapchain tests. Windowing belongs to the
// test executable; the RHI only ever sees the native handle.
// ============================================================================

namespace
{
#if defined(_WIN32)
LRESULT CALLBACK TestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool EnsureTestWindowClass()
{
    static bool registered = false;
    if (registered) {
        return true;
    }
    WNDCLASSW wc{};
    wc.lpfnWndProc = TestWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"RhiTestHidden";
    if (RegisterClassW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }
    registered = true;
    return true;
}
#endif

HWND MakeHiddenWindow(
    std::uint32_t width = 640,
    std::uint32_t height = 480)
{
#if defined(_WIN32)
    if (!EnsureTestWindowClass()) {
        return nullptr;
    }
    return CreateWindowExW(
        0, L"RhiTestHidden", L"rhi_test",
        WS_POPUP,
        0, 0, static_cast<int>(width), static_cast<int>(height),
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
#else
    (void)width;
    (void)height;
    return nullptr;
#endif
}

void DestroyTestWindow(HWND hwnd)
{
#if defined(_WIN32)
    if (hwnd != nullptr) {
        DestroyWindow(hwnd);
    }
#else
    (void)hwnd;
#endif
}

rhi::NativeWindow MakeNativeWindow(HWND hwnd)
{
    rhi::NativeWindow native;
#if defined(_WIN32)
    native.type = rhi::NativeWindowType::Win32;
    native.handle = hwnd;
    native.display = GetModuleHandle(nullptr);
#else
    (void)hwnd;
#endif
    return native;
}
}


// ============================================================================
// Surface Tests (all backends; surfaces are built over hidden windows)
// ============================================================================

namespace
{
rhi::SurfaceDesc MakeSurfaceDesc(rhi::NativeWindow window)
{
    rhi::SurfaceDesc desc;
    desc.window = window;
    return desc;
}
}

TEST_F(RHITest, CreateSurfaceFromHiddenWindow)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);

    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));

    ASSERT_NE(surface.id, InvalidHandle);
    EXPECT_EQ(surface.desc.window.handle, window.handle);

    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, RejectsSurfaceWithInvalidWindow)
{
    rhi::SurfaceDesc desc;
    auto surface = rhi.CreateSurface(desc);

    EXPECT_EQ(surface.id, InvalidHandle);
}

TEST_F(RHITest, CreateMultipleSurfaces)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);

    auto surface1 = rhi.CreateSurface(MakeSurfaceDesc(window));
    auto surface2 = rhi.CreateSurface(MakeSurfaceDesc(window));

    ASSERT_NE(surface1.id, InvalidHandle);
    ASSERT_NE(surface2.id, InvalidHandle);
    EXPECT_NE(surface1.id, surface2.id);

    rhi.DestroySurface(surface2);
    rhi.DestroySurface(surface1);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, DestroySurfaceIsIdempotent)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);

    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);

    rhi.DestroySurface(surface);

    // Double destroy should not crash.
    rhi.DestroySurface(surface);

    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, ReusesSurfaceSlotAfterDestroy)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);

    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);
    rhi.DestroySurface(surface);

    auto reused = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(reused.id, InvalidHandle);
    EXPECT_EQ(reused.id, surface.id);

    rhi.DestroySurface(reused);
    DestroyTestWindow(hwnd);
}

#if defined(RHI_VULKAN) || defined(RHI_WEBGPU) || defined(RHI_DX12)
namespace
{
rhi::SwapchainDesc MakeSwapchainDesc(rhi::SurfaceHandle surface)
{
    rhi::SwapchainDesc desc;
    desc.surface = surface;
    desc.width = 640;
    desc.height = 480;
    desc.imageCount = 2;
    return desc;
}
}
#endif

#if defined(RHI_VULKAN)
// ============================================================================
// Swapchain Tests (Vulkan; surfaces are built over hidden windows)
// ============================================================================

TEST_F(RHITest, CreateSwapchainFromHiddenWindowSurface)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);

    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));

    ASSERT_NE(swapchain.id, InvalidHandle);
    EXPECT_EQ(swapchain.desc.surface.id, surface.id);
    EXPECT_NE(swapchain.desc.width, 0u);
    EXPECT_NE(swapchain.desc.height, 0u);

    rhi.DestroySwapchain(swapchain);
    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, RejectsSwapchainWithInvalidSurface)
{
    rhi::SwapchainDesc desc;
    auto swapchain = rhi.CreateSwapchain(desc);

    EXPECT_EQ(swapchain.id, InvalidHandle);
}

TEST_F(RHITest, RejectsZeroSizedSwapchain)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);

    auto desc = MakeSwapchainDesc(surface);
    desc.width = 0;
    desc.height = 0;
    auto swapchain = rhi.CreateSwapchain(desc);

    EXPECT_EQ(swapchain.id, InvalidHandle);

    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, AcquireAndPresentSwapchainImage)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);
    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(swapchain.id, InvalidHandle);

    auto acquired = rhi.AcquireNextImage(swapchain);
    EXPECT_TRUE(acquired.status == rhi::AcquireStatus::Success ||
                acquired.status == rhi::AcquireStatus::Suboptimal)
        << "Acquire status: " << static_cast<int>(acquired.status);
    ASSERT_NE(acquired.imageIndex, 0xFFFFFFFFu);

    EXPECT_NE(rhi.GetSwapchainImage(swapchain, acquired.imageIndex), VK_NULL_HANDLE);
    EXPECT_NE(rhi.GetSwapchainImageView(swapchain, acquired.imageIndex), VK_NULL_HANDLE);

    // Should not crash.
    rhi.Present(swapchain);

    rhi.DestroySwapchain(swapchain);
    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, DestroySwapchainIsIdempotent)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);
    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(swapchain.id, InvalidHandle);

    rhi.DestroySwapchain(swapchain);

    // Double destroy should not crash.
    rhi.DestroySwapchain(swapchain);

    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, ReusesSwapchainSlotAfterDestroy)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);

    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(swapchain.id, InvalidHandle);
    rhi.DestroySwapchain(swapchain);

    auto reused = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(reused.id, InvalidHandle);
    EXPECT_EQ(reused.id, swapchain.id);

    rhi.DestroySwapchain(reused);
    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}
#endif

#if defined(RHI_DX12)
// ============================================================================
// Swapchain Tests (DirectX 12; surfaces are built over hidden windows)
// ============================================================================

TEST_F(RHITest, CreateSwapchainFromHiddenWindowSurface)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);

    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));

    ASSERT_NE(swapchain.id, InvalidHandle);
    EXPECT_EQ(swapchain.desc.surface.id, surface.id);

    rhi.DestroySwapchain(swapchain);
    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, RejectsSwapchainWithInvalidSurface)
{
    rhi::SwapchainDesc desc;
    auto swapchain = rhi.CreateSwapchain(desc);

    EXPECT_EQ(swapchain.id, InvalidHandle);
}

TEST_F(RHITest, RejectsZeroSizedSwapchain)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);

    auto desc = MakeSwapchainDesc(surface);
    desc.width = 0;
    desc.height = 0;
    auto swapchain = rhi.CreateSwapchain(desc);

    EXPECT_EQ(swapchain.id, InvalidHandle);

    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, AcquireAndPresentSwapchainImage)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);
    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(swapchain.id, InvalidHandle);

    auto acquired = rhi.AcquireNextImage(swapchain);
    EXPECT_EQ(acquired.status, rhi::AcquireStatus::Success);
    ASSERT_NE(acquired.imageIndex, 0xFFFFFFFFu);
    EXPECT_NE(rhi.GetSwapchainBuffer(swapchain, acquired.imageIndex), nullptr);

    // Should not crash.
    rhi.Present(swapchain);

    rhi.DestroySwapchain(swapchain);
    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, DestroySwapchainIsIdempotent)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);
    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(swapchain.id, InvalidHandle);

    rhi.DestroySwapchain(swapchain);

    // Double destroy should not crash.
    rhi.DestroySwapchain(swapchain);

    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, ReusesSwapchainSlotAfterDestroy)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);

    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(swapchain.id, InvalidHandle);
    rhi.DestroySwapchain(swapchain);

    auto reused = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(reused.id, InvalidHandle);
    EXPECT_EQ(reused.id, swapchain.id);

    rhi.DestroySwapchain(reused);
    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}
#endif

#if defined(RHI_WEBGPU)
// ============================================================================
// Swapchain Tests (WebGPU; surfaces are built over hidden windows)
// ============================================================================

TEST_F(RHITest, CreateSwapchainFromHiddenWindowSurface)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);

    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));

    ASSERT_NE(swapchain.id, InvalidHandle);
    EXPECT_EQ(swapchain.desc.surface.id, surface.id);

    rhi.DestroySwapchain(swapchain);
    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, RejectsSwapchainWithInvalidSurface)
{
    rhi::SwapchainDesc desc;
    auto swapchain = rhi.CreateSwapchain(desc);

    EXPECT_EQ(swapchain.id, InvalidHandle);
}

TEST_F(RHITest, RejectsZeroSizedSwapchain)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);

    auto desc = MakeSwapchainDesc(surface);
    desc.width = 0;
    desc.height = 0;
    auto swapchain = rhi.CreateSwapchain(desc);

    EXPECT_EQ(swapchain.id, InvalidHandle);

    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, AcquireAndPresentSwapchainImage)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);
    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(swapchain.id, InvalidHandle);

    auto acquired = rhi.AcquireNextImage(swapchain);
    EXPECT_TRUE(acquired.status == rhi::AcquireStatus::Success ||
                acquired.status == rhi::AcquireStatus::Suboptimal)
        << "Acquire status: " << static_cast<int>(acquired.status);
    EXPECT_EQ(acquired.imageIndex, 0u);
    EXPECT_TRUE(rhi.GetSwapchainTexture(swapchain));

    // Should not crash.
    rhi.Present(swapchain);

    rhi.DestroySwapchain(swapchain);
    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, DestroySwapchainIsIdempotent)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);
    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(swapchain.id, InvalidHandle);

    rhi.DestroySwapchain(swapchain);

    // Double destroy should not crash.
    rhi.DestroySwapchain(swapchain);

    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}

TEST_F(RHITest, ReusesSwapchainSlotAfterDestroy)
{
    HWND hwnd = MakeHiddenWindow();
    ASSERT_NE(hwnd, nullptr);
    auto window = MakeNativeWindow(hwnd);
    auto surface = rhi.CreateSurface(MakeSurfaceDesc(window));
    ASSERT_NE(surface.id, InvalidHandle);

    auto swapchain = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(swapchain.id, InvalidHandle);
    rhi.DestroySwapchain(swapchain);

    auto reused = rhi.CreateSwapchain(MakeSwapchainDesc(surface));
    ASSERT_NE(reused.id, InvalidHandle);
    EXPECT_EQ(reused.id, swapchain.id);

    rhi.DestroySwapchain(reused);
    rhi.DestroySurface(surface);
    DestroyTestWindow(hwnd);
}
#endif
