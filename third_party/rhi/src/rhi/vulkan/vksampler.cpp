#include "rhi/vulkan/vkrhi.h"


rhi::SamplerHandle VulkanRHI::CreateSampler(const rhi::SamplerDesc& desc) {
    if (desc.anisotropyEnabled && !m_Capabilities.SupportsAnisotropy) {
        printf("[ERROR] Sampler anisotropy requested but unsupported by device\n");
        return rhi::SamplerHandle{};
    }
    float maxAnisotropy = 1.0f;
    if (desc.anisotropyEnabled) {
        maxAnisotropy = desc.maxAnisotropy < 1 ? 1.0f : static_cast<float>(desc.maxAnisotropy);
        if (maxAnisotropy > m_MaxSamplerAnisotropy) {
            maxAnisotropy = m_MaxSamplerAnisotropy;
        }
    }
    VkSamplerCreateInfo samplerDesc = {};
    samplerDesc.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerDesc.magFilter = toVk(desc.magFilter);
    samplerDesc.minFilter = toVk(desc.minFilter);
    samplerDesc.mipmapMode = toVk(desc.mipFilter);
    samplerDesc.addressModeU = toVk(desc.addressU);
    samplerDesc.addressModeV = toVk(desc.addressV);
    samplerDesc.addressModeW = toVk(desc.addressW);
    samplerDesc.mipLodBias = desc.mipLodBias;
    samplerDesc.anisotropyEnable = desc.anisotropyEnabled ? VK_TRUE : VK_FALSE;
    samplerDesc.maxAnisotropy = maxAnisotropy;
    samplerDesc.compareEnable = desc.compareEnabled;
    samplerDesc.compareOp = toVk(desc.compareOp);
    samplerDesc.minLod = desc.minLOD;
    samplerDesc.maxLod = desc.maxLOD;
    samplerDesc.borderColor = toVk(desc.borderColor);

    VkSampler sampler;

    if (vkCreateSampler(m_Device, &samplerDesc, nullptr, &sampler) != VK_SUCCESS) {
        printf("[ERROR] Failed to create sampler\n");
        return rhi::SamplerHandle{};
    }

    rhi::SamplerHandle handle;
    handle.desc = desc;
    if (!m_FreeSamplers.empty()) {
        handle.id = m_FreeSamplers.back();
        m_FreeSamplers.pop_back();
        m_Samplers[handle.id] = sampler;
    } else {
        handle.id = static_cast<uint64_t>(m_Samplers.size());
        m_Samplers.push_back(sampler);
    }
    return handle;
}

void VulkanRHI::DestroySampler(rhi::SamplerHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Samplers.size() ||
        m_Samplers[handle.id] == VK_NULL_HANDLE) {
        return;
    }
    vkDestroySampler(m_Device, m_Samplers[handle.id], nullptr);
    m_Samplers[handle.id] = VK_NULL_HANDLE;
    m_FreeSamplers.push_back(handle.id);
}
