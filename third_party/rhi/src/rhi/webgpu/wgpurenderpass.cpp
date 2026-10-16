#include "rhi/webgpu/wgpurhi.h"

#include <algorithm>
#include <cstring>
#include <vector>

WebGPURHI::WebGPUCommandList* WebGPURHI::GetCommandList(rhi::CommandListHandle handle)
{
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_CommandLists.size()) {
        return nullptr;
    }
    WebGPUCommandList& list = m_CommandLists[handle.id];
    if (!list.encoder) {
        return nullptr;
    }
    return &list;
}

rhi::CommandListHandle WebGPURHI::BeginCommandList()
{
    rhi::CommandListHandle handle;
    WebGPUCommandList list;
    list.encoder = m_Device.CreateCommandEncoder();
    if (!list.encoder) {
        return handle;
    }
    if (!m_FreeCommandLists.empty()) {
        handle.id = m_FreeCommandLists.back();
        m_FreeCommandLists.pop_back();
        m_CommandLists[handle.id] = std::move(list);
    } else {
        handle.id = static_cast<std::uint64_t>(m_CommandLists.size());
        m_CommandLists.push_back(std::move(list));
    }
    return handle;
}

void WebGPURHI::BeginRenderPass(rhi::CommandListHandle cmd, const rhi::RenderPassDesc& desc)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list) {
        printf("BeginRenderPass invalid command list\n");
        return;
    }
    if (list->pass != CurrentPass::None) {
        printf("Another pass is currently running. End it before beginning a new pass.");
        return;
    }

    wgpu::TextureView swapchainView;
    for (const auto& attachment : desc.colors) {
        if (!attachment.useSwapchain) {
            continue;
        }
        if (desc.swapchain.id == 0xFFFFFFFFFFFFFFFF ||
            desc.swapchain.id >= m_Swapchains.size()) {
            printf("BeginRenderPass swapchain handle is invalid\n");
            return;
        }
        WebGPUSwapchainData& swapchain = m_Swapchains[desc.swapchain.id];
        if (!swapchain.surface || !swapchain.currentView) {
            printf("BeginRenderPass needs AcquireNextImage first\n");
            return;
        }
        swapchainView = swapchain.currentView;
        break;
    }

    std::vector<wgpu::RenderPassColorAttachment> colors;
    colors.reserve(desc.colors.size());
    for (const auto& attachment : desc.colors) {
        wgpu::TextureView view;
        if (attachment.useSwapchain) {
            view = swapchainView;
        } else {
            if (attachment.view.id == 0xFFFFFFFFFFFFFFFF ||
                attachment.view.id >= m_TextureViews.size() ||
                !m_TextureViews[attachment.view.id]) {
                printf("BeginRenderPass color view handle is invalid\n");
                return;
            }
            view = m_TextureViews[attachment.view.id];
        }

        wgpu::RenderPassColorAttachment color{};
        color.view = view;
        color.loadOp = attachment.clear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        color.storeOp = wgpu::StoreOp::Store;
        color.clearValue = {
            attachment.clearColor[0],
            attachment.clearColor[1],
            attachment.clearColor[2],
            attachment.clearColor[3],
        };
        colors.push_back(color);
    }
    if (colors.empty() && !desc.depth.enabled) {
        printf("BeginRenderPass needs at least one color attachment or a depth attachment\n");
        return;
    }

    wgpu::RenderPassDepthStencilAttachment depthAttachment{};
    bool hasDepth = false;
    if (desc.depth.enabled) {
        if (desc.depth.view.id == 0xFFFFFFFFFFFFFFFF ||
            desc.depth.view.id >= m_TextureViews.size() ||
            !m_TextureViews[desc.depth.view.id]) {
            printf("BeginRenderPass depth view handle is invalid\n");
            return;
        }
        depthAttachment.view = m_TextureViews[desc.depth.view.id];
        depthAttachment.depthLoadOp =
            desc.depth.clearDepth ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        depthAttachment.depthStoreOp = wgpu::StoreOp::Store;
        depthAttachment.depthClearValue = desc.depth.clearDepthValue;
        if (HasStencilAspect(desc.depth.view.desc.format)) {
            depthAttachment.stencilLoadOp =
                desc.depth.clearStencil ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
            depthAttachment.stencilStoreOp = wgpu::StoreOp::Store;
            depthAttachment.stencilClearValue = desc.depth.clearStencilValue;
        } else {
            depthAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
            depthAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
        }
        hasDepth = true;
    }

    wgpu::RenderPassDescriptor pass{};
    pass.colorAttachmentCount = colors.size();
    pass.colorAttachments = colors.empty() ? nullptr : colors.data();
    if (hasDepth) {
        pass.depthStencilAttachment = &depthAttachment;
    }
    list->renderPass = list->encoder.BeginRenderPass(&pass);
    list->pass = CurrentPass::Graphics;
}

void WebGPURHI::BeginComputePass(rhi::CommandListHandle cmd)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list) {
        printf("BeginComputePass invalid command list\n");
        return;
    }
    if (list->pass != CurrentPass::None) {
        printf("Another pass is currently running. End it before beginning a new pass.");
        return;
    }
    wgpu::ComputePassDescriptor pass{};
    list->computePass = list->encoder.BeginComputePass(&pass);
    list->pass = CurrentPass::Compute;
}

void WebGPURHI::EndPass(rhi::CommandListHandle cmd)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list) {
        return;
    }
    if (list->computePass) {
        list->computePass.End();
        list->computePass = nullptr;
        list->pass = CurrentPass::None;
    }
    if (list->renderPass) {
        list->renderPass.End();
        list->renderPass = nullptr;
        list->pass = CurrentPass::None;
    }
}

void WebGPURHI::SetPipeline(rhi::CommandListHandle cmd, rhi::GraphicsPipelineHandle handle)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Graphics) {
        printf("Currently not in graphics pass, cannot set graphics pipeline\n");
        return;
    }
    if (handle.id >= m_Pipelines.size() || !m_Pipelines[handle.id]) {
        printf("Invalid pipeline handle\n");
        return;
    }
    if (!list->renderPass) {
        printf("No render pass encoder\n");
        return;
    }
    list->renderPass.SetPipeline(m_Pipelines[handle.id]);
}

void WebGPURHI::SetPipeline(rhi::CommandListHandle cmd, rhi::ComputePipelineHandle handle)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Compute) {
        printf("Currently not in compute pass, cannot set compute pipeline\n");
        return;
    }
    if (handle.id >= m_ComputePipelines.size() || !m_ComputePipelines[handle.id]) {
        printf("Invalid pipeline handle\n");
        return;
    }
    if (!list->computePass) {
        printf("No compute pass encoder\n");
        return;
    }
    list->computePass.SetPipeline(m_ComputePipelines[handle.id]);
}

void WebGPURHI::SetBindGroup(rhi::CommandListHandle cmd, std::uint32_t slot, rhi::BindGroupHandle handle)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list) {
        return;
    }
    if (handle.id >= m_BindGroups.size() || !m_BindGroups[handle.id]) {
        printf("Invalid bind group handle\n");
        return;
    }
    if (list->pass == CurrentPass::None) {
        printf("No current pass\n");
        return;
    }
    if (list->pass == CurrentPass::Compute) {
        if (list->computePass) {
            list->computePass.SetBindGroup(slot, m_BindGroups[handle.id]);
        }
    } else {
        if (list->renderPass) {
            list->renderPass.SetBindGroup(slot, m_BindGroups[handle.id]);
        }
    }
}

void WebGPURHI::SetVertexBuffer(rhi::CommandListHandle cmd, std::uint32_t slot, rhi::BufferHandle handle, std::size_t offset)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Graphics) {
        printf("Currently not in graphics pass, cannot set vertex buffer\n");
        return;
    }
    if (handle.id >= m_Buffers.size() || !m_Buffers[handle.id]) {
        printf("Invalid buffer handle\n");
        return;
    }
    if (!list->renderPass) {
        printf("No render pass encoder\n");
        return;
    }
    if (offset > handle.desc.size) {
        printf("SetVertexBuffer offset is out of range\n");
        return;
    }
    list->renderPass.SetVertexBuffer(slot, m_Buffers[handle.id], offset);
}

void WebGPURHI::SetIndexBuffer(rhi::CommandListHandle cmd, rhi::BufferHandle handle, rhi::IndexFormat format, std::size_t offset)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Graphics) {
        printf("Currently not in graphics pass, cannot set index buffer\n");
        return;
    }
    if (handle.id >= m_Buffers.size() || !m_Buffers[handle.id]) {
        printf("Invalid buffer handle\n");
        return;
    }
    if (!list->renderPass) {
        printf("No render pass encoder\n");
        return;
    }
    if (offset > handle.desc.size) {
        printf("SetIndexBuffer offset is out of range\n");
        return;
    }
    list->renderPass.SetIndexBuffer(m_Buffers[handle.id], toWebGPU(format), offset);
}

void WebGPURHI::SetViewport(rhi::CommandListHandle cmd, float x, float y, float width, float height)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Graphics) {
        printf("Currently not in graphics pass, cannot set viewport\n");
        return;
    }
    if (!list->renderPass) {
        printf("No render pass encoder\n");
        return;
    }
    list->renderPass.SetViewport(x, y, width, height, 0.0f, 1.0f);
}

void WebGPURHI::SetScissor(rhi::CommandListHandle cmd, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Graphics) {
        printf("Currently not in graphics pass, cannot set scissor\n");
        return;
    }
    if (!list->renderPass) {
        printf("No render pass encoder\n");
        return;
    }
    list->renderPass.SetScissorRect(x, y, width, height);
}

void WebGPURHI::Draw(rhi::CommandListHandle cmd, std::uint32_t vertexCount, std::uint32_t instanceCount,
                      std::uint32_t firstVertex, std::uint32_t firstInstance)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Graphics) {
        printf("Currently not in graphics pass, cannot draw\n");
        return;
    }
    if (!list->renderPass) {
        printf("No render pass encoder\n");
        return;
    }
    list->renderPass.Draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void WebGPURHI::DrawIndexed(rhi::CommandListHandle cmd, std::uint32_t indexCount, std::uint32_t instanceCount,
                             std::uint32_t firstIndex, std::int32_t baseVertex,
                             std::uint32_t firstInstance)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Graphics) {
        printf("Currently not in graphics pass, cannot draw\n");
        return;
    }
    if (!list->renderPass) {
        printf("No render pass encoder\n");
        return;
    }
    list->renderPass.DrawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void WebGPURHI::DrawIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset,
                              std::uint32_t drawCount, std::uint32_t stride)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Graphics) {
        printf("Currently not in graphics pass, cannot draw\n");
        return;
    }
    if (params.id >= m_Buffers.size() || !m_Buffers[params.id]) {
        printf("Invalid params buffer handle\n");
        return;
    }
    if (offset > params.desc.size) {
        printf("DrawIndirect offset is out of range\n");
        return;
    }
    if (offset % 4 != 0) {
        printf("DrawIndirect offset must be 4-byte aligned\n");
        return;
    }
    if (stride < 16) {
        printf("DrawIndirect stride must hold one draw call\n");
        return;
    }
    if (drawCount > 0 && (offset + static_cast<std::size_t>(drawCount - 1) * stride + 16 > params.desc.size)) {
        printf("DrawIndirect range exceeds params buffer\n");
        return;
    }
    if (!list->renderPass) {
        printf("No render pass encoder\n");
        return;
    }
    wgpu::Buffer buffer = m_Buffers[params.id];
    for (std::uint32_t i = 0; i < drawCount; ++i) {
        list->renderPass.DrawIndirect(buffer, offset + i * stride);
    }
}

void WebGPURHI::DrawIndexedIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset,
                                     std::uint32_t drawCount, std::uint32_t stride)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Graphics) {
        printf("Currently not in graphics pass, cannot draw\n");
        return;
    }
    if (params.id >= m_Buffers.size() || !m_Buffers[params.id]) {
        printf("Invalid params buffer handle\n");
        return;
    }
    if (offset > params.desc.size) {
        printf("DrawIndexedIndirect offset is out of range\n");
        return;
    }
    if (offset % 4 != 0) {
        printf("DrawIndexedIndirect offset must be 4-byte aligned\n");
        return;
    }
    if (stride < 20) {
        printf("DrawIndexedIndirect stride must hold one indexed draw call\n");
        return;
    }
    if (drawCount > 0 && (offset + static_cast<std::size_t>(drawCount - 1) * stride + 20 > params.desc.size)) {
        printf("DrawIndexedIndirect range exceeds params buffer\n");
        return;
    }
    if (!list->renderPass) {
        printf("No render pass encoder\n");
        return;
    }
    wgpu::Buffer buffer = m_Buffers[params.id];
    for (std::uint32_t i = 0; i < drawCount; ++i) {
        list->renderPass.DrawIndexedIndirect(buffer, offset + i * stride);
    }
}

void WebGPURHI::Dispatch(rhi::CommandListHandle cmd, std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Compute) {
        printf("Currently not in compute pass, cannot dispatch\n");
        return;
    }
    if (!list->computePass) {
        printf("No compute pass encoder\n");
        return;
    }
    list->computePass.DispatchWorkgroups(x, y, z);
}

void WebGPURHI::DispatchIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset)
{
    WebGPUCommandList* list = GetCommandList(cmd);
    if (!list || list->pass != CurrentPass::Compute) {
        printf("Currently not in compute pass, cannot dispatch\n");
        return;
    }
    if (params.id >= m_Buffers.size() || !m_Buffers[params.id]) {
        printf("Invalid params buffer handle\n");
        return;
    }
    if (offset > params.desc.size) {
        printf("DispatchIndirect offset is out of range\n");
        return;
    }
    if (offset % 4 != 0 || offset + 12 > params.desc.size) {
        printf("DispatchIndirect range exceeds params buffer\n");
        return;
    }
    if (!list->computePass) {
        printf("No compute pass encoder\n");
        return;
    }
    list->computePass.DispatchWorkgroupsIndirect(m_Buffers[params.id], offset);
}

bool WebGPURHI::EnsureUploadReady()
{
    if (!m_Device || !m_Queue) {
        return false;
    }
    if (!m_UploadEncoder) {
        m_UploadEncoder = m_Device.CreateCommandEncoder();
        if (!m_UploadEncoder) {
            return false;
        }
    }
    return true;
}

void WebGPURHI::FlushUploads(bool wait)
{
    if (!m_UploadHasWork || !m_UploadEncoder) {
        return;
    }
    m_UploadHasWork = false;
    wgpu::CommandBuffer commands = m_UploadEncoder.Finish();
    m_UploadEncoder = nullptr;
    m_Queue.Submit(1, &commands);
    m_UploadStaging.clear();
    if (wait) {
        auto future = m_Queue.OnSubmittedWorkDone(
            wgpu::CallbackMode::WaitAnyOnly,
            [&](wgpu::QueueWorkDoneStatus, wgpu::StringView) {});
        m_Instance.WaitAny(future, UINT64_MAX);
    }
}

void WebGPURHI::FlushTransfers(bool wait)
{
    FlushUploads(wait);
}

wgpu::Buffer WebGPURHI::MakeUploadStaging(std::size_t size, void*& mappedOut)
{
    mappedOut = nullptr;
    wgpu::BufferDescriptor stagingDesc{};
    stagingDesc.size = size;
    stagingDesc.usage = wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc;
    wgpu::Buffer staging = m_Device.CreateBuffer(&stagingDesc);
    if (!staging) {
        printf("[RHI] Failed to create WebGPU staging buffer\n");
        return nullptr;
    }
    bool success = false;
    auto future = staging.MapAsync(
        wgpu::MapMode::Write, 0, size,
        wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::MapAsyncStatus status, wgpu::StringView message) {
            if (status != wgpu::MapAsyncStatus::Success) {
                printf("[RHI] Staging map failed: %.*s\n",
                       static_cast<int>(message.length),
                       message.data ? message.data : "");
                return;
            }
            success = true;
        });
    m_Instance.WaitAny(future, UINT64_MAX);
    if (!success) {
        staging.Destroy();
        return nullptr;
    }
    mappedOut = staging.GetMappedRange(0, size);
    if (!mappedOut) {
        printf("[RHI] Failed to get mapped WebGPU staging range\n");
        staging.Unmap();
        staging.Destroy();
        return nullptr;
    }
    return staging;
}

void WebGPURHI::UploadBuffer(rhi::BufferHandle dst, std::size_t dstOffset,
                             const void* data, std::size_t size)
{
    if (!EnsureUploadReady() || data == nullptr) {
        return;
    }
    if (dst.id >= m_Buffers.size() || !m_Buffers[dst.id]) {
        printf("[RHI] UploadBuffer: invalid buffer handle\n");
        return;
    }
    if (size == 0 || dstOffset > dst.desc.size || size > dst.desc.size - dstOffset) {
        printf("[RHI] UploadBuffer range is out of bounds\n");
        return;
    }
    void* mapped = nullptr;
    wgpu::Buffer staging = MakeUploadStaging(size, mapped);
    if (!staging || !mapped) {
        return;
    }
    std::memcpy(mapped, data, size);
    staging.Unmap();
    m_UploadEncoder.CopyBufferToBuffer(staging, 0, m_Buffers[dst.id], dstOffset, size);
    m_UploadStaging.push_back(staging);
    m_UploadHasWork = true;
}

void WebGPURHI::UploadTexture(rhi::TextureHandle dst, std::uint32_t mipLevel,
                              const void* data, std::uint32_t width, std::uint32_t height)
{
    if (!EnsureUploadReady() || data == nullptr) {
        return;
    }
    if (dst.id >= m_Textures.size() || !m_Textures[dst.id]) {
        printf("[RHI] UploadTexture: invalid texture handle\n");
        return;
    }
    if (dst.desc.dimension != rhi::TextureDimension::Tex2D || dst.desc.arrayLayers != 1) {
        printf("[RHI] UploadTexture: only plain 2D textures for now\n");
        return;
    }
    const std::uint32_t mipWidth = (dst.desc.width >> mipLevel) > 1u ? (dst.desc.width >> mipLevel) : 1u;
    const std::uint32_t mipHeight = (dst.desc.height >> mipLevel) > 1u ? (dst.desc.height >> mipLevel) : 1u;
    if (width != mipWidth || height != mipHeight) {
        printf("[RHI] UploadTexture: extent %ux%u does not match mip %u (%ux%u)\n",
               width, height, mipLevel, mipWidth, mipHeight);
        return;
    }
    const std::uint32_t texelBytes = TexelBytes(dst.desc.format);
    if (texelBytes == 0) {
        printf("[RHI] UploadTexture: unsupported texture format\n");
        return;
    }
    const std::size_t tightRow = static_cast<std::size_t>(width) * texelBytes;
    const std::size_t pitch = (tightRow + 255) & ~static_cast<std::size_t>(255);
    void* mapped = nullptr;
    wgpu::Buffer staging = MakeUploadStaging(pitch * height, mapped);
    if (!staging || !mapped) {
        return;
    }
    for (std::uint32_t y = 0; y < height; ++y) {
        std::memcpy(static_cast<std::uint8_t*>(mapped) + static_cast<std::size_t>(y) * pitch,
                    static_cast<const std::uint8_t*>(data) + static_cast<std::size_t>(y) * tightRow,
                    tightRow);
    }
    staging.Unmap();

    wgpu::TexelCopyBufferInfo source{};
    source.buffer = staging;
    source.layout.offset = 0;
    source.layout.bytesPerRow = static_cast<std::uint32_t>(pitch);
    source.layout.rowsPerImage = height;

    wgpu::TexelCopyTextureInfo destination{};
    destination.texture = m_Textures[dst.id];
    destination.mipLevel = mipLevel;
    destination.origin = { 0, 0, 0 };
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::Extent3D extent{ width, height, 1 };
    m_UploadEncoder.CopyBufferToTexture(&source, &destination, &extent);
    m_UploadStaging.push_back(staging);
    m_UploadHasWork = true;
}

void WebGPURHI::CopyBufferToBuffer(rhi::BufferHandle src, rhi::BufferHandle dst, std::size_t size,
                                    std::size_t srcOffset, std::size_t dstOffset)
{
    if (!EnsureUploadReady()) {
        return;
    }
    if (src.id >= m_Buffers.size() || !m_Buffers[src.id] ||
        dst.id >= m_Buffers.size() || !m_Buffers[dst.id]) {
        printf("Invalid buffer handle\n");
        return;
    }
    if (size == 0 || srcOffset > src.desc.size || size > src.desc.size - srcOffset ||
        dstOffset > dst.desc.size || size > dst.desc.size - dstOffset) {
        printf("CopyBufferToBuffer range is out of bounds\n");
        return;
    }
    m_UploadEncoder.CopyBufferToBuffer(
        m_Buffers[src.id], srcOffset, m_Buffers[dst.id], dstOffset, size);
    m_UploadHasWork = true;
}

void WebGPURHI::CopyBufferToTexture(rhi::BufferHandle src, rhi::TextureHandle dst,
                                     const rhi::TextureWriteDesc& region)
{
    if (!EnsureUploadReady()) {
        return;
    }
    if (src.id >= m_Buffers.size() || !m_Buffers[src.id] ||
        dst.id >= m_Textures.size() || !m_Textures[dst.id]) {
        printf("Invalid copy handle\n");
        return;
    }
    if (region.bytesPerRow % 256 != 0) {
        printf("CopyBufferToTexture row pitch must be 256-byte aligned\n");
        return;
    }
    const std::size_t needed =
        static_cast<std::size_t>(region.bytesPerRow) * region.rowsPerImage;
    if (needed > src.desc.size) {
        printf("CopyBufferToTexture source buffer is too small\n");
        return;
    }

    wgpu::TexelCopyBufferInfo source{};
    source.buffer = m_Buffers[src.id];
    source.layout.offset = 0;
    source.layout.bytesPerRow = region.bytesPerRow;
    source.layout.rowsPerImage = region.rowsPerImage;

    wgpu::TexelCopyTextureInfo destination{};
    destination.texture = m_Textures[dst.id];
    destination.mipLevel = region.mipLevel;
    destination.origin = { region.x, region.y, region.z };
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::Extent3D extent{ region.width, region.height, region.depth };
    m_UploadEncoder.CopyBufferToTexture(&source, &destination, &extent);
    m_UploadHasWork = true;
}

void WebGPURHI::CopyTextureToBuffer(rhi::TextureHandle src, rhi::BufferHandle dst,
                                     const rhi::TextureWriteDesc& region)
{
    if (!EnsureUploadReady()) {
        return;
    }
    if (src.id >= m_Textures.size() || !m_Textures[src.id] ||
        dst.id >= m_Buffers.size() || !m_Buffers[dst.id]) {
        printf("Invalid copy handle\n");
        return;
    }
    if (region.bytesPerRow % 256 != 0) {
        printf("CopyTextureToBuffer row pitch must be 256-byte aligned\n");
        return;
    }
    const std::size_t needed =
        static_cast<std::size_t>(region.bytesPerRow) * region.rowsPerImage;
    if (needed > dst.desc.size) {
        printf("CopyTextureToBuffer destination buffer is too small\n");
        return;
    }

    wgpu::TexelCopyTextureInfo source{};
    source.texture = m_Textures[src.id];
    source.mipLevel = region.mipLevel;
    source.origin = { region.x, region.y, region.z };
    source.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferInfo destination{};
    destination.buffer = m_Buffers[dst.id];
    destination.layout.offset = 0;
    destination.layout.bytesPerRow = region.bytesPerRow;
    destination.layout.rowsPerImage = region.rowsPerImage;

    wgpu::Extent3D extent{ region.width, region.height, region.depth };
    m_UploadEncoder.CopyTextureToBuffer(&source, &destination, &extent);
    m_UploadHasWork = true;
}

void WebGPURHI::CopyTextureToTexture(rhi::TextureHandle src, rhi::TextureHandle dst,
                                     const rhi::TextureCopyRegion& region)
{
    if (!EnsureUploadReady()) {
        return;
    }
    if (src.id >= m_Textures.size() || !m_Textures[src.id] ||
        dst.id >= m_Textures.size() || !m_Textures[dst.id]) {
        printf("CopyTextureToTexture: invalid texture handle\n");
        return;
    }

    if (region.arrayLayers == 0 || region.width == 0 || region.height == 0 || region.depth == 0) {
        printf("CopyTextureToTexture: empty copy region\n");
        return;
    }
    wgpu::TexelCopyTextureInfo source{};
    source.texture = m_Textures[src.id];
    source.mipLevel = region.srcMipLevel;
    source.origin = { region.srcX, region.srcY, region.srcZ };
    source.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyTextureInfo destination{};
    destination.texture = m_Textures[dst.id];
    destination.mipLevel = region.dstMipLevel;
    destination.origin = { region.dstX, region.dstY, region.dstZ };
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::Extent3D copySize{ region.width, region.height, region.depth };
    if (src.desc.dimension != rhi::TextureDimension::Tex3D ||
        dst.desc.dimension != rhi::TextureDimension::Tex3D) {
        if (region.srcBaseArrayLayer + region.arrayLayers > src.desc.arrayLayers ||
            region.dstBaseArrayLayer + region.arrayLayers > dst.desc.arrayLayers) {
            printf("CopyTextureToTexture: array layer range out of bounds\n");
            return;
        }
    }
    m_UploadEncoder.CopyTextureToTexture(&source, &destination, &copySize);
    m_UploadHasWork = true;
}

void WebGPURHI::Submit(std::span<const rhi::CommandListHandle> cmds)
{
    // Uploads recorded since the last flush ride ahead of this submit.
    FlushUploads(false);
    for (rhi::CommandListHandle handle : cmds) {
        WebGPUCommandList* list = GetCommandList(handle);
        if (!list) {
            continue;
        }
        if (list->pass != CurrentPass::None) {
            printf("Submit needs EndPass first\n");
            return;
        }
    }
    std::vector<wgpu::CommandBuffer> buffers;
    buffers.reserve(cmds.size());
    std::vector<std::uint64_t> done;
    done.reserve(cmds.size());
    for (rhi::CommandListHandle handle : cmds) {
        WebGPUCommandList* list = GetCommandList(handle);
        if (!list) {
            continue;
        }
        buffers.push_back(list->encoder.Finish());
        list->encoder = nullptr;
        list->submitted = true;
        done.push_back(handle.id);
    }
    if (!buffers.empty()) {
        m_Queue.Submit(buffers.size(), buffers.data());
    }
    for (std::uint64_t id : done) {
        m_FreeCommandLists.push_back(id);
    }
}
