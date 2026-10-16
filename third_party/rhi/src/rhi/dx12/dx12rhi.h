#pragma once

#include "rhi/resources/capabilities.h"
#include "rhi/resources/texture.h"
#include "rhi/resources/shader.h"
#include "rhi/resources/bindgroup.h"
#include "rhi/resources/pipeline.h"
#include "rhi/resources/swapchain.h"
#include "rhi/resources/surface.h"
#include "rhi/resources/renderpass.h"
#include "rhi/resources/commands.h"

#include <span>
#include <optional>
#include <array>

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include <map>
#include <third_party/dxc/inc/dxcapi.h>

struct ID3D12ShaderReflection;

class DX12RHI
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
    rhi::TextureViewHandle CreateTextureView(const rhi::TextureViewDesc& desc, rhi::TextureHandle handle);
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
    ID3D12Resource* GetSwapchainBuffer(rhi::SwapchainHandle handle, std::uint32_t imageIndex) const;
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
    static D3D12_FILTER toD3D12(rhi::Filter min, rhi::Filter mag, rhi::MipmapFilter mip, bool anisotropic, bool compare);
    static D3D12_TEXTURE_ADDRESS_MODE toD3D12(rhi::AddressMode mode);
    static D3D12_COMPARISON_FUNC toD3D12(rhi::CompareOp mode);
    static std::array<float, 4> toD3D12(rhi::BorderColor color);
    static D3D12_HEAP_PROPERTIES toD3D12(rhi::MemoryUsage usage);
    static D3D12_RESOURCE_DIMENSION toD3D12(rhi::TextureDimension dimension);
    static DXGI_FORMAT toD3D12(rhi::TextureFormat format);
    static DXGI_ALPHA_MODE toD3D12(rhi::AlphaMode mode);
    static UINT toD3D12SyncInterval(rhi::PresentMode mode);
    static uint32_t toD3D12(rhi::SampleCount count);
    static D3D12_RESOURCE_FLAGS toD3D12(rhi::TextureUsage usage);
    static DXGI_FORMAT toD3D12Typeless(rhi::TextureFormat format);
    static std::uint32_t TexelBytes(rhi::TextureFormat format);
    static DXGI_FORMAT toD3D12DepthSrv(rhi::TextureFormat format);
    static DXGI_FORMAT toD3D12StencilSrv(rhi::TextureFormat format);
    static bool IsDepthFormat(rhi::TextureFormat format);
    static D3D12_SRV_DIMENSION toD3D12SrvDimension(rhi::TextureViewDimension dimension);
    static D3D12_UAV_DIMENSION toD3D12UavDimension(rhi::TextureViewDimension dimension);
    static D3D12_RTV_DIMENSION toD3D12RtvDimension(rhi::TextureViewDimension dimension);
    static D3D12_DSV_DIMENSION toD3D12DsvDimension(rhi::TextureViewDimension dimension);
    static DXGI_FORMAT toD3D12(rhi::VertexFormat format);
    static D3D12_PRIMITIVE_TOPOLOGY_TYPE toD3D12(rhi::PrimitiveTopology topology);
    static D3D12_PRIMITIVE_TOPOLOGY toDxTopology(rhi::PrimitiveTopology topology);
    static UINT CalcSubresource(UINT mip, UINT arraySlice, UINT planeSlice,
                                UINT mipLevels, UINT arraySize);
    static D3D12_CULL_MODE toD3D12(rhi::CullMode mode);
    static D3D12_FILL_MODE toD3D12(rhi::FillMode mode);
    static D3D12_BLEND toD3D12(rhi::BlendFactor factor);
    static D3D12_BLEND_OP toD3D12(rhi::BlendOp op);
    static UINT8 toD3D12(rhi::ColorWriteMask mask);
    static D3D12_DESCRIPTOR_RANGE_TYPE toD3D12(rhi::BindingType type);
    static D3D12_SHADER_VISIBILITY toD3D12(rhi::ShaderStageFlags stage);
    static D3D12_SHADER_VISIBILITY toD3D12(const rhi::BindGroupLayoutDesc& desc);
    static const rhi::BindGroupEntry* FindEntry(
        const rhi::BindGroupDesc& desc,
        std::uint32_t binding);
    static const rhi::ShaderBinding* FindBinding(
        const rhi::BindGroupLayoutDesc& desc,
        std::uint32_t binding);
    static rhi::ShaderReflection ParseReflection(
        ID3D12ShaderReflection* reflection,
        rhi::ShaderStageFlags stage);

    rhi::Capabilities m_Capabilities;
    Microsoft::WRL::ComPtr<IDXGIFactory6> m_Factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> m_Adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_CommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_ComputeQueue;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_SamplerHeap;
    uint32_t m_SamplerHeapOffset = 0;
    uint32_t m_SamplerDescriptorSize = 0;
    struct SamplerViewData {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        bool hasSampler = false;
    };
    std::vector<SamplerViewData> m_SamplerViews;
    std::vector<std::uint64_t> m_FreeSamplers;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_Buffers;
    struct BufferViewData {
        D3D12_CPU_DESCRIPTOR_HANDLE cbv{};
        D3D12_CPU_DESCRIPTOR_HANDLE srv{};
        D3D12_CPU_DESCRIPTOR_HANDLE uav{};

        D3D12_GPU_DESCRIPTOR_HANDLE gpuCbv{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuUav{};

        bool hasCbv = false;
        bool hasSrv = false;
        bool hasUav = false;
    };
    std::vector<BufferViewData> m_BufferViews;
    std::vector<std::uint64_t> m_FreeBuffers;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_Textures;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DsvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_CbvSrvUavHeap;
    uint32_t m_RtvHeapOffset = 0, m_RtvDescriptorSize = 0;
    uint32_t m_DsvHeapOffset = 0, m_DsvDescriptorSize = 0;
    uint32_t m_CbvSrvUavHeapOffset = 0, m_CbvSrvUavDescriptorSize = 0;
    struct TextureViewData {
        D3D12_CPU_DESCRIPTOR_HANDLE srv{};
        D3D12_CPU_DESCRIPTOR_HANDLE uav{};
        D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
        D3D12_CPU_DESCRIPTOR_HANDLE dsv{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuUav{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuRtv{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuDsv{};
        bool hasSrv = false, hasUav = false, hasRtv = false, hasDsv = false;
    };
    std::vector<TextureViewData> m_TextureViews;
    std::vector<std::uint64_t> m_FreeTextures;
    std::vector<std::uint64_t> m_FreeTextureViews;
        struct DescriptorHandles
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        bool valid = false;
    };
    DX12RHI::DescriptorHandles AllocateDescriptor(
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& heap,
        uint32_t& offset,
        uint32_t descriptorSize
    );
    struct BindGroupLayoutData
    {
        rhi::BindGroupLayoutDesc desc;
    };

    struct BindGroupData
    {
        rhi::BindGroupDesc desc;
        D3D12_GPU_DESCRIPTOR_HANDLE cbvSrvUavTable{};
        D3D12_GPU_DESCRIPTOR_HANDLE samplerTable{};
        uint32_t cbvSrvUavCount = 0;
        uint32_t samplerCount = 0;
    };

    struct PipelineLayoutData
    {
        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
        std::vector<UINT> resourceParam;
        std::vector<UINT> samplerParam;
    };

    std::vector<rhi::ShaderModule> m_ShaderModules;
    std::vector<std::uint64_t> m_FreeShaderModules;
    std::vector<BindGroupLayoutData> m_BindGroupLayouts;
    std::vector<std::uint64_t> m_FreeBindGroupLayouts;
    std::vector<BindGroupData> m_BindGroups;
    std::vector<std::uint64_t> m_FreeBindGroups;
    std::vector<PipelineLayoutData> m_PipelineLayouts;
    std::vector<std::uint64_t> m_FreePipelineLayouts;
    std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_Pipelines;
    std::vector<std::uint64_t> m_FreePipelines;
    std::vector<Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_ComputePipelines;
    std::vector<std::uint64_t> m_FreeComputePipelines;
    struct DX12SwapchainData {
        Microsoft::WRL::ComPtr<IDXGISwapChain4> swapchain;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> buffers;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
        std::vector<D3D12_RESOURCE_STATES> bufferStates;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        UINT syncInterval = 1;
    };
    std::vector<DX12SwapchainData> m_Swapchains;
    std::vector<std::uint64_t> m_FreeSwapchains;
    // DX12 has no surface object, so a surface just borrows the window's HWND
    std::vector<HWND> m_Surfaces;
    std::vector<std::uint64_t> m_FreeSurfaces;

    struct FrameContext {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
        Microsoft::WRL::ComPtr<ID3D12Fence> fence;
        std::uint64_t fenceValue = 0;
        bool submitted = false;
    };

    std::array<FrameContext, 2> m_Frames;
    std::uint64_t m_FrameCounter = 0;
    std::uint32_t m_CurrentFrame = 0xFFFFFFFF;
    HANDLE m_FenceEvent = nullptr;

    enum class CurrentPass {
        None,
        Graphics,
        Compute
    };
    struct DX12CommandList {
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
        std::uint32_t frame = 0xFFFFFFFF;
        bool open = false;

        CurrentPass pass = CurrentPass::None;
        bool computeBindPoint = false;
        rhi::PipelineLayoutHandle boundLayout{};
        rhi::GraphicsPipelineHandle boundGraphicsPipeline{};
        std::uint64_t swapchain = 0xFFFFFFFFFFFFFFFFull;
        std::uint32_t swapchainImageIndex = 0xFFFFFFFF;
    };
    std::vector<DX12CommandList> m_CommandLists;
    std::vector<std::uint64_t> m_FreeCommandLists;
    ID3D12GraphicsCommandList* GetCommandList(rhi::CommandListHandle handle);

    //resource transition functions + helpers
    void TransitionResource(ID3D12GraphicsCommandList* cmd, ID3D12Resource* resource,
                            D3D12_RESOURCE_STATES& stored, D3D12_RESOURCE_STATES target);
    void RequireBufferState(ID3D12GraphicsCommandList* cmd, const rhi::BufferHandle& buffer,
                            D3D12_RESOURCE_STATES target);
    void RequireTextureState(ID3D12GraphicsCommandList* cmd, const rhi::TextureHandle& texture,
                             D3D12_RESOURCE_STATES target);
    void RequireSwapchainState(ID3D12GraphicsCommandList* cmd, std::uint64_t swapchain,
                               std::uint32_t imageIndex, D3D12_RESOURCE_STATES target);
    std::vector<D3D12_RESOURCE_STATES> m_BufferStates;
    std::vector<D3D12_RESOURCE_STATES> m_TextureStates;

    Microsoft::WRL::ComPtr<ID3D12Fence> m_IdleFence;
    HANDLE m_IdleEvent = nullptr;
    std::uint64_t m_IdleValue = 0;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_UploadAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_UploadList;
    bool m_UploadHasWork = false;
    std::uint64_t m_UploadFenceValue = 0;
    bool EnsureUploadReady();
    void FlushUploads(bool wait);
    Microsoft::WRL::ComPtr<ID3D12Resource> m_StagingBuffer;
    void* m_StagingMapped = nullptr;
    std::size_t m_StagingHead = 0;
    static constexpr std::size_t kUploadRingSize = 64 * 1024 * 1024;
    bool AllocStaging(std::size_t size, std::size_t align, std::size_t& outOffset);

    struct CommandSignatureKey {
        D3D12_INDIRECT_ARGUMENT_TYPE type;
        UINT stride;
        bool operator<(const CommandSignatureKey& other) const
        {
            return type != other.type ? type < other.type : stride < other.stride;
        }
    };
    std::map<CommandSignatureKey, Microsoft::WRL::ComPtr<ID3D12CommandSignature>> m_CommandSignatures;
    ID3D12CommandSignature* GetOrCreateSignature(D3D12_INDIRECT_ARGUMENT_TYPE type, UINT stride);

    bool CreateFrameContexts();
    void DestroyFrameContexts();
};
