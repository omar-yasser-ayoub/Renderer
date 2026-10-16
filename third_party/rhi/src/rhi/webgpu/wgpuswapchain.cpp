#include "rhi/webgpu/wgpurhi.h"

#include <iostream>

rhi::SwapchainHandle WebGPURHI::CreateSwapchain(const rhi::SwapchainDesc& desc)
{
    if (!m_Device || !m_Adapter) {
        return {};
    }
    if (desc.surface.id == 0xFFFFFFFFFFFFFFFF || desc.surface.id >= m_Surfaces.size() ||
        !m_Surfaces[desc.surface.id]) {
        return {};
    }
    if (desc.width == 0 || desc.height == 0) {
        return {};
    }

    wgpu::Surface surface = m_Surfaces[desc.surface.id];

    wgpu::SurfaceCapabilities caps{};
    surface.GetCapabilities(m_Adapter, &caps);
    if (caps.formatCount == 0 || caps.formats == nullptr) {
        std::cerr << "Failed to query WebGPU surface capabilities.\n";
        return {};
    }

    wgpu::TextureFormat format = toWebGPU(desc.format);
    bool formatSupported = false;
    for (size_t i = 0; i < caps.formatCount; ++i) {
        if (caps.formats[i] == format) {
            formatSupported = true;
            break;
        }
    }
    if (format == wgpu::TextureFormat::Undefined || !formatSupported) {
        format = caps.formats[0];
    }

    wgpu::PresentMode presentMode = toWebGPU(desc.presentMode);
    bool modeSupported = false;
    for (size_t i = 0; i < caps.presentModeCount; ++i) {
        if (caps.presentModes[i] == presentMode) {
            modeSupported = true;
            break;
        }
    }
    if (!modeSupported) {
        presentMode = wgpu::PresentMode::Fifo;
    }

    wgpu::CompositeAlphaMode alphaMode = toWebGPU(desc.alphaMode);
    bool alphaSupported = false;
    for (size_t i = 0; i < caps.alphaModeCount; ++i) {
        if (caps.alphaModes[i] == alphaMode) {
            alphaSupported = true;
            break;
        }
    }
    if (!alphaSupported) {
        alphaMode = wgpu::CompositeAlphaMode::Auto;
    }

    wgpu::TextureUsage usage = toWebGPU(desc.usage) | wgpu::TextureUsage::RenderAttachment;
    usage &= caps.usages;
    if ((usage & wgpu::TextureUsage::RenderAttachment) == wgpu::TextureUsage::None) {
        std::cerr << "WebGPU surface does not support render attachment usage.\n";
        return {};
    }

    wgpu::SurfaceConfiguration config{};
    config.device = m_Device;
    config.format = format;
    config.usage = usage;
    config.width = desc.width;
    config.height = desc.height;
    config.alphaMode = alphaMode;
    config.presentMode = presentMode;
    surface.Configure(&config);

    WebGPUSwapchainData data;
    data.surface = surface;
    data.format = format;
    data.width = desc.width;
    data.height = desc.height;

    rhi::SwapchainHandle handle;
    handle.desc = desc;
    const rhi::TextureFormat actualFormat = fromWebGPU(format);
    handle.desc.format = actualFormat != rhi::TextureFormat::Unknown ? actualFormat : desc.format;
    handle.desc.presentMode = fromWebGPU(presentMode);
    handle.desc.alphaMode = fromWebGPU(alphaMode);
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

void WebGPURHI::DestroySwapchain(rhi::SwapchainHandle handle)
{
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Swapchains.size()) {
        return;
    }
    WebGPUSwapchainData& data = m_Swapchains[handle.id];
    if (!data.surface) {
        return;
    }
    data.currentTexture = {};
    data.surface.Unconfigure();
    data = {};
    m_FreeSwapchains.push_back(handle.id);
}

rhi::AcquireResult WebGPURHI::AcquireNextImage(rhi::SwapchainHandle handle)
{
    rhi::AcquireResult result{};
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Swapchains.size()) {
        return result;
    }
    WebGPUSwapchainData& data = m_Swapchains[handle.id];
    if (!data.surface) {
        return result;
    }

    data.currentTexture = {};
    data.currentView = {};
    wgpu::SurfaceTexture surfaceTexture{};
    data.surface.GetCurrentTexture(&surfaceTexture);

    switch (surfaceTexture.status) {
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal:
        result.status = rhi::AcquireStatus::Success;
        break;
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
        result.status = rhi::AcquireStatus::Suboptimal;
        break;
    case wgpu::SurfaceGetCurrentTextureStatus::Timeout:
        result.status = rhi::AcquireStatus::Timeout;
        return result;
    case wgpu::SurfaceGetCurrentTextureStatus::Outdated:
        result.status = rhi::AcquireStatus::OutOfDate;
        return result;
    case wgpu::SurfaceGetCurrentTextureStatus::Lost:
    case wgpu::SurfaceGetCurrentTextureStatus::Error:
    default:
        return result;
    }

    if (!surfaceTexture.texture) {
        result.status = rhi::AcquireStatus::Failed;
        return result;
    }

    data.currentTexture = surfaceTexture.texture;
    data.currentView = surfaceTexture.texture.CreateView();
    if (!data.currentView) {
        data.currentTexture = {};
        result.status = rhi::AcquireStatus::Failed;
        return result;
    }
    result.imageIndex = 0;
    return result;
}

wgpu::Texture WebGPURHI::GetSwapchainTexture(rhi::SwapchainHandle handle) const
{
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Swapchains.size()) {
        return {};
    }
    return m_Swapchains[handle.id].currentTexture;
}

void WebGPURHI::WaitForIdle()
{
    if (!m_Device || !m_Instance || !m_Queue) {
        return;
    }
    FlushUploads(false);
    auto future = m_Queue.OnSubmittedWorkDone(
        wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::QueueWorkDoneStatus, wgpu::StringView) {});
    m_Instance.WaitAny(future, UINT64_MAX);
}

void WebGPURHI::Present(rhi::SwapchainHandle handle)
{
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Swapchains.size()) {
        return;
    }
    WebGPUSwapchainData& data = m_Swapchains[handle.id];
    if (!data.surface || !data.currentTexture) {
        return;
    }
    data.surface.Present();
    data.currentTexture = {};
    data.currentView = {};
}
