#pragma once

#include "rhi/resources/texture.h"
#include "rhi/resources/surface.h"

#include <cstdint>

namespace rhi
{
    enum class PresentMode
    {
        Fifo,
        FifoRelaxed,
        Mailbox,
        Immediate,
    };

    enum class AlphaMode
    {
        Opaque,
        Premultiplied,
    };

    struct SwapchainDesc
    {
        SurfaceHandle surface{};

        std::uint32_t width = 0;
        std::uint32_t height = 0;

        TextureFormat format = TextureFormat::BGRA8Unorm;
        PresentMode presentMode = PresentMode::Fifo;

        std::uint32_t imageCount = 3;
        std::uint32_t maxFramesInFlight = 3;

        TextureUsage usage = TextureUsage::RenderTarget;
        AlphaMode alphaMode = AlphaMode::Opaque;
    };

    struct SwapchainHandle
    {
        std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
        SwapchainDesc desc{};
    };

    enum class AcquireStatus
    {
        Success,
        Suboptimal,
        OutOfDate,
        Timeout,
        Failed,
    };

    struct AcquireResult
    {
        AcquireStatus status = AcquireStatus::Failed;
        std::uint32_t imageIndex = 0xFFFFFFFF;
    };
}
