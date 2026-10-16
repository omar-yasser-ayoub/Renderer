#include "rhi/vulkan/vkrhi.h"
#include <vector>

rhi::BindGroupLayoutHandle VulkanRHI::CreateBindGroupLayout(const rhi::BindGroupLayoutDesc& desc) {
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
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.reserve(desc.shaderBindings.size());

    int currentSet = -1;
    for (const auto& shaderBinding : desc.shaderBindings) {
        if (currentSet == -1)
            currentSet = shaderBinding.set;
        else if (currentSet != shaderBinding.set)
            return handle;

        for (const auto& other : desc.shaderBindings) {
            if (&other != &shaderBinding && other.binding == shaderBinding.binding)
                return handle;
        }

        VkDescriptorSetLayoutBinding vkBinding{};
        vkBinding.binding = shaderBinding.binding;
        vkBinding.descriptorCount = shaderBinding.arraySize;
        vkBinding.stageFlags = 0;
        vkBinding.pImmutableSamplers = nullptr;

        if (rhi::HasFlag(shaderBinding.stage, rhi::ShaderStageFlags::Vertex))
            vkBinding.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;

        if (rhi::HasFlag(shaderBinding.stage, rhi::ShaderStageFlags::Fragment))
            vkBinding.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;

        if (rhi::HasFlag(shaderBinding.stage, rhi::ShaderStageFlags::Compute))
            vkBinding.stageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;

        switch(shaderBinding.type) {
            case rhi::BindingType::UniformBuffer:
                vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                break;
            case rhi::BindingType::StorageBuffer:
                vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                break;
            case rhi::BindingType::SampledTexture:
                vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                break;
            case rhi::BindingType::StorageTexture:
                vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                break;
            case rhi::BindingType::Sampler:
                vkBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                break;
        }
        bindings.push_back(vkBinding);
    }

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(
            m_Device,
            &createInfo,
            nullptr,
            &layout) != VK_SUCCESS)
    {
        return handle;
    }

    handle.id = 0xFFFFFFFFFFFFFFFFull;
    handle.desc = desc;
    if (!m_FreeBindGroupLayouts.empty()) {
        handle.id = m_FreeBindGroupLayouts.back();
        m_FreeBindGroupLayouts.pop_back();
        m_BindGroupLayouts[handle.id] = layout;
    } else {
        handle.id = m_BindGroupLayouts.size();
        m_BindGroupLayouts.push_back(layout);
    }

    return handle;

}

rhi::BindGroupHandle VulkanRHI::CreateBindGroup(const rhi::BindGroupDesc& desc) {
    rhi::BindGroupHandle handle;
    handle.id = 0xFFFFFFFFFFFFFFFFull;

    if (desc.layout.id >= m_BindGroupLayouts.size() ||
        m_BindGroupLayouts[desc.layout.id] == VK_NULL_HANDLE)
        return handle;
    if (desc.entries.empty())
        return handle;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = 1;

    VkDescriptorSetLayout layout = m_BindGroupLayouts[desc.layout.id];
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set;
    if (vkAllocateDescriptorSets(
            m_Device,
            &allocInfo,
            &set) != VK_SUCCESS)
    {
        return handle;
    }

    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    writes.reserve(desc.entries.size());
    imageInfos.reserve(desc.entries.size());
    bufferInfos.reserve(desc.entries.size());

    auto findLayoutType = [&](std::uint32_t binding) -> const rhi::ShaderBinding* {
        for (const auto& shaderBinding : desc.layout.desc.shaderBindings) {
            if (shaderBinding.binding == binding)
                return &shaderBinding;
        }
        return nullptr;
    };

    for (const auto& binding : desc.entries)
    {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = binding.binding;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;

        const bool hasBuffer = binding.buffer.id != 0xFFFFFFFFFFFFFFFFull;
        const bool hasTexture = binding.textureView.id != 0xFFFFFFFFFFFFFFFFull;
        const bool hasSampler = binding.sampler.id != 0xFFFFFFFFFFFFFFFFull;
        if (!hasBuffer && !hasTexture && !hasSampler) {
            vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &set);
            return rhi::BindGroupHandle{};
        }

        const rhi::ShaderBinding* layoutBinding = findLayoutType(binding.binding);
        if (layoutBinding == nullptr) {
            vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &set);
            return rhi::BindGroupHandle{};
        }

        if (hasBuffer && !hasTexture && !hasSampler)
        {
            if (binding.buffer.id >= m_Buffers.size() ||  m_Buffers[binding.buffer.id] == VK_NULL_HANDLE) {
                    vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &set);
                    return rhi::BindGroupHandle{};
            }

            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_Buffers[binding.buffer.id];
            bufferInfo.offset = 0;
            bufferInfo.range = VK_WHOLE_SIZE;

            // Descriptor type comes from the layout; buffer usage is validated.
            if (layoutBinding->type == rhi::BindingType::UniformBuffer) {
                if (!rhi::HasFlag(binding.buffer.desc.usage, rhi::BufferUsage::Uniform)) {
                    vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &set);
                    return rhi::BindGroupHandle{};
                }
                write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            } else if (layoutBinding->type == rhi::BindingType::StorageBuffer) {
                if (!rhi::HasFlag(binding.buffer.desc.usage, rhi::BufferUsage::Storage)) {
                    vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &set);
                    return rhi::BindGroupHandle{};
                }
                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            } else {
               vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &set);
               return rhi::BindGroupHandle{};
            }
            bufferInfos.push_back(bufferInfo);
            write.pBufferInfo = &bufferInfos.back();
        }
        else if (hasTexture || hasSampler)
        {
            const bool combined = hasTexture && hasSampler;
            if (hasTexture && (binding.textureView.id >= m_TextureViews.size() || m_TextureViews[binding.textureView.id] == VK_NULL_HANDLE)) {
                vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &set);
                return rhi::BindGroupHandle{};
            }
            if (hasSampler && (binding.sampler.id >= m_Samplers.size() || m_Samplers[binding.sampler.id] == VK_NULL_HANDLE)) {
                vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &set);
                return rhi::BindGroupHandle{};
            }

            VkDescriptorImageInfo imageInfo{};
            if (hasTexture)
                imageInfo.imageView = m_TextureViews[binding.textureView.id];
            if (hasSampler)
                imageInfo.sampler = m_Samplers[binding.sampler.id];

            if (layoutBinding->type == rhi::BindingType::SampledTexture && !combined) {
                write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            } else if (layoutBinding->type == rhi::BindingType::StorageTexture && hasTexture && !hasSampler) {
                write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            } else if (layoutBinding->type == rhi::BindingType::Sampler && hasSampler && !hasTexture) {
                write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            } else if (combined &&
                       (layoutBinding->type == rhi::BindingType::SampledTexture)) {
                write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            } else {
                vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &set);
                return rhi::BindGroupHandle{};
            }
            imageInfos.push_back(imageInfo);
            write.pImageInfo = &imageInfos.back();
        }
        else
        {
            vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &set);
            return rhi::BindGroupHandle{};
        }

        writes.push_back(write);
    }

    for (std::size_t i = 0, b = 0, im = 0; i < writes.size(); ++i) {
        if (writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
            writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
            writes[i].pBufferInfo = &bufferInfos[b++];
        else
            writes[i].pImageInfo = &imageInfos[im++];
    }

    vkUpdateDescriptorSets(
           m_Device,
           static_cast<uint32_t>(writes.size()),
           writes.data(),
           0,
           nullptr);


    handle.id = 0xFFFFFFFFFFFFFFFFull;
    handle.desc = desc;
    if (!m_FreeBindGroups.empty()) {
        handle.id = m_FreeBindGroups.back();
        m_FreeBindGroups.pop_back();
        m_BindGroups[handle.id] = set;
    } else {
        handle.id = m_BindGroups.size();
        m_BindGroups.push_back(set);
    }

    return handle;

}

void VulkanRHI::DestroyBindGroupLayout(rhi::BindGroupLayoutHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_BindGroupLayouts.size() ||
        m_BindGroupLayouts[handle.id] == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyDescriptorSetLayout(m_Device, m_BindGroupLayouts[handle.id], nullptr);
    m_BindGroupLayouts[handle.id] = VK_NULL_HANDLE;
    m_FreeBindGroupLayouts.push_back(handle.id);
}

void VulkanRHI::DestroyBindGroup(rhi::BindGroupHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_BindGroups.size() ||
        m_BindGroups[handle.id] == VK_NULL_HANDLE) {
        return;
    }
    vkFreeDescriptorSets(m_Device, m_DescriptorPool, 1, &m_BindGroups[handle.id]);
    m_BindGroups[handle.id] = VK_NULL_HANDLE;
    m_FreeBindGroups.push_back(handle.id);
}
