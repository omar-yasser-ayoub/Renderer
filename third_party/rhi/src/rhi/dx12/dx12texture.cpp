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
    // 3D textures use depth; 1D/2D/cube use the array layer count.
    resourceDesc.DepthOrArraySize = (desc.dimension == rhi::TextureDimension::Tex3D)
        ? static_cast<UINT16>(desc.depth)
        : static_cast<UINT16>(desc.arrayLayers);
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

        D3D12_SRV_DIMENSION srvDim = toD3D12SrvDimension(desc.dimension);
        // A Tex2D view onto an array texture must address a single slice, which
        // requires the array view dimension.
        if (srvDim == D3D12_SRV_DIMENSION_TEXTURE2D && textureDesc.arrayLayers > 1)
            srvDim = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.ViewDimension = srvDim;

        const UINT planeSlice = (desc.aspect == rhi::TextureAspect::Stencil) ? 1 : 0;
        if (srvDim == D3D12_SRV_DIMENSION_TEXTURE2DARRAY) {
            srvDesc.Texture2DArray.MostDetailedMip = desc.baseMipLevel;
            srvDesc.Texture2DArray.MipLevels = desc.mipLevels;
            srvDesc.Texture2DArray.FirstArraySlice = desc.baseArrayLayer;
            srvDesc.Texture2DArray.ArraySize = desc.arrayLayers;
            srvDesc.Texture2DArray.PlaneSlice = planeSlice;
        } else {
            srvDesc.Texture2D.MostDetailedMip = desc.baseMipLevel;
            srvDesc.Texture2D.MipLevels = desc.mipLevels;
            srvDesc.Texture2D.PlaneSlice = planeSlice;
        }

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

        D3D12_UAV_DIMENSION uavDim = toD3D12UavDimension(desc.dimension);
        if (uavDim == D3D12_UAV_DIMENSION_TEXTURE2D && textureDesc.arrayLayers > 1)
            uavDim = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
        uavDesc.ViewDimension = uavDim;

        if (uavDim == D3D12_UAV_DIMENSION_TEXTURE2DARRAY) {
            uavDesc.Texture2DArray.MipSlice = desc.baseMipLevel;
            uavDesc.Texture2DArray.FirstArraySlice = desc.baseArrayLayer;
            uavDesc.Texture2DArray.ArraySize = desc.arrayLayers;
        } else {
            uavDesc.Texture2D.MipSlice = desc.baseMipLevel;
        }

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

        D3D12_RTV_DIMENSION rtvDim = toD3D12RtvDimension(desc.dimension);
        if (rtvDim == D3D12_RTV_DIMENSION_TEXTURE2D && textureDesc.arrayLayers > 1)
            rtvDim = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.ViewDimension = rtvDim;

        if (rtvDim == D3D12_RTV_DIMENSION_TEXTURE2DARRAY) {
            rtvDesc.Texture2DArray.MipSlice = desc.baseMipLevel;
            rtvDesc.Texture2DArray.FirstArraySlice = desc.baseArrayLayer;
            rtvDesc.Texture2DArray.ArraySize = desc.arrayLayers;
        } else {
            rtvDesc.Texture2D.MipSlice = desc.baseMipLevel;
        }

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

        D3D12_DSV_DIMENSION dsvDim = toD3D12DsvDimension(desc.dimension);
        if (dsvDim == D3D12_DSV_DIMENSION_TEXTURE2D && textureDesc.arrayLayers > 1)
            dsvDim = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.ViewDimension = dsvDim;

        if (dsvDim == D3D12_DSV_DIMENSION_TEXTURE2DARRAY) {
            dsvDesc.Texture2DArray.MipSlice = desc.baseMipLevel;
            dsvDesc.Texture2DArray.FirstArraySlice = desc.baseArrayLayer;
            dsvDesc.Texture2DArray.ArraySize = desc.arrayLayers;
        } else {
            dsvDesc.Texture2D.MipSlice = desc.baseMipLevel;
        }

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
