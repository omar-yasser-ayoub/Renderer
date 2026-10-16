#include "rhi/dx12/dx12rhi.h"
#include <combaseapi.h>
#include <d3d12.h>
#include <cstdio>
#include <iostream>


bool DX12RHI::Initialize() {

    UINT factoryFlags = 0;
    {
        Microsoft::WRL::ComPtr<ID3D12Debug> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }

    HRESULT result = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_Factory));
    if (FAILED(result)) {
        std::cerr << "Failed to create DXGI factory: " << result << std::endl;
        return false;
    }

    result = m_Factory->EnumAdapterByGpuPreference(
        0,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(&m_Adapter)
    );
    if (FAILED(result)) {
        std::cerr << "Failed to enumerate adapter by GPU preference: " << result << std::endl;
        return false;
    }
    DXGI_ADAPTER_DESC1 desc;
    result = m_Adapter->GetDesc1(&desc);
    if (FAILED(result)) {
        std::cerr << "Failed to get adapter description: " << result << std::endl;
        return false;
    }

    std::wstring wName(desc.Description);
    m_Capabilities.deviceName = std::string(wName.begin(), wName.end());

    result = D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_Device));
    if (FAILED(result)) {
        std::cerr << "Failed to create D3D12 device: " << result << std::endl;
        return false;
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS7 options7 = {};
    result = m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &options7, sizeof(options7));
    if (SUCCEEDED(result)) {
            // In DX12, mesh shaders are supported if the tier is Tier 1 or higher
            m_Capabilities.SupportsMeshShaders = (options7.MeshShaderTier != D3D12_MESH_SHADER_TIER_NOT_SUPPORTED);
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    result = m_Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5));
    if (SUCCEEDED(result)) {
        // Ray tracing is supported if the tier is 1.0 or 1.1
        m_Capabilities.SupportsRayTracing = (options5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);
    }

    // Anisotropic filtering (up to 16x) is core D3D12, no feature flag needed.
    // CreateSampler still clamps to [1, 16].
    m_Capabilities.SupportsAnisotropy = true;

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;
    result = m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue));
    if (FAILED(result)) {
        std::cerr << "Failed to create command queue: " << result << std::endl;
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
    computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    computeQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    computeQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    computeQueueDesc.NodeMask = 0;
    result = m_Device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&m_ComputeQueue));
    if (FAILED(result)) {
        std::cerr << "Failed to create compute command queue: " << result << std::endl;
        return false;
    }

    //create sampler descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    heapDesc.NumDescriptors = 2048;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_SamplerHeap));
    m_SamplerDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    //create render target view descriptor heap
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = 2048;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_RtvHeap));
    m_RtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    //create depth stencil view descriptor heap
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heapDesc.NumDescriptors = 2048;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_DsvHeap));
    m_DsvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    //create cbv/srv/uav descriptor heap
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 2048;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_Device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_CbvSrvUavHeap));
    m_CbvSrvUavDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    if (FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_IdleFence)))) {
        std::cerr << "Failed to create idle fence.\n";
        return false;
    }
    m_IdleEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_IdleEvent == nullptr) {
        std::cerr << "Failed to create idle event.\n";
        return false;
    }

    if (!CreateFrameContexts()) {
        return false;
    }

    return true;
}

bool DX12RHI::CreateFrameContexts()
{
    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_FenceEvent == nullptr) {
        std::cerr << "Failed to create frame fence event.\n";
        return false;
    }
    for (std::uint32_t i = 0; i < m_Frames.size(); ++i) {
        FrameContext& frame = m_Frames[i];
        if (FAILED(m_Device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.allocator)))) {
            std::cerr << "Failed to create frame command allocator.\n";
            DestroyFrameContexts();
            return false;
        }
        if (FAILED(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frame.fence)))) {
            std::cerr << "Failed to create frame fence.\n";
            DestroyFrameContexts();
            return false;
        }
    }
    if (FAILED(m_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_UploadAllocator)))) {
        std::cerr << "Failed to create upload command allocator.\n";
        DestroyFrameContexts();
        return false;
    }
    if (FAILED(m_Device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_UploadAllocator.Get(),
            nullptr, IID_PPV_ARGS(&m_UploadList)))) {
        std::cerr << "Failed to create upload command list.\n";
        DestroyFrameContexts();
        return false;
    }
    D3D12_HEAP_PROPERTIES stagingHeap{};
    stagingHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    stagingHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    stagingHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    D3D12_RESOURCE_DESC stagingDesc{};
    stagingDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    stagingDesc.Width = kUploadRingSize;
    stagingDesc.Height = 1;
    stagingDesc.DepthOrArraySize = 1;
    stagingDesc.MipLevels = 1;
    stagingDesc.Format = DXGI_FORMAT_UNKNOWN;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(m_Device->CreateCommittedResource(
            &stagingHeap, D3D12_HEAP_FLAG_NONE, &stagingDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_StagingBuffer)))) {
        std::cerr << "Failed to create staging ring buffer.\n";
        DestroyFrameContexts();
        return false;
    }
    if (FAILED(m_StagingBuffer->Map(0, nullptr, &m_StagingMapped)) ||
        m_StagingMapped == nullptr) {
        std::cerr << "Failed to map staging ring buffer.\n";
        DestroyFrameContexts();
        return false;
    }
    m_StagingHead = 0;
    return true;
}

void DX12RHI::DestroyFrameContexts()
{
    if (m_StagingBuffer && m_StagingMapped) {
        m_StagingBuffer->Unmap(0, nullptr);
    }
    m_StagingMapped = nullptr;
    m_StagingBuffer.Reset();
    m_StagingHead = 0;
    m_UploadList.Reset();
    m_UploadAllocator.Reset();
    m_UploadHasWork = false;
    m_UploadFenceValue = 0;
    for (FrameContext& frame : m_Frames) {
        frame.allocator.Reset();
        frame.fence.Reset();
        frame.fenceValue = 0;
        frame.submitted = false;
    }
    m_CommandLists.clear();
    m_FreeCommandLists.clear();
    m_CurrentFrame = 0xFFFFFFFF;
    if (m_FenceEvent != nullptr) {
        CloseHandle(m_FenceEvent);
        m_FenceEvent = nullptr;
    }
}

void DX12RHI::Shutdown() {
    WaitForIdle();
    DestroyFrameContexts();
    m_CommandSignatures.clear();
    m_IdleFence.Reset();
    if (m_IdleEvent != nullptr) {
        CloseHandle(m_IdleEvent);
        m_IdleEvent = nullptr;
    }
    m_IdleValue = 0;
    m_ComputePipelines.clear();
    m_FreeComputePipelines.clear();
    m_Swapchains.clear();
    m_FreeSwapchains.clear();
    m_Surfaces.clear();
    m_FreeSurfaces.clear();
}
