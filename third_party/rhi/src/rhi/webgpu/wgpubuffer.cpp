#include "rhi/webgpu/wgpurhi.h"


bool NeedsStagingWrite(const rhi::BufferDesc& desc)
{
    if (desc.memoryUsage != rhi::MemoryUsage::CpuToGpu) {
        return false;
    }
    const rhi::BufferUsage bindMask =
        rhi::BufferUsage::Vertex | rhi::BufferUsage::Index | rhi::BufferUsage::Uniform |
        rhi::BufferUsage::Storage | rhi::BufferUsage::Indirect;
    return (desc.usage & bindMask) != rhi::BufferUsage::None;
}

rhi::BufferHandle WebGPURHI::CreateBuffer(const rhi::BufferDesc& desc) {

    rhi::BufferHandle handle;

    if (!m_Device || desc.size == 0) {
        printf("[RHI] Invalid WebGPU buffer description\n");
        return handle;
    }

    if (desc.memoryUsage == rhi::MemoryUsage::GpuToCpu) {
        const rhi::BufferUsage bindMask =
            rhi::BufferUsage::Vertex | rhi::BufferUsage::Index | rhi::BufferUsage::Uniform |
            rhi::BufferUsage::Storage | rhi::BufferUsage::Indirect;
        if ((desc.usage & bindMask) != rhi::BufferUsage::None) {
            printf("[RHI] GpuToCpu buffers cannot bind to the pipeline\n");
            return handle;
        }
    }
    wgpu::BufferDescriptor bufferDesc{};
    bufferDesc.size = desc.size;
    if (NeedsStagingWrite(desc)) {
        bufferDesc.usage = toWebGPU(desc.usage) | wgpu::BufferUsage::CopyDst;
    } else {
        bufferDesc.usage = toWebGPU(desc.usage) | toWebGPU(desc.memoryUsage);
    }

    wgpu::Buffer buffer = m_Device.CreateBuffer(&bufferDesc);
    if (!buffer) {
        printf("[RHI] Failed to create WebGPU buffer\n");
        return handle;
    }

    handle.desc = desc;
    if (!m_FreeBuffers.empty()) {
        handle.id = m_FreeBuffers.back();
        m_FreeBuffers.pop_back();
        m_Buffers[handle.id] = buffer;
        m_BufferMapped[handle.id] = false;
    } else {
        handle.id = static_cast<std::uint64_t>(m_Buffers.size());
        m_Buffers.push_back(buffer);
        m_BufferMapped.push_back(false);
    }

    return handle;
}

void* WebGPURHI::MapBuffer(rhi::BufferHandle handle) {
    if (handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Buffers.size() ||
        !m_Buffers[handle.id]) {
        return nullptr;
    }
    if (handle.desc.memoryUsage == rhi::MemoryUsage::GpuOnly) {
        printf("[RHI] Cannot map a GPU-only buffer\n");
        return nullptr;
    }
    if (NeedsStagingWrite(handle.desc)) {
        printf("[RHI] Cannot map a buffer that binds to the pipeline; stage explicitly\n");
        return nullptr;
    }
    wgpu::Buffer buffer = m_Buffers[handle.id];
    wgpu::BufferUsage usage = buffer.GetUsage();
    bool canWrite = (usage & wgpu::BufferUsage::MapWrite) != wgpu::BufferUsage::None;
    bool canRead  = (usage & wgpu::BufferUsage::MapRead)  != wgpu::BufferUsage::None;
    if (!canWrite && !canRead)
    {
        printf("[RHI] MapBuffer called on a buffer that is not mappable\n");
        return nullptr;
    }
    wgpu::MapMode mode = canWrite ? wgpu::MapMode::Write : wgpu::MapMode::Read;

    bool success = false;
    auto future = buffer.MapAsync(mode, 0, buffer.GetSize(), wgpu::CallbackMode::WaitAnyOnly,
            [&](wgpu::MapAsyncStatus status, wgpu::StringView message)
            {
                if (status != wgpu::MapAsyncStatus::Success) {
                    printf("[RHI] MapBuffer failed: %.*s\n",
                           static_cast<int>(message.length), message.data ? message.data : "");
                    return;
                }
                success = true;
            });

    m_Instance.WaitAny(future, UINT64_MAX);
    if (!success) {
        return nullptr;
    }
    if (handle.id < m_BufferMapped.size()) {
        m_BufferMapped[handle.id] = true;
    }
    if (mode == wgpu::MapMode::Read) {
        return const_cast<void*>(buffer.GetConstMappedRange(0, buffer.GetSize()));
    }
    return buffer.GetMappedRange(0, buffer.GetSize());
}

void WebGPURHI::UnmapBuffer(rhi::BufferHandle handle) {
    if (handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Buffers.size() ||
        !m_Buffers[handle.id]) {
        return;
    }
    if (handle.desc.memoryUsage == rhi::MemoryUsage::GpuOnly) {
        printf("[RHI] Cannot unmap a GPU-only buffer\n");
        return;
    }
    if (handle.id < m_BufferMapped.size() && !m_BufferMapped[handle.id]) {
        return;
    }
    m_Buffers[handle.id].Unmap();
    if (handle.id < m_BufferMapped.size()) {
        m_BufferMapped[handle.id] = false;
    }
}

void WebGPURHI::WriteBuffer(rhi::BufferHandle handle, const void* data, std::size_t size, std::size_t offset) {
    if (!m_Device || !data || size == 0 ||
        handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Buffers.size() ||
        !m_Buffers[handle.id]) {
        return;
    }

    if (offset > handle.desc.size || size > handle.desc.size - offset) {
        printf("[RHI] WriteBuffer range is outside the WebGPU buffer\n");
        return;
    }
    if (handle.desc.memoryUsage == rhi::MemoryUsage::GpuToCpu) {
        printf("[RHI] Cannot write directly to a GPU-to-CPU WebGPU buffer\n");
        return;
    }

    if (handle.desc.memoryUsage == rhi::MemoryUsage::CpuToGpu) {
        if (NeedsStagingWrite(handle.desc)) {
            // The buffer has COPY_DST: queue.WriteBuffer copies the data in
            // queue order, i.e. after any work already submitted and before
            // the next submit. No staging buffer and no mapping wait.
            m_Queue.WriteBuffer(m_Buffers[handle.id], offset, data, size);
            return;
        }

        // A mappable buffer: write through its mapped range.
        wgpu::Buffer mappedBuffer = m_Buffers[handle.id];
        bool success = false;
        auto future = mappedBuffer.MapAsync(
            wgpu::MapMode::Write,
            offset,
            size,
            wgpu::CallbackMode::WaitAnyOnly,
            [&](wgpu::MapAsyncStatus status, wgpu::StringView message) {
                if (status != wgpu::MapAsyncStatus::Success) {
                    printf("[RHI] MapBuffer failed: %.*s\n",
                           static_cast<int>(message.length),
                           message.data ? message.data : "");
                    return;
                }
                success = true;
            });
        m_Instance.WaitAny(future, UINT64_MAX);
        if (!success) {
            return;
        }

        void* mapped = mappedBuffer.GetMappedRange(offset, size);
        if (!mapped) {
            printf("[RHI] Failed to get mapped WebGPU buffer range\n");
            mappedBuffer.Unmap();
            return;
        }
        std::memcpy(mapped, data, size);
        mappedBuffer.Unmap();
        return;
    }

    m_Queue.WriteBuffer(m_Buffers[handle.id], offset, data, size);
}

void WebGPURHI::DestroyBuffer(rhi::BufferHandle handle)
{
    if (handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Buffers.size() ||
        !m_Buffers[handle.id]) {
        return;
    }
    m_Buffers[handle.id].Destroy();
    m_Buffers[handle.id] = {};
    if (handle.id < m_BufferMapped.size()) {
        m_BufferMapped[handle.id] = false;
    }
    m_FreeBuffers.push_back(handle.id);
}
