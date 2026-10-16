#include "rhi/dx12/dx12rhi.h"
#include "rhi/resources/bindgroup.h"

#include <algorithm>
#include <cstdint>

rhi::BindGroupLayoutHandle DX12RHI::CreateBindGroupLayout(
    const rhi::BindGroupLayoutDesc& desc)
{
    rhi::BindGroupLayoutHandle handle;
    handle.id = 0xFFFFFFFFFFFFFFFFull;

    if (desc.shaderBindings.empty()) {
        return handle;
    }

    const std::uint32_t set = desc.shaderBindings.front().set;
    for (const auto& binding : desc.shaderBindings) {
        if (binding.set != set || binding.arraySize != 1 ||
            binding.stage == rhi::ShaderStageFlags::None) {
            return handle;
        }

        const auto duplicate = std::count_if(
            desc.shaderBindings.begin(),
            desc.shaderBindings.end(),
            [&binding](const rhi::ShaderBinding& other) {
                return other.binding == binding.binding;
            });
        if (duplicate != 1) {
            return handle;
        }
    }

    handle.id = 0xFFFFFFFFFFFFFFFFull;
    handle.desc = desc;
    if (!m_FreeBindGroupLayouts.empty()) {
        handle.id = m_FreeBindGroupLayouts.back();
        m_FreeBindGroupLayouts.pop_back();
        m_BindGroupLayouts[handle.id] = {desc};
    } else {
        handle.id = static_cast<std::uint64_t>(m_BindGroupLayouts.size());
        m_BindGroupLayouts.push_back({desc});
    }
    return handle;
}

rhi::BindGroupHandle DX12RHI::CreateBindGroup(
    const rhi::BindGroupDesc& desc)
{
    rhi::BindGroupHandle handle;
    handle.id = 0xFFFFFFFFFFFFFFFFull;

    if (desc.layout.id >= m_BindGroupLayouts.size() || desc.entries.empty() ||
        m_BindGroupLayouts[desc.layout.id].desc.shaderBindings.empty()) {
        return handle;
    }

    const auto& layout = m_BindGroupLayouts[desc.layout.id].desc;
    for (const auto& shaderBinding : layout.shaderBindings) {
        const auto* entry = FindEntry(desc, shaderBinding.binding);
        if (entry == nullptr) {
            return handle;
        }

        const bool hasBuffer = entry->buffer.id != 0xFFFFFFFFFFFFFFFFull;
        const bool hasTexture = entry->textureView.id != 0xFFFFFFFFFFFFFFFFull;
        const bool hasSampler = entry->sampler.id != 0xFFFFFFFFFFFFFFFFull;
        if (static_cast<unsigned>(hasBuffer) +
                static_cast<unsigned>(hasTexture) +
                static_cast<unsigned>(hasSampler) != 1) {
            return handle;
        }
    }

    for (const auto& entry : desc.entries) {
        if (FindBinding(layout, entry.binding) == nullptr) {
            return handle;
        }
    }

    const auto cbvSrvUavOffset = m_CbvSrvUavHeapOffset;
    const auto samplerOffset = m_SamplerHeapOffset;
    BindGroupData bindGroup{};
    bindGroup.desc = desc;

    for (const auto& shaderBinding : layout.shaderBindings) {
        const auto* entry = FindEntry(desc, shaderBinding.binding);
        D3D12_CPU_DESCRIPTOR_HANDLE source{};
        D3D12_DESCRIPTOR_HEAP_TYPE heapType{};

        switch (shaderBinding.type) {
        case rhi::BindingType::UniformBuffer:
            if (entry->buffer.id >= m_BufferViews.size() ||
                !m_BufferViews[entry->buffer.id].hasCbv) {
                goto fail;
            }
            source = m_BufferViews[entry->buffer.id].cbv;
            heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            break;
        case rhi::BindingType::StorageBuffer:
            if (entry->buffer.id >= m_BufferViews.size() ||
                !m_BufferViews[entry->buffer.id].hasUav) {
                goto fail;
            }
            source = m_BufferViews[entry->buffer.id].uav;
            heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            break;
        case rhi::BindingType::SampledTexture:
            if (entry->textureView.id >= m_TextureViews.size() ||
                !m_TextureViews[entry->textureView.id].hasSrv) {
                goto fail;
            }
            source = m_TextureViews[entry->textureView.id].srv;
            heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            break;
        case rhi::BindingType::StorageTexture:
            if (entry->textureView.id >= m_TextureViews.size() ||
                !m_TextureViews[entry->textureView.id].hasUav) {
                goto fail;
            }
            source = m_TextureViews[entry->textureView.id].uav;
            heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            break;
        case rhi::BindingType::Sampler:
            if (entry->sampler.id >= m_SamplerViews.size() ||
                !m_SamplerViews[entry->sampler.id].hasSampler) {
                goto fail;
            }
            source = m_SamplerViews[entry->sampler.id].cpu;
            heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            break;
        }

        if (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
            auto destination = AllocateDescriptor(
                m_SamplerHeap, m_SamplerHeapOffset, m_SamplerDescriptorSize);
            if (!destination.valid) {
                goto fail;
            }
            if (bindGroup.samplerCount++ == 0) {
                bindGroup.samplerTable = destination.gpu;
            }
            m_Device->CopyDescriptorsSimple(
                1, destination.cpu, source, heapType);
        } else {
            auto destination = AllocateDescriptor(
                m_CbvSrvUavHeap,
                m_CbvSrvUavHeapOffset,
                m_CbvSrvUavDescriptorSize);
            if (!destination.valid) {
                goto fail;
            }
            if (bindGroup.cbvSrvUavCount++ == 0) {
                bindGroup.cbvSrvUavTable = destination.gpu;
            }
            m_Device->CopyDescriptorsSimple(
                1, destination.cpu, source, heapType);
        }
    }

    handle.id = 0xFFFFFFFFFFFFFFFFull;
    handle.desc = desc;
    if (!m_FreeBindGroups.empty()) {
        handle.id = m_FreeBindGroups.back();
        m_FreeBindGroups.pop_back();
        m_BindGroups[handle.id] = bindGroup;
    } else {
        handle.id = static_cast<std::uint64_t>(m_BindGroups.size());
        m_BindGroups.push_back(bindGroup);
    }
    return handle;

fail:
    m_CbvSrvUavHeapOffset = cbvSrvUavOffset;
    m_SamplerHeapOffset = samplerOffset;
    return handle;
}

void DX12RHI::DestroyBindGroupLayout(rhi::BindGroupLayoutHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_BindGroupLayouts.size() ||
        m_BindGroupLayouts[handle.id].desc.shaderBindings.empty()) {
        return;
    }
    m_BindGroupLayouts[handle.id] = {};
    m_FreeBindGroupLayouts.push_back(handle.id);
}

void DX12RHI::DestroyBindGroup(rhi::BindGroupHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_BindGroups.size() ||
        m_BindGroups[handle.id].desc.entries.empty()) {
        return;
    }
    m_BindGroups[handle.id] = {};
    m_FreeBindGroups.push_back(handle.id);
}
