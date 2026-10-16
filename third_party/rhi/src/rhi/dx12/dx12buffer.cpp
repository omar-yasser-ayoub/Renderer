#include "rhi/dx12/dx12rhi.h"
#include <cstring>

rhi::BufferHandle DX12RHI::CreateBuffer(const rhi::BufferDesc& desc) {
    if (!m_Device || desc.size == 0) {
        printf("[ERROR] Invalid buffer description\n");
        return rhi::BufferHandle{};
    }

    D3D12_HEAP_PROPERTIES heapProps = toD3D12(desc.memoryUsage);
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
    if (desc.memoryUsage == rhi::MemoryUsage::CpuToGpu) {
        initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
    } else if (desc.memoryUsage == rhi::MemoryUsage::GpuToCpu) {
        initialState = D3D12_RESOURCE_STATE_COPY_DEST;
    }
    UINT64 resourceWidth = desc.size;
    if (rhi::HasFlag(desc.usage, rhi::BufferUsage::Uniform)) {
        resourceWidth = (static_cast<UINT64>(desc.size) + 255) & ~255ull;
    }

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment          = 0;
    resourceDesc.Width              = resourceWidth;
    resourceDesc.Height             = 1;
    resourceDesc.DepthOrArraySize   = 1;
    resourceDesc.MipLevels          = 1;
    resourceDesc.Format             = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count   = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;
    if (rhi::HasFlag(desc.usage, rhi::BufferUsage::Storage))
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT result = m_Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&resource));
    if (FAILED(result)) {
        printf("[ERROR] Failed to create buffer: 0x%08X\n",
               static_cast<unsigned>(result));
        return rhi::BufferHandle{};
    }

    // buffers can't be accessed in dx12 without a view, unlike vulkan. So, we
    // create a default view for the buffer here, and add it to the vector of buffer views, with the same index
    // as the buffer itself.

    BufferViewData viewData{};
    if (rhi::HasFlag(desc.usage, rhi::BufferUsage::Uniform)) {
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
            cbvDesc.BufferLocation = resource->GetGPUVirtualAddress();

            cbvDesc.SizeInBytes =
                static_cast<UINT>((desc.size + 255) & ~255ull);

            auto handles = AllocateDescriptor(
                m_CbvSrvUavHeap,
                m_CbvSrvUavHeapOffset,
                m_CbvSrvUavDescriptorSize);
            if (!handles.valid) {
                return rhi::BufferHandle{};
            }

            viewData.cbv = handles.cpu;
            viewData.gpuCbv = handles.gpu;

            m_Device->CreateConstantBufferView(
                &cbvDesc,
                viewData.cbv);

            viewData.hasCbv = true;
        }

    // Storage buffer → raw UAV
       if (rhi::HasFlag(desc.usage, rhi::BufferUsage::Storage)) {

           D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
           uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
           uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
           uavDesc.Buffer.FirstElement = 0;
           uavDesc.Buffer.NumElements =
               static_cast<UINT>(desc.size / 4);
           uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

           auto handles = AllocateDescriptor(
               m_CbvSrvUavHeap,
               m_CbvSrvUavHeapOffset,
               m_CbvSrvUavDescriptorSize);
           if (!handles.valid) {
               return rhi::BufferHandle{};
           }

           viewData.uav = handles.cpu;
           viewData.gpuUav = handles.gpu;


           m_Device->CreateUnorderedAccessView(
               resource.Get(),
               nullptr,
               &uavDesc,
               handles.cpu);

           viewData.hasUav = true;
       }

        rhi::BufferHandle handle;
        handle.desc = desc;
        if (!m_FreeBuffers.empty()) {
            handle.id = m_FreeBuffers.back();
            m_FreeBuffers.pop_back();
            m_Buffers[handle.id] = std::move(resource);
            m_BufferViews[handle.id] = viewData;
            m_BufferStates[handle.id] = initialState;
        } else {
            handle.id = m_Buffers.size();
            m_Buffers.push_back(std::move(resource));
            m_BufferViews.push_back(viewData);
            m_BufferStates.push_back(initialState);
        }

        return handle;
}



void DX12RHI::DestroyBuffer(rhi::BufferHandle handle)
{
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Buffers.size() ||
        !m_Buffers[handle.id]) {
        return;
    }
    m_Buffers[handle.id].Reset();
    m_BufferViews[handle.id] = {};
    m_FreeBuffers.push_back(handle.id);
}

void* DX12RHI::MapBuffer(rhi::BufferHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Buffers.size() ||
        !m_Buffers[handle.id]) {
        return nullptr;
    }
    if (handle.desc.memoryUsage == rhi::MemoryUsage::GpuOnly) {
        printf("[ERROR] Cannot map a GPU-only buffer\n");
        return nullptr;
    }

    void* ptr = nullptr;
    D3D12_RANGE readRange{};
    if (handle.desc.memoryUsage == rhi::MemoryUsage::GpuToCpu) {
        readRange.End = static_cast<SIZE_T>(handle.desc.size);
    }

    if (FAILED(m_Buffers[handle.id]->Map(0, &readRange, &ptr)))
        {
            printf("[ERROR] Failed to map buffer\n");
            return nullptr;
        }

    return ptr;
}

void DX12RHI::UnmapBuffer(rhi::BufferHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Buffers.size() ||
        !m_Buffers[handle.id]) {
        return;
    }
    if (handle.desc.memoryUsage == rhi::MemoryUsage::GpuOnly) {
        printf("[ERROR] Cannot unmap a GPU-only buffer\n");
        return;
    }
    m_Buffers[handle.id]->Unmap(0, nullptr);
}

void DX12RHI::WriteBuffer(rhi::BufferHandle handle, const void* data, std::size_t size, std::size_t offset) {
    if (!m_Device || !data || size == 0 ||
        handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Buffers.size() ||
        !m_Buffers[handle.id]) {
        return;
    }

    if (offset > handle.desc.size || size > handle.desc.size - offset) {
        printf("[ERROR] WriteBuffer range is outside the DX12 buffer\n");
        return;
    }
    if (handle.desc.memoryUsage == rhi::MemoryUsage::GpuOnly) {
        printf("[ERROR] GPU-only DX12 buffer writes require an upload buffer\n");
        return;
    }

    void* mapped = nullptr;
    D3D12_RANGE readRange{};
    HRESULT result = m_Buffers[handle.id]->Map(0, &readRange, &mapped);
    if (FAILED(result)) {
        printf("[ERROR] Failed to map DX12 buffer for writing: 0x%08X\n",
               static_cast<unsigned>(result));
        return;
    }

    std::memcpy(static_cast<std::byte*>(mapped) + offset, data, size);
    D3D12_RANGE writtenRange{
        static_cast<SIZE_T>(offset),
        static_cast<SIZE_T>(offset + size)
    };
    m_Buffers[handle.id]->Unmap(0, &writtenRange);
}
