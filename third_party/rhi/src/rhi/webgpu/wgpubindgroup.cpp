#include "rhi/webgpu/wgpurhi.h"

rhi::BindGroupLayoutHandle WebGPURHI::CreateBindGroupLayout(const rhi::BindGroupLayoutDesc& desc)
{
    // we have many shaderBindings, each with a list of bindings,
    // so we need to make them first into one structure then iterate over it
    // to create the bind group layout, then return the handle

    rhi::BindGroupLayoutHandle handle;
    handle.id = 0xFFFFFFFFFFFFFFFFull;

    if (desc.shaderBindings.empty())
        return handle;

    // for a bind group layout, all bindings must belong to the same set.
    // while iterating, we need to keep track of the set and reject the input
    // if any bindings belong to a different set
    wgpu::BindGroupLayoutDescriptor layoutDesc{};

    std::vector<wgpu::BindGroupLayoutEntry> entries;
    entries.reserve(desc.shaderBindings.size());

    int currentSet = -1;

    for (const auto& shaderBinding : desc.shaderBindings)
    {
        if (currentSet == -1)
            currentSet = shaderBinding.set;
        else if (currentSet != shaderBinding.set)
            return handle;

        for (const auto& other : desc.shaderBindings) {
            if (&other != &shaderBinding && other.binding == shaderBinding.binding)
                return handle;
        }

        wgpu::BindGroupLayoutEntry entry{};
        entry.binding = shaderBinding.binding;

        wgpu::ShaderStage visibility = wgpu::ShaderStage::None;

        if (HasFlag(shaderBinding.stage, rhi::ShaderStageFlags::Vertex))
            visibility |= wgpu::ShaderStage::Vertex;

        if (HasFlag(shaderBinding.stage, rhi::ShaderStageFlags::Fragment))
            visibility |= wgpu::ShaderStage::Fragment;

        if (HasFlag(shaderBinding.stage, rhi::ShaderStageFlags::Compute))
            visibility |= wgpu::ShaderStage::Compute;

        entry.visibility = visibility;

        switch (shaderBinding.type)
        {
        case rhi::BindingType::UniformBuffer:
            entry.buffer.type = wgpu::BufferBindingType::Uniform;
            entry.buffer.hasDynamicOffset = false;
            break;

        case rhi::BindingType::StorageBuffer:
            entry.buffer.type = wgpu::BufferBindingType::Storage;
            entry.buffer.hasDynamicOffset = false;
            break;

        case rhi::BindingType::SampledTexture:
            entry.texture.sampleType = shaderBinding.depthSampled
                ? wgpu::TextureSampleType::Depth
                : (shaderBinding.unfilterableSampled
                    ? wgpu::TextureSampleType::UnfilterableFloat
                    : wgpu::TextureSampleType::Float);
            entry.texture.viewDimension = shaderBinding.arrayed
                ? wgpu::TextureViewDimension::e2DArray
                : wgpu::TextureViewDimension::e2D;
            entry.texture.multisampled = false;
            break;

        case rhi::BindingType::StorageTexture:
            entry.storageTexture.access =
                wgpu::StorageTextureAccess::ReadWrite;
            entry.storageTexture.format =
                wgpu::TextureFormat::R32Float;
            entry.storageTexture.viewDimension = shaderBinding.arrayed
                ? wgpu::TextureViewDimension::e2DArray
                : wgpu::TextureViewDimension::e2D;
            break;

        case rhi::BindingType::Sampler:
            entry.sampler.type = shaderBinding.comparison
                ? wgpu::SamplerBindingType::Comparison
                : wgpu::SamplerBindingType::Filtering;
            break;

        default:
            return handle;
        }

        entries.push_back(entry);
    }

    layoutDesc.entryCount = entries.size();
    layoutDesc.entries = entries.data();

    wgpu::BindGroupLayout bindGroupLayout =
        m_Device.CreateBindGroupLayout(&layoutDesc);

    if (!bindGroupLayout)
        return handle;

    handle.id = 0xFFFFFFFFFFFFFFFFull;
    handle.desc = desc;
    if (!m_FreeBindGroupLayouts.empty()) {
        handle.id = m_FreeBindGroupLayouts.back();
        m_FreeBindGroupLayouts.pop_back();
        m_BindGroupLayouts[handle.id] = bindGroupLayout;
    } else {
        handle.id = m_BindGroupLayouts.size();
        m_BindGroupLayouts.push_back(bindGroupLayout);
    }

    return handle;
}

rhi::BindGroupHandle WebGPURHI::CreateBindGroup(const rhi::BindGroupDesc& desc)
{
    rhi::BindGroupHandle handle;
    handle.id = 0xFFFFFFFFFFFFFFFFull;

    if (desc.layout.id >= m_BindGroupLayouts.size() ||
        !m_BindGroupLayouts[desc.layout.id])
        return handle;
    if (desc.entries.empty())
        return handle;
    std::vector<wgpu::BindGroupEntry> entries;
    entries.reserve(desc.entries.size());
    for (const auto& binding : desc.entries)
    {
        const bool hasBuffer = binding.buffer.id != 0xFFFFFFFFFFFFFFFFull;
        const bool hasTexture = binding.textureView.id != 0xFFFFFFFFFFFFFFFFull;
        const bool hasSampler = binding.sampler.id != 0xFFFFFFFFFFFFFFFFull;
        const int setCount = (hasBuffer ? 1 : 0) + (hasTexture ? 1 : 0) + (hasSampler ? 1 : 0);
        if (setCount != 1) {
            printf("CreateBindGroup: each entry must set exactly one of buffer/textureView/sampler\n");
            return handle;
        }

        wgpu::BindGroupEntry entry{};
        entry.binding = binding.binding;

        if (hasBuffer) {
            if (binding.buffer.id >= m_Buffers.size() || !m_Buffers[binding.buffer.id])
                return handle;
            entry.buffer = m_Buffers[binding.buffer.id];
        }
        else if (hasTexture) {
            if (binding.textureView.id >= m_TextureViews.size() || !m_TextureViews[binding.textureView.id])
                return handle;
            entry.textureView = m_TextureViews[binding.textureView.id];
        }
        else {
            if (binding.sampler.id >= m_Samplers.size() || !m_Samplers[binding.sampler.id])
                return handle;
            entry.sampler = m_Samplers[binding.sampler.id];
        }

        entries.push_back(entry);
    }

    wgpu::BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout = m_BindGroupLayouts[desc.layout.id];
    bindGroupDesc.entryCount = entries.size();
    bindGroupDesc.entries = entries.data();

    wgpu::BindGroup bindGroup = m_Device.CreateBindGroup(&bindGroupDesc);

    if (!bindGroup)
        return handle;

    handle.id = 0xFFFFFFFFFFFFFFFFull;
    handle.desc = desc;
    if (!m_FreeBindGroups.empty()) {
        handle.id = m_FreeBindGroups.back();
        m_FreeBindGroups.pop_back();
        m_BindGroups[handle.id] = bindGroup;
    } else {
        handle.id = m_BindGroups.size();
        m_BindGroups.push_back(bindGroup);
    }
    return handle;
}

void WebGPURHI::DestroyBindGroupLayout(rhi::BindGroupLayoutHandle handle) {
    if (handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_BindGroupLayouts.size() ||
        !m_BindGroupLayouts[handle.id]) {
        return;
    }
    m_BindGroupLayouts[handle.id] = {};
    m_FreeBindGroupLayouts.push_back(handle.id);
}

void WebGPURHI::DestroyBindGroup(rhi::BindGroupHandle handle) {
    if (handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_BindGroups.size() ||
        !m_BindGroups[handle.id]) {
        return;
    }
    m_BindGroups[handle.id] = {};
    m_FreeBindGroups.push_back(handle.id);
}
