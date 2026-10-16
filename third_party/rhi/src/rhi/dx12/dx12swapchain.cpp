#include "rhi/dx12/dx12rhi.h"

#include <iostream>

rhi::SwapchainHandle DX12RHI::CreateSwapchain(const rhi::SwapchainDesc& desc)
{
    if (!m_Device || !m_Factory || !m_CommandQueue) {
        return {};
    }
    if (desc.surface.id == 0xFFFFFFFFFFFFFFFF || desc.surface.id >= m_Surfaces.size() ||
        m_Surfaces[desc.surface.id] == nullptr) {
        return {};
    }
    if (desc.width == 0 || desc.height == 0) {
        return {};
    }

    HWND hwnd = m_Surfaces[desc.surface.id];

    const DXGI_FORMAT format = toD3D12(desc.format);
    if (format == DXGI_FORMAT_UNKNOWN) {
        std::cerr << "Unsupported swapchain format.\n";
        return {};
    }

    UINT bufferCount = desc.imageCount < 2 ? 2 : desc.imageCount;
    if (bufferCount > DXGI_MAX_SWAP_CHAIN_BUFFERS) {
        bufferCount = DXGI_MAX_SWAP_CHAIN_BUFFERS;
    }

    DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
    swapchainDesc.Width = desc.width;
    swapchainDesc.Height = desc.height;
    swapchainDesc.Format = format;
    swapchainDesc.Stereo = FALSE;
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.BufferCount = bufferCount;
    swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.AlphaMode = toD3D12(desc.alphaMode);
    swapchainDesc.Flags = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapchain1;
    HRESULT result = m_Factory->CreateSwapChainForHwnd(
        m_CommandQueue.Get(), hwnd, &swapchainDesc, nullptr, nullptr, &swapchain1);
    if (FAILED(result)) {
        std::cerr << "Failed to create swapchain: " << std::hex << result << std::endl;
        return {};
    }

    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapchain;
    result = swapchain1.As(&swapchain);
    if (FAILED(result)) {
        std::cerr << "Failed to query IDXGISwapChain4: " << std::hex << result << std::endl;
        return {};
    }

    m_Factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    DX12SwapchainData data;
    data.swapchain = swapchain;
    data.format = format;
    data.width = desc.width;
    data.height = desc.height;
    data.syncInterval = toD3D12SyncInterval(desc.presentMode);

    for (UINT i = 0; i < bufferCount; ++i) {
        Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
        result = swapchain->GetBuffer(i, IID_PPV_ARGS(&buffer));
        if (FAILED(result)) {
            std::cerr << "Failed to get swapchain buffer " << i << std::endl;
            return {};
        }

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = format;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        auto handles = AllocateDescriptor(m_RtvHeap, m_RtvHeapOffset, m_RtvDescriptorSize);
        if (!handles.valid) {
            std::cerr << "Out of RTV heap descriptors for swapchain.\n";
            return {};
        }
        m_Device->CreateRenderTargetView(buffer.Get(), &rtvDesc, handles.cpu);

        data.buffers.push_back(buffer);
        data.rtvs.push_back(handles.cpu);
    }
    data.bufferStates.assign(data.buffers.size(), D3D12_RESOURCE_STATE_PRESENT);

    rhi::SwapchainHandle handle;
    handle.desc = desc;
    if (!m_FreeSwapchains.empty()) {
        handle.id = m_FreeSwapchains.back();
        m_FreeSwapchains.pop_back();
        m_Swapchains[handle.id] = std::move(data);
    } else {
        handle.id = static_cast<std::uint64_t>(m_Swapchains.size());
        m_Swapchains.push_back(std::move(data));
    }

    return handle;
}

void DX12RHI::DestroySwapchain(rhi::SwapchainHandle handle)
{
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Swapchains.size()) {
        return;
    }
    DX12SwapchainData& data = m_Swapchains[handle.id];
    if (!data.swapchain) {
        return;
    }
    WaitForIdle();
    data = {};
    m_FreeSwapchains.push_back(handle.id);
}

rhi::AcquireResult DX12RHI::AcquireNextImage(rhi::SwapchainHandle handle)
{
    rhi::AcquireResult result{};
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Swapchains.size()) {
        return result;
    }
    DX12SwapchainData& data = m_Swapchains[handle.id];
    if (!data.swapchain || data.buffers.empty()) {
        return result;
    }

    const std::uint32_t imageIndex = data.swapchain->GetCurrentBackBufferIndex();
    if (imageIndex >= data.buffers.size()) {
        return result;
    }

    m_CurrentFrame = static_cast<std::uint32_t>(m_FrameCounter % m_Frames.size());
    FrameContext& frame = m_Frames[m_CurrentFrame];
    if (!frame.allocator || !frame.fence || m_FenceEvent == nullptr) {
        return result;
    }
    if (frame.submitted) {
        if (frame.fence->GetCompletedValue() < frame.fenceValue) {
            frame.fence->SetEventOnCompletion(frame.fenceValue, m_FenceEvent);
            WaitForSingleObject(m_FenceEvent, INFINITE);
        }
    }
    frame.allocator->Reset();
    frame.submitted = false;

    result.status = rhi::AcquireStatus::Success;
    result.imageIndex = imageIndex;
    return result;
}

ID3D12Resource* DX12RHI::GetSwapchainBuffer(
    rhi::SwapchainHandle handle,
    std::uint32_t imageIndex) const
{
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Swapchains.size()) {
        return nullptr;
    }
    const DX12SwapchainData& data = m_Swapchains[handle.id];
    if (!data.swapchain || imageIndex >= data.buffers.size()) {
        return nullptr;
    }
    return data.buffers[imageIndex].Get();
}

void DX12RHI::WaitForIdle()
{
    if (!m_Device || !m_CommandQueue || !m_IdleFence || m_IdleEvent == nullptr) {
        return;
    }
    FlushUploads(false);
    m_CommandQueue->Signal(m_IdleFence.Get(), ++m_IdleValue);
    if (m_IdleFence->GetCompletedValue() < m_IdleValue) {
        m_IdleFence->SetEventOnCompletion(m_IdleValue, m_IdleEvent);
        WaitForSingleObject(m_IdleEvent, INFINITE);
    }
}

void DX12RHI::Present(rhi::SwapchainHandle handle)
{
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Swapchains.size()) {
        return;
    }
    DX12SwapchainData& data = m_Swapchains[handle.id];
    if (!data.swapchain || m_CurrentFrame == 0xFFFFFFFF) {
        return;
    }
    data.swapchain->Present(data.syncInterval, 0);
    m_CurrentFrame = 0xFFFFFFFF;
    ++m_FrameCounter;
}
