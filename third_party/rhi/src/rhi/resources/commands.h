#pragma once

#include <cstdint>
#include <vector>

namespace rhi
{
    struct CommandListHandle
    {
        std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
    };
}
