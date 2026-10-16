#include "rhi/vulkan/vkrhi.h"

#include <cstdio>
#include <vector>

rhi::SwapchainHandle VulkanRHI::CreateSwapchain(const rhi::SwapchainDesc& desc)
{
    if (m_PhysicalDevice == VK_NULL_HANDLE) {
        return {};
    }
    if (desc.surface.id == 0xFFFFFFFFFFFFFFFF || desc.surface.id >= m_Surfaces.size() ||
        m_Surfaces[desc.surface.id] == VK_NULL_HANDLE) {
        return {};
    }
    if (desc.width == 0 || desc.height == 0) {
        return {};
    }

    VkSurfaceKHR surface = m_Surfaces[desc.surface.id];

    VkSurfaceCapabilitiesKHR caps{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, surface, &caps) !=
        VK_SUCCESS) {
        return {};
    }

    //clamp image count to supported range based on caps
    std::uint32_t imageCount = desc.imageCount < caps.minImageCount ? caps.minImageCount : desc.imageCount;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    //clamp extent to supported range based on caps
    VkExtent2D extent{ desc.width, desc.height };
    if (caps.currentExtent.width != 0xFFFFFFFF) {
        extent = caps.currentExtent;
    } else {
        if (extent.width < caps.minImageExtent.width) {
            extent.width = caps.minImageExtent.width;
        } else if (extent.width > caps.maxImageExtent.width) {
            extent.width = caps.maxImageExtent.width;
        }
        if (extent.height < caps.minImageExtent.height) {
            extent.height = caps.minImageExtent.height;
        } else if (extent.height > caps.maxImageExtent.height) {
            extent.height = caps.maxImageExtent.height;
        }
    }

    VkImageUsageFlags imageUsage = toVk(desc.usage) | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageUsage &= caps.supportedUsageFlags;
    if ((imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0) {
        printf("[ERROR] Swapchain color-attachment usage not supported\n");
        return {};
    }

    //verify that present mode is supported, else fallback to fifo
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    {
        std::uint32_t modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, surface, &modeCount, nullptr);

        std::vector<VkPresentModeKHR> modes(modeCount);
        if (modeCount > 0) {
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                m_PhysicalDevice, surface, &modeCount, modes.data());
        }

        const VkPresentModeKHR requested = toVk(desc.presentMode);
        for (VkPresentModeKHR mode : modes) {
            if (mode == requested) {
                presentMode = mode;
                break;
            }
        }
    }

    // verify that transform is supported, else fallback to identity
    VkSurfaceTransformFlagBitsKHR preTransform = caps.currentTransform;
    if ((caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0) {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (desc.alphaMode == rhi::AlphaMode::Premultiplied &&
        (caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) != 0) {
        compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    } else if ((caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0) {
        compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    } else {
        const VkCompositeAlphaFlagBitsKHR fallbacks[] = {
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        };
        bool found = false;
        for (VkCompositeAlphaFlagBitsKHR fallback : fallbacks) {
            if ((caps.supportedCompositeAlpha & fallback) != 0) {
                compositeAlpha = fallback;
                found = true;
                break;
            }
        }
        if (!found) {
            printf("[ERROR] No supported swapchain composite alpha\n");
            return {};
        }
    }

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    // TODO: revisit these flags when initial implementation is done
    swapchainInfo.flags = 0;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = toVk(desc.format);
    swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = imageUsage;
    // we only have one queue family for graphics for now, can revisit this later
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // we don't need to define queueFamilyIndexCount or pQueueFamilyIndices
    swapchainInfo.preTransform = preTransform;
    swapchainInfo.compositeAlpha = compositeAlpha;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkResult result = vkCreateSwapchainKHR(m_Device, &swapchainInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS) {
        printf("[ERROR] Failed to create swapchain: %d\n", result);
        return {};
    }

    std::uint32_t swapImageCount = 0;
    vkGetSwapchainImagesKHR(m_Device, swapchain, &swapImageCount, nullptr);
    if (swapImageCount == 0) {
        printf("[ERROR] Swapchain returned no images\n");
        vkDestroySwapchainKHR(m_Device, swapchain, nullptr);
        return {};
    }

    std::vector<VkImage> images(swapImageCount);
    if (vkGetSwapchainImagesKHR(m_Device, swapchain, &swapImageCount, images.data()) !=
        VK_SUCCESS) {
        printf("[ERROR] Failed to get swapchain images\n");
        vkDestroySwapchainKHR(m_Device, swapchain, nullptr);
        return {};
    }

    const VkFormat imageFormat = toVk(desc.format);
    std::vector<VkImageView> views;
    views.reserve(images.size());
    for (VkImage image : images) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = imageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView view = VK_NULL_HANDLE;
        if (vkCreateImageView(m_Device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
            printf("[ERROR] Failed to create swapchain image view\n");
            for (VkImageView created : views) {
                vkDestroyImageView(m_Device, created, nullptr);
            }
            vkDestroySwapchainKHR(m_Device, swapchain, nullptr);
            return {};
        }
        views.push_back(view);
    }

    VkSwapchainData data;
    data.swapchain = swapchain;
    data.images = std::move(images);
    data.views = std::move(views);
    data.layouts.assign(data.images.size(), VK_IMAGE_LAYOUT_UNDEFINED);
    data.format = imageFormat;
    data.extent = extent;

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    data.renderFinished.resize(data.images.size(), VK_NULL_HANDLE);
    for (std::size_t i = 0; i < data.images.size(); ++i) {
        if (vkCreateSemaphore(m_Device, &semInfo, nullptr, &data.renderFinished[i]) != VK_SUCCESS) {
            printf("[ERROR] Failed to create swapchain semaphores\n");
            for (VkSemaphore sem : data.renderFinished) {
                if (sem != VK_NULL_HANDLE) {
                    vkDestroySemaphore(m_Device, sem, nullptr);
                }
            }
            for (VkImageView view : data.views) {
                if (view != VK_NULL_HANDLE) {
                    vkDestroyImageView(m_Device, view, nullptr);
                }
            }
            vkDestroySwapchainKHR(m_Device, swapchain, nullptr);
            return {};
        }
    }

    rhi::SwapchainHandle handle;
    handle.desc = desc;
    handle.desc.width = extent.width;
    handle.desc.height = extent.height;
    if (!m_FreeSwapchains.empty()) {
        handle.id = m_FreeSwapchains.back();
        m_FreeSwapchains.pop_back();
        m_Swapchains[handle.id] = std::move(data);
    } else {
        handle.id = static_cast<std::uint64_t>(m_Swapchains.size());
        m_Swapchains.push_back(std::move(data));
    }

    return handle;
}

void VulkanRHI::DestroySwapchain(rhi::SwapchainHandle handle)
{
    if (m_Device == VK_NULL_HANDLE || handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Swapchains.size() ||
        m_Swapchains[handle.id].swapchain == VK_NULL_HANDLE) {
        return;
    }
    VkSwapchainData& data = m_Swapchains[handle.id];
    vkDeviceWaitIdle(m_Device);
    for (VkSemaphore sem : data.renderFinished) {
        if (sem != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_Device, sem, nullptr);
        }
    }
    for (VkImageView view : data.views) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(m_Device, view, nullptr);
        }
    }
    vkDestroySwapchainKHR(m_Device, data.swapchain, nullptr);
    data = {};
    m_FreeSwapchains.push_back(handle.id);
}

rhi::AcquireResult VulkanRHI::AcquireNextImage(rhi::SwapchainHandle handle)
{
    rhi::AcquireResult result{};
    if (m_Device == VK_NULL_HANDLE || handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Swapchains.size()) {
        return result;
    }
    VkSwapchainData& data = m_Swapchains[handle.id];
    if (data.swapchain == VK_NULL_HANDLE) {
        return result;
    }

    m_CurrentFrame = static_cast<std::uint32_t>(m_FrameCounter % m_Frames.size());
    FrameContext& frame = m_Frames[m_CurrentFrame];

    if (frame.submitted) {
        for (std::uint32_t i = 0; i < frame.pendingFences.size(); ++i) {
            vkWaitForFences(m_Device, 1, &frame.pendingFences[i], VK_TRUE, UINT64_MAX);
        }
    }
    for (std::uint32_t i = 0; i < frame.pendingFences.size(); ++i) {
        vkDestroyFence(m_Device, frame.pendingFences[i], nullptr);
    }
    frame.pendingFences.clear();
    if (!frame.cmds.empty()) {
        vkFreeCommandBuffers(m_Device, frame.pool,
            static_cast<std::uint32_t>(frame.cmds.size()), frame.cmds.data());
    }
    vkResetCommandPool(m_Device, frame.pool, 0);
    frame.cmds.clear();
    frame.submitted = false;

    std::uint32_t imageIndex = 0;

    constexpr std::uint64_t kAcquireTimeoutNs = 1'000'000'000ull;
    const VkResult acquire =
        vkAcquireNextImageKHR(m_Device, data.swapchain, kAcquireTimeoutNs,
                              frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);

    switch (acquire) {
    case VK_SUCCESS:
        result.status = rhi::AcquireStatus::Success;
        result.imageIndex = imageIndex;
        data.lastImageIndex = imageIndex;
        data.layouts[imageIndex] = VK_IMAGE_LAYOUT_UNDEFINED;
        frame.acquiredSwapchain = handle.id;
        frame.acquiredImage = imageIndex;
        break;
    case VK_SUBOPTIMAL_KHR:
        result.status = rhi::AcquireStatus::Suboptimal;
        result.imageIndex = imageIndex;
        data.lastImageIndex = imageIndex;
        data.layouts[imageIndex] = VK_IMAGE_LAYOUT_UNDEFINED;
        frame.acquiredSwapchain = handle.id;
        frame.acquiredImage = imageIndex;
        break;
    case VK_TIMEOUT:
    case VK_NOT_READY:
        result.status = rhi::AcquireStatus::Timeout;
        return result;
    case VK_ERROR_OUT_OF_DATE_KHR:
        result.status = rhi::AcquireStatus::OutOfDate;
        return result;
    default:
        return result;
    }

    return result;
}

VkImage VulkanRHI::GetSwapchainImage(rhi::SwapchainHandle handle, std::uint32_t imageIndex) const
{
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Swapchains.size()) {
        return VK_NULL_HANDLE;
    }
    const VkSwapchainData& data = m_Swapchains[handle.id];
    if (data.swapchain == VK_NULL_HANDLE || imageIndex >= data.images.size()) {
        return VK_NULL_HANDLE;
    }
    return data.images[imageIndex];
}

VkImageView VulkanRHI::GetSwapchainImageView(
    rhi::SwapchainHandle handle,
    std::uint32_t imageIndex) const
{
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Swapchains.size()) {
        return VK_NULL_HANDLE;
    }
    const VkSwapchainData& data = m_Swapchains[handle.id];
    if (data.swapchain == VK_NULL_HANDLE || imageIndex >= data.views.size()) {
        return VK_NULL_HANDLE;
    }
    return data.views[imageIndex];
}

void VulkanRHI::WaitForIdle()
{
    if (!m_Device) {
        return;
    }
    FlushUploads(false);
    vkDeviceWaitIdle(m_Device);
}

void VulkanRHI::Present(rhi::SwapchainHandle handle)
{
    if (m_GraphicsQueue == VK_NULL_HANDLE || handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Swapchains.size() || m_CurrentFrame == 0xFFFFFFFF) {
        return;
    }
    VkSwapchainData& data = m_Swapchains[handle.id];
    if (data.swapchain == VK_NULL_HANDLE || data.lastImageIndex >= data.images.size()) {
        return;
    }
    FrameContext& frame = m_Frames[m_CurrentFrame];

    if (!frame.submitted) {
        // present without a pass/commandlist/etc needs to be handled, hence the code below
        rhi::RenderPassDesc pass{};
        rhi::RenderPassColorAttachment color{};
        color.useSwapchain = true;
        color.clear = false;
        pass.colors.push_back(color);
        pass.swapchain = handle;
        pass.imageIndex = data.lastImageIndex;
        rhi::CommandListHandle cmd = BeginCommandList();
        if (cmd.id == 0xFFFFFFFFFFFFFFFFull) {
            printf("[ERROR] Present: failed to begin transition command list, skipping present\n");
            return;
        }
        BeginRenderPass(cmd, pass);
        EndPass(cmd);
        Submit({&cmd, 1});
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    if (frame.submitted && data.lastImageIndex < data.renderFinished.size() &&
        data.renderFinished[data.lastImageIndex] != VK_NULL_HANDLE) {
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &data.renderFinished[data.lastImageIndex];
    } else {
        presentInfo.waitSemaphoreCount = 0;
        presentInfo.pWaitSemaphores = nullptr;
    }
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &data.swapchain;
    presentInfo.pImageIndices = &data.lastImageIndex;

    vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);
    data.lastImageIndex = 0xFFFFFFFF;
    m_CurrentFrame = 0xFFFFFFFF;
    ++m_FrameCounter;
}
