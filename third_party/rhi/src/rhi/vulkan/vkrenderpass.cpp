#include "rhi/vulkan/vkrhi.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

rhi::CommandListHandle VulkanRHI::BeginCommandList()
{
    rhi::CommandListHandle handle;
    if (m_Device == VK_NULL_HANDLE || m_CurrentFrame == 0xFFFFFFFF) {
        return handle;
    }
    FrameContext& frame = m_Frames[m_CurrentFrame];

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = frame.pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer buffer = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(m_Device, &allocInfo, &buffer) != VK_SUCCESS) {
        return handle;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(buffer, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(m_Device, frame.pool, 1, &buffer);
        return handle;
    }

    VulkanCommandList list;
    list.frame = m_CurrentFrame;
    list.bufferIndex = static_cast<std::uint32_t>(frame.cmds.size());
    list.open = true;
    frame.cmds.push_back(buffer);

    if (!m_FreeCommandLists.empty()) {
        handle.id = m_FreeCommandLists.back();
        m_FreeCommandLists.pop_back();
        m_CommandLists[handle.id] = list;
    } else {
        handle.id = static_cast<std::uint64_t>(m_CommandLists.size());
        m_CommandLists.push_back(list);
    }
    return handle;
}

VkCommandBuffer VulkanRHI::GetVkCommandBuffer(rhi::CommandListHandle handle)
{
    if (handle.id >= m_CommandLists.size()) {
        return VK_NULL_HANDLE;
    }
    VulkanCommandList& list = m_CommandLists[handle.id];
    if (!list.open || list.frame >= m_Frames.size() ||
        list.bufferIndex >= m_Frames[list.frame].cmds.size()) {
        return VK_NULL_HANDLE;
    }
    return m_Frames[list.frame].cmds[list.bufferIndex];
}

void VulkanRHI::TransitionImage(VkCommandBuffer buffer, VkImage image,
                            VkImageAspectFlags aspect, VkImageLayout target,
                            VkImageLayout& stored)
{
    if (image == VK_NULL_HANDLE || stored == target) {
        return;
    }
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = stored;
    barrier.newLayout = target;
    barrier.srcAccessMask = accessForLayout(stored);
    barrier.dstAccessMask = accessForLayout(target);
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    vkCmdPipelineBarrier(buffer,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
    stored = target;
}

void VulkanRHI::TransitionTexture(VkCommandBuffer cmd, const rhi::TextureHandle& tex,
                                  VkImageAspectFlags aspect, VkImageLayout target)
{
    if (cmd == VK_NULL_HANDLE || tex.id == 0xFFFFFFFFFFFFFFFFull ||
        tex.id >= m_Textures.size() || tex.id >= m_TextureLayouts.size() ||
        m_Textures[tex.id] == VK_NULL_HANDLE) {
        return;
    }
    TransitionImage(cmd, m_Textures[tex.id], aspect, target, m_TextureLayouts[tex.id]);
}

void VulkanRHI::BeginRenderPass(rhi::CommandListHandle cmd, const rhi::RenderPassDesc& desc)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    VulkanCommandList& list = m_CommandLists[cmd.id];
    if (list.pass != CurrentPass::None) {
        printf("BeginRenderPass: a pass is already open on this command list\n");
        return;
    }
    if (desc.colors.empty() && !desc.depth.enabled) {
        printf("BeginRenderPass needs at least one color attachment or a depth attachment\n");
        return;
    }

    std::vector<VkRenderingAttachmentInfo> colors;
    colors.reserve(desc.colors.size());
    VkExtent2D extent{};
    bool extentSet = false;

    for (const auto& attachment : desc.colors) {
        VkImageView view = VK_NULL_HANDLE;
        if (attachment.useSwapchain) {
            if (desc.swapchain.id == 0xFFFFFFFFFFFFFFFF ||
                desc.swapchain.id >= m_Swapchains.size()) {
                printf("BeginRenderPass swapchain handle is invalid\n");
                return;
            }
            VkSwapchainData& swapchain = m_Swapchains[desc.swapchain.id];
            if (desc.imageIndex >= swapchain.views.size()) {
                printf("BeginRenderPass swapchain image index is invalid\n");
                return;
            }
            view = swapchain.views[desc.imageIndex];
            VkImage image = swapchain.images[desc.imageIndex];
            list.swapchain = desc.swapchain.id;
            list.swapchainImageIndex = desc.imageIndex;
            TransitionImage(buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            swapchain.layouts[desc.imageIndex]);
            extent = swapchain.extent;
            extentSet = true;
        } else {
            if (attachment.view.id == 0xFFFFFFFFFFFFFFFF ||
                attachment.view.id >= m_TextureViews.size() ||
                m_TextureViews[attachment.view.id] == VK_NULL_HANDLE) {
                printf("BeginRenderPass color view handle is invalid\n");
                return;
            }
            view = m_TextureViews[attachment.view.id];
            const rhi::TextureHandle& parent = attachment.view.texture;
            if (parent.id == 0xFFFFFFFFFFFFFFFF ||
                parent.id >= m_Textures.size() ||
                m_Textures[parent.id] == VK_NULL_HANDLE) {
                printf("BeginRenderPass color view has no parent texture\n");
                return;
            }
            const std::uint32_t mip = attachment.view.desc.baseMipLevel;
            const std::uint32_t mipWidth = parent.desc.width >> mip;
            const std::uint32_t mipHeight = parent.desc.height >> mip;
            const VkExtent2D viewExtent{
                mipWidth > 1u ? mipWidth : 1u,
                mipHeight > 1u ? mipHeight : 1u,
            };
            if (!extentSet) {
                extent = viewExtent;
                extentSet = true;
            } else if (extent.width != viewExtent.width || extent.height != viewExtent.height) {
                printf("BeginRenderPass: color attachments have mismatched extents\n");
                return;
            }
            TransitionTexture(buffer, parent, VK_IMAGE_ASPECT_COLOR_BIT,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        VkRenderingAttachmentInfo color{};
        color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color.imageView = view;
        color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp = attachment.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                        : VK_ATTACHMENT_LOAD_OP_LOAD;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.clearValue.color.float32[0] = attachment.clearColor[0];
        color.clearValue.color.float32[1] = attachment.clearColor[1];
        color.clearValue.color.float32[2] = attachment.clearColor[2];
        color.clearValue.color.float32[3] = attachment.clearColor[3];
        colors.push_back(color);
    }

    VkRenderingAttachmentInfo depthStorage{};
    if (desc.depth.enabled) {
        if (desc.depth.view.id == 0xFFFFFFFFFFFFFFFF ||
            desc.depth.view.id >= m_TextureViews.size() ||
            m_TextureViews[desc.depth.view.id] == VK_NULL_HANDLE) {
            printf("BeginRenderPass depth view handle is invalid\n");
            return;
        }
        const rhi::TextureHandle& depthParent = desc.depth.view.texture;
        if (depthParent.id == 0xFFFFFFFFFFFFFFFF ||
            depthParent.id >= m_Textures.size() ||
            m_Textures[depthParent.id] == VK_NULL_HANDLE) {
            printf("BeginRenderPass depth view has no parent texture\n");
            return;
        }
        depthStorage.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depthStorage.imageView = m_TextureViews[desc.depth.view.id];
        depthStorage.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthStorage.loadOp = desc.depth.clearDepth ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                                    : VK_ATTACHMENT_LOAD_OP_LOAD;
        depthStorage.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthStorage.clearValue.depthStencil.depth = desc.depth.clearDepthValue;
        depthStorage.clearValue.depthStencil.stencil = desc.depth.clearStencilValue;
        TransitionTexture(buffer, depthParent,
                          toVkAspect(depthParent.desc.format),
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea.offset = { 0, 0 };
    renderInfo.renderArea.extent = extent;
    if (!extentSet) {
        if (desc.depth.enabled) {
            const rhi::TextureHandle& depthParent = desc.depth.view.texture;
            renderInfo.renderArea.extent = {
                depthParent.desc.width, depthParent.desc.height
            };
        } else {
            renderInfo.renderArea.extent = VkExtent2D{ 1, 1 };
        }
    }
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = static_cast<std::uint32_t>(colors.size());
    renderInfo.pColorAttachments = colors.data();
    renderInfo.pDepthAttachment = desc.depth.enabled ? &depthStorage : nullptr;

    vkCmdBeginRendering(buffer, &renderInfo);
    list.pass = CurrentPass::Graphics;
    list.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
}

void VulkanRHI::BeginComputePass(rhi::CommandListHandle cmd)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    VulkanCommandList& list = m_CommandLists[cmd.id];
    if (list.pass != CurrentPass::None) {
        printf("BeginComputePass: a pass is already open on this command list\n");
        return;
    }
    list.pass = CurrentPass::Compute;
    list.bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
}

void VulkanRHI::EndPass(rhi::CommandListHandle cmd)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    VulkanCommandList& list = m_CommandLists[cmd.id];
    if (list.pass == CurrentPass::Graphics) {
        vkCmdEndRendering(buffer);
        if (list.swapchain != 0xFFFFFFFFFFFFFFFFull &&
            list.swapchain < m_Swapchains.size() &&
            list.swapchainImageIndex <
                m_Swapchains[list.swapchain].layouts.size() &&
            list.swapchainImageIndex <
                m_Swapchains[list.swapchain].images.size()) {
            VkImage image = m_Swapchains[list.swapchain].images[list.swapchainImageIndex];
            TransitionImage(buffer, image, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                            m_Swapchains[list.swapchain].layouts[list.swapchainImageIndex]);
        }
        list.pass = CurrentPass::None;
        list.swapchain = 0xFFFFFFFFFFFFFFFFull;
        list.swapchainImageIndex = 0xFFFFFFFF;
    } else if (list.pass == CurrentPass::Compute) {
        list.pass = CurrentPass::None;
    }
}

void VulkanRHI::SetPipeline(rhi::CommandListHandle cmd, rhi::GraphicsPipelineHandle handle)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    if (handle.id >= m_Pipelines.size() || m_Pipelines[handle.id] == VK_NULL_HANDLE) {
        printf("SetPipeline: invalid graphics pipeline handle\n");
        return;
    }
    VulkanCommandList& list = m_CommandLists[cmd.id];
    if (handle.desc.layout.id < m_PipelineLayouts.size()) {
        list.boundLayout = m_PipelineLayouts[handle.desc.layout.id];
    }
    list.bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipelines[handle.id]);
}

void VulkanRHI::SetPipeline(rhi::CommandListHandle cmd, rhi::ComputePipelineHandle handle)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    if (handle.id >= m_ComputePipelines.size() ||
        m_ComputePipelines[handle.id] == VK_NULL_HANDLE) {
        printf("SetPipeline: invalid compute pipeline handle\n");
        return;
    }
    VulkanCommandList& list = m_CommandLists[cmd.id];
    if (handle.desc.layout.id < m_PipelineLayouts.size()) {
        list.boundLayout = m_PipelineLayouts[handle.desc.layout.id];
    }
    list.bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_ComputePipelines[handle.id]);
}

void VulkanRHI::SetBindGroup(rhi::CommandListHandle cmd, std::uint32_t slot, rhi::BindGroupHandle handle)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    if (handle.id >= m_BindGroups.size() || m_BindGroups[handle.id] == VK_NULL_HANDLE) {
        printf("SetBindGroup: invalid bind group handle\n");
        return;
    }
    VulkanCommandList& list = m_CommandLists[cmd.id];
    if (list.boundLayout == VK_NULL_HANDLE) {
        printf("SetBindGroup: no pipeline bound yet\n");
        return;
    }
    for (const auto& entry : handle.desc.entries) {
        if (entry.textureView.id == 0xFFFFFFFFFFFFFFFFull ||
            entry.textureView.id >= m_TextureViews.size() ||
            m_TextureViews[entry.textureView.id] == VK_NULL_HANDLE) {
            continue;
        }
        const rhi::TextureHandle& parent = entry.textureView.texture;
        if (parent.id == 0xFFFFFFFFFFFFFFFFull || parent.id >= m_Textures.size() ||
            parent.id >= m_TextureLayouts.size() ||
            m_Textures[parent.id] == VK_NULL_HANDLE) {
            continue;
        }
        for (const auto& shaderBinding : handle.desc.layout.desc.shaderBindings) {
            if (shaderBinding.binding != entry.binding) {
                continue;
            }
            const VkImageAspectFlags aspect = toVkAspect(parent.desc.format);
            if (shaderBinding.type == rhi::BindingType::SampledTexture) {
                if (m_TextureLayouts[parent.id] != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                    TransitionTexture(buffer, parent, aspect,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }
            } else if (shaderBinding.type == rhi::BindingType::StorageTexture) {
                if (m_TextureLayouts[parent.id] != VK_IMAGE_LAYOUT_GENERAL) {
                    TransitionTexture(buffer, parent, aspect, VK_IMAGE_LAYOUT_GENERAL);
                }
            }
            break;
        }
    }
    vkCmdBindDescriptorSets(buffer, list.bindPoint, list.boundLayout,
                            slot, 1, &m_BindGroups[handle.id], 0, nullptr);
}

void VulkanRHI::SetVertexBuffer(rhi::CommandListHandle cmd, std::uint32_t slot, rhi::BufferHandle handle, std::size_t offset)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    if (handle.id >= m_Buffers.size() || m_Buffers[handle.id] == VK_NULL_HANDLE) {
        printf("SetVertexBuffer: invalid buffer handle\n");
        return;
    }
    if (offset > handle.desc.size) {
        printf("SetVertexBuffer offset is out of range\n");
        return;
    }
    VkDeviceSize vkOffset = static_cast<VkDeviceSize>(offset);
    vkCmdBindVertexBuffers(buffer, slot, 1, &m_Buffers[handle.id], &vkOffset);
}

void VulkanRHI::SetIndexBuffer(rhi::CommandListHandle cmd, rhi::BufferHandle handle, rhi::IndexFormat format, std::size_t offset)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    if (handle.id >= m_Buffers.size() || m_Buffers[handle.id] == VK_NULL_HANDLE) {
        printf("SetIndexBuffer: invalid buffer handle\n");
        return;
    }
    if (offset > handle.desc.size) {
        printf("SetIndexBuffer offset is out of range\n");
        return;
    }
    VkIndexType type = (format == rhi::IndexFormat::Uint16) ? VK_INDEX_TYPE_UINT16
                                                            : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(buffer, m_Buffers[handle.id],
                         static_cast<VkDeviceSize>(offset), type);
}

void VulkanRHI::SetViewport(rhi::CommandListHandle cmd, float x, float y, float width, float height)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    VkViewport viewport{};
    viewport.x = x;
    viewport.y = y + height;
    viewport.width = width;
    viewport.height = -height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(buffer, 0, 1, &viewport);
}

void VulkanRHI::SetScissor(rhi::CommandListHandle cmd, std::uint32_t x, std::uint32_t y, std::uint32_t width, std::uint32_t height)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    VkRect2D scissor{};
    scissor.offset = { static_cast<std::int32_t>(x), static_cast<std::int32_t>(y) };
    scissor.extent = { width, height };
    vkCmdSetScissor(buffer, 0, 1, &scissor);
}

void VulkanRHI::Draw(rhi::CommandListHandle cmd, std::uint32_t vertexCount, std::uint32_t instanceCount,
                      std::uint32_t firstVertex, std::uint32_t firstInstance)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    vkCmdDraw(buffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanRHI::DrawIndexed(rhi::CommandListHandle cmd, std::uint32_t indexCount, std::uint32_t instanceCount,
                             std::uint32_t firstIndex, std::int32_t baseVertex,
                             std::uint32_t firstInstance)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    vkCmdDrawIndexed(buffer, indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void VulkanRHI::DrawIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset,
                              std::uint32_t drawCount, std::uint32_t stride)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    if (params.id >= m_Buffers.size() || m_Buffers[params.id] == VK_NULL_HANDLE) {
        printf("DrawIndirect: invalid params buffer handle\n");
        return;
    }
    vkCmdDrawIndirect(buffer, m_Buffers[params.id],
                      static_cast<VkDeviceSize>(offset), drawCount, stride);
}

void VulkanRHI::DrawIndexedIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset,
                                     std::uint32_t drawCount, std::uint32_t stride)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    if (params.id >= m_Buffers.size() || m_Buffers[params.id] == VK_NULL_HANDLE) {
        printf("DrawIndexedIndirect: invalid params buffer handle\n");
        return;
    }
    vkCmdDrawIndexedIndirect(buffer, m_Buffers[params.id],
                             static_cast<VkDeviceSize>(offset), drawCount, stride);
}

void VulkanRHI::Dispatch(rhi::CommandListHandle cmd, std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    vkCmdDispatch(buffer, x, y, z);
}

void VulkanRHI::DispatchIndirect(rhi::CommandListHandle cmd, rhi::BufferHandle params, std::size_t offset)
{
    VkCommandBuffer buffer = GetVkCommandBuffer(cmd);
    if (buffer == VK_NULL_HANDLE) {
        return;
    }
    if (params.id >= m_Buffers.size() || m_Buffers[params.id] == VK_NULL_HANDLE) {
        printf("DispatchIndirect: invalid params buffer handle\n");
        return;
    }
    vkCmdDispatchIndirect(buffer, m_Buffers[params.id], static_cast<VkDeviceSize>(offset));
}

bool VulkanRHI::EnsureUploadReady()
{
    if (m_Device == VK_NULL_HANDLE || m_UploadBuffer == VK_NULL_HANDLE) {
        return false;
    }
    if (m_UploadFence != VK_NULL_HANDLE) {
        vkWaitForFences(m_Device, 1, &m_UploadFence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(m_Device, m_UploadFence, nullptr);
        m_UploadFence = VK_NULL_HANDLE;
        m_StagingHead = 0;
        vkResetCommandBuffer(m_UploadBuffer, 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(m_UploadBuffer, &beginInfo) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

void VulkanRHI::FlushUploads(bool wait)
{
    if (!m_UploadHasWork) {
        return;
    }
    m_UploadHasWork = false;
    if (vkEndCommandBuffer(m_UploadBuffer) != VK_SUCCESS) {
        return;
    }
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_UploadBuffer;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(m_Device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        return;
    }
    if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        vkDestroyFence(m_Device, fence, nullptr);
        return;
    }
    m_UploadFence = fence;
    if (wait) {
        vkWaitForFences(m_Device, 1, &m_UploadFence, VK_TRUE, UINT64_MAX);
    }
}

void VulkanRHI::FlushTransfers(bool wait)
{
    FlushUploads(wait);
}

bool VulkanRHI::AllocStaging(std::size_t size, std::size_t align, std::size_t& outOffset)
{
    if (m_StagingMapped == nullptr || size == 0 || size > kStagingRingSize || align == 0) {
        printf("AllocStaging: bad request (size %zu, ring %zu)\n", size, kStagingRingSize);
        return false;
    }
    std::size_t head = (m_StagingHead + align - 1) & ~(align - 1);
    if (head + size > kStagingRingSize) {
        FlushUploads(false);
        if (m_UploadFence != VK_NULL_HANDLE) {
            vkWaitForFences(m_Device, 1, &m_UploadFence, VK_TRUE, UINT64_MAX);
            vkDestroyFence(m_Device, m_UploadFence, nullptr);
            m_UploadFence = VK_NULL_HANDLE;
        }
        vkResetCommandBuffer(m_UploadBuffer, 0);
        VkCommandBufferBeginInfo wrapBegin{};
        wrapBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        wrapBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(m_UploadBuffer, &wrapBegin) != VK_SUCCESS) {
            return false;
        }
        head = 0;
    } else if (m_UploadFence != VK_NULL_HANDLE &&
               vkGetFenceStatus(m_Device, m_UploadFence) == VK_SUCCESS) {
        // Fast reclaim: prior flush is done, ring is fully reusable.
        vkDestroyFence(m_Device, m_UploadFence, nullptr);
        m_UploadFence = VK_NULL_HANDLE;
        head = 0;
    }
    outOffset = head;
    m_StagingHead = head + size;
    return true;
}

void VulkanRHI::UploadBuffer(rhi::BufferHandle dst, std::size_t dstOffset,
                             const void* data, std::size_t size)
{
    if (!EnsureUploadReady() || data == nullptr) {
        return;
    }
    if (dst.id >= m_Buffers.size() || m_Buffers[dst.id] == VK_NULL_HANDLE) {
        printf("UploadBuffer: invalid buffer handle\n");
        return;
    }
    if (size == 0 || dstOffset > dst.desc.size || size > dst.desc.size - dstOffset) {
        printf("UploadBuffer range is out of bounds\n");
        return;
    }
    std::size_t stagingOffset = 0;
    if (!AllocStaging(size, 16, stagingOffset)) {
        return;
    }
    std::memcpy(static_cast<std::byte*>(m_StagingMapped) + stagingOffset, data, size);
    vmaFlushAllocation(m_Allocator, m_StagingMemory, stagingOffset, size);
    VkBufferCopy region{};
    region.srcOffset = static_cast<VkDeviceSize>(stagingOffset);
    region.dstOffset = static_cast<VkDeviceSize>(dstOffset);
    region.size = static_cast<VkDeviceSize>(size);
    vkCmdCopyBuffer(m_UploadBuffer, m_StagingBuffer, m_Buffers[dst.id], 1, &region);
    m_UploadHasWork = true;
}

void VulkanRHI::UploadTexture(rhi::TextureHandle dst, std::uint32_t mipLevel,
                              const void* data, std::uint32_t width, std::uint32_t height)
{
    if (!EnsureUploadReady() || data == nullptr) {
        return;
    }
    if (dst.id >= m_Textures.size() || m_Textures[dst.id] == VK_NULL_HANDLE) {
        printf("UploadTexture: invalid texture handle\n");
        return;
    }
    const std::uint32_t texelBytes = TexelBytes(dst.desc.format);
    if (texelBytes == 0) {
        printf("UploadTexture: unsupported (block-compressed) format\n");
        return;
    }
    const std::uint32_t mipWidth = (dst.desc.width >> mipLevel) > 1u ? (dst.desc.width >> mipLevel) : 1u;
    const std::uint32_t mipHeight = (dst.desc.height >> mipLevel) > 1u ? (dst.desc.height >> mipLevel) : 1u;
    if (width != mipWidth || height != mipHeight) {
        printf("UploadTexture: extent %ux%u does not match mip %u (%ux%u)\n",
            width, height, mipLevel, mipWidth, mipHeight);
        return;
    }
    const std::size_t tightBytes =
        static_cast<std::size_t>(width) * height * texelBytes;
    std::size_t stagingOffset = 0;
    if (!AllocStaging(tightBytes, 256, stagingOffset)) {
        return;
    }
    std::memcpy(static_cast<std::byte*>(m_StagingMapped) + stagingOffset, data, tightBytes);
    vmaFlushAllocation(m_Allocator, m_StagingMemory, stagingOffset, tightBytes);
    const VkImageAspectFlags aspect = toVkAspect(dst.desc.format);
    VkBufferImageCopy copy{};
    copy.bufferOffset = static_cast<VkDeviceSize>(stagingOffset);
    copy.bufferRowLength = 0; // tightly packed
    copy.bufferImageHeight = 0;
    copy.imageSubresource.aspectMask = aspect;
    copy.imageSubresource.mipLevel = mipLevel;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset = { 0, 0, 0 };
    copy.imageExtent = { width, height, 1 };
    TransitionTexture(m_UploadBuffer, dst, aspect,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdCopyBufferToImage(m_UploadBuffer, m_StagingBuffer, m_Textures[dst.id],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    m_UploadHasWork = true;
}

void VulkanRHI::CopyBufferToBuffer(rhi::BufferHandle src, rhi::BufferHandle dst, std::size_t size,
                                   std::size_t srcOffset, std::size_t dstOffset)
{
    if (!EnsureUploadReady()) {
        return;
    }
    if (src.id >= m_Buffers.size() || m_Buffers[src.id] == VK_NULL_HANDLE ||
        dst.id >= m_Buffers.size() || m_Buffers[dst.id] == VK_NULL_HANDLE) {
        printf("CopyBufferToBuffer: invalid buffer handle\n");
        return;
    }
    if (size == 0 || srcOffset > src.desc.size || size > src.desc.size - srcOffset ||
        dstOffset > dst.desc.size || size > dst.desc.size - dstOffset) {
        printf("CopyBufferToBuffer range is out of bounds\n");
        return;
    }
    VkBufferCopy region{};
    region.srcOffset = static_cast<VkDeviceSize>(srcOffset);
    region.dstOffset = static_cast<VkDeviceSize>(dstOffset);
    region.size = static_cast<VkDeviceSize>(size);
    vkCmdCopyBuffer(m_UploadBuffer, m_Buffers[src.id], m_Buffers[dst.id], 1, &region);
    m_UploadHasWork = true;
}

void VulkanRHI::CopyBufferToTexture(rhi::BufferHandle src, rhi::TextureHandle dst,
                                     const rhi::TextureWriteDesc& region)
{
    if (!EnsureUploadReady()) {
        return;
    }
    if (src.id >= m_Buffers.size() || m_Buffers[src.id] == VK_NULL_HANDLE ||
        dst.id >= m_Textures.size() || m_Textures[dst.id] == VK_NULL_HANDLE) {
        printf("CopyBufferToTexture: invalid copy handle\n");
        return;
    }
    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    const std::uint32_t dstTexelBytes = TexelBytes(dst.desc.format);
    if (dstTexelBytes == 0) {
        printf("CopyBufferToTexture: unsupported (block-compressed) format\n");
        return;
    }
    if (region.bytesPerRow % dstTexelBytes != 0) {
        printf("CopyBufferToTexture: bytesPerRow must be a multiple of texel size\n");
        return;
    }
    const VkImageAspectFlags dstAspect = toVkAspect(dst.desc.format);

    copy.bufferRowLength = region.bytesPerRow / dstTexelBytes;
    copy.bufferImageHeight = region.rowsPerImage;
    copy.imageSubresource.aspectMask = dstAspect;
    copy.imageSubresource.mipLevel = region.mipLevel;
    copy.imageSubresource.baseArrayLayer = region.baseArrayLayer;
    copy.imageSubresource.layerCount = region.arrayLayers;
    copy.imageOffset = { static_cast<std::int32_t>(region.x),
                         static_cast<std::int32_t>(region.y),
                         static_cast<std::int32_t>(region.z) };
    copy.imageExtent = { region.width, region.height, region.depth };
    TransitionTexture(m_UploadBuffer, dst, dstAspect,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    vkCmdCopyBufferToImage(m_UploadBuffer, m_Buffers[src.id], m_Textures[dst.id],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    m_UploadHasWork = true;
}

void VulkanRHI::CopyTextureToBuffer(rhi::TextureHandle src, rhi::BufferHandle dst,
                                     const rhi::TextureWriteDesc& region)
{
    if (!EnsureUploadReady()) {
        return;
    }
    if (src.id >= m_Textures.size() || m_Textures[src.id] == VK_NULL_HANDLE ||
        dst.id >= m_Buffers.size() || m_Buffers[dst.id] == VK_NULL_HANDLE) {
        printf("CopyTextureToBuffer: invalid copy handle\n");
        return;
    }
    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    const std::uint32_t srcTexelBytes = TexelBytes(src.desc.format);
    if (srcTexelBytes == 0) {
        printf("CopyTextureToBuffer: unsupported (block-compressed) format\n");
        return;
    }
    if (region.bytesPerRow % srcTexelBytes != 0) {
        printf("CopyTextureToBuffer: bytesPerRow must be a multiple of texel size\n");
        return;
    }
    const VkImageAspectFlags srcAspect = toVkAspect(src.desc.format);

    copy.bufferRowLength = region.bytesPerRow / srcTexelBytes;
    copy.bufferImageHeight = region.rowsPerImage;
    copy.imageSubresource.aspectMask = srcAspect;
    copy.imageSubresource.mipLevel = region.mipLevel;
    copy.imageSubresource.baseArrayLayer = region.baseArrayLayer;
    copy.imageSubresource.layerCount = region.arrayLayers;
    copy.imageOffset = { static_cast<std::int32_t>(region.x),
                         static_cast<std::int32_t>(region.y),
                         static_cast<std::int32_t>(region.z) };
    copy.imageExtent = { region.width, region.height, region.depth };
    TransitionTexture(m_UploadBuffer, src, srcAspect,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkCmdCopyImageToBuffer(m_UploadBuffer, m_Textures[src.id],
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           m_Buffers[dst.id], 1, &copy);
    m_UploadHasWork = true;
}

void VulkanRHI::CopyTextureToTexture(rhi::TextureHandle src, rhi::TextureHandle dst,
                                     const rhi::TextureCopyRegion& region)
{
    if (!EnsureUploadReady()) {
        return;
    }
    if (src.id >= m_Textures.size() || m_Textures[src.id] == VK_NULL_HANDLE ||
        dst.id >= m_Textures.size() || m_Textures[dst.id] == VK_NULL_HANDLE) {
        printf("CopyTextureToTexture: invalid texture handle\n");
        return;
    }

    const VkImageAspectFlags copySrcAspect = toVkAspect(src.desc.format);
    const VkImageAspectFlags copyDstAspect = toVkAspect(dst.desc.format);
    if (copySrcAspect != copyDstAspect) {
        printf("CopyTextureToTexture: source and destination aspects differ\n");
        return;
    }
    TransitionTexture(m_UploadBuffer, src, copySrcAspect,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    TransitionTexture(m_UploadBuffer, dst, copyDstAspect,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageCopy copy{};
    copy.srcSubresource.aspectMask = copySrcAspect;
    copy.srcSubresource.mipLevel = region.srcMipLevel;
    copy.srcSubresource.baseArrayLayer = region.srcBaseArrayLayer;
    copy.srcSubresource.layerCount = region.arrayLayers;
    copy.srcOffset = { static_cast<std::int32_t>(region.srcX),
                       static_cast<std::int32_t>(region.srcY),
                       static_cast<std::int32_t>(region.srcZ) };
    copy.dstSubresource.aspectMask = copyDstAspect;
    copy.dstSubresource.mipLevel = region.dstMipLevel;
    copy.dstSubresource.baseArrayLayer = region.dstBaseArrayLayer;
    copy.dstSubresource.layerCount = region.arrayLayers;
    copy.dstOffset = { static_cast<std::int32_t>(region.dstX),
                       static_cast<std::int32_t>(region.dstY),
                       static_cast<std::int32_t>(region.dstZ) };
    copy.extent = { region.width, region.height, region.depth };

    vkCmdCopyImage(m_UploadBuffer, m_Textures[src.id], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   m_Textures[dst.id], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    m_UploadHasWork = true;
}

void VulkanRHI::Submit(std::span<const rhi::CommandListHandle> cmds)
{
    if (m_Device == VK_NULL_HANDLE || cmds.empty() || m_CurrentFrame == 0xFFFFFFFF) {
        return;
    }
    FlushUploads(false);
    FrameContext& frame = m_Frames[m_CurrentFrame];

    std::vector<VkCommandBuffer> buffers;
    buffers.reserve(cmds.size());
    for (rhi::CommandListHandle handle : cmds) {
        if (handle.id >= m_CommandLists.size()) {
            return;
        }
        VulkanCommandList& list = m_CommandLists[handle.id];
        if (!list.open || list.frame != m_CurrentFrame ||
            list.bufferIndex >= frame.cmds.size()) {
            return;
        }
        VkCommandBuffer buffer = frame.cmds[list.bufferIndex];
        if (list.pass != CurrentPass::None) {
            printf("Submit: command list still has an open pass, call EndPass first\n");
            return;
        }
        if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
            return;
        }
        buffers.push_back(buffer);
    }

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    VkSemaphore* renderFinished = nullptr;
    if (frame.acquiredSwapchain < m_Swapchains.size()) {
        VkSwapchainData& swapchain = m_Swapchains[frame.acquiredSwapchain];
        if (frame.acquiredImage < swapchain.renderFinished.size() &&
            swapchain.renderFinished[frame.acquiredImage] != VK_NULL_HANDLE) {
            renderFinished = &swapchain.renderFinished[frame.acquiredImage];
        }
    }
    if (renderFinished == nullptr) {
        return;
    }
    if (frame.pendingFences.empty()) {
        submitInfo.pWaitSemaphores = &frame.imageAvailable;
    } else {
        submitInfo.pWaitSemaphores = renderFinished;
    }
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = static_cast<std::uint32_t>(buffers.size());
    submitInfo.pCommandBuffers = buffers.data();
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = renderFinished;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(m_Device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        return;
    }

    if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, fence) != VK_SUCCESS) {
        vkDestroyFence(m_Device, fence, nullptr);
        return;
    }
    frame.pendingFences.push_back(fence);
    frame.submitted = true;

    for (rhi::CommandListHandle handle : cmds) {
        m_CommandLists[handle.id].open = false;
        m_FreeCommandLists.push_back(handle.id);
    }
}
