#pragma once
#include <cstdint>

namespace rhi
{
    enum class Filter { Nearest, Linear };
    enum class MipmapFilter { Nearest, Linear };
    enum class AddressMode { Repeat, MirrorRepeat, ClampToEdge, ClampToBorder, MirrorClampToEdge };
    enum class BorderColor { TransparentBlack, OpaqueBlack, OpaqueWhite };
    enum class CompareOp { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };

    struct SamplerDesc
    {
        Filter minFilter = Filter::Linear;
        Filter magFilter = Filter::Linear;
        MipmapFilter mipFilter = MipmapFilter::Linear;
        AddressMode addressU = AddressMode::Repeat;
        AddressMode addressV = AddressMode::Repeat;
        AddressMode addressW = AddressMode::Repeat;
        BorderColor borderColor = BorderColor::TransparentBlack;
        float minLOD = 0.0f;
        float maxLOD = 32.0f;
        float mipLodBias = 0.0f;
        bool anisotropyEnabled = false;
        std::uint32_t maxAnisotropy = 1;
        bool compareEnabled = false;
        CompareOp compareOp = CompareOp::Less;
    };

    struct SamplerHandle
    {
        std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
        SamplerDesc desc{};
    };
}
