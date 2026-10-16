#include "rhi/vulkan/vkrhi.h"

rhi::PipelineLayoutHandle VulkanRHI::CreatePipelineLayout(
    const rhi::PipelineLayoutDesc& desc)
{
    rhi::PipelineLayoutHandle handle;

    std::vector<VkDescriptorSetLayout> setLayouts;
    setLayouts.reserve(desc.layouts.size());
    for (const auto& layout : desc.layouts) {
        if (layout.id >= m_BindGroupLayouts.size() ||
            m_BindGroupLayouts[layout.id] == VK_NULL_HANDLE) {
            return rhi::PipelineLayoutHandle{};
        }
        setLayouts.push_back(m_BindGroupLayouts[layout.id]);
    }

    VkPipelineLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    createInfo.setLayoutCount = static_cast<std::uint32_t>(setLayouts.size());
    createInfo.pSetLayouts = setLayouts.data();

    VkPipelineLayout layout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(m_Device, &createInfo, nullptr, &layout) != VK_SUCCESS) {
        return rhi::PipelineLayoutHandle{};
    }

    handle.id = 0xFFFFFFFFFFFFFFFFull;
    handle.desc = desc;
    if (!m_FreePipelineLayouts.empty()) {
        handle.id = m_FreePipelineLayouts.back();
        m_FreePipelineLayouts.pop_back();
        m_PipelineLayouts[handle.id] = layout;
    } else {
        handle.id = static_cast<std::uint64_t>(m_PipelineLayouts.size());
        m_PipelineLayouts.push_back(layout);
    }
    return handle;
}

rhi::GraphicsPipelineHandle VulkanRHI::CreateGraphicsPipeline(
    const rhi::GraphicsPipelineDesc& desc)
{
    rhi::GraphicsPipelineHandle handle;

    if (desc.fillMode != rhi::FillMode::Solid) {
        printf("[RHI] Wireframe fill mode is unsupported (fillModeNonSolid not enabled)\n");
        return handle;
    }
    if (desc.colorTargets.empty() &&
        desc.depthStencil.format == rhi::TextureFormat::Unknown) {
        return handle;
    }
    if (desc.layout.id >= m_PipelineLayouts.size() ||
        m_PipelineLayouts[desc.layout.id] == VK_NULL_HANDLE) {
        return handle;
    }
    const bool depthOnly = desc.colorTargets.empty();
    if (desc.vertexShader.id >= m_ShaderModules.size() ||
        (!depthOnly && desc.fragmentShader.id >= m_ShaderModules.size())) {
        return handle;
    }

    const rhi::ShaderModule& vsModule = m_ShaderModules[desc.vertexShader.id];
    if (vsModule.bytecode.empty() || vsModule.bytecode.size() % 4 != 0) {
        return handle;
    }
    for (const auto& buffer : desc.vertexBuffers) {
        for (const auto& attr : buffer.attributes) {
            if (attr.bufferSlot >= desc.vertexBuffers.size()) {
                return handle;
            }
        }
    }

    VkShaderModule vs = VK_NULL_HANDLE;
    VkShaderModule fs = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = vsModule.bytecode.size();
    moduleInfo.pCode = reinterpret_cast<const std::uint32_t*>(vsModule.bytecode.data());
    if (vkCreateShaderModule(m_Device, &moduleInfo, nullptr, &vs) != VK_SUCCESS) {
        return handle;
    }
    if (!depthOnly) {
        const rhi::ShaderModule& fsModule = m_ShaderModules[desc.fragmentShader.id];
        if (fsModule.bytecode.empty() || fsModule.bytecode.size() % 4 != 0) {
            vkDestroyShaderModule(m_Device, vs, nullptr);
            return handle;
        }
        moduleInfo.codeSize = fsModule.bytecode.size();
        moduleInfo.pCode = reinterpret_cast<const std::uint32_t*>(fsModule.bytecode.data());
        if (vkCreateShaderModule(m_Device, &moduleInfo, nullptr, &fs) != VK_SUCCESS) {
            vkDestroyShaderModule(m_Device, vs, nullptr);
            return handle;
        }
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    uint32_t stageCount = 1;
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = vsModule.entryPoint.c_str();
    if (!depthOnly) {
        const rhi::ShaderModule& fsModule = m_ShaderModules[desc.fragmentShader.id];
        stageCount = 2;
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fs;
        stages[1].pName = fsModule.entryPoint.c_str();
    }

    std::vector<VkVertexInputBindingDescription> bindings;
    bindings.reserve(desc.vertexBuffers.size());
    std::vector<VkVertexInputAttributeDescription> attributes;
    for (std::uint32_t i = 0; i < desc.vertexBuffers.size(); ++i) {
        const auto& buffer = desc.vertexBuffers[i];
        VkVertexInputBindingDescription binding{};
        binding.binding = i;
        binding.stride = buffer.stride;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        bindings.push_back(binding);
        for (const auto& attr : buffer.attributes) {
            VkVertexInputAttributeDescription attribute{};
            attribute.location = attr.location;
            attribute.binding = attr.bufferSlot;
            attribute.format = toVk(attr.format);
            attribute.offset = attr.offset;
            if (attribute.format == VK_FORMAT_UNDEFINED) {
                vkDestroyShaderModule(m_Device, vs, nullptr);
                vkDestroyShaderModule(m_Device, fs, nullptr);
                return handle;
            }
            attributes.push_back(attribute);
        }
    }

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<std::uint32_t>(bindings.size());
    vertexInput.pVertexBindingDescriptions = bindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = toVk(desc.topology);
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.depthClampEnable = VK_FALSE;
    raster.rasterizerDiscardEnable = VK_FALSE;
    raster.polygonMode = toVk(desc.fillMode);
    raster.cullMode = toVk(desc.cullMode);
    raster.frontFace = toVk(desc.frontFace);
    raster.depthBiasEnable = VK_FALSE;
    raster.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = toVk(desc.sampleCount);
    multisample.sampleShadingEnable = VK_FALSE;
    multisample.minSampleShading = 1.0f;

    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    if (desc.depthStencil.format != rhi::TextureFormat::Unknown) {
        depthFormat = toVk(desc.depthStencil.format);
        if (depthFormat == VK_FORMAT_UNDEFINED) {
            vkDestroyShaderModule(m_Device, vs, nullptr);
            vkDestroyShaderModule(m_Device, fs, nullptr);
            return handle;
        }
    }

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = desc.depthStencil.depthTestEnabled ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = desc.depthStencil.depthWriteEnabled ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = toVk(desc.depthStencil.depthFunc);
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    std::vector<VkFormat> colorFormats;
    colorFormats.reserve(desc.colorTargets.size());
    std::vector<VkPipelineColorBlendAttachmentState> attachments;
    attachments.reserve(desc.colorTargets.size());
    for (const auto& target : desc.colorTargets) {
        const VkFormat format = toVk(target.format);
        if (format == VK_FORMAT_UNDEFINED) {
            vkDestroyShaderModule(m_Device, vs, nullptr);
            vkDestroyShaderModule(m_Device, fs, nullptr);
            return handle;
        }
        colorFormats.push_back(format);

        VkPipelineColorBlendAttachmentState attachment{};
        attachment.blendEnable = target.blend.enabled ? VK_TRUE : VK_FALSE;
        attachment.srcColorBlendFactor = toVk(target.blend.srcColor);
        attachment.dstColorBlendFactor = toVk(target.blend.dstColor);
        attachment.colorBlendOp = toVk(target.blend.colorOp);
        attachment.srcAlphaBlendFactor = toVk(target.blend.srcAlpha);
        attachment.dstAlphaBlendFactor = toVk(target.blend.dstAlpha);
        attachment.alphaBlendOp = toVk(target.blend.alphaOp);
        attachment.colorWriteMask = toVk(target.blend.writeMask);
        attachments.push_back(attachment);
    }

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    colorBlend.pAttachments = attachments.data();

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;

    VkPipelineRenderingCreateInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    rendering.colorAttachmentCount = static_cast<std::uint32_t>(colorFormats.size());
    rendering.pColorAttachmentFormats = colorFormats.data();
    rendering.depthAttachmentFormat = depthFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &rendering;
    pipelineInfo.stageCount = stageCount;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewport;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamic;
    pipelineInfo.layout = m_PipelineLayouts[desc.layout.id];
    pipelineInfo.renderPass = VK_NULL_HANDLE;

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult result = vkCreateGraphicsPipelines(
        m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    vkDestroyShaderModule(m_Device, vs, nullptr);
    vkDestroyShaderModule(m_Device, fs, nullptr);

    if (result != VK_SUCCESS) {
        return handle;
    }

    handle.id = 0xFFFFFFFFFFFFFFFFull;
    handle.desc = desc;
    if (!m_FreePipelines.empty()) {
        handle.id = m_FreePipelines.back();
        m_FreePipelines.pop_back();
        m_Pipelines[handle.id] = pipeline;
    } else {
        handle.id = static_cast<std::uint64_t>(m_Pipelines.size());
        m_Pipelines.push_back(pipeline);
    }
    return handle;
}

void VulkanRHI::DestroyPipelineLayout(rhi::PipelineLayoutHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_PipelineLayouts.size() ||
        m_PipelineLayouts[handle.id] == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyPipelineLayout(m_Device, m_PipelineLayouts[handle.id], nullptr);
    m_PipelineLayouts[handle.id] = VK_NULL_HANDLE;
    m_FreePipelineLayouts.push_back(handle.id);
}

void VulkanRHI::DestroyPipeline(rhi::GraphicsPipelineHandle handle) {
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_Pipelines.size() ||
        m_Pipelines[handle.id] == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyPipeline(m_Device, m_Pipelines[handle.id], nullptr);
    m_Pipelines[handle.id] = VK_NULL_HANDLE;
    m_FreePipelines.push_back(handle.id);
}

rhi::ComputePipelineHandle VulkanRHI::CreateComputePipeline(const rhi::ComputePipelineDesc& desc)
{
    rhi::ComputePipelineHandle handle;

    if (desc.layout.id >= m_PipelineLayouts.size() ||
        m_PipelineLayouts[desc.layout.id] == VK_NULL_HANDLE) {
        return handle;
    }
    if (desc.computeShader.id >= m_ShaderModules.size()) {
        return handle;
    }

    const rhi::ShaderModule& csModule = m_ShaderModules[desc.computeShader.id];
    if (csModule.bytecode.empty() || csModule.bytecode.size() % 4 != 0) {
        return handle;
    }

    VkShaderModule cs = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = csModule.bytecode.size();
    moduleInfo.pCode = reinterpret_cast<const std::uint32_t*>(csModule.bytecode.data());
    if (vkCreateShaderModule(m_Device, &moduleInfo, nullptr, &cs) != VK_SUCCESS) {
        return handle;
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = cs;
    pipelineInfo.stage.pName = csModule.entryPoint.c_str();
    pipelineInfo.layout = m_PipelineLayouts[desc.layout.id];

    VkPipeline pipeline = VK_NULL_HANDLE;
    const VkResult result = vkCreateComputePipelines(
        m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    vkDestroyShaderModule(m_Device, cs, nullptr);

    if (result != VK_SUCCESS) {
        return handle;
    }

    handle.id = 0xFFFFFFFFFFFFFFFFull;
    handle.desc = desc;
    if (!m_FreeComputePipelines.empty()) {
        handle.id = m_FreeComputePipelines.back();
        m_FreeComputePipelines.pop_back();
        m_ComputePipelines[handle.id] = pipeline;
    } else {
        handle.id = static_cast<std::uint64_t>(m_ComputePipelines.size());
        m_ComputePipelines.push_back(pipeline);
    }
    return handle;
}

void VulkanRHI::DestroyComputePipeline(rhi::ComputePipelineHandle handle)
{
    if (!m_Device || handle.id == 0xFFFFFFFFFFFFFFFFull ||
        handle.id >= m_ComputePipelines.size() ||
        m_ComputePipelines[handle.id] == VK_NULL_HANDLE) {
        return;
    }
    vkDestroyPipeline(m_Device, m_ComputePipelines[handle.id], nullptr);
    m_ComputePipelines[handle.id] = VK_NULL_HANDLE;
    m_FreeComputePipelines.push_back(handle.id);
}
