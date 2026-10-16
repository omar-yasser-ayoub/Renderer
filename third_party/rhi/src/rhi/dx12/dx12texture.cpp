#include "rhi/dx12/dx12rhi.h"
#include <d3d12.h>

rhi::TextureHandle DX12RHI::CreateTexture(const rhi::TextureDesc& desc) {
    D3D12_RESOURCE_DESC resourceDesc = {};

    bool isDepthFormat = IsDepthFormat(desc.format);
    bool needsSrv = (desc.usage & rhi::TextureUsage::Sampled) != rhi::TextureUsage::None;

    resourceDesc.Format = (isDepthFormat && needsSrv)
        ? toD3D12Typeless(desc.format)
        : toD3D12(desc.format);
    resourceDesc.Dimension = toD3D12(desc.dimension);
    resourceDesc.Width = desc.width;
    resourceDesc.Height = desc.height;
    resourceDesc.DepthOrArraySize = desc.depth;
    resourceDesc.MipLevels = desc.mipLevels;
    resourceDesc.SampleDesc.Count = toD3D12(desc.sampleCount);
    resourceDesc.Flags = toD3D12(desc.usage);
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Alignment = 0;

    D3D12_HEAP_PROPERTIES heapProps = toD3D12(rhi::MemoryUsage::GpuOnly);

    D3D12_CLEAR_VALUE clearValue = {};
    D3D12_CLEAR_VALUE* pClearValue = nullptr;
    bool isRtOrDs = (desc.usage & (rhi::TextureUsage::RenderTarget | rhi::TextureUsage::DepthStencil)) != rhi::TextureUsage::None;
    if (isRtOrDs) {
        clearValue.Format = toD3D12(desc.format);
        pClearValue = &clearValue;
    }

    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = m_Device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        pClearValue,
        IID_PPV_ARGS(&resource));

    if (FAILED(hr)) {
        return rhi::TextureHandle{};
    }

    rhi::TextureHandle handle;
    handle.desc = desc;
    if (!m_FreeTextures.empty()) {
        handle.id = m_FreeTextures.back();
        m_FreeTextures.pop_back();
        m_Textures[handle.id] = resource;
        m_TextureStates[handle.id] = initialState;
    } else {
        handle.id = static_cast<uint32_t>(m_Textures.size());
        m_Textures.push_back(resource);
        m_TextureStates.push_back(initialState);
    }
    return handle;
}

rhi::TextureViewHandle DX12RHI::CreateTextureView(const rhi::TextureViewDesc& desc, rhi::TextureHandle handle) {
    ID3D12Resource* resource = m_Textures[handle.id].Get();
    const rhi::TextureDesc& textureDesc = handle.desc;

    TextureViewData viewData;

    bool wantsSrv = (textureDesc.usage & rhi::TextureUsage::Sampled) != rhi::TextureUsage::None;
    bool wantsUav = (textureDesc.usage & rhi::TextureUsage::Storage) != rhi::TextureUsage::None;
    bool wantsRtv = (textureDesc.usage & rhi::TextureUsage::RenderTarget) != rhi::TextureUsage::None;
    bool wantsDsv = (textureDesc.usage & rhi::TextureUsage::DepthStencil) != rhi::TextureUsage::None;

    if (wantsSrv) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        if (desc.aspect == rhi::TextureAspect::Stencil) {
            srvDesc.Format = toD3D12StencilSrv(textureDesc.format);
        } else if (desc.aspect == rhi::TextureAspect::Depth) {
            srvDesc.Format = toD3D12DepthSrv(textureDesc.format);
        } else {
            srvDesc.Format = toD3D12(desc.format);
        }
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.ViewDimension = toD3D12SrvDimension(desc.dimension);
        srvDesc.Texture2D.MostDetailedMip = desc.baseMipLevel;
        srvDesc.Texture2D.MipLevels = desc.mipLevels;
        srvDesc.Texture2D.PlaneSlice = (desc.aspect == rhi::TextureAspect::Stencil) ? 1 : 0;

        auto handles = AllocateDescriptor(
               m_CbvSrvUavHeap,
               m_CbvSrvUavHeapOffset,
               m_CbvSrvUavDescriptorSize);
        if (!handles.valid) {
            return rhi::TextureViewHandle{};
        }

        viewData.srv = handles.cpu;
        viewData.gpuSrv = handles.gpu;

        m_Device->CreateShaderResourceView(resource, &srvDesc, viewData.srv);
        viewData.hasSrv = true;
    }

    if (wantsUav) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = toD3D12(desc.format);
        uavDesc.ViewDimension = toD3D12UavDimension(desc.dimension);
        uavDesc.Texture2D.MipSlice = desc.baseMipLevel;

        auto handles = AllocateDescriptor(
               m_CbvSrvUavHeap,
               m_CbvSrvUavHeapOffset,
               m_CbvSrvUavDescriptorSize);

        if (!handles.valid) {
            return rhi::TextureViewHandle{};
        }

        viewData.uav = handles.cpu;
        viewData.gpuUav = handles.gpu;
        m_Device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, viewData.uav);
        viewData.hasUav = true;
    }

    if (wantsRtv) {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = toD3D12(desc.format);
        rtvDesc.ViewDimension = toD3D12RtvDimension(desc.dimension);
        rtvDesc.Texture2D.MipSlice = desc.baseMipLevel;

        auto handles = AllocateDescriptor(
               m_RtvHeap,
               m_RtvHeapOffset,
               m_RtvDescriptorSize);
        if (!handles.valid) {
            return rhi::TextureViewHandle{};
        }

        viewData.rtv = handles.cpu;
        viewData.gpuRtv = handles.gpu;
        m_Device->CreateRenderTargetView(resource, &rtvDesc, viewData.rtv);
        viewData.hasRtv = true;
    }

    if (wantsDsv) {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = toD3D12(desc.format);
        dsvDesc.ViewDimension = toD3D12DsvDimension(desc.dimension);
        dsvDesc.Texture2D.MipSlice = desc.baseMipLevel;

        auto handles = AllocateDescriptor(
               m_DsvHeap,
               m_DsvHeapOffset,
               m_DsvDescriptorSize);
        if (!handles.valid) {
            return rhi::TextureViewHandle{};
        }

        viewData.dsv = handles.cpu;
        viewData.gpuDsv = handles.gpu;
        m_Device->CreateDepthStencilView(resource, &dsvDesc, viewData.dsv);
        viewData.hasDsv = true;
    }

    rhi::TextureViewHandle result;
    result.desc = desc;
    result.texture = handle;
    if (!m_FreeTextureViews.empty()) {
        result.id = m_FreeTextureViews.back();
        m_FreeTextureViews.pop_back();
        m_TextureViews[result.id] = viewData;
    } else {
        result.id = static_cast<uint32_t>(m_TextureViews.size());
        m_TextureViews.push_back(viewData);
    }
    return result;
}

void DX12RHI::DestroyTextureView(rhi::TextureViewHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_TextureViews.size()) {
        return;
    }
    m_TextureViews[handle.id] = {};
    m_FreeTextureViews.push_back(handle.id);
}

void DX12RHI::DestroyTexture(rhi::TextureHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_Textures.size() ||
        !m_Textures[handle.id]) {
        return;
    }
    m_Textures[handle.id].Reset();
    m_FreeTextures.push_back(handle.id);
}
