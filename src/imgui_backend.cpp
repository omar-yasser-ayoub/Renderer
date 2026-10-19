#include "imgui_backend.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "rhi/resources/bindgroup.h"
#include "rhi/resources/buffer.h"
#include "rhi/resources/pipeline.h"
#include "rhi/resources/renderpass.h"
#include "rhi/resources/sampler.h"
#include "rhi/resources/shader.h"
#include "rhi/resources/swapchain.h"
#include "rhi/resources/texture.h"

#include <algorithm>
#include <cstring>
#include <iostream>

struct ImGuiTransformUBO
{
    float scale[2];
    float offset[2];
};

bool ImGuiRenderer::Init(RHI& rhi, GLFWwindow* window,
                         const std::filesystem::path& shaderDir,
                         rhi::TextureFormat swapchainFormat,
                         std::uint32_t frameWidth, std::uint32_t frameHeight,
                         const rhi::SwapchainHandle& swapchain)
{
    m_Rhi = &rhi;
    m_Swapchain = swapchain;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGui::GetIO().DisplaySize = ImVec2(
        static_cast<float>(frameWidth), static_cast<float>(frameHeight));

    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        std::cerr << "[ImGuiRenderer] Failed to init GLFW platform backend\n";
        Shutdown(rhi);
        return false;
    }

    unsigned char* pixels = nullptr;
    int atlasWidth = 0, atlasHeight = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &atlasWidth, &atlasHeight);

    rhi::TextureDesc fontDesc;
    fontDesc.width = static_cast<std::uint32_t>(atlasWidth);
    fontDesc.height = static_cast<std::uint32_t>(atlasHeight);
    fontDesc.format = rhi::TextureFormat::RGBA8Unorm;
    fontDesc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst;
    m_FontTexture = rhi.CreateTexture(fontDesc);
    rhi.UploadTexture(m_FontTexture, 0, pixels, fontDesc.width, fontDesc.height);
    ImGui::GetIO().Fonts->SetTexID(
        static_cast<ImTextureID>(static_cast<std::uintptr_t>(m_FontTexture.id)));

    rhi::TextureViewDesc fontViewDesc;
    fontViewDesc.format = fontDesc.format;
    m_FontView = rhi.CreateTextureView(fontViewDesc, m_FontTexture);

    m_FontSampler = rhi.CreateSampler({
        .addressU = rhi::AddressMode::ClampToEdge,
        .addressV = rhi::AddressMode::ClampToEdge,
    });

    rhi::ShaderSource vsSource;
    vsSource.path = shaderDir / "imgui.vert.hlsl";
    vsSource.stage = rhi::ShaderStageFlags::Vertex;
    vsSource.entryPoint = "main";
    m_VertexShader = rhi.CompileShader(vsSource);

    rhi::ShaderSource fsSource;
    fsSource.path = shaderDir / "imgui.frag.hlsl";
    fsSource.stage = rhi::ShaderStageFlags::Fragment;
    fsSource.entryPoint = "main";
    m_FragmentShader = rhi.CompileShader(fsSource);

    const auto vsModule = rhi.GetShaderModule(m_VertexShader);
    const auto fsModule = rhi.GetShaderModule(m_FragmentShader);
    if (!vsModule || !fsModule) {
        std::cerr << "[ImGuiRenderer] Failed to compile imgui shaders\n";
        Shutdown(rhi);
        return false;
    }

    std::vector<rhi::ShaderReflection> reflections;
    reflections.push_back(vsModule->reflection);
    reflections.push_back(fsModule->reflection);

    auto layoutDescs = rhi::BuildBindGroupLayoutDescs(reflections);
    std::sort(layoutDescs.begin(), layoutDescs.end(),
        [](const rhi::BindGroupLayoutDesc& a, const rhi::BindGroupLayoutDesc& b) {
            return a.shaderBindings[0].set < b.shaderBindings[0].set;
        });

    rhi::PipelineLayoutDesc pipelineLayoutDesc;
    for (const auto& desc : layoutDescs) {
        rhi::BindGroupLayoutHandle layout = rhi.CreateBindGroupLayout(desc);
        pipelineLayoutDesc.layouts.push_back(layout);
        if (desc.shaderBindings[0].set == 0) {
            m_TransformLayout = layout;
        } else if (desc.shaderBindings[0].set == 1) {
            m_FontLayout = layout;
        }
    }

    if (m_TransformLayout.id == 0xFFFFFFFFFFFFFFFFull
        || m_FontLayout.id == 0xFFFFFFFFFFFFFFFFull) {
        std::cerr << "[ImGuiRenderer] Unexpected imgui shader layout\n";
        Shutdown(rhi);
        return false;
    }

    m_PipelineLayout = rhi.CreatePipelineLayout(pipelineLayoutDesc);

    rhi::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.layout = m_PipelineLayout;
    pipelineDesc.vertexShader = m_VertexShader;
    pipelineDesc.fragmentShader = m_FragmentShader;
    pipelineDesc.vertexBuffers.push_back({
        .stride = kVertexStride,
        .attributes = {
            {.bufferSlot = 0, .location = 0, .format = rhi::VertexFormat::Float32x2, .offset = 0},  // pos
            {.bufferSlot = 0, .location = 1, .format = rhi::VertexFormat::Float32x2, .offset = 8},  // uv
            {.bufferSlot = 0, .location = 2, .format = rhi::VertexFormat::UInt32,     .offset = 16} // col
        },
    });
    rhi::ColorTarget colorTarget;
    colorTarget.format = swapchainFormat;
    colorTarget.blend.enabled = true;
    pipelineDesc.colorTargets.push_back(colorTarget);

    m_Pipeline = rhi.CreateGraphicsPipeline(pipelineDesc);
    if (m_Pipeline.id == 0xFFFFFFFFFFFFFFFFull) {
        std::cerr << "[ImGuiRenderer] Failed to create imgui pipeline\n";
        Shutdown(rhi);
        return false;
    }

    rhi::BufferDesc transformDesc;
    transformDesc.size = sizeof(ImGuiTransformUBO);
    transformDesc.usage = rhi::BufferUsage::Uniform;
    transformDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
    m_TransformBuffer = rhi.CreateBuffer(transformDesc);
    ImGuiTransformUBO transform;
    transform.scale[0] = 2.0f / frameWidth;
    transform.scale[1] = -2.0f / frameHeight;
    transform.offset[0] = -1.0f;
    transform.offset[1] = 1.0f;
    rhi.WriteBuffer(m_TransformBuffer, &transform, sizeof(ImGuiTransformUBO));

    rhi::BindGroupDesc transformBGDesc;
    rhi::BindGroupEntry transformEntry;
    transformEntry.binding = 0;
    transformEntry.buffer = m_TransformBuffer;
    transformBGDesc.entries.push_back(transformEntry);
    transformBGDesc.layout = m_TransformLayout;
    m_TransformBindGroup = rhi.CreateBindGroup(transformBGDesc);

    rhi::BindGroupDesc fontBGDesc;
    rhi::BindGroupEntry fontTextureEntry;
    fontTextureEntry.binding = 0;
    fontTextureEntry.textureView = m_FontView;
    fontBGDesc.entries.push_back(fontTextureEntry);
    rhi::BindGroupEntry fontSamplerEntry;
    fontSamplerEntry.binding = 1;
    fontSamplerEntry.sampler = m_FontSampler;
    fontBGDesc.entries.push_back(fontSamplerEntry);
    fontBGDesc.layout = m_FontLayout;
    m_FontBindGroup = rhi.CreateBindGroup(fontBGDesc);

    m_FrameWidth = frameWidth;
    m_FrameHeight = frameHeight;
    return true;
}

void ImGuiRenderer::NewFrame(double deltaTimeSeconds, std::uint32_t frameWidth,
                              std::uint32_t frameHeight)
{
    ImGui::GetIO().DeltaTime =
        std::max(static_cast<float>(deltaTimeSeconds), 1.0f / 2000.0f);
    ImGui_ImplGlfw_NewFrame();
    ImGui::GetIO().DisplaySize = ImVec2(
        static_cast<float>(frameWidth), static_cast<float>(frameHeight));
    ImGui::GetIO().DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    ImGui::NewFrame();
}

bool ImGuiRenderer::UploadBuffers(RHI& rhi, const ImDrawData* drawData)
{
    const std::size_t vtxCount = static_cast<std::size_t>(drawData->TotalVtxCount);
    const std::size_t idxCount = static_cast<std::size_t>(drawData->TotalIdxCount);

    if (m_VertexCapacity < vtxCount || m_IndexCapacity < idxCount) {
        m_VertexCapacity = std::max<std::size_t>(
            vtxCount, m_VertexCapacity ? m_VertexCapacity * 2 : 8192);
        m_IndexCapacity = std::max<std::size_t>(
            idxCount, m_IndexCapacity ? m_IndexCapacity * 2 : 16384);

        rhi::BufferDesc vbDesc;
        vbDesc.size = m_VertexCapacity * kVertexStride;
        vbDesc.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst;
        vbDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
        rhi::BufferDesc ibDesc;
        ibDesc.size = m_IndexCapacity * sizeof(ImDrawIdx);
        ibDesc.usage = rhi::BufferUsage::Index | rhi::BufferUsage::CopyDst;
        ibDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;

        for (std::size_t i = 0; i < kRingSize; ++i) {
            if (m_VertexBuffers[i].id != 0xFFFFFFFFFFFFFFFFull) {
                rhi.DestroyBuffer(m_VertexBuffers[i]);
            }
            if (m_IndexBuffers[i].id != 0xFFFFFFFFFFFFFFFFull) {
                rhi.DestroyBuffer(m_IndexBuffers[i]);
            }
            m_VertexBuffers[i] = rhi.CreateBuffer(vbDesc);
            m_IndexBuffers[i] = rhi.CreateBuffer(ibDesc);
        }
    }

    rhi::BufferHandle vb = m_VertexBuffers[m_RingSlot];
    rhi::BufferHandle ib = m_IndexBuffers[m_RingSlot];

    m_VertexScratch.resize(vtxCount * kVertexStride);
    m_IndexScratch.resize(idxCount * sizeof(ImDrawIdx));
    std::size_t vtxByte = 0, idxByte = 0;
    for (int l = 0; l < drawData->CmdListsCount; ++l) {
        const ImDrawList* cmdList = drawData->CmdLists[l];
        std::memcpy(m_VertexScratch.data() + vtxByte, cmdList->VtxBuffer.Data,
            static_cast<std::size_t>(cmdList->VtxBuffer.Size) * kVertexStride);
        std::memcpy(m_IndexScratch.data() + idxByte, cmdList->IdxBuffer.Data,
            static_cast<std::size_t>(cmdList->IdxBuffer.Size) * sizeof(ImDrawIdx));
        vtxByte += static_cast<std::size_t>(cmdList->VtxBuffer.Size) * kVertexStride;
        idxByte += static_cast<std::size_t>(cmdList->IdxBuffer.Size) * sizeof(ImDrawIdx);
    }

    rhi.WriteBuffer(vb, m_VertexScratch.data(), m_VertexScratch.size());
    rhi.WriteBuffer(ib, m_IndexScratch.data(), m_IndexScratch.size());
    return true;
}

void ImGuiRenderer::Render(RHI& rhi, rhi::CommandListHandle cmd,
                           std::uint32_t swapchainImageIndex)
{
    ImGui::Render();
    const ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || drawData->TotalVtxCount == 0 || drawData->TotalIdxCount == 0) {
        return;
    }

    if (!UploadBuffers(rhi, drawData)) {
        std::cerr << "[ImGuiRenderer] buffer upload failed\n";
        return;
    }

    rhi::BufferHandle vb = m_VertexBuffers[m_RingSlot];
    rhi::BufferHandle ib = m_IndexBuffers[m_RingSlot];
    m_RingSlot = (m_RingSlot + 1) % kRingSize;

    // UI pass: same swapchain image the scene pass just wrote (LOAD, not
    // CLEAR), no depth attachment.
    rhi::RenderPassDesc pass;
    pass.swapchain = m_Swapchain;
    pass.imageIndex = swapchainImageIndex;
    rhi::RenderPassColorAttachment color;
    color.useSwapchain = true;
    color.clear = false;
    pass.colors.push_back(color);
    rhi.BeginRenderPass(cmd, pass);

    rhi.SetPipeline(cmd, m_Pipeline);
    rhi.SetBindGroup(cmd, 0, m_TransformBindGroup);
    rhi.SetBindGroup(cmd, 1, m_FontBindGroup);
    rhi.SetViewport(cmd, 0, 0, drawData->DisplaySize.x, drawData->DisplaySize.y);
    rhi.SetScissor(cmd, 0, 0, m_FrameWidth, m_FrameHeight);

    std::size_t listVtxOffset = 0, listIdxOffset = 0;
    for (int l = 0; l < drawData->CmdListsCount; ++l) {
        const ImDrawList* cmdList = drawData->CmdLists[l];
        rhi.SetVertexBuffer(cmd, 0, vb, listVtxOffset * kVertexStride);
        rhi.SetIndexBuffer(cmd, ib, rhi::IndexFormat::Uint16,
            listIdxOffset * sizeof(ImDrawIdx));

        for (int c = 0; c < cmdList->CmdBuffer.Size; ++c) {
            const ImDrawCmd& drawCmd = cmdList->CmdBuffer[c];
            if (drawCmd.UserCallback) {
                // No render-state callbacks on this backend.
                continue;
            }
            const std::int32_t clipX =
                std::max(static_cast<std::int32_t>(drawCmd.ClipRect.x), 0);
            const std::int32_t clipY =
                std::max(static_cast<std::int32_t>(drawCmd.ClipRect.y), 0);
            const std::int32_t clipW =
                std::min(static_cast<std::int32_t>(drawCmd.ClipRect.z),
                    static_cast<std::int32_t>(m_FrameWidth)) - clipX;
            const std::int32_t clipH =
                std::min(static_cast<std::int32_t>(drawCmd.ClipRect.w),
                    static_cast<std::int32_t>(m_FrameHeight)) - clipY;
            if (clipW <= 0 || clipH <= 0) {
                continue;
            }
            rhi.SetScissor(cmd, static_cast<std::uint32_t>(clipX),
                static_cast<std::uint32_t>(clipY),
                static_cast<std::uint32_t>(clipW),
                static_cast<std::uint32_t>(clipH));
            rhi.DrawIndexed(cmd, drawCmd.ElemCount, 1,
                static_cast<std::uint32_t>(drawCmd.IdxOffset - listIdxOffset),
                static_cast<std::int32_t>(drawCmd.VtxOffset - listVtxOffset), 0);
        }

        listVtxOffset += static_cast<std::size_t>(cmdList->VtxBuffer.Size);
        listIdxOffset += static_cast<std::size_t>(cmdList->IdxBuffer.Size);
    }

    rhi.EndPass(cmd);
}

void ImGuiRenderer::Shutdown(RHI& rhi)
{
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    for (std::size_t i = 0; i < kRingSize; ++i) {
        if (m_VertexBuffers[i].id != 0xFFFFFFFFFFFFFFFFull) {
            rhi.DestroyBuffer(m_VertexBuffers[i]);
            m_VertexBuffers[i] = rhi::BufferHandle{};
        }
        if (m_IndexBuffers[i].id != 0xFFFFFFFFFFFFFFFFull) {
            rhi.DestroyBuffer(m_IndexBuffers[i]);
            m_IndexBuffers[i] = rhi::BufferHandle{};
        }
    }
    m_VertexCapacity = m_IndexCapacity = 0;
    m_VertexScratch.clear();
    m_VertexScratch.shrink_to_fit();
    m_IndexScratch.clear();
    m_IndexScratch.shrink_to_fit();

    if (m_FontView.id != 0xFFFFFFFFFFFFFFFFull) {
        rhi.DestroyTextureView(m_FontView);
        m_FontView = rhi::TextureViewHandle{};
    }
    if (m_FontSampler.id != 0xFFFFFFFFFFFFFFFFull) {
        rhi.DestroySampler(m_FontSampler);
        m_FontSampler = rhi::SamplerHandle{};
    }
    if (m_FontTexture.id != 0xFFFFFFFFFFFFFFFFull) {
        rhi.DestroyTexture(m_FontTexture);
        m_FontTexture = rhi::TextureHandle{};
    }
    if (m_TransformBuffer.id != 0xFFFFFFFFFFFFFFFFull) {
        rhi.DestroyBuffer(m_TransformBuffer);
        m_TransformBuffer = rhi::BufferHandle{};
    }
    m_TransformBindGroup = rhi::BindGroupHandle{};
    m_FontBindGroup = rhi::BindGroupHandle{};
    m_Pipeline = rhi::GraphicsPipelineHandle{};
    m_PipelineLayout = rhi::PipelineLayoutHandle{};
    m_TransformLayout = rhi::BindGroupLayoutHandle{};
    m_FontLayout = rhi::BindGroupLayoutHandle{};
    m_Rhi = nullptr;
}
