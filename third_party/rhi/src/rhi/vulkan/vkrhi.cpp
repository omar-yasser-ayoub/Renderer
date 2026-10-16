#define VMA_IMPLEMENTATION
#include "rhi/vulkan/vkrhi.h"
#include <vulkan/vulkan.h>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <vector>


bool VulkanRHI::Initialize() {
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "reeka's RHI";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "N/A";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> surfaceExtensions;
    surfaceExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    surfaceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_XCB_KHR)
    surfaceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    surfaceExtensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    surfaceExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif

    // Enable the standard validation layer when available (dev builds).
    // Also requires VK_EXT_debug_utils for the messenger callback.
    bool enableValidation = false;
    {
        std::uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> layers(layerCount);
        if (layerCount > 0) {
            vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
        }
        for (const auto& layer : layers) {
            if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                enableValidation = true;
                break;
            }
        }
    }

    std::vector<const char*> instanceExtensions = surfaceExtensions;
    if (enableValidation) {
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(instanceExtensions.size());
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();

    const char* validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
    if (enableValidation) {
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = validationLayers;
    }

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance: " << result << std::endl;
        return false;
    }

    if (enableValidation) {
        auto createMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT"));
        if (createMessenger != nullptr) {
            VkDebugUtilsMessengerCreateInfoEXT messengerInfo{};
            messengerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            messengerInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            messengerInfo.messageType =
                VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            messengerInfo.pfnUserCallback =
                [](VkDebugUtilsMessageSeverityFlagBitsEXT,
                   VkDebugUtilsMessageTypeFlagsEXT,
                   const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                   void*) -> VkBool32 {
                std::cerr << "[Vulkan validation] " << callbackData->pMessage << '\n';
                return VK_FALSE;
            };
            if (createMessenger(m_Instance, &messengerInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
                std::cerr << "Failed to create Vulkan debug messenger (continuing without it).\n";
                m_DebugMessenger = VK_NULL_HANDLE;
            }
        }
    }

    uint32_t deviceCount = 0;

    result = vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS) {
        std::cerr << "Failed to enumerate Vulkan devices: " << result << std::endl;
        DestroyInstance();
        return false;
    }
    if (deviceCount == 0) {
        std::cerr << "No Vulkan devices found." << std::endl;
        DestroyInstance();
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to enumerate Vulkan devices: " << result << std::endl;
        DestroyInstance();
        return false;
    }

    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    int bestScore = -1;
    for (VkPhysicalDevice candidate : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(candidate, &props);

        std::uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(
            candidate, nullptr, &extCount, exts.data());
        bool hasSwapchain = false;
        for (const auto& ext : exts) {
            if (std::strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
                hasSwapchain = true;
                break;
            }
        }
        if (!hasSwapchain) {
            continue;
        }

        std::uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qProps(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &qCount, qProps.data());
        bool hasGraphics = false;
        bool hasDedicatedCompute = false;
        for (const auto& q : qProps) {
            if ((q.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
                hasGraphics = true;
            }
            if ((q.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 &&
                (q.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
                hasDedicatedCompute = true;
            }
        }
        if (!hasGraphics) {
            continue;
        }

        VkPhysicalDeviceDynamicRenderingFeatures dynCheck{};
        dynCheck.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
        VkPhysicalDeviceFeatures2 featCheck{};
        featCheck.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        featCheck.pNext = &dynCheck;
        vkGetPhysicalDeviceFeatures2(candidate, &featCheck);
        if (dynCheck.dynamicRendering == VK_FALSE) {
            continue;
        }

        int score = 0;
        switch (props.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   score += 4000; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score += 3000; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    score += 2000; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:            score += 1000; break;
        default: break;
        }
        if (hasDedicatedCompute) {
            score += 100;
        }

        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(candidate, &memProps);
        for (std::uint32_t i = 0; i < memProps.memoryHeapCount; ++i) {
            if ((memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0) {
                score += static_cast<int>(
                    memProps.memoryHeaps[i].size >> 30); // +1 per GiB
                break;
            }
        }

        if (score > bestScore) {
            bestScore = score;
            bestDevice = candidate;
        }
    }

    if (bestDevice == VK_NULL_HANDLE) {
        std::cerr << "No suitable Vulkan device found (need graphics queue + "
                     "swapchain extension + dynamic rendering).\n";
        DestroyInstance();
        return false;
    }
    m_PhysicalDevice = bestDevice;

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &deviceProperties);

    m_Capabilities.deviceName = deviceProperties.deviceName;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR
    };

    VkPhysicalDeviceVulkan13Features vk13Features{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES
    };

    VkPhysicalDeviceFeatures2 features2{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2
    };

    features2.pNext = &meshFeatures;
    meshFeatures.pNext = &rtFeatures;
    rtFeatures.pNext = &vk13Features;

    vkGetPhysicalDeviceFeatures2(m_PhysicalDevice, &features2);

    m_Capabilities.SupportsMeshShaders = meshFeatures.meshShader;
    m_Capabilities.SupportsRayTracing = rtFeatures.rayTracingPipeline;
    m_Capabilities.SupportsAnisotropy =
        features2.features.samplerAnisotropy == VK_TRUE;
    m_MaxSamplerAnisotropy = deviceProperties.limits.maxSamplerAnisotropy;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        m_PhysicalDevice,
        &queueFamilyCount,
        nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

    vkGetPhysicalDeviceQueueFamilyProperties(
        m_PhysicalDevice,
        &queueFamilyCount,
        queueFamilies.data());

    uint32_t graphicsQueueFamily = UINT32_MAX;
    uint32_t computeQueueFamily = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        if (graphicsQueueFamily == UINT32_MAX &&
            (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            graphicsQueueFamily = i;
        }
        if (computeQueueFamily == UINT32_MAX &&
            (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
        {
            computeQueueFamily = i;
        }
    }

    if (graphicsQueueFamily == UINT32_MAX) {
        std::cerr << "No graphics queue family found.\n";
        DestroyInstance();
        m_PhysicalDevice = VK_NULL_HANDLE;
        return false;
    }
    if (computeQueueFamily == UINT32_MAX) {
        std::cerr << "No dedicated compute queue family found, sharing the graphics queue.\n";
        computeQueueFamily = graphicsQueueFamily;
    }

    m_GraphicsQueueFamily = graphicsQueueFamily;
    m_ComputeQueueFamily = computeQueueFamily;

    float priorities[] = { 1.0f, 1.0f };

    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceQueueCreateInfo queueInfos[2]{};
    uint32_t queueInfoCount = 0;

    if (graphicsQueueFamily == computeQueueFamily) {
        const uint32_t available = queueFamilies[graphicsQueueFamily].queueCount;
        const uint32_t requested = available >= 2 ? 2 : 1;
        queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[0].queueFamilyIndex = graphicsQueueFamily;
        queueInfos[0].queueCount = requested;
        queueInfos[0].pQueuePriorities = priorities;
        queueInfoCount = 1;
    } else {
        queueInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[0].queueFamilyIndex = graphicsQueueFamily;
        queueInfos[0].queueCount = 1;
        queueInfos[0].pQueuePriorities = &priorities[0];
        queueInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfos[1].queueFamilyIndex = computeQueueFamily;
        queueInfos[1].queueCount = 1;
        queueInfos[1].pQueuePriorities = &priorities[1];
        queueInfoCount = 2;
    }

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = queueInfoCount;
    deviceInfo.pQueueCreateInfos = queueInfos;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    VkPhysicalDeviceVulkan13Features vk13FeaturesEnable{};
    vk13FeaturesEnable.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13FeaturesEnable.dynamicRendering = VK_TRUE;
    vk13FeaturesEnable.shaderDemoteToHelperInvocation =
        vk13Features.shaderDemoteToHelperInvocation;
    deviceInfo.pNext = &vk13FeaturesEnable;

    VkPhysicalDeviceFeatures enabledFeatures{};
    enabledFeatures.samplerAnisotropy = m_Capabilities.SupportsAnisotropy;
    deviceInfo.pEnabledFeatures = &enabledFeatures;

    result = vkCreateDevice(
        m_PhysicalDevice,
        &deviceInfo,
        nullptr,
        &m_Device);

    if (result != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan device: " << result << '\n';
        DestroyInstance();
        m_PhysicalDevice = VK_NULL_HANDLE;
        return false;
    }

    vkGetDeviceQueue(m_Device, graphicsQueueFamily, 0, &m_GraphicsQueue);
    if (graphicsQueueFamily == computeQueueFamily) {
        const uint32_t computeIndex =
            queueFamilies[graphicsQueueFamily].queueCount >= 2 ? 1 : 0;
        vkGetDeviceQueue(m_Device, computeQueueFamily, computeIndex, &m_ComputeQueue);
    } else {
        vkGetDeviceQueue(m_Device, computeQueueFamily, 0, &m_ComputeQueue);
    }

    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = m_PhysicalDevice;
    allocatorInfo.device = m_Device;
    allocatorInfo.instance = m_Instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    if (vmaCreateAllocator(&allocatorInfo, &m_Allocator) != VK_SUCCESS) {
        std::cerr << "Failed to create VMA allocator\n";
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
        DestroyInstance();
        m_PhysicalDevice = VK_NULL_HANDLE;
        return false;
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 2048;

    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2048 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 512 },
        { VK_DESCRIPTOR_TYPE_SAMPLER, 512 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2048 },
    };

    poolInfo.poolSizeCount = 6;
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(
            m_Device,
            &poolInfo,
            nullptr,
            &m_DescriptorPool) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor pool\n";
        vmaDestroyAllocator(m_Allocator);
        m_Allocator = nullptr;
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
        DestroyInstance();
        m_PhysicalDevice = VK_NULL_HANDLE;
        return false;
    }

    if (!CreateFrameContexts()) {
        vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
        vmaDestroyAllocator(m_Allocator);
        m_Allocator = nullptr;
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
        DestroyInstance();
        m_PhysicalDevice = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void VulkanRHI::DestroyInstance()
{
    if (m_Instance == VK_NULL_HANDLE) {
        return;
    }
    if (m_DebugMessenger != VK_NULL_HANDLE) {
        auto destroyMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyMessenger != nullptr) {
            destroyMessenger(m_Instance, m_DebugMessenger, nullptr);
        }
        m_DebugMessenger = VK_NULL_HANDLE;
    }
    vkDestroyInstance(m_Instance, nullptr);
    m_Instance = VK_NULL_HANDLE;
}

bool VulkanRHI::CreateFrameContexts()
{
    for (std::uint32_t i = 0; i < 2; ++i) {
        FrameContext& frame = m_Frames[i];

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                         VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_GraphicsQueueFamily;
        if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &frame.pool) != VK_SUCCESS) {
            std::cerr << "Failed to create frame command pool\n";
            DestroyFrameContexts();
            return false;
        }

        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(m_Device, &semInfo, nullptr, &frame.imageAvailable) != VK_SUCCESS) {
            std::cerr << "Failed to create frame acquire semaphore\n";
            DestroyFrameContexts();
            return false;
        }
    }
    VkCommandPoolCreateInfo transferPoolInfo{};
    transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    transferPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                             VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    transferPoolInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    if (vkCreateCommandPool(m_Device, &transferPoolInfo, nullptr, &m_TransferPool) != VK_SUCCESS) {
        std::cerr << "Failed to create transfer command pool\n";
        DestroyFrameContexts();
        return false;
    }
    VkCommandBufferAllocateInfo uploadAlloc{};
    uploadAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    uploadAlloc.commandPool = m_TransferPool;
    uploadAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    uploadAlloc.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(m_Device, &uploadAlloc, &m_UploadBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to allocate upload command buffer\n";
        DestroyFrameContexts();
        return false;
    }
    VkCommandBufferBeginInfo uploadBegin{};
    uploadBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    uploadBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(m_UploadBuffer, &uploadBegin) != VK_SUCCESS) {
        std::cerr << "Failed to begin upload command buffer\n";
        DestroyFrameContexts();
        return false;
    }
    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = kStagingRingSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VmaAllocationCreateInfo stagingAlloc = ToVmaAllocInfo(rhi::MemoryUsage::CpuToGpu);
    stagingAlloc.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VmaAllocationInfo stagingAllocInfo{};
    if (vmaCreateBuffer(m_Allocator, &stagingInfo, &stagingAlloc,
            &m_StagingBuffer, &m_StagingMemory, &stagingAllocInfo) != VK_SUCCESS ||
        stagingAllocInfo.pMappedData == nullptr) {
        std::cerr << "Failed to create staging ring buffer\n";
        DestroyFrameContexts();
        return false;
    }
    m_StagingMapped = stagingAllocInfo.pMappedData;
    m_StagingHead = 0;
    return true;
}

void VulkanRHI::DestroyFrameContexts()
{
    if (m_Device == VK_NULL_HANDLE) {
        return;
    }
    if (m_UploadFence != VK_NULL_HANDLE) {
        vkWaitForFences(m_Device, 1, &m_UploadFence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(m_Device, m_UploadFence, nullptr);
        m_UploadFence = VK_NULL_HANDLE;
    }
    m_UploadBuffer = VK_NULL_HANDLE;
    m_UploadHasWork = false;
    // Ring is persistently mapped (MAPPED_BIT): VMA unmaps it on free,
    // explicit vmaUnmapMemory here would underflow the map count.
    m_StagingMapped = nullptr;
    if (m_StagingBuffer != VK_NULL_HANDLE && m_StagingMemory != nullptr) {
        vmaDestroyBuffer(m_Allocator, m_StagingBuffer, m_StagingMemory);
    }
    m_StagingBuffer = VK_NULL_HANDLE;
    m_StagingMemory = nullptr;
    m_StagingHead = 0;
    if (m_TransferPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_Device, m_TransferPool, nullptr);
        m_TransferPool = VK_NULL_HANDLE;
    }
    for (FrameContext& frame : m_Frames) {
        for (VkFence fence : frame.pendingFences) {
            if (fence != VK_NULL_HANDLE) {
                vkDestroyFence(m_Device, fence, nullptr);
            }
        }
        frame.pendingFences.clear();
        if (frame.imageAvailable != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_Device, frame.imageAvailable, nullptr);
            frame.imageAvailable = VK_NULL_HANDLE;
        }
        if (frame.pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_Device, frame.pool, nullptr);
            frame.pool = VK_NULL_HANDLE;
        }
        frame.cmds.clear();
        frame.acquiredSwapchain = 0xFFFFFFFFFFFFFFFFull;
        frame.acquiredImage = 0xFFFFFFFF;
    }
    m_CommandLists.clear();
    m_FreeCommandLists.clear();
    m_CurrentFrame = 0xFFFFFFFF;
}

void VulkanRHI::Shutdown() {
    if (m_Device) {
        vkDeviceWaitIdle(m_Device);

        for (auto& data : m_Swapchains) {
            if (data.swapchain == VK_NULL_HANDLE) {
                continue;
            }
            for (VkImageView view : data.views) {
                if (view != VK_NULL_HANDLE) {
                    vkDestroyImageView(m_Device, view, nullptr);
                }
            }
            vkDestroySwapchainKHR(m_Device, data.swapchain, nullptr);
        }
        m_Swapchains.clear();
        m_FreeSwapchains.clear();

        for (std::size_t i = 0; i < m_Buffers.size(); ++i) {
            if (m_Buffers[i] != VK_NULL_HANDLE) {
                vmaDestroyBuffer(m_Allocator, m_Buffers[i], m_BufferMemory[i]);
                m_Buffers[i] = VK_NULL_HANDLE;
                m_BufferMemory[i] = nullptr;
            }
        }
        for (std::size_t i = 0; i < m_Textures.size(); ++i) {
            if (m_Textures[i] != VK_NULL_HANDLE) {
                vmaDestroyImage(m_Allocator, m_Textures[i], m_TextureMemory[i]);
                m_Textures[i] = VK_NULL_HANDLE;
                m_TextureMemory[i] = nullptr;
            }
        }
        for (VkImageView view : m_TextureViews) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_Device, view, nullptr);
            }
        }
        for (VkSampler sampler : m_Samplers) {
            if (sampler != VK_NULL_HANDLE) {
                vkDestroySampler(m_Device, sampler, nullptr);
            }
        }
        for (VkPipeline pipeline : m_Pipelines) {
            if (pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_Device, pipeline, nullptr);
            }
        }
        for (VkPipeline pipeline : m_ComputePipelines) {
            if (pipeline != VK_NULL_HANDLE) {
                vkDestroyPipeline(m_Device, pipeline, nullptr);
            }
        }
        for (VkPipelineLayout layout : m_PipelineLayouts) {
            if (layout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_Device, layout, nullptr);
            }
        }

        m_Buffers.clear();
        m_BufferMemory.clear();
        m_FreeBuffers.clear();
        m_Textures.clear();
        m_TextureMemory.clear();
        m_TextureLayouts.clear();
        m_TextureViews.clear();
        m_FreeTextures.clear();
        m_FreeTextureViews.clear();
        m_Samplers.clear();
        m_FreeSamplers.clear();
        m_ShaderModules.clear();
        m_FreeShaderModules.clear();
        m_Pipelines.clear();
        m_FreePipelines.clear();
        m_ComputePipelines.clear();
        m_FreeComputePipelines.clear();
        m_PipelineLayouts.clear();
        m_FreePipelineLayouts.clear();
        for (VkDescriptorSetLayout layout : m_BindGroupLayouts) {
            if (layout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_Device, layout, nullptr);
            }
        }
        m_BindGroupLayouts.clear();
        m_FreeBindGroupLayouts.clear();
        m_BindGroups.clear();
        m_FreeBindGroups.clear();
        if (m_DescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
            m_DescriptorPool = VK_NULL_HANDLE;
        }
        DestroyFrameContexts();
        vmaDestroyAllocator(m_Allocator);
        m_Allocator = nullptr;
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
        m_GraphicsQueue = VK_NULL_HANDLE;
        m_ComputeQueue = VK_NULL_HANDLE;
        m_GraphicsQueueFamily = UINT32_MAX;
        m_ComputeQueueFamily = UINT32_MAX;
    }

    if (m_Instance) {
        for (VkSurfaceKHR surface : m_Surfaces) {
            if (surface != VK_NULL_HANDLE) {
                vkDestroySurfaceKHR(m_Instance, surface, nullptr);
            }
        }
        m_Surfaces.clear();
        m_FreeSurfaces.clear();
        DestroyInstance();
    }
    m_PhysicalDevice = VK_NULL_HANDLE;
}
