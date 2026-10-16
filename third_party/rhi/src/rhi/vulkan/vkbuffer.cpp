#include "rhi/vulkan/vkrhi.h"

#include <cstring>

rhi::BufferHandle VulkanRHI::CreateBuffer(const rhi::BufferDesc& desc) {
    VkBufferCreateInfo bufferDesc{};

    if (desc.size == 0) {
        printf("[ERROR] Buffer size must be greater than 0\n");
        return rhi::BufferHandle{};
    }

    bufferDesc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferDesc.size = desc.size;
    bufferDesc.usage = toVk(desc.usage);
    bufferDesc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo = ToVmaAllocInfo(desc.memoryUsage);

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    if (vmaCreateBuffer(m_Allocator, &bufferDesc, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS) {
        printf("[ERROR] Failed to create buffer\n");
        return rhi::BufferHandle{};
    }

    rhi::BufferHandle handle;
    handle.desc = desc;
    if (!m_FreeBuffers.empty()) {
        handle.id = m_FreeBuffers.back();
        m_FreeBuffers.pop_back();
        m_BufferMemory[handle.id] = allocation;
        m_Buffers[handle.id] = buffer;
    } else {
        handle.id = static_cast<uint64_t>(m_Buffers.size());
        m_BufferMemory.push_back(allocation);
        m_Buffers.push_back(buffer);
    }

    return handle;
}

void VulkanRHI::DestroyBuffer(rhi::BufferHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_Buffers.size() ||
        handle.id >= m_BufferMemory.size() ||
        m_Buffers[handle.id] == VK_NULL_HANDLE) {
        return;
    }

    vmaDestroyBuffer(m_Allocator, m_Buffers[handle.id], m_BufferMemory[handle.id]);
    m_Buffers[handle.id] = VK_NULL_HANDLE;
    m_BufferMemory[handle.id] = nullptr;
    m_FreeBuffers.push_back(handle.id);
}

void* VulkanRHI::MapBuffer(rhi::BufferHandle handle) {
    void* ptr = nullptr;
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_Buffers.size() ||
        handle.id >= m_BufferMemory.size() ||
        m_Buffers[handle.id] == VK_NULL_HANDLE ||
        m_BufferMemory[handle.id] == nullptr) {
        return ptr;
    }
    if (handle.desc.memoryUsage == rhi::MemoryUsage::GpuOnly) {
        printf("[ERROR] Cannot map a GPU-only buffer\n");
        return nullptr;
    }

    if (vmaMapMemory(m_Allocator, m_BufferMemory[handle.id], &ptr) != VK_SUCCESS) {
        printf("[ERROR] Failed to map buffer\n");
        return nullptr;
    }
    if (handle.desc.memoryUsage == rhi::MemoryUsage::GpuToCpu) {
        vmaInvalidateAllocation(m_Allocator, m_BufferMemory[handle.id], 0, VK_WHOLE_SIZE);
    }

    return ptr;
}

void VulkanRHI::UnmapBuffer(rhi::BufferHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_BufferMemory.size() ||
        m_BufferMemory[handle.id] == nullptr) {
        return;
    }
    if (handle.desc.memoryUsage == rhi::MemoryUsage::GpuOnly) {
        printf("[ERROR] Cannot unmap a GPU-only buffer\n");
        return;
    }
    vmaUnmapMemory(m_Allocator, m_BufferMemory[handle.id]);
}

void VulkanRHI::WriteBuffer(rhi::BufferHandle handle, const void* data, std::size_t size, std::size_t offset) {
    if (!m_Device || !data || size == 0 ||
        handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_Buffers.size() ||
        handle.id >= m_BufferMemory.size() ||
        m_Buffers[handle.id] == VK_NULL_HANDLE ||
        m_BufferMemory[handle.id] == nullptr) {
        return;
    }

    if (offset > handle.desc.size || size > handle.desc.size - offset) {
        printf("[ERROR] WriteBuffer range is outside the Vulkan buffer\n");
        return;
    }
    if (handle.desc.memoryUsage == rhi::MemoryUsage::GpuOnly) {
        printf("[ERROR] GPU-only Vulkan buffer writes require a staging buffer\n");
        return;
    }

    void* mapped = nullptr;
    if (vmaMapMemory(m_Allocator, m_BufferMemory[handle.id], &mapped) != VK_SUCCESS) {
        printf("[ERROR] Failed to map Vulkan buffer for writing\n");
        return;
    }

    std::memcpy(static_cast<std::byte*>(mapped) + offset, data, size);
    vmaFlushAllocation(m_Allocator, m_BufferMemory[handle.id], offset, size);
    vmaUnmapMemory(m_Allocator, m_BufferMemory[handle.id]);
}
