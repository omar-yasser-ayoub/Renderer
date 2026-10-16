#include "rhi/webgpu/wgpurhi.h"
#include <iostream>
#include <webgpu/webgpu_cpp.h>

bool WebGPURHI::Initialize()
{
    static constexpr auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
    wgpu::InstanceDescriptor instanceDesc{};
    instanceDesc.requiredFeatureCount = 1;
    instanceDesc.requiredFeatures = &kTimedWaitAny;

    m_Instance = wgpu::CreateInstance(&instanceDesc);
    if (!m_Instance) {
        std::cerr << "Failed to create WebGPU instance.\n";
        return false;
    }

    wgpu::Adapter adapter;
    wgpu::RequestAdapterOptions adapterOpts{};
    auto adapterFuture = m_Instance.RequestAdapter(
        &adapterOpts,
        wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status,
            wgpu::Adapter a,
            wgpu::StringView message)
        {
            if (status != wgpu::RequestAdapterStatus::Success) {
                std::cerr << "Failed to request adapter: " << message.data << '\n';
                return;
            }
            adapter = std::move(a);
        });

    m_Instance.WaitAny(adapterFuture, UINT64_MAX);

    if (!adapter) {
        std::cerr << "No adapter acquired.\n";
        return false;
    }
    m_Adapter = std::move(adapter);

    wgpu::Device device;
    wgpu::DeviceDescriptor deviceDesc{};
    deviceDesc.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
            printf("[RHI WebGPU] device error %d: %.*s\n", static_cast<int>(type),
                   static_cast<int>(message.length), message.data);
        });
    auto deviceFuture = m_Adapter.RequestDevice(
        &deviceDesc,
        wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestDeviceStatus status,
            wgpu::Device d,
            wgpu::StringView message)
        {
            if (status != wgpu::RequestDeviceStatus::Success) {
                std::cerr << "Failed to request device: " << message.data << '\n';
                return;
            }
            device = std::move(d);
        });

    m_Instance.WaitAny(deviceFuture, UINT64_MAX);

    if (!device) {
        std::cerr << "No device acquired.\n";
        return false;
    }
    m_Device = std::move(device);

    m_Queue = m_Device.GetQueue();

    wgpu::AdapterInfo info{};
    m_Adapter.GetInfo(&info);
    m_Capabilities.deviceName = info.device.data;

    m_Capabilities.SupportsMeshShaders = false;
    m_Capabilities.SupportsRayTracing  = false;
    m_Capabilities.SupportsAnisotropy = true;

    return true;
}

void WebGPURHI::Shutdown()
{
    m_CommandLists.clear();
    m_FreeCommandLists.clear();
    m_UploadEncoder = nullptr;
    m_UploadHasWork = false;
    m_UploadStaging.clear();

    for (auto& buffer : m_Buffers) {
        if (buffer) {
            buffer.Destroy();
            buffer = {};
        }
    }
    m_Buffers.clear();
    m_FreeBuffers.clear();
    m_BufferMapped.clear();
    for (auto& sampler : m_Samplers) {
        sampler = {};
    }
    m_Samplers.clear();
    m_FreeSamplers.clear();
    for (auto& texture : m_Textures) {
        if (texture) {
            texture.Destroy();
            texture = {};
        }
    }
    m_Textures.clear();
    m_FreeTextures.clear();
    for (auto& view : m_TextureViews) {
        view = {};
    }
    m_TextureViews.clear();
    m_FreeTextureViews.clear();
    m_ShaderModules.clear();
    m_FreeShaderModules.clear();
    for (auto& layout : m_BindGroupLayouts) {
        layout = {};
    }
    m_BindGroupLayouts.clear();
    m_FreeBindGroupLayouts.clear();
    for (auto& group : m_BindGroups) {
        group = {};
    }
    m_BindGroups.clear();
    m_FreeBindGroups.clear();
    for (auto& layout : m_PipelineLayouts) {
        layout = {};
    }
    m_PipelineLayouts.clear();
    m_FreePipelineLayouts.clear();
    for (auto& pipeline : m_Pipelines) {
        pipeline = {};
    }
    m_Pipelines.clear();
    m_FreePipelines.clear();
    for (auto& pipeline : m_ComputePipelines) {
        pipeline = {};
    }
    m_ComputePipelines.clear();
    m_FreeComputePipelines.clear();
    for (auto& data : m_Swapchains) {
        if (data.surface) {
            data.currentTexture = {};
            data.currentView = {};
            data.surface.Unconfigure();
        }
    }
    m_Swapchains.clear();
    m_FreeSwapchains.clear();
    m_Surfaces.clear();
    m_FreeSurfaces.clear();
    m_Queue = {};
    m_Device = {};
    m_Adapter = {};
    m_Instance = {};
}
