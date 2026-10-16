#include "rhi/resources/texture.h"
#include "rhi/webgpu/wgpurhi.h"

rhi::TextureHandle WebGPURHI::CreateTexture(const rhi::TextureDesc& desc) {
    if (desc.width == 0 || desc.height == 0 || desc.mipLevels == 0 || desc.arrayLayers == 0) {
        printf("[RHI] Invalid WebGPU texture description\n");
        return rhi::TextureHandle{};
    }
    const wgpu::TextureFormat format = toWebGPU(desc.format);
    if (format == wgpu::TextureFormat::Undefined) {
        printf("[RHI] Unsupported texture format for WebGPU\n");
        return rhi::TextureHandle{};
    }
    if (HasFlag(desc.usage, rhi::TextureUsage::Storage) && !IsStorageCompatible(desc.format)) {
        printf("[RHI] Rejected texture: format does not support Storage usage in WebGPU. "
               "Storage-compatible formats are R32Float/Uint/Sint, RG32Float/Uint/Sint, "
               "RGBA8Unorm/Snorm/Uint/Sint, RGBA16Float/Uint/Sint, RGBA32Float/Uint/Sint, "
               "and RGB10A2Uint. Recreate without TextureUsage::Storage.\n");
        return rhi::TextureHandle{};
    }
    const wgpu::TextureUsage usage = toWebGPU(desc.usage);
    if (usage == wgpu::TextureUsage::None) {
        printf("[RHI] Texture usage must not be None\n");
        return rhi::TextureHandle{};
    }
    if (desc.sampleCount != rhi::SampleCount::x1 && desc.sampleCount != rhi::SampleCount::x4) {
        printf("[RHI] Only 1x and 4x MSAA are supported in WebGPU\n");
        return rhi::TextureHandle{};
    }
    wgpu::TextureDescriptor texDesc{};

    const std::uint32_t depthOrLayers =
        desc.dimension == rhi::TextureDimension::Tex3D ? desc.depth : desc.arrayLayers;
    texDesc.size = wgpu::Extent3D{ desc.width, desc.height, depthOrLayers };
    texDesc.mipLevelCount = desc.mipLevels;
    texDesc.sampleCount = toWebGPU(desc.sampleCount);
    texDesc.dimension = toWebGPU(desc.dimension);
    texDesc.format = format;
    texDesc.usage = usage;

    // for now, I've chosen to ignore viewFormats and TextureViewDimension, both since they are not required,
    // but also since color space reinterpretation isnt something i particularly care about right now.

    wgpu::Texture texture = m_Device.CreateTexture(&texDesc);

    if (!texture) {
        return rhi::TextureHandle{};
    }

    rhi::TextureHandle handle{};
    handle.desc = desc;
    if (!m_FreeTextures.empty()) {
        handle.id = m_FreeTextures.back();
        m_FreeTextures.pop_back();
        m_Textures[handle.id] = texture;
    } else {
        handle.id = static_cast<std::uint64_t>(m_Textures.size());
        m_Textures.push_back(texture);
    }

    return handle;
}

rhi::TextureViewHandle WebGPURHI::CreateTextureView(const rhi::TextureViewDesc& desc, rhi::TextureHandle texture) {
    if (texture.id == 0xFFFFFFFFFFFFFFFFull || texture.id >= m_Textures.size() ||
        !m_Textures[texture.id]) {
        printf("[RHI] Invalid texture handle for view creation\n");
        return rhi::TextureViewHandle{};
    }
    const wgpu::TextureFormat format = toWebGPU(desc.format);
    if (format == wgpu::TextureFormat::Undefined) {
        printf("[RHI] Unsupported texture view format for WebGPU\n");
        return rhi::TextureViewHandle{};
    }
    const wgpu::TextureViewDimension viewDimension = toWebGPU(desc.dimension);
    if (viewDimension == wgpu::TextureViewDimension::Undefined) {
        printf("[RHI] Unsupported texture view dimension for WebGPU\n");
        return rhi::TextureViewHandle{};
    }
    wgpu::TextureViewDescriptor viewDesc{};
    viewDesc.baseMipLevel = desc.baseMipLevel;
    viewDesc.baseArrayLayer = desc.baseArrayLayer;
    viewDesc.mipLevelCount = desc.mipLevels;
    viewDesc.arrayLayerCount = desc.arrayLayers;
    viewDesc.format = format;
    viewDesc.dimension = viewDimension;
    viewDesc.aspect = toWebGPU(desc.aspect);

    wgpu::TextureView view = m_Textures[texture.id].CreateView(&viewDesc);

    if (!view) {
        return rhi::TextureViewHandle{};
    }

    rhi::TextureViewHandle handle{};
    handle.desc = desc;
    handle.texture = texture;
    if (!m_FreeTextureViews.empty()) {
        handle.id = m_FreeTextureViews.back();
        m_FreeTextureViews.pop_back();
        m_TextureViews[handle.id] = view;
    } else {
        handle.id = static_cast<std::uint64_t>(m_TextureViews.size());
        m_TextureViews.push_back(view);
    }

    return handle;
}

void WebGPURHI::DestroyTexture(rhi::TextureHandle handle) {
    if (handle.id >= m_Textures.size() || !m_Textures[handle.id]) {
        return;
    }

    m_Textures[handle.id].Destroy();
    m_Textures[handle.id] = nullptr;
    m_FreeTextures.push_back(handle.id);
    return;
}

void WebGPURHI::DestroyTextureView(rhi::TextureViewHandle handle) {
    if (handle.id >= m_TextureViews.size() || !m_TextureViews[handle.id]) {
        return;
    }

    m_TextureViews[handle.id] = {};
    m_FreeTextureViews.push_back(handle.id);
    return;
}
