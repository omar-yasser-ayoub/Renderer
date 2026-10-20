#pragma once

#include "rhi/rhi.h"
#include <array>
#include <cstddef>
#include <filesystem>
#include <vector>

struct GLFWwindow;
struct ImDrawData;

class ImGuiRenderer
{
public:
    // Call once after swapchain creation, before the frame loop.
    bool Init(RHI& rhi, GLFWwindow* window, const std::filesystem::path& shaderDir,
              rhi::TextureFormat swapchainFormat, std::uint32_t frameWidth,
              std::uint32_t frameHeight, const rhi::SwapchainHandle& swapchain);

    void NewFrame(double deltaTimeSeconds, std::uint32_t frameWidth,
                  std::uint32_t frameHeight);

    void SetCanvasSize(std::uint32_t width, std::uint32_t height);

    void Render(RHI& rhi, rhi::CommandListHandle cmd, std::uint32_t swapchainImageIndex);

    void Shutdown(RHI& rhi);

private:
    static constexpr std::size_t kRingSize = 3;
    static constexpr std::size_t kVertexStride = 20;

    RHI* m_Rhi = nullptr;
    GLFWwindow* m_Window = nullptr;
    rhi::SwapchainHandle m_Swapchain{};

    rhi::ShaderModuleHandle m_VertexShader, m_FragmentShader;
    rhi::PipelineLayoutHandle m_PipelineLayout;
    rhi::GraphicsPipelineHandle m_Pipeline;
    rhi::BindGroupLayoutHandle m_TransformLayout, m_FontLayout;

    rhi::BufferHandle m_TransformBuffer{};
    rhi::BindGroupHandle m_TransformBindGroup{};

    rhi::TextureHandle m_FontTexture{};
    rhi::TextureViewHandle m_FontView{};
    rhi::SamplerHandle m_FontSampler{};
    rhi::BindGroupHandle m_FontBindGroup{};

    std::array<rhi::BufferHandle, kRingSize> m_VertexBuffers{};
    std::array<rhi::BufferHandle, kRingSize> m_IndexBuffers{};
    std::size_t m_VertexCapacity = 0; // vertices
    std::size_t m_IndexCapacity = 0;  // indices
    std::size_t m_RingSlot = 0;

    std::uint32_t m_FrameWidth = 0, m_FrameHeight = 0;
    std::uint32_t m_CanvasWidth = 0, m_CanvasHeight = 0;

    std::vector<std::size_t> m_ListVertexStarts;
    std::vector<std::size_t> m_ListIndexStarts; // in indices
    std::size_t m_VertexUploadBytes = 0;
    std::size_t m_IndexUploadBytes = 0;         // 4-byte padded

    std::vector<std::uint8_t> m_VertexScratch, m_IndexScratch;

    bool UploadBuffers(RHI& rhi, const ImDrawData* drawData);
};
