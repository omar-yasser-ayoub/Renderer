#pragma once

#include "rhi/resources/buffer.h"
#include "rhi/resources/swapchain.h"
#include "rhi/resources/texture.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rhi
{
    enum class IndexFormat
    {
        Uint16,
        Uint32,
    };

    struct RenderPassColorAttachment
    {
        TextureViewHandle view{};
        bool useSwapchain = false;
        float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        bool clear = true;
    };

    struct RenderPassDepthAttachment
    {
        bool enabled = false;
        TextureViewHandle view{};
        bool clearDepth = true;
        float clearDepthValue = 1.0f;
        bool clearStencil = false;
        std::uint32_t clearStencilValue = 0;
    };

    struct RenderPassDesc
    {
        SwapchainHandle swapchain{};
        std::uint32_t imageIndex = 0xFFFFFFFF;
        std::vector<RenderPassColorAttachment> colors;
        RenderPassDepthAttachment depth;
    };
}
