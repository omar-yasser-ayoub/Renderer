#pragma once

#if defined(_WIN32) && !defined(VK_USE_PLATFORM_WIN32_KHR)
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#if defined(__linux__) && !defined(__ANDROID__)
#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#include "rhi/resources/capabilities.h"
#include "rhi/resources/buffer.h"
#include "rhi/resources/texture.h"
#include <rhi/resources/shader.h>
#include <rhi/resources/bindgroup.h>
#include <rhi/resources/pipeline.h>
#include <rhi/resources/swapchain.h>
#include <rhi/resources/surface.h>
#include "rhi/resources/renderpass.h"
#include "rhi/resources/commands.h"

#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>
#include <array>
#include <optional>
#include <span>
#include <vector>
#if defined(_WIN32)
#include <Windows.h>
#endif
#include <third_party/dxc/inc/dxcapi.h>
#include <third_party/vma/vk_mem_alloc.h>
#include <wrl/client.h>

struct SpvReflectShaderModule;


class VulkanRHI
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
    VkImage GetSwapchainImage(rhi::SwapchainHandle handle, std::uint32_t imageIndex) const;
    VkImageView GetSwapchainImageView(rhi::SwapchainHandle handle, std::uint32_t imageIndex) const;
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
    rhi::ShaderReflection ParseReflection(
        SpvReflectShaderModule* reflection,
        rhi::ShaderStageFlags stage);

    VkFilter toVk(rhi::Filter mode);
    VkSamplerMipmapMode toVk(rhi::MipmapFilter mode);
    VkSamplerAddressMode toVk(rhi::AddressMode mode);
    VkCompareOp toVk(rhi::CompareOp mode);
    VkBorderColor toVk(rhi::BorderColor mode);
    VkBufferUsageFlags toVk(rhi::BufferUsage usage);
    VkMemoryPropertyFlags toVk(rhi::MemoryUsage usage);
    VkImageViewType toVk(rhi::TextureViewDimension type, int layerCount);
    VkFormat toVk(rhi::TextureFormat format);
    VkImageAspectFlags toVk(rhi::TextureAspect aspect);
    VkImageAspectFlags toVkAspect(rhi::TextureFormat format);
    VkImageType toVk(rhi::TextureDimension type);
    VkSampleCountFlagBits toVk(rhi::SampleCount count);
    VkImageUsageFlags toVk(rhi::TextureUsage usage);
    VkFormat toVk(rhi::VertexFormat format);
    VkPrimitiveTopology toVk(rhi::PrimitiveTopology topology);
    VkCullModeFlags toVk(rhi::CullMode mode);
    VkFrontFace toVk(rhi::FrontFace face);
    VkPolygonMode toVk(rhi::FillMode mode);
    VkBlendFactor toVk(rhi::BlendFactor factor);
    VkBlendOp toVk(rhi::BlendOp op);
    VkColorComponentFlags toVk(rhi::ColorWriteMask mask);
    VkPresentModeKHR toVk(rhi::PresentMode mode);
    VmaAllocationCreateInfo ToVmaAllocInfo(rhi::MemoryUsage memoryUsage);
    void TransitionImage(VkCommandBuffer buffer, VkImage image, VkImageAspectFlags aspect, VkImageLayout target, VkImageLayout& stored);
    static VkAccessFlags accessForLayout(VkImageLayout layout);
    static std::uint32_t TexelBytes(rhi::TextureFormat format);

    rhi::Capabilities m_Capabilities;
    VkInstance m_Instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_ComputeQueue = VK_NULL_HANDLE;
    std::uint32_t m_GraphicsQueueFamily = UINT32_MAX;
    std::uint32_t m_ComputeQueueFamily = UINT32_MAX;
    float m_MaxSamplerAnisotropy = 1.0f;
    VmaAllocator m_Allocator = nullptr;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
    std::vector<VkSampler> m_Samplers;
    std::vector<std::uint64_t> m_FreeSamplers;
    std::vector<VkBuffer> m_Buffers;
    std::vector<VmaAllocation> m_BufferMemory;
    std::vector<std::uint64_t> m_FreeBuffers;
    std::vector<VkImage> m_Textures;
    std::vector<VmaAllocation> m_TextureMemory;
    std::vector<VkImageLayout> m_TextureLayouts;
    std::vector<std::uint64_t> m_FreeTextures;
    std::vector<VkImageView> m_TextureViews;
    std::vector<std::uint64_t> m_FreeTextureViews;
    std::vector<rhi::ShaderModule> m_ShaderModules;
    std::vector<std::uint64_t> m_FreeShaderModules;
    std::vector<VkDescriptorSetLayout> m_BindGroupLayouts;
    std::vector<std::uint64_t> m_FreeBindGroupLayouts;
    std::vector<VkDescriptorSet> m_BindGroups;
    std::vector<std::uint64_t> m_FreeBindGroups;
    std::vector<VkPipelineLayout> m_PipelineLayouts;
    std::vector<std::uint64_t> m_FreePipelineLayouts;
    std::vector<VkPipeline> m_Pipelines;
    std::vector<std::uint64_t> m_FreePipelines;
    std::vector<VkPipeline> m_ComputePipelines;
    std::vector<std::uint64_t> m_FreeComputePipelines;
    std::vector<VkSurfaceKHR> m_Surfaces;
    std::vector<std::uint64_t> m_FreeSurfaces;
    struct VkSwapchainData {
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        std::vector<VkImage> images;
        std::vector<VkImageView> views;
        std::vector<VkImageLayout> layouts;
        std::vector<VkSemaphore> renderFinished;
        std::uint32_t lastImageIndex = 0xFFFFFFFF;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent2D extent{};
    };
    std::vector<VkSwapchainData> m_Swapchains;
    std::vector<uint64_t> m_FreeSwapchains;

    struct FrameContext {
        VkCommandPool pool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> cmds;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        std::vector<VkFence> pendingFences;
        bool submitted = false;
        std::uint64_t acquiredSwapchain = 0xFFFFFFFFFFFFFFFFull;
        std::uint32_t acquiredImage = 0xFFFFFFFF;
    };
    std::array<FrameContext, 2> m_Frames;
    std::uint64_t m_FrameCounter = 0;
    std::uint32_t m_CurrentFrame = 0xFFFFFFFF;
    VkCommandPool m_TransferPool = VK_NULL_HANDLE;
    VkCommandBuffer m_UploadBuffer = VK_NULL_HANDLE;
    bool m_UploadHasWork = false;
    VkFence m_UploadFence = VK_NULL_HANDLE;
    bool EnsureUploadReady();
    void FlushUploads(bool wait);
    static constexpr std::size_t kStagingRingSize = 64 * 1024 * 1024;
    VkBuffer m_StagingBuffer = VK_NULL_HANDLE;
    VmaAllocation m_StagingMemory = nullptr;
    void* m_StagingMapped = nullptr;
    std::size_t m_StagingHead = 0;
    bool AllocStaging(std::size_t size, std::size_t align, std::size_t& outOffset);
    enum class CurrentPass {
        None,
        Graphics,
        Compute
    };
    struct VulkanCommandList {
        std::uint32_t frame = 0xFFFFFFFF;
        std::uint32_t bufferIndex = 0xFFFFFFFF;
        bool open = false;

        CurrentPass pass = CurrentPass::None;
        VkPipelineLayout boundLayout = VK_NULL_HANDLE;
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        std::uint64_t swapchain = 0xFFFFFFFFFFFFFFFFull;
        std::uint32_t swapchainImageIndex = 0xFFFFFFFF;
    };
    std::vector<VulkanCommandList> m_CommandLists;
    std::vector<std::uint64_t> m_FreeCommandLists;
    VkCommandBuffer GetVkCommandBuffer(rhi::CommandListHandle handle);
    void TransitionTexture(VkCommandBuffer cmd, const rhi::TextureHandle& texture,
                           VkImageAspectFlags aspect, VkImageLayout target);

    bool CreateFrameContexts();
    void DestroyFrameContexts();
    void DestroyInstance();
};
