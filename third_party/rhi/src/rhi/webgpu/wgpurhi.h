#pragma once

#include "rhi/resources/capabilities.h"
#include "rhi/resources/buffer.h"
#include "rhi/resources/shader.h"
#include "rhi/resources/texture.h"
#include "rhi/resources/bindgroup.h"
#include "rhi/resources/pipeline.h"
#include "rhi/resources/swapchain.h"
#include "rhi/resources/surface.h"
#include "rhi/resources/renderpass.h"
#include "rhi/resources/commands.h"
#include <webgpu/webgpu_cpp.h>

#include <cstdint>
#include <optional>
#include <span>

#include <cstdint>
#include <unordered_map>

struct SpvReflectShaderModule;

class WebGPURHI
{
public:
    bool Initialize();
    void Shutdown();

    const rhi::Capabilities& GetCapabilities() const
    {
        return m_Capabilities;
    }

    rhi::SamplerHandle CreateSampler(const rhi::SamplerDesc& desc);
    rhi::TextureHandle CreateTexture(const rhi::TextureDesc& desc);
    rhi::BufferHandle CreateBuffer(const rhi::BufferDesc& desc);
    rhi::TextureViewHandle CreateTextureView(const rhi::TextureViewDesc& desc, rhi::TextureHandle texture);
    rhi::ShaderModuleHandle CompileShader(rhi::ShaderSource source);
    std::optional<rhi::ShaderModule> GetShaderModule(rhi::ShaderModuleHandle handle) const;
    rhi::BindGroupLayoutHandle CreateBindGroupLayout(const rhi::BindGroupLayoutDesc& desc);
    rhi::BindGroupHandle CreateBindGroup(const rhi::BindGroupDesc& desc);
    rhi::PipelineLayoutHandle CreatePipelineLayout(const rhi::PipelineLayoutDesc& desc);
    rhi::GraphicsPipelineHandle CreateGraphicsPipeline(const rhi::GraphicsPipelineDesc& desc);
    rhi::ComputePipelineHandle CreateComputePipeline(const rhi::ComputePipelineDesc& desc);
    rhi::SurfaceHandle CreateSurface(const rhi::SurfaceDesc& desc);
    rhi::SwapchainHandle CreateSwapchain(const rhi::SwapchainDesc& desc);

    rhi::AcquireResult AcquireNextImage(rhi::SwapchainHandle handle);
    wgpu::Texture GetSwapchainTexture(rhi::SwapchainHandle handle) const;
    void Present(rhi::SwapchainHandle handle);

    void BeginRenderPass(rhi::CommandListHandle cmd, const rhi::RenderPassDesc& desc);
    void BeginComputePass(rhi::CommandListHandle cmd);
    void EndPass(rhi::CommandListHandle cmd);

    void SetPipeline(rhi::CommandListHandle cmd, rhi::GraphicsPipelineHandle handle);
    void SetPipeline(rhi::CommandListHandle cmd, rhi::ComputePipelineHandle handle);
    void SetBindGroup(rhi::CommandListHandle cmd, std::uint32_t slot, rhi::BindGroupHandle handle);
    void SetVertexBuffer(rhi::CommandListHandle cmd, std::uint32_t slot, rhi::BufferHandle handle, std::size_t offset = 0);
    void SetIndexBuffer(rhi::CommandListHandle cmd, rhi::BufferHandle handle, rhi::IndexFormat format, std::size_t offset = 0);
    void SetViewport(rhi::CommandListHandle cmd, float x, float y, float width, float height);
    void SetScissor(rhi::CommandListHandle cmd, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height);

    void Draw(rhi::CommandListHandle cmd, std::uint32_t vertexCount, std::uint32_t instanceCount,
              std::uint32_t firstVertex, std::uint32_t firstInstance);
    void DrawIndexed(rhi::CommandListHandle cmd, std::uint32_t indexCount, std::uint32_t instanceCount,
                     std::uint32_t firstIndex, std::int32_t baseVertex,
                     std::uint32_t firstInstance);
    void DrawIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset,
                      std::uint32_t drawCount, std::uint32_t stride);
    void DrawIndexedIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset,
                             std::uint32_t drawCount, std::uint32_t stride);

    void Dispatch(rhi::CommandListHandle cmd, std::uint32_t x, std::uint32_t y, std::uint32_t z);
    void DispatchIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset);

    void CopyBufferToBuffer(rhi::BufferHandle src, rhi::BufferHandle dst, std::size_t size,
                            std::size_t srcOffset = 0, std::size_t dstOffset = 0);
    void CopyBufferToTexture(rhi::BufferHandle src, rhi::TextureHandle dst,
                             const rhi::TextureWriteDesc& region);
    void CopyTextureToBuffer(rhi::TextureHandle src, rhi::BufferHandle dst,
                             const rhi::TextureWriteDesc& region);
    void CopyTextureToTexture(rhi::TextureHandle src, rhi::TextureHandle dst,
                              const rhi::TextureCopyRegion& region);
    // Submits pending upload work now (init/readback paths with no frame
    // submit). With wait=true, staging memory may be freed on return.
    void FlushTransfers(bool wait = false);

    void UploadBuffer(rhi::BufferHandle dst, std::size_t dstOffset,
                      const void* data, std::size_t size);
    void UploadTexture(rhi::TextureHandle dst, std::uint32_t mipLevel,
                       const void* data, std::uint32_t width, std::uint32_t height);

    rhi::CommandListHandle BeginCommandList();
    void Submit(std::span<const rhi::CommandListHandle> cmds);

    void DestroySampler(rhi::SamplerHandle handle);
    void DestroyTexture(rhi::TextureHandle handle);
    void DestroyBuffer(rhi::BufferHandle handle);
    void DestroyTextureView(rhi::TextureViewHandle handle);
    void DestroyBindGroupLayout(rhi::BindGroupLayoutHandle handle);
    void DestroyBindGroup(rhi::BindGroupHandle handle);
    void DestroyShaderModule(rhi::ShaderModuleHandle handle);
    void DestroyPipelineLayout(rhi::PipelineLayoutHandle handle);
    void DestroyPipeline(rhi::GraphicsPipelineHandle handle);
    void DestroyComputePipeline(rhi::ComputePipelineHandle handle);
    void DestroySurface(rhi::SurfaceHandle handle);
    void DestroySwapchain(rhi::SwapchainHandle handle);

    void WriteBuffer(rhi::BufferHandle handle, const void* data, std::size_t size, std::size_t offset = 0);

    void* MapBuffer(rhi::BufferHandle handle);
    void UnmapBuffer(rhi::BufferHandle handle);

    void WaitForIdle();

private:
    static wgpu::FilterMode toWebGPU(rhi::Filter f);
    static wgpu::MipmapFilterMode toWebGPU(rhi::MipmapFilter f);
    static wgpu::AddressMode toWebGPU(rhi::AddressMode f);
    static wgpu::CompareFunction toWebGPU(rhi::CompareOp op);
    static wgpu::BufferUsage toWebGPU(rhi::BufferUsage usage);
    static wgpu::BufferUsage toWebGPU(rhi::MemoryUsage usage);
    static wgpu::TextureDimension toWebGPU(rhi::TextureDimension dim);
    static wgpu::TextureViewDimension toWebGPU(rhi::TextureViewDimension dim);
    static wgpu::TextureAspect toWebGPU(rhi::TextureAspect aspect);
    static bool HasStencilAspect(rhi::TextureFormat format);
    static bool IsStorageCompatible(rhi::TextureFormat format);
    static wgpu::TextureFormat toWebGPU(rhi::TextureFormat usage);
    static rhi::TextureFormat fromWebGPU(wgpu::TextureFormat format);
    static rhi::PresentMode fromWebGPU(wgpu::PresentMode mode);
    static rhi::AlphaMode fromWebGPU(wgpu::CompositeAlphaMode mode);
    static uint32_t toWebGPU(rhi::SampleCount count);
    static wgpu::TextureUsage toWebGPU(rhi::TextureUsage usage);
    static wgpu::IndexFormat toWebGPU(rhi::IndexFormat format);
    static wgpu::PresentMode toWebGPU(rhi::PresentMode mode);
    static wgpu::CompositeAlphaMode toWebGPU(rhi::AlphaMode mode);
    static wgpu::VertexFormat toWebGPU(rhi::VertexFormat format);
    static wgpu::PrimitiveTopology toWebGPU(rhi::PrimitiveTopology topology);
    static wgpu::CullMode toWebGPU(rhi::CullMode mode);
    static wgpu::FrontFace toWebGPU(rhi::FrontFace face);
    static wgpu::BlendFactor toWebGPU(rhi::BlendFactor factor);
    static wgpu::BlendOperation toWebGPU(rhi::BlendOp op);
    static wgpu::ColorWriteMask toWebGPU(rhi::ColorWriteMask mask);
    static uint32_t TexelBytes(rhi::TextureFormat format);
    static rhi::ShaderReflection ParseReflection(
        SpvReflectShaderModule* reflection,
        rhi::ShaderStageFlags stage);
    wgpu::ShaderModule MakeShaderModule(const rhi::ShaderModule& module);

    rhi::Capabilities m_Capabilities;
    wgpu::Instance m_Instance;
    wgpu::Adapter  m_Adapter;
    wgpu::Device   m_Device;
    wgpu::Queue    m_Queue;
    enum class CurrentPass {
        None,
        Graphics,
        Compute,
    };
    struct WebGPUCommandList {
        wgpu::CommandEncoder encoder;
        wgpu::RenderPassEncoder renderPass;
        wgpu::ComputePassEncoder computePass;
        CurrentPass pass = CurrentPass::None;
        bool submitted = false;
    };
    std::vector<WebGPUCommandList> m_CommandLists;
    std::vector<std::uint64_t> m_FreeCommandLists;
    WebGPUCommandList* GetCommandList(rhi::CommandListHandle handle);
    wgpu::CommandEncoder m_UploadEncoder;
    bool m_UploadHasWork = false;
    std::vector<wgpu::Buffer> m_UploadStaging;
    bool EnsureUploadReady();
    void FlushUploads(bool wait);
    wgpu::Buffer MakeUploadStaging(std::size_t size, void*& mappedOut);
    std::vector<wgpu::Sampler> m_Samplers;
    std::vector<std::uint64_t> m_FreeSamplers;
    std::vector<wgpu::Buffer> m_Buffers;
    std::vector<std::uint64_t> m_FreeBuffers;
    // Tracks client-visible mapped state so Unmap/Destroy never unmap an
    // unmapped buffer (Dawn validation error).
    std::vector<bool> m_BufferMapped;

    std::vector<wgpu::Texture> m_Textures;
    std::vector<std::uint64_t> m_FreeTextures;
    std::vector<wgpu::TextureView> m_TextureViews;
    std::vector<std::uint64_t> m_FreeTextureViews;
    std::vector<rhi::ShaderModule> m_ShaderModules;
    std::vector<std::uint64_t> m_FreeShaderModules;
    std::vector<wgpu::BindGroupLayout> m_BindGroupLayouts;
    std::vector<std::uint64_t> m_FreeBindGroupLayouts;
    std::vector<wgpu::BindGroup> m_BindGroups;
    std::vector<std::uint64_t> m_FreeBindGroups;
    std::vector<wgpu::PipelineLayout> m_PipelineLayouts;
    std::vector<std::uint64_t> m_FreePipelineLayouts;
    std::vector<wgpu::RenderPipeline> m_Pipelines;
    std::vector<std::uint64_t> m_FreePipelines;
    std::vector<wgpu::ComputePipeline> m_ComputePipelines;
    std::vector<std::uint64_t> m_FreeComputePipelines;
    std::vector<wgpu::Surface> m_Surfaces;
    std::vector<std::uint64_t> m_FreeSurfaces;
    struct WebGPUSwapchainData {
        wgpu::Surface surface;
        wgpu::Texture currentTexture;
        wgpu::TextureView currentView;
        wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };
    std::vector<WebGPUSwapchainData> m_Swapchains;
    std::vector<std::uint64_t> m_FreeSwapchains;
};
