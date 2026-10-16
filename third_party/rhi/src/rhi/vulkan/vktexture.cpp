#include "rhi/resources/texture.h"
#include "rhi/vulkan/vkrhi.h"

rhi::TextureHandle VulkanRHI::CreateTexture(const rhi::TextureDesc& desc) {

    if (desc.width <= 0 || desc.height <= 0 || desc.depth <= 0 || desc.mipLevels <= 0 || desc.arrayLayers <= 0) {
        printf("[ERROR] Texture dimensions must be greater than 0\n");
        return rhi::TextureHandle{};
    }

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = toVk(desc.dimension);
    imageInfo.extent.width = desc.width;
    imageInfo.extent.height = desc.height;
    imageInfo.extent.depth = desc.depth;
    imageInfo.mipLevels = desc.mipLevels;
    imageInfo.arrayLayers = desc.arrayLayers;
    imageInfo.format = toVk(desc.format);
    imageInfo.samples = toVk(desc.sampleCount);
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = toVk(desc.usage);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.flags = 0;
    if (desc.dimension == rhi::TextureDimension::Tex2D && desc.arrayLayers >= 6) {
        imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    VkImage image = VK_NULL_HANDLE;

    VmaAllocationCreateInfo allocInfo = ToVmaAllocInfo(rhi::MemoryUsage::GpuOnly);

    VmaAllocation allocation = nullptr;
    VkResult result = vmaCreateImage(
        m_Allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr);

    if (result != VK_SUCCESS) {
        printf("[ERROR] Failed to create texture image\n");
        return rhi::TextureHandle{};
    }

    rhi::TextureHandle handle;
    handle.desc = desc;
    if (!m_FreeTextures.empty()) {
        handle.id = m_FreeTextures.back();
        m_FreeTextures.pop_back();
        m_Textures[handle.id] = image;
        m_TextureMemory[handle.id] = allocation;
        m_TextureLayouts[handle.id] = VK_IMAGE_LAYOUT_UNDEFINED;
    } else {
        handle.id = m_Textures.size();
        m_Textures.push_back(image);
        m_TextureMemory.push_back(allocation);
        m_TextureLayouts.push_back(VK_IMAGE_LAYOUT_UNDEFINED);
    }
    return handle;
}

void VulkanRHI::DestroyTexture(rhi::TextureHandle handle) {
    if (handle.id >= m_Textures.size() || handle.id >= m_TextureMemory.size()) {
        return;
    }
    if (m_Textures[handle.id] == VK_NULL_HANDLE &&
        m_TextureMemory[handle.id] == nullptr) {
        return;
    }

    if (m_Textures[handle.id] != VK_NULL_HANDLE) {
        vmaDestroyImage(m_Allocator, m_Textures[handle.id], m_TextureMemory[handle.id]);
        m_Textures[handle.id] = VK_NULL_HANDLE;
        m_TextureMemory[handle.id] = nullptr;
        m_TextureLayouts[handle.id] = VK_IMAGE_LAYOUT_UNDEFINED;
    }
    m_FreeTextures.push_back(handle.id);
}

rhi::TextureViewHandle VulkanRHI::CreateTextureView(const rhi::TextureViewDesc& desc, rhi::TextureHandle texture) {
    if (texture.id >= m_Textures.size() || texture.id >= m_TextureMemory.size()) {
        printf("[ERROR] Texture handle out of bounds\n");
        return rhi::TextureViewHandle{};
    }

    if (m_Textures[texture.id] == VK_NULL_HANDLE) {
        printf("[ERROR] Texture handle is null\n");
        return rhi::TextureViewHandle{};
    }

    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_Textures[texture.id];

    VkImageViewType viewType = toVk(desc.dimension, desc.arrayLayers);
    if (viewType == VK_IMAGE_VIEW_TYPE_MAX_ENUM) {
        printf("[ERROR] Invalid texture dimension\n");
        return rhi::TextureViewHandle{};
    }
    viewInfo.viewType = viewType;
    viewInfo.format = toVk(desc.format);
    viewInfo.subresourceRange.aspectMask = toVk(desc.aspect);
    viewInfo.subresourceRange.baseMipLevel = desc.baseMipLevel;
    viewInfo.subresourceRange.levelCount = desc.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = desc.baseArrayLayer;
    viewInfo.subresourceRange.layerCount = desc.arrayLayers;
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    VkImageView view = VK_NULL_HANDLE;
    VkResult result = vkCreateImageView(m_Device, &viewInfo, nullptr, &view);
    if (result != VK_SUCCESS) {
        printf("[ERROR] Failed to create texture image view\n");
        return rhi::TextureViewHandle{};
    }

    rhi::TextureViewHandle handle;
    handle.desc = desc;
    handle.texture = texture;
    if (!m_FreeTextureViews.empty()) {
        handle.id = m_FreeTextureViews.back();
        m_FreeTextureViews.pop_back();
        m_TextureViews[handle.id] = view;
    } else {
        handle.id = m_TextureViews.size();
        m_TextureViews.push_back(view);
    }
    return handle;
}

void VulkanRHI::DestroyTextureView(rhi::TextureViewHandle handle) {
    if (handle.id >= m_TextureViews.size()) {
        return;
    }
    VkImageView view = m_TextureViews[handle.id];
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_Device, view, nullptr);
        m_TextureViews[handle.id] = VK_NULL_HANDLE;
        m_FreeTextureViews.push_back(handle.id);
    }
    return;
}
