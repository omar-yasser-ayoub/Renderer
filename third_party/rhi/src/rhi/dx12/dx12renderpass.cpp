#include "rhi/dx12/dx12rhi.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

rhi::CommandListHandle DX12RHI::BeginCommandList()
{
    rhi::CommandListHandle handle;
    if (!m_Device || m_CurrentFrame == 0xFFFFFFFF) {
        return handle;
    }
    FrameContext& frame = m_Frames[m_CurrentFrame];
    if (!frame.allocator) {
        return handle;
    }

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
    if (FAILED(m_Device->CreateCommandList(
            0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame.allocator.Get(),
            nullptr, IID_PPV_ARGS(&list)))) {
        return handle;
    }

    DX12CommandList data;
    data.list = list;
    data.frame = m_CurrentFrame;
    data.open = true;

    if (!m_FreeCommandLists.empty()) {
        handle.id = m_FreeCommandLists.back();
        m_FreeCommandLists.pop_back();
        m_CommandLists[handle.id] = std::move(data);
    } else {
        handle.id = static_cast<std::uint64_t>(m_CommandLists.size());
        m_CommandLists.push_back(std::move(data));
    }
    return handle;
}

ID3D12GraphicsCommandList* DX12RHI::GetCommandList(rhi::CommandListHandle handle)
{
    if (handle.id >= m_CommandLists.size()) {
        return nullptr;
    }
    DX12CommandList& list = m_CommandLists[handle.id];
    if (!list.open || !list.list) {
        return nullptr;
    }
    return list.list.Get();
}

void DX12RHI::TransitionResource(ID3D12GraphicsCommandList* cmd, ID3D12Resource* resource,
                                 D3D12_RESOURCE_STATES& stored, D3D12_RESOURCE_STATES target)
{
    if (cmd == nullptr || resource == nullptr || stored == target) {
        return;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stored;
    barrier.Transition.StateAfter = target;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &barrier);
    stored = target;
}

void DX12RHI::RequireBufferState(ID3D12GraphicsCommandList* cmd, const rhi::BufferHandle& buffer,
                                 D3D12_RESOURCE_STATES target)
{
    if (cmd == nullptr || buffer.id == 0xFFFFFFFFFFFFFFFFull ||
        buffer.id >= m_Buffers.size() || buffer.id >= m_BufferStates.size() ||
        !m_Buffers[buffer.id]) {
        return;
    }
    if (buffer.desc.memoryUsage != rhi::MemoryUsage::GpuOnly) {
        return;
    }
    TransitionResource(cmd, m_Buffers[buffer.id].Get(), m_BufferStates[buffer.id], target);
}

void DX12RHI::RequireTextureState(ID3D12GraphicsCommandList* cmd, const rhi::TextureHandle& texture,
                                  D3D12_RESOURCE_STATES target)
{
    if (cmd == nullptr || texture.id == 0xFFFFFFFFFFFFFFFFull ||
        texture.id >= m_Textures.size() || texture.id >= m_TextureStates.size() ||
        !m_Textures[texture.id]) {
        return;
    }
    TransitionResource(cmd, m_Textures[texture.id].Get(), m_TextureStates[texture.id], target);
}

void DX12RHI::RequireSwapchainState(ID3D12GraphicsCommandList* cmd, std::uint64_t swapchain,
                                    std::uint32_t imageIndex, D3D12_RESOURCE_STATES target)
{
    if (cmd == nullptr || swapchain >= m_Swapchains.size()) {
        return;
    }
    DX12SwapchainData& data = m_Swapchains[swapchain];
    if (!data.swapchain || imageIndex >= data.buffers.size() ||
        imageIndex >= data.bufferStates.size() || !data.buffers[imageIndex]) {
        return;
    }
    TransitionResource(cmd, data.buffers[imageIndex].Get(), data.bufferStates[imageIndex], target);
}

void DX12RHI::BeginRenderPass(rhi::CommandListHandle cmd, const rhi::RenderPassDesc& desc)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    DX12CommandList& state = m_CommandLists[cmd.id];
    if (state.pass != CurrentPass::None) {
        printf("[ERROR] BeginRenderPass: a pass is already open on this command list\n");
        return;
    }
    if (desc.colors.empty() && !desc.depth.enabled) {
        printf("[ERROR] BeginRenderPass needs at least one color attachment or a depth attachment\n");
        return;
    }

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs;
    rtvs.reserve(desc.colors.size());
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    bool extentSet = false;

    for (const auto& attachment : desc.colors) {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
        bool hasRtv = false;
        if (attachment.useSwapchain) {
            if (desc.swapchain.id == 0xFFFFFFFFFFFFFFFF ||
                desc.swapchain.id >= m_Swapchains.size()) {
                printf("[ERROR] BeginRenderPass swapchain handle is invalid\n");
                return;
            }
            DX12SwapchainData& swapchain = m_Swapchains[desc.swapchain.id];
            if (desc.imageIndex >= swapchain.buffers.size() ||
                desc.imageIndex >= swapchain.rtvs.size()) {
                printf("[ERROR] BeginRenderPass swapchain image index is invalid\n");
                return;
            }
            state.swapchain = desc.swapchain.id;
            state.swapchainImageIndex = desc.imageIndex;
            RequireSwapchainState(list, desc.swapchain.id, desc.imageIndex,
                                  D3D12_RESOURCE_STATE_RENDER_TARGET);
            rtv = swapchain.rtvs[desc.imageIndex];
            hasRtv = true;
            if (!extentSet) {
                width = swapchain.width;
                height = swapchain.height;
                extentSet = true;
            }
        } else {
            if (attachment.view.id == 0xFFFFFFFFFFFFFFFF ||
                attachment.view.id >= m_TextureViews.size()) {
                printf("[ERROR] BeginRenderPass color view handle is invalid\n");
                return;
            }
            const rhi::TextureHandle& parent = attachment.view.texture;
            if (parent.id == 0xFFFFFFFFFFFFFFFF ||
                parent.id >= m_Textures.size() ||
                !m_Textures[parent.id]) {
                printf("[ERROR] BeginRenderPass color view has no parent texture\n");
                return;
            }
            const std::uint32_t mip = attachment.view.desc.baseMipLevel;
            const std::uint32_t mipWidth = parent.desc.width >> mip;
            const std::uint32_t mipHeight = parent.desc.height >> mip;
            const std::uint32_t viewWidth = mipWidth > 1u ? mipWidth : 1u;
            const std::uint32_t viewHeight = mipHeight > 1u ? mipHeight : 1u;
            if (!extentSet) {
                width = viewWidth;
                height = viewHeight;
                extentSet = true;
            } else if (width != viewWidth || height != viewHeight) {
                printf("[ERROR] BeginRenderPass: color attachments have mismatched extents\n");
                return;
            }
            RequireTextureState(list, parent, D3D12_RESOURCE_STATE_RENDER_TARGET);
            if (!m_TextureViews[attachment.view.id].hasRtv) {
                printf("[ERROR] BeginRenderPass color view has no render-target view\n");
                return;
            }
            rtv = m_TextureViews[attachment.view.id].rtv;
            hasRtv = true;
        }
        if (hasRtv && attachment.clear) {
            list->ClearRenderTargetView(rtv, attachment.clearColor, 0, nullptr);
        }
        rtvs.push_back(rtv);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE dsvStorage{};
    D3D12_CPU_DESCRIPTOR_HANDLE* dsv = nullptr;
    if (desc.depth.enabled) {
        if (desc.depth.view.id == 0xFFFFFFFFFFFFFFFF ||
            desc.depth.view.id >= m_TextureViews.size()) {
            printf("[ERROR] BeginRenderPass depth view handle is invalid\n");
            return;
        }
        const rhi::TextureHandle& depthParent = desc.depth.view.texture;
        if (depthParent.id == 0xFFFFFFFFFFFFFFFF ||
            depthParent.id >= m_Textures.size() ||
            !m_Textures[depthParent.id]) {
            printf("[ERROR] BeginRenderPass depth view has no parent texture\n");
            return;
        }
        if (!m_TextureViews[desc.depth.view.id].hasDsv) {
            printf("[ERROR] BeginRenderPass depth view has no depth-stencil view\n");
            return;
        }
        RequireTextureState(list, depthParent, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        dsvStorage = m_TextureViews[desc.depth.view.id].dsv;
        dsv = &dsvStorage;
        if (desc.depth.clearDepth) {
            list->ClearDepthStencilView(
                dsvStorage, D3D12_CLEAR_FLAG_DEPTH,
                desc.depth.clearDepthValue, 0, 0, nullptr);
        }
    }

    list->OMSetRenderTargets(
        static_cast<UINT>(rtvs.size()), rtvs.empty() ? nullptr : rtvs.data(), FALSE, dsv);
    state.pass = CurrentPass::Graphics;
    state.computeBindPoint = false;
}

void DX12RHI::BeginComputePass(rhi::CommandListHandle cmd)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    DX12CommandList& state = m_CommandLists[cmd.id];
    if (state.pass != CurrentPass::None) {
        printf("[ERROR] BeginComputePass: a pass is already open on this command list\n");
        return;
    }
    state.pass = CurrentPass::Compute;
    state.computeBindPoint = true;
}

void DX12RHI::EndPass(rhi::CommandListHandle cmd)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    DX12CommandList& state = m_CommandLists[cmd.id];
    if (state.pass == CurrentPass::Graphics) {
        if (state.swapchain != 0xFFFFFFFFFFFFFFFFull) {
            RequireSwapchainState(list, state.swapchain, state.swapchainImageIndex,
                                  D3D12_RESOURCE_STATE_PRESENT);
        }
        state.pass = CurrentPass::None;
        state.swapchain = 0xFFFFFFFFFFFFFFFFull;
        state.swapchainImageIndex = 0xFFFFFFFF;
    } else if (state.pass == CurrentPass::Compute) {
        state.pass = CurrentPass::None;
    }
}

void DX12RHI::SetPipeline(rhi::CommandListHandle cmd, rhi::GraphicsPipelineHandle handle)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    if (handle.id >= m_Pipelines.size() || !m_Pipelines[handle.id]) {
        printf("[ERROR] SetPipeline: invalid graphics pipeline handle\n");
        return;
    }
    DX12CommandList& state = m_CommandLists[cmd.id];
    if (handle.desc.layout.id >= m_PipelineLayouts.size() ||
        !m_PipelineLayouts[handle.desc.layout.id].rootSignature) {
        printf("[ERROR] SetPipeline: pipeline layout is invalid\n");
        return;
    }
    state.boundLayout = handle.desc.layout;
    state.boundGraphicsPipeline = handle;
    state.computeBindPoint = false;
    list->SetGraphicsRootSignature(m_PipelineLayouts[handle.desc.layout.id].rootSignature.Get());
    list->SetPipelineState(m_Pipelines[handle.id].Get());
    list->IASetPrimitiveTopology(toDxTopology(handle.desc.topology));
}

void DX12RHI::SetPipeline(rhi::CommandListHandle cmd, rhi::ComputePipelineHandle handle)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    if (handle.id >= m_ComputePipelines.size() || !m_ComputePipelines[handle.id]) {
        printf("[ERROR] SetPipeline: invalid compute pipeline handle\n");
        return;
    }
    DX12CommandList& state = m_CommandLists[cmd.id];
    if (handle.desc.layout.id >= m_PipelineLayouts.size() ||
        !m_PipelineLayouts[handle.desc.layout.id].rootSignature) {
        printf("[ERROR] SetPipeline: pipeline layout is invalid\n");
        return;
    }
    state.boundLayout = handle.desc.layout;
    state.computeBindPoint = true;
    list->SetComputeRootSignature(m_PipelineLayouts[handle.desc.layout.id].rootSignature.Get());
    list->SetPipelineState(m_ComputePipelines[handle.id].Get());
}

void DX12RHI::SetBindGroup(rhi::CommandListHandle cmd, std::uint32_t slot, rhi::BindGroupHandle handle)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    if (handle.id >= m_BindGroups.size() ||
        m_BindGroups[handle.id].desc.entries.empty()) {
        printf("[ERROR] SetBindGroup: invalid bind group handle\n");
        return;
    }
    DX12CommandList& state = m_CommandLists[cmd.id];
    if (state.boundLayout.id == 0xFFFFFFFFFFFFFFFFull ||
        state.boundLayout.id >= m_PipelineLayouts.size()) {
        printf("[ERROR] SetBindGroup: no pipeline bound yet\n");
        return;
    }
    const PipelineLayoutData& layout = m_PipelineLayouts[state.boundLayout.id];
    if (slot >= layout.resourceParam.size() || slot >= layout.samplerParam.size()) {
        printf("[ERROR] SetBindGroup: slot is out of range\n");
        return;
    }
    const BindGroupData& group = m_BindGroups[handle.id];

    for (const auto& entry : group.desc.entries) {
        const rhi::ShaderBinding* shaderBinding = nullptr;
        for (const auto& candidate : group.desc.layout.desc.shaderBindings) {
            if (candidate.binding == entry.binding) {
                shaderBinding = &candidate;
                break;
            }
        }
        if (shaderBinding == nullptr) {
            continue;
        }
        if (entry.buffer.id != 0xFFFFFFFFFFFFFFFFull) {
            D3D12_RESOURCE_STATES target;
            if (shaderBinding->type == rhi::BindingType::StorageBuffer) {
                target = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                target = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            } else {
                target = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
            }
            RequireBufferState(list, entry.buffer, target);
        } else if (entry.textureView.id != 0xFFFFFFFFFFFFFFFFull) {
            const rhi::TextureHandle& parent = entry.textureView.texture;
            RequireTextureState(
                list, parent,
                shaderBinding->type == rhi::BindingType::StorageTexture
                    ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS
                    : D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        }
    }

    ID3D12DescriptorHeap* heaps[] = { m_CbvSrvUavHeap.Get(), m_SamplerHeap.Get() };
    list->SetDescriptorHeaps(2, heaps);

    if (group.cbvSrvUavCount > 0) {
        const UINT param = layout.resourceParam[slot];
        if (param == UINT_MAX) {
            printf("[ERROR] SetBindGroup: no resource table for slot\n");
            return;
        }
        if (state.computeBindPoint) {
            list->SetComputeRootDescriptorTable(param, group.cbvSrvUavTable);
        } else {
            list->SetGraphicsRootDescriptorTable(param, group.cbvSrvUavTable);
        }
    }
    if (group.samplerCount > 0) {
        const UINT param = layout.samplerParam[slot];
        if (param == UINT_MAX) {
            printf("[ERROR] SetBindGroup: no sampler table for slot\n");
            return;
        }
        if (state.computeBindPoint) {
            list->SetComputeRootDescriptorTable(param, group.samplerTable);
        } else {
            list->SetGraphicsRootDescriptorTable(param, group.samplerTable);
        }
    }
}

void DX12RHI::SetVertexBuffer(rhi::CommandListHandle cmd, std::uint32_t slot, rhi::BufferHandle handle, std::size_t offset)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    if (handle.id >= m_Buffers.size() || !m_Buffers[handle.id]) {
        printf("[ERROR] SetVertexBuffer: invalid buffer handle\n");
        return;
    }
    if (offset > handle.desc.size) {
        printf("[ERROR] SetVertexBuffer offset is out of range\n");
        return;
    }
    const DX12CommandList& state = m_CommandLists[cmd.id];
    if (state.boundGraphicsPipeline.id == 0xFFFFFFFFFFFFFFFFull ||
        slot >= state.boundGraphicsPipeline.desc.vertexBuffers.size()) {
        printf("[ERROR] SetVertexBuffer: no compatible pipeline bound yet\n");
        return;
    }
    RequireBufferState(list, handle, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    D3D12_VERTEX_BUFFER_VIEW view{};
    view.BufferLocation = m_Buffers[handle.id]->GetGPUVirtualAddress() +
        static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(offset);
    view.SizeInBytes = static_cast<UINT>(handle.desc.size - offset);
    view.StrideInBytes = state.boundGraphicsPipeline.desc.vertexBuffers[slot].stride;
    list->IASetVertexBuffers(slot, 1, &view);
}

void DX12RHI::SetIndexBuffer(rhi::CommandListHandle cmd, rhi::BufferHandle handle, rhi::IndexFormat format, std::size_t offset)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    if (handle.id >= m_Buffers.size() || !m_Buffers[handle.id]) {
        printf("[ERROR] SetIndexBuffer: invalid buffer handle\n");
        return;
    }
    if (offset > handle.desc.size) {
        printf("[ERROR] SetIndexBuffer offset is out of range\n");
        return;
    }
    RequireBufferState(list, handle, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    D3D12_INDEX_BUFFER_VIEW view{};
    view.BufferLocation = m_Buffers[handle.id]->GetGPUVirtualAddress() +
        static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(offset);
    view.SizeInBytes = static_cast<UINT>(handle.desc.size - offset);
    view.Format = (format == rhi::IndexFormat::Uint16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    list->IASetIndexBuffer(&view);
}

void DX12RHI::SetViewport(rhi::CommandListHandle cmd, float x, float y, float width, float height)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = x;
    viewport.TopLeftY = y;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    list->RSSetViewports(1, &viewport);
}

void DX12RHI::SetScissor(rhi::CommandListHandle cmd, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    D3D12_RECT scissor{};
    scissor.left = static_cast<LONG>(x);
    scissor.top = static_cast<LONG>(y);
    scissor.right = static_cast<LONG>(x + width);
    scissor.bottom = static_cast<LONG>(y + height);
    list->RSSetScissorRects(1, &scissor);
}

void DX12RHI::Draw(rhi::CommandListHandle cmd, std::uint32_t vertexCount, std::uint32_t instanceCount,
                    std::uint32_t firstVertex, std::uint32_t firstInstance)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    list->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void DX12RHI::DrawIndexed(rhi::CommandListHandle cmd, std::uint32_t indexCount, std::uint32_t instanceCount,
                           std::uint32_t firstIndex, std::int32_t baseVertex,
                           std::uint32_t firstInstance)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    list->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

ID3D12CommandSignature* DX12RHI::GetOrCreateSignature(D3D12_INDIRECT_ARGUMENT_TYPE type, UINT stride)
{
    UINT minStride = 0;
    switch (type) {
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:         minStride = sizeof(D3D12_DRAW_ARGUMENTS); break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED: minStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS); break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:     minStride = sizeof(D3D12_DISPATCH_ARGUMENTS); break;
    default:
        printf("[ERROR] GetOrCreateSignature: unsupported argument type\n");
        return nullptr;
    }
    if (stride < minStride) {
        printf("[ERROR] GetOrCreateSignature: stride %u is smaller than the argument size %u\n",
               stride, minStride);
        return nullptr;
    }

    const CommandSignatureKey key{ type, stride };
    auto it = m_CommandSignatures.find(key);
    if (it != m_CommandSignatures.end()) {
        return it->second.Get();
    }

    D3D12_INDIRECT_ARGUMENT_DESC arg{};
    arg.Type = type;
    D3D12_COMMAND_SIGNATURE_DESC sig{};
    sig.NumArgumentDescs = 1;
    sig.pArgumentDescs = &arg;
    sig.ByteStride = stride;

    Microsoft::WRL::ComPtr<ID3D12CommandSignature> signature;
    if (FAILED(m_Device->CreateCommandSignature(&sig, nullptr, IID_PPV_ARGS(&signature)))) {
        printf("[ERROR] GetOrCreateSignature: failed to bake command signature\n");
        return nullptr;
    }
    ID3D12CommandSignature* result = signature.Get();
    m_CommandSignatures.emplace(key, std::move(signature));
    return result;
}

void DX12RHI::DrawIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset,
                            std::uint32_t drawCount, std::uint32_t stride)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    if (params.id >= m_Buffers.size() || !m_Buffers[params.id]) {
        printf("[ERROR] DrawIndirect: invalid params buffer handle\n");
        return;
    }
    if (params.desc.memoryUsage != rhi::MemoryUsage::GpuOnly) {
        printf("[ERROR] DrawIndirect: params buffer must be GPU-only\n");
        return;
    }
    ID3D12CommandSignature* signature =
        GetOrCreateSignature(D3D12_INDIRECT_ARGUMENT_TYPE_DRAW, static_cast<UINT>(stride));
    if (signature == nullptr) {
        return;
    }
    RequireBufferState(list, params, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    list->ExecuteIndirect(signature, drawCount, m_Buffers[params.id].Get(),
                           static_cast<UINT64>(offset), nullptr, 0);
}

void DX12RHI::DrawIndexedIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset,
                                   std::uint32_t drawCount, std::uint32_t stride)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    if (params.id >= m_Buffers.size() || !m_Buffers[params.id]) {
        printf("[ERROR] DrawIndexedIndirect: invalid params buffer handle\n");
        return;
    }
    if (params.desc.memoryUsage != rhi::MemoryUsage::GpuOnly) {
        printf("[ERROR] DrawIndexedIndirect: params buffer must be GPU-only\n");
        return;
    }
    ID3D12CommandSignature* signature =
        GetOrCreateSignature(D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED, static_cast<UINT>(stride));
    if (signature == nullptr) {
        return;
    }
    RequireBufferState(list, params, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    list->ExecuteIndirect(signature, drawCount, m_Buffers[params.id].Get(),
                           static_cast<UINT64>(offset), nullptr, 0);
}

void DX12RHI::Dispatch(rhi::CommandListHandle cmd, std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    list->Dispatch(x, y, z);
}

void DX12RHI::DispatchIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset)
{
    ID3D12GraphicsCommandList* list = GetCommandList(cmd);
    if (list == nullptr) {
        return;
    }
    if (params.id >= m_Buffers.size() || !m_Buffers[params.id]) {
        printf("[ERROR] DispatchIndirect: invalid params buffer handle\n");
        return;
    }
    if (params.desc.memoryUsage != rhi::MemoryUsage::GpuOnly) {
        printf("[ERROR] DispatchIndirect: params buffer must be GPU-only\n");
        return;
    }
    ID3D12CommandSignature* signature =
        GetOrCreateSignature(D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH, sizeof(D3D12_DISPATCH_ARGUMENTS));
    if (signature == nullptr) {
        return;
    }
    RequireBufferState(list, params, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    list->ExecuteIndirect(signature, 1, m_Buffers[params.id].Get(),
                           static_cast<UINT64>(offset), nullptr, 0);
}

bool DX12RHI::EnsureUploadReady()
{
    if (!m_Device || !m_UploadList || !m_UploadAllocator) {
        return false;
    }
    if (m_UploadFenceValue != 0) {
        if (m_IdleFence && m_IdleEvent &&
            m_IdleFence->GetCompletedValue() < m_UploadFenceValue) {
            m_IdleFence->SetEventOnCompletion(m_UploadFenceValue, m_IdleEvent);
            WaitForSingleObject(m_IdleEvent, INFINITE);
        }
        m_UploadFenceValue = 0;
        m_StagingHead = 0;
        m_UploadAllocator->Reset();
        m_UploadList->Reset(m_UploadAllocator.Get(), nullptr);
    }
    return true;
}

void DX12RHI::FlushUploads(bool wait)
{
    if (!m_UploadHasWork) {
        return;
    }
    m_UploadHasWork = false;
    if (FAILED(m_UploadList->Close())) {
        return;
    }
    ID3D12CommandList* lists[] = { m_UploadList.Get() };
    m_CommandQueue->ExecuteCommandLists(1, lists);
    m_CommandQueue->Signal(m_IdleFence.Get(), ++m_IdleValue);
    m_UploadFenceValue = m_IdleValue;
    if (wait && m_IdleFence->GetCompletedValue() < m_UploadFenceValue) {
        m_IdleFence->SetEventOnCompletion(m_UploadFenceValue, m_IdleEvent);
        WaitForSingleObject(m_IdleEvent, INFINITE);
    }
}

void DX12RHI::FlushTransfers(bool wait)
{
    FlushUploads(wait);
}

static std::size_t AlignUp(std::size_t value, std::size_t align)
{
    return (value + align - 1) & ~(align - 1);
}

bool DX12RHI::AllocStaging(std::size_t size, std::size_t align, std::size_t& outOffset)
{
    if (m_StagingMapped == nullptr || size == 0 || size > kUploadRingSize || align == 0) {
        printf("[ERROR] AllocStaging: bad request (size %zu, ring %zu)\n", size, kUploadRingSize);
        return false;
    }
    std::size_t head = AlignUp(m_StagingHead, align);
    if (head + size > kUploadRingSize) {
        FlushUploads(false);
        if (m_UploadFenceValue != 0) {
            if (m_IdleFence &&
                m_IdleFence->GetCompletedValue() < m_UploadFenceValue) {
                m_IdleFence->SetEventOnCompletion(m_UploadFenceValue, m_IdleEvent);
                WaitForSingleObject(m_IdleEvent, INFINITE);
            }
            m_UploadFenceValue = 0;
        }
        m_UploadAllocator->Reset();
        m_UploadList->Reset(m_UploadAllocator.Get(), nullptr);
        head = 0;
    } else if (m_UploadFenceValue != 0 && m_IdleFence &&
               m_IdleFence->GetCompletedValue() >= m_UploadFenceValue) {
        m_UploadFenceValue = 0;
        head = 0;
    }
    outOffset = head;
    m_StagingHead = head + size;
    return true;
}

void DX12RHI::UploadBuffer(rhi::BufferHandle dst, std::size_t dstOffset,
                           const void* data, std::size_t size)
{
    if (!EnsureUploadReady() || data == nullptr) {
        return;
    }
    if (dst.id >= m_Buffers.size() || !m_Buffers[dst.id]) {
        printf("[ERROR] UploadBuffer: invalid buffer handle\n");
        return;
    }
    if (size == 0 || dstOffset > dst.desc.size || size > dst.desc.size - dstOffset) {
        printf("[ERROR] UploadBuffer range is out of bounds\n");
        return;
    }
    std::size_t stagingOffset = 0;
    if (!AllocStaging(size, 16, stagingOffset)) {
        return;
    }
    std::memcpy(static_cast<std::uint8_t*>(m_StagingMapped) + stagingOffset, data, size);
    ID3D12GraphicsCommandList* list = m_UploadList.Get();
    RequireBufferState(list, dst, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyBufferRegion(m_Buffers[dst.id].Get(), static_cast<UINT64>(dstOffset),
                           m_StagingBuffer.Get(), static_cast<UINT64>(stagingOffset),
                           static_cast<UINT64>(size));
    m_UploadHasWork = true;
}

void DX12RHI::UploadTexture(rhi::TextureHandle dst, std::uint32_t mipLevel,
                            const void* data, std::uint32_t width, std::uint32_t height)
{
    if (!EnsureUploadReady() || data == nullptr) {
        return;
    }
    if (dst.id >= m_Textures.size() || !m_Textures[dst.id]) {
        printf("[ERROR] UploadTexture: invalid texture handle\n");
        return;
    }
    D3D12_RESOURCE_DESC texDesc = m_Textures[dst.id]->GetDesc();
    const std::uint64_t mipWidth =
        (texDesc.Width >> mipLevel) > 1u ? (texDesc.Width >> mipLevel) : 1u;
    const std::uint32_t mipHeight =
        (texDesc.Height >> mipLevel) > 1u ? (texDesc.Height >> mipLevel) : 1u;
    if (static_cast<std::uint64_t>(width) != mipWidth || height != mipHeight) {
        printf("[ERROR] UploadTexture: extent %ux%u does not match mip %u (%llux%u)\n",
            width, height, mipLevel, mipWidth, mipHeight);
        return;
    }
    const std::uint32_t texelBytes = TexelBytes(dst.desc.format);
    if (texelBytes == 0) {
        printf("[ERROR] UploadTexture: unsupported texture format\n");
        return;
    }
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    m_Device->GetCopyableFootprints(&texDesc, mipLevel, 1, 0, &footprint,
                                    nullptr, nullptr, nullptr);
    const std::size_t pitch = footprint.Footprint.RowPitch;
    const std::size_t tightRow = static_cast<std::size_t>(width) * texelBytes;
    std::size_t stagingOffset = 0;
    if (!AllocStaging(pitch * height, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, stagingOffset)) {
        return;
    }
    for (std::uint32_t y = 0; y < height; ++y) {
        std::memcpy(static_cast<std::uint8_t*>(m_StagingMapped) + stagingOffset +
                        static_cast<std::size_t>(y) * pitch,
                    static_cast<const std::uint8_t*>(data) +
                        static_cast<std::size_t>(y) * tightRow,
                    tightRow);
    }
    ID3D12GraphicsCommandList* list = m_UploadList.Get();
    RequireTextureState(list, dst, D3D12_RESOURCE_STATE_COPY_DEST);

    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource = m_StagingBuffer.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint = footprint;
    srcLoc.PlacedFootprint.Offset = static_cast<UINT64>(stagingOffset);
    srcLoc.PlacedFootprint.Footprint.RowPitch =
        static_cast<UINT>(pitch);

    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource = m_Textures[dst.id].Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = CalcSubresource(
        mipLevel, 0, 0, texDesc.MipLevels,
        static_cast<UINT>(texDesc.DepthOrArraySize));

    list->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
    m_UploadHasWork = true;
}

void DX12RHI::CopyBufferToBuffer(rhi::BufferHandle src, rhi::BufferHandle dst, std::size_t size,
                                  std::size_t srcOffset, std::size_t dstOffset)
{
    if (!EnsureUploadReady()) {
        return;
    }
    ID3D12GraphicsCommandList* list = m_UploadList.Get();
    if (src.id >= m_Buffers.size() || !m_Buffers[src.id] ||
        dst.id >= m_Buffers.size() || !m_Buffers[dst.id]) {
        printf("[ERROR] CopyBufferToBuffer: invalid buffer handle\n");
        return;
    }
    if (size == 0 || srcOffset > src.desc.size || size > src.desc.size - srcOffset ||
        dstOffset > dst.desc.size || size > dst.desc.size - dstOffset) {
        printf("[ERROR] CopyBufferToBuffer range is out of bounds\n");
        return;
    }
    RequireBufferState(list, src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    RequireBufferState(list, dst, D3D12_RESOURCE_STATE_COPY_DEST);
    list->CopyBufferRegion(m_Buffers[dst.id].Get(), static_cast<UINT64>(dstOffset),
                           m_Buffers[src.id].Get(), static_cast<UINT64>(srcOffset),
                           static_cast<UINT64>(size));
    m_UploadHasWork = true;
}

void DX12RHI::CopyBufferToTexture(rhi::BufferHandle src, rhi::TextureHandle dst,
                                   const rhi::TextureWriteDesc& region)
{
    if (!EnsureUploadReady()) {
        return;
    }
    ID3D12GraphicsCommandList* list = m_UploadList.Get();
    if (src.id >= m_Buffers.size() || !m_Buffers[src.id] ||
        dst.id >= m_Textures.size() || !m_Textures[dst.id]) {
        printf("[ERROR] CopyBufferToTexture: invalid copy handle\n");
        return;
    }
    if (region.bytesPerRow % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT != 0) {
        printf("[ERROR] CopyBufferToTexture: bytesPerRow must be 256-byte aligned\n");
        return;
    }
    RequireBufferState(list, src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    RequireTextureState(list, dst, D3D12_RESOURCE_STATE_COPY_DEST);

    D3D12_RESOURCE_DESC texDesc = m_Textures[dst.id]->GetDesc();
    const std::uint32_t rowsPerImage =
        region.rowsPerImage != 0 ? region.rowsPerImage : region.height;
    const bool is3D = dst.desc.dimension == rhi::TextureDimension::Tex3D;
    const std::uint32_t layerCount = is3D ? 1 : region.arrayLayers;
    const UINT arraySize = is3D ? 1 : static_cast<UINT>(texDesc.DepthOrArraySize);

    for (std::uint32_t layer = 0; layer < layerCount; ++layer) {
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        m_Device->GetCopyableFootprints(&texDesc, region.mipLevel, 1, 0, &footprint,
                                        nullptr, nullptr, nullptr);
        footprint.Offset = static_cast<UINT64>(layer) *
            static_cast<UINT64>(region.bytesPerRow) *
            static_cast<UINT64>(rowsPerImage);
        footprint.Footprint.RowPitch = region.bytesPerRow;

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = m_Buffers[src.id].Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint = footprint;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = m_Textures[dst.id].Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = CalcSubresource(
            region.mipLevel, region.baseArrayLayer + layer, 0,
            texDesc.MipLevels, arraySize);

        list->CopyTextureRegion(&dstLoc, region.x, region.y, region.z, &srcLoc, nullptr);
    }
    m_UploadHasWork = true;
}

void DX12RHI::CopyTextureToBuffer(rhi::TextureHandle src, rhi::BufferHandle dst,
                                   const rhi::TextureWriteDesc& region)
{
    if (!EnsureUploadReady()) {
        return;
    }
    ID3D12GraphicsCommandList* list = m_UploadList.Get();
    if (src.id >= m_Textures.size() || !m_Textures[src.id] ||
        dst.id >= m_Buffers.size() || !m_Buffers[dst.id]) {
        printf("[ERROR] CopyTextureToBuffer: invalid copy handle\n");
        return;
    }
    if (region.bytesPerRow % D3D12_TEXTURE_DATA_PITCH_ALIGNMENT != 0) {
        printf("[ERROR] CopyTextureToBuffer: bytesPerRow must be 256-byte aligned\n");
        return;
    }
    RequireTextureState(list, src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    RequireBufferState(list, dst, D3D12_RESOURCE_STATE_COPY_DEST);

    D3D12_RESOURCE_DESC texDesc = m_Textures[src.id]->GetDesc();
    const std::uint32_t rowsPerImage =
        region.rowsPerImage != 0 ? region.rowsPerImage : region.height;
    const bool is3D = src.desc.dimension == rhi::TextureDimension::Tex3D;
    const std::uint32_t layerCount = is3D ? 1 : region.arrayLayers;
    const UINT arraySize = is3D ? 1 : static_cast<UINT>(texDesc.DepthOrArraySize);

    for (std::uint32_t layer = 0; layer < layerCount; ++layer) {
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        m_Device->GetCopyableFootprints(&texDesc, region.mipLevel, 1, 0, &footprint,
                                        nullptr, nullptr, nullptr);
        footprint.Offset = static_cast<UINT64>(layer) *
            static_cast<UINT64>(region.bytesPerRow) *
            static_cast<UINT64>(rowsPerImage);
        footprint.Footprint.RowPitch = region.bytesPerRow;

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = m_Textures[src.id].Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = CalcSubresource(
            region.mipLevel, region.baseArrayLayer + layer, 0,
            texDesc.MipLevels, arraySize);

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = m_Buffers[dst.id].Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstLoc.PlacedFootprint = footprint;

        D3D12_BOX srcBox{};
        srcBox.left = region.x;
        srcBox.top = region.y;
        srcBox.front = region.z;
        srcBox.right = region.x + region.width;
        srcBox.bottom = region.y + region.height;
        srcBox.back = region.z + region.depth;
        list->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);
    }
    m_UploadHasWork = true;
}

void DX12RHI::CopyTextureToTexture(rhi::TextureHandle src, rhi::TextureHandle dst,
                                    const rhi::TextureCopyRegion& region)
{
    if (!EnsureUploadReady()) {
        return;
    }
    ID3D12GraphicsCommandList* list = m_UploadList.Get();
    if (src.id >= m_Textures.size() || !m_Textures[src.id] ||
        dst.id >= m_Textures.size() || !m_Textures[dst.id]) {
        printf("[ERROR] CopyTextureToTexture: invalid texture handle\n");
        return;
    }
    if (IsDepthFormat(src.desc.format) != IsDepthFormat(dst.desc.format)) {
        printf("[ERROR] CopyTextureToTexture: cannot copy between depth and color formats\n");
        return;
    }
    RequireTextureState(list, src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    RequireTextureState(list, dst, D3D12_RESOURCE_STATE_COPY_DEST);

    D3D12_RESOURCE_DESC srcDesc = m_Textures[src.id]->GetDesc();
    D3D12_RESOURCE_DESC dstDesc = m_Textures[dst.id]->GetDesc();
    const bool is3D = src.desc.dimension == rhi::TextureDimension::Tex3D;
    const std::uint32_t layerCount = is3D ? 1 : region.arrayLayers;

    for (std::uint32_t layer = 0; layer < layerCount; ++layer) {
        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = m_Textures[src.id].Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = CalcSubresource(
            region.srcMipLevel, region.srcBaseArrayLayer + layer, 0,
            srcDesc.MipLevels, is3D ? 1 : static_cast<UINT>(srcDesc.DepthOrArraySize));

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = m_Textures[dst.id].Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = CalcSubresource(
            region.dstMipLevel, region.dstBaseArrayLayer + layer, 0,
            dstDesc.MipLevels, is3D ? 1 : static_cast<UINT>(dstDesc.DepthOrArraySize));

        D3D12_BOX srcBox{};
        srcBox.left = region.srcX;
        srcBox.top = region.srcY;
        srcBox.front = region.srcZ;
        srcBox.right = region.srcX + region.width;
        srcBox.bottom = region.srcY + region.height;
        srcBox.back = region.srcZ + region.depth;
        list->CopyTextureRegion(&dstLoc, region.dstX, region.dstY, region.dstZ,
                                &srcLoc, &srcBox);
    }
    m_UploadHasWork = true;
}

void DX12RHI::Submit(std::span<const rhi::CommandListHandle> cmds)
{
    if (!m_Device || !m_CommandQueue || cmds.empty() || m_CurrentFrame == 0xFFFFFFFF) {
        return;
    }
    // Uploads recorded since the last flush ride ahead of frame work on the
    // same queue, so copies are visible to the passes below.
    FlushUploads(false);
    FrameContext& frame = m_Frames[m_CurrentFrame];
    if (!frame.allocator || !frame.fence) {
        return;
    }

    std::vector<ID3D12CommandList*> lists;
    lists.reserve(cmds.size());
    for (rhi::CommandListHandle handle : cmds) {
        if (handle.id >= m_CommandLists.size()) {
            return;
        }
        DX12CommandList& state = m_CommandLists[handle.id];
        if (!state.open || !state.list || state.frame != m_CurrentFrame) {
            return;
        }
        if (state.pass != CurrentPass::None) {
            printf("[ERROR] Submit: command list still has an open pass, call EndPass first\n");
            return;
        }
        if (FAILED(state.list->Close())) {
            return;
        }
        lists.push_back(state.list.Get());
    }

    // All lists are DIRECT type, so everything goes to the direct queue.
    // m_ComputeQueue is reserved for future COMPUTE-type lists.
    m_CommandQueue->ExecuteCommandLists(
        static_cast<UINT>(lists.size()), lists.data());
    m_CommandQueue->Signal(frame.fence.Get(), ++frame.fenceValue);
    frame.submitted = true;

    for (rhi::CommandListHandle handle : cmds) {
        m_CommandLists[handle.id].open = false;
        m_FreeCommandLists.push_back(handle.id);
    }
}
