#pragma once

#include <cstdint>

namespace rhi
{


    enum class TextureDimension
    {
        Tex1D,
        Tex2D,
        Tex3D,
    };

    enum class SampleCount
    {
        x1 = 1,
        x2 = 2,
        x4 = 4,
        x8 = 8,
        x16 = 16,
    };

   enum class TextureFormat
   {
        Unknown = 0,

        // 8-bit
        R8Unorm, R8Snorm, R8Uint, R8Sint,
        RG8Unorm, RG8Snorm, RG8Uint, RG8Sint,
        RGBA8Unorm, RGBA8UnormSrgb, RGBA8Snorm, RGBA8Uint, RGBA8Sint,
        BGRA8Unorm, BGRA8UnormSrgb,

        // 16-bit
        R16Unorm, R16Snorm, R16Uint, R16Sint, R16Float,
        RG16Unorm, RG16Snorm, RG16Uint, RG16Sint, RG16Float,
        RGBA16Unorm, RGBA16Snorm, RGBA16Uint, RGBA16Sint, RGBA16Float,

        // 32-bit
        R32Uint, R32Sint, R32Float,
        RG32Uint, RG32Sint, RG32Float,
        RGB32Uint, RGB32Sint, RGB32Float,
        RGBA32Uint, RGBA32Sint, RGBA32Float,

        // Packed
        RGB10A2Unorm,
        RGB10A2Uint,
        RG11B10Float,
        RGB9E5Float,

        // Depth / Stencil
        Depth16Unorm,
        Depth24Plus,
        Depth24PlusStencil8,
        Depth32Float,
        Depth32FloatStencil8,
        Stencil8,

        // BC
        BC1RGBAUnorm, BC1RGBAUnormSrgb,
        BC2RGBAUnorm, BC2RGBAUnormSrgb,
        BC3RGBAUnorm, BC3RGBAUnormSrgb,
        BC4RUnorm, BC4RSnorm,
        BC5RGUnorm, BC5RGSnorm,
        BC6HRGBUFloat, BC6HRGBFloat,
        BC7RGBAUnorm, BC7RGBAUnormSrgb,

        // ETC2 / EAC
        ETC2RGB8Unorm, ETC2RGB8UnormSrgb,
        ETC2RGB8A1Unorm, ETC2RGB8A1UnormSrgb,
        ETC2RGBA8Unorm, ETC2RGBA8UnormSrgb,
        EACR11Unorm, EACR11Snorm,
        EACRG11Unorm, EACRG11Snorm,

        // ASTC
        ASTC4x4Unorm, ASTC4x4UnormSrgb,
        ASTC5x4Unorm, ASTC5x4UnormSrgb,
        ASTC5x5Unorm, ASTC5x5UnormSrgb,
        ASTC6x5Unorm, ASTC6x5UnormSrgb,
        ASTC6x6Unorm, ASTC6x6UnormSrgb,
        ASTC8x5Unorm, ASTC8x5UnormSrgb,
        ASTC8x6Unorm, ASTC8x6UnormSrgb,
        ASTC8x8Unorm, ASTC8x8UnormSrgb,
        ASTC10x5Unorm, ASTC10x5UnormSrgb,
        ASTC10x6Unorm, ASTC10x6UnormSrgb,
        ASTC10x8Unorm, ASTC10x8UnormSrgb,
        ASTC10x10Unorm, ASTC10x10UnormSrgb,
        ASTC12x10Unorm, ASTC12x10UnormSrgb,
        ASTC12x12Unorm, ASTC12x12UnormSrgb,
   };

    enum class TextureUsage : std::uint32_t
    {
        None          = 0,

        Sampled       = 1 << 0,
        Storage       = 1 << 1,

        RenderTarget  = 1 << 2,
        DepthStencil  = 1 << 3,

        CopySrc       = 1 << 4,
        CopyDst       = 1 << 5,
    };

    enum class TextureAspect
    {
        Color        = 1 << 0,
        Depth        = 1 << 1,
        Stencil      = 1 << 2,
    };

    enum class TextureViewDimension
    {
        Tex1D,
        Tex2D,
        Tex3D,
        Tex1DArray,
        Tex2DArray,
        TexCube,
        TexCubeArray,
    };

    // Bitwise operators

    constexpr TextureUsage operator|(TextureUsage lhs, TextureUsage rhs)
    {
        return static_cast<TextureUsage>(
            static_cast<std::uint32_t>(lhs) |
            static_cast<std::uint32_t>(rhs));
    }

    constexpr TextureUsage operator&(TextureUsage lhs, TextureUsage rhs)
    {
        return static_cast<TextureUsage>(
            static_cast<std::uint32_t>(lhs) &
            static_cast<std::uint32_t>(rhs));
    }

    constexpr TextureUsage operator~(TextureUsage value)
    {
        return static_cast<TextureUsage>(
            ~static_cast<std::uint32_t>(value));
    }

    constexpr TextureUsage& operator|=(TextureUsage& lhs, TextureUsage rhs)
    {
        lhs = lhs | rhs;
        return lhs;
    }

    constexpr TextureUsage& operator&=(TextureUsage& lhs, TextureUsage rhs)
    {
        lhs = lhs & rhs;
        return lhs;
    }

    constexpr bool HasFlag(TextureUsage value, TextureUsage flag)
    {
        return (static_cast<std::uint32_t>(value) &
                static_cast<std::uint32_t>(flag)) != 0;
    }

    struct TextureDesc
    {
        TextureDimension dimension = TextureDimension::Tex2D;

        std::uint32_t width = 1;
        std::uint32_t height = 1;
        std::uint32_t depth = 1;

        std::uint32_t mipLevels = 1;
        std::uint32_t arrayLayers = 1;

        TextureFormat format = TextureFormat::RGBA8Unorm;

        TextureUsage usage = TextureUsage::None;

        SampleCount sampleCount = SampleCount::x1;
    };

    struct TextureWriteDesc
    {
        std::uint32_t mipLevel = 0;

        std::uint32_t x = 0;
        std::uint32_t y = 0;
        std::uint32_t z = 0;

        std::uint32_t width = 1;
        std::uint32_t height = 1;
        std::uint32_t depth = 1;

        std::uint32_t baseArrayLayer = 0;
        std::uint32_t arrayLayers = 1;

        std::uint32_t bytesPerRow = 0;
        std::uint32_t rowsPerImage = 0;
    };

    struct TextureViewDesc
    {
        TextureViewDimension dimension = TextureViewDimension::Tex2D;

        TextureFormat format = TextureFormat::RGBA8Unorm;

        TextureAspect aspect = TextureAspect::Color;

        std::uint32_t baseMipLevel = 0;
        std::uint32_t baseArrayLayer = 0;

        std::uint32_t mipLevels = 1;
        std::uint32_t arrayLayers = 1;
    };

    struct TextureCopyRegion
    {
        std::uint32_t srcMipLevel = 0;
        std::uint32_t dstMipLevel = 0;

        std::uint32_t srcX = 0;
        std::uint32_t srcY = 0;
        std::uint32_t srcZ = 0;
        std::uint32_t dstX = 0;
        std::uint32_t dstY = 0;
        std::uint32_t dstZ = 0;

        std::uint32_t width = 1;
        std::uint32_t height = 1;
        std::uint32_t depth = 1;

        std::uint32_t srcBaseArrayLayer = 0;
        std::uint32_t dstBaseArrayLayer = 0;
        std::uint32_t arrayLayers = 1;
    };

    struct TextureHandle
    {
        std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
        TextureDesc desc{};
    };

    struct TextureViewHandle
    {
        std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
        TextureViewDesc desc{};
        TextureHandle texture{};
    };
}
