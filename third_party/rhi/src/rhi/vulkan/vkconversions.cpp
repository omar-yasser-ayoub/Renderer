#include "rhi/vulkan/vkrhi.h"
#include "third_party/spirv-reflect/spirv_reflect.h"

#include <vector>

VkFilter VulkanRHI::toVk(rhi::Filter mode){
    switch (mode) {
        case rhi::Filter::Nearest: return VK_FILTER_NEAREST;
        case rhi::Filter::Linear: return VK_FILTER_LINEAR;
    }
    return VK_FILTER_NEAREST;
}

VkSamplerMipmapMode VulkanRHI::toVk(rhi::MipmapFilter mode){
    switch (mode) {
        case rhi::MipmapFilter::Nearest: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case rhi::MipmapFilter::Linear: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
}

VkSamplerAddressMode VulkanRHI::toVk(rhi::AddressMode mode){
    switch (mode) {
        case rhi::AddressMode::Repeat: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case rhi::AddressMode::MirrorRepeat: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case rhi::AddressMode::ClampToEdge: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case rhi::AddressMode::ClampToBorder: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        case rhi::AddressMode::MirrorClampToEdge: return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    }
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

VkCompareOp VulkanRHI::toVk(rhi::CompareOp mode){
    switch (mode) {
        case rhi::CompareOp::Never: return VK_COMPARE_OP_NEVER;
        case rhi::CompareOp::Less: return VK_COMPARE_OP_LESS;
        case rhi::CompareOp::Equal: return VK_COMPARE_OP_EQUAL;
        case rhi::CompareOp::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
        case rhi::CompareOp::Greater: return VK_COMPARE_OP_GREATER;
        case rhi::CompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
        case rhi::CompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
        case rhi::CompareOp::Always: return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_NEVER;
}

VkBorderColor VulkanRHI::toVk(rhi::BorderColor mode){
    switch (mode) {
        case rhi::BorderColor::TransparentBlack: return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        case rhi::BorderColor::OpaqueBlack: return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        case rhi::BorderColor::OpaqueWhite: return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    }
    return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
}

VkBufferUsageFlags VulkanRHI::toVk(rhi::BufferUsage usage) {
    VkBufferUsageFlags flags = 0;
    if (rhi::HasFlag(usage, rhi::BufferUsage::Vertex)) flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (rhi::HasFlag(usage, rhi::BufferUsage::Index)) flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (rhi::HasFlag(usage, rhi::BufferUsage::Uniform)) flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (rhi::HasFlag(usage, rhi::BufferUsage::Storage)) flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (rhi::HasFlag(usage, rhi::BufferUsage::Indirect)) flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (rhi::HasFlag(usage, rhi::BufferUsage::CopySrc)) flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (rhi::HasFlag(usage, rhi::BufferUsage::CopyDst)) flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    return flags;
}

VkMemoryPropertyFlags VulkanRHI::toVk(rhi::MemoryUsage usage)
{
    switch (usage)
    {
        case rhi::MemoryUsage::GpuOnly:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case rhi::MemoryUsage::CpuToGpu:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case rhi::MemoryUsage::GpuToCpu:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                 | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        default:
            return 0;
    }
}

VkImageViewType VulkanRHI::toVk(rhi::TextureViewDimension type, int layerCount) {
    switch (type)
    {
        case rhi::TextureViewDimension::Tex1D:
            return layerCount > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
        case rhi::TextureViewDimension::Tex2D:
            return layerCount > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        case rhi::TextureViewDimension::Tex3D:
            return VK_IMAGE_VIEW_TYPE_3D;
        case rhi::TextureViewDimension::Tex1DArray:
            return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        case rhi::TextureViewDimension::Tex2DArray:
            return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        case rhi::TextureViewDimension::TexCube:
            return layerCount > 6 ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
        case rhi::TextureViewDimension::TexCubeArray:
            return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
        default:
            return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    }
}

VkFormat VulkanRHI::toVk(rhi::TextureFormat format) {
    switch (format) {
        case rhi::TextureFormat::Unknown:              return VK_FORMAT_UNDEFINED;

        // 8-bit
        case rhi::TextureFormat::R8Unorm:               return VK_FORMAT_R8_UNORM;
        case rhi::TextureFormat::R8Snorm:               return VK_FORMAT_R8_SNORM;
        case rhi::TextureFormat::R8Uint:                return VK_FORMAT_R8_UINT;
        case rhi::TextureFormat::R8Sint:                return VK_FORMAT_R8_SINT;
        case rhi::TextureFormat::RG8Unorm:              return VK_FORMAT_R8G8_UNORM;
        case rhi::TextureFormat::RG8Snorm:              return VK_FORMAT_R8G8_SNORM;
        case rhi::TextureFormat::RG8Uint:               return VK_FORMAT_R8G8_UINT;
        case rhi::TextureFormat::RG8Sint:               return VK_FORMAT_R8G8_SINT;
        case rhi::TextureFormat::RGBA8Unorm:            return VK_FORMAT_R8G8B8A8_UNORM;
        case rhi::TextureFormat::RGBA8UnormSrgb:        return VK_FORMAT_R8G8B8A8_SRGB;
        case rhi::TextureFormat::RGBA8Snorm:            return VK_FORMAT_R8G8B8A8_SNORM;
        case rhi::TextureFormat::RGBA8Uint:             return VK_FORMAT_R8G8B8A8_UINT;
        case rhi::TextureFormat::RGBA8Sint:             return VK_FORMAT_R8G8B8A8_SINT;
        case rhi::TextureFormat::BGRA8Unorm:            return VK_FORMAT_B8G8R8A8_UNORM;
        case rhi::TextureFormat::BGRA8UnormSrgb:        return VK_FORMAT_B8G8R8A8_SRGB;

        // 16-bit
        case rhi::TextureFormat::R16Unorm:              return VK_FORMAT_R16_UNORM;
        case rhi::TextureFormat::R16Snorm:              return VK_FORMAT_R16_SNORM;
        case rhi::TextureFormat::R16Uint:               return VK_FORMAT_R16_UINT;
        case rhi::TextureFormat::R16Sint:               return VK_FORMAT_R16_SINT;
        case rhi::TextureFormat::R16Float:              return VK_FORMAT_R16_SFLOAT;
        case rhi::TextureFormat::RG16Unorm:             return VK_FORMAT_R16G16_UNORM;
        case rhi::TextureFormat::RG16Snorm:             return VK_FORMAT_R16G16_SNORM;
        case rhi::TextureFormat::RG16Uint:              return VK_FORMAT_R16G16_UINT;
        case rhi::TextureFormat::RG16Sint:              return VK_FORMAT_R16G16_SINT;
        case rhi::TextureFormat::RG16Float:             return VK_FORMAT_R16G16_SFLOAT;
        case rhi::TextureFormat::RGBA16Unorm:           return VK_FORMAT_R16G16B16A16_UNORM;
        case rhi::TextureFormat::RGBA16Snorm:           return VK_FORMAT_R16G16B16A16_SNORM;
        case rhi::TextureFormat::RGBA16Uint:            return VK_FORMAT_R16G16B16A16_UINT;
        case rhi::TextureFormat::RGBA16Sint:            return VK_FORMAT_R16G16B16A16_SINT;
        case rhi::TextureFormat::RGBA16Float:           return VK_FORMAT_R16G16B16A16_SFLOAT;

        // 32-bit
        case rhi::TextureFormat::R32Uint:               return VK_FORMAT_R32_UINT;
        case rhi::TextureFormat::R32Sint:               return VK_FORMAT_R32_SINT;
        case rhi::TextureFormat::R32Float:              return VK_FORMAT_R32_SFLOAT;
        case rhi::TextureFormat::RG32Uint:              return VK_FORMAT_R32G32_UINT;
        case rhi::TextureFormat::RG32Sint:              return VK_FORMAT_R32G32_SINT;
        case rhi::TextureFormat::RG32Float:             return VK_FORMAT_R32G32_SFLOAT;
        case rhi::TextureFormat::RGB32Uint:             return VK_FORMAT_R32G32B32_UINT;
        case rhi::TextureFormat::RGB32Sint:             return VK_FORMAT_R32G32B32_SINT;
        case rhi::TextureFormat::RGB32Float:            return VK_FORMAT_R32G32B32_SFLOAT;
        case rhi::TextureFormat::RGBA32Uint:            return VK_FORMAT_R32G32B32A32_UINT;
        case rhi::TextureFormat::RGBA32Sint:            return VK_FORMAT_R32G32B32A32_SINT;
        case rhi::TextureFormat::RGBA32Float:           return VK_FORMAT_R32G32B32A32_SFLOAT;

        // Packed
        case rhi::TextureFormat::RGB10A2Unorm:          return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case rhi::TextureFormat::RGB10A2Uint:           return VK_FORMAT_A2B10G10R10_UINT_PACK32;
        case rhi::TextureFormat::RG11B10Float:          return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case rhi::TextureFormat::RGB9E5Float:           return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;

        // Depth / Stencil
        case rhi::TextureFormat::Depth16Unorm:          return VK_FORMAT_D16_UNORM;
        case rhi::TextureFormat::Depth24Plus:           return VK_FORMAT_X8_D24_UNORM_PACK32;
        case rhi::TextureFormat::Depth24PlusStencil8:   return VK_FORMAT_D24_UNORM_S8_UINT;
        case rhi::TextureFormat::Depth32Float:          return VK_FORMAT_D32_SFLOAT;
        case rhi::TextureFormat::Depth32FloatStencil8:  return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case rhi::TextureFormat::Stencil8:              return VK_FORMAT_S8_UINT;

        // BC
        case rhi::TextureFormat::BC1RGBAUnorm:          return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case rhi::TextureFormat::BC1RGBAUnormSrgb:      return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case rhi::TextureFormat::BC2RGBAUnorm:          return VK_FORMAT_BC2_UNORM_BLOCK;
        case rhi::TextureFormat::BC2RGBAUnormSrgb:      return VK_FORMAT_BC2_SRGB_BLOCK;
        case rhi::TextureFormat::BC3RGBAUnorm:          return VK_FORMAT_BC3_UNORM_BLOCK;
        case rhi::TextureFormat::BC3RGBAUnormSrgb:      return VK_FORMAT_BC3_SRGB_BLOCK;
        case rhi::TextureFormat::BC4RUnorm:             return VK_FORMAT_BC4_UNORM_BLOCK;
        case rhi::TextureFormat::BC4RSnorm:             return VK_FORMAT_BC4_SNORM_BLOCK;
        case rhi::TextureFormat::BC5RGUnorm:            return VK_FORMAT_BC5_UNORM_BLOCK;
        case rhi::TextureFormat::BC5RGSnorm:            return VK_FORMAT_BC5_SNORM_BLOCK;
        case rhi::TextureFormat::BC6HRGBUFloat:         return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case rhi::TextureFormat::BC6HRGBFloat:          return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case rhi::TextureFormat::BC7RGBAUnorm:          return VK_FORMAT_BC7_UNORM_BLOCK;
        case rhi::TextureFormat::BC7RGBAUnormSrgb:      return VK_FORMAT_BC7_SRGB_BLOCK;

        // ETC2 / EAC
        case rhi::TextureFormat::ETC2RGB8Unorm:         return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        case rhi::TextureFormat::ETC2RGB8UnormSrgb:     return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
        case rhi::TextureFormat::ETC2RGB8A1Unorm:       return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
        case rhi::TextureFormat::ETC2RGB8A1UnormSrgb:   return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
        case rhi::TextureFormat::ETC2RGBA8Unorm:        return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
        case rhi::TextureFormat::ETC2RGBA8UnormSrgb:    return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
        case rhi::TextureFormat::EACR11Unorm:           return VK_FORMAT_EAC_R11_UNORM_BLOCK;
        case rhi::TextureFormat::EACR11Snorm:           return VK_FORMAT_EAC_R11_SNORM_BLOCK;
        case rhi::TextureFormat::EACRG11Unorm:          return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
        case rhi::TextureFormat::EACRG11Snorm:          return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;

        // ASTC
        case rhi::TextureFormat::ASTC4x4Unorm:          return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC4x4UnormSrgb:      return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC5x4Unorm:          return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC5x4UnormSrgb:      return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC5x5Unorm:          return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC5x5UnormSrgb:      return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC6x5Unorm:          return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC6x5UnormSrgb:      return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC6x6Unorm:          return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC6x6UnormSrgb:      return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC8x5Unorm:          return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC8x5UnormSrgb:      return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC8x6Unorm:          return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC8x6UnormSrgb:      return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC8x8Unorm:          return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC8x8UnormSrgb:      return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC10x5Unorm:         return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC10x5UnormSrgb:     return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC10x6Unorm:         return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC10x6UnormSrgb:     return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC10x8Unorm:         return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC10x8UnormSrgb:     return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC10x10Unorm:        return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC10x10UnormSrgb:    return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC12x10Unorm:        return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC12x10UnormSrgb:    return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
        case rhi::TextureFormat::ASTC12x12Unorm:        return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
        case rhi::TextureFormat::ASTC12x12UnormSrgb:    return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
        default:                                   return VK_FORMAT_UNDEFINED;
    }
}

VkImageAspectFlags VulkanRHI::toVk(rhi::TextureAspect aspect) {
    VkImageAspectFlags flags = 0;

    if (static_cast<uint32_t>(aspect) & static_cast<uint32_t>(rhi::TextureAspect::Color))
        flags |= VK_IMAGE_ASPECT_COLOR_BIT;
    if (static_cast<uint32_t>(aspect) & static_cast<uint32_t>(rhi::TextureAspect::Depth))
        flags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    if (static_cast<uint32_t>(aspect) & static_cast<uint32_t>(rhi::TextureAspect::Stencil))
        flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

    return flags;
}

VkImageAspectFlags VulkanRHI::toVkAspect(rhi::TextureFormat format) {
    using rhi::TextureFormat;
    switch (format) {
    case TextureFormat::Depth16Unorm:
    case TextureFormat::Depth24Plus:
    case TextureFormat::Depth32Float:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case TextureFormat::Depth24PlusStencil8:
    case TextureFormat::Depth32FloatStencil8:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    case TextureFormat::Stencil8:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    default:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

VkAccessFlags VulkanRHI::accessForLayout(VkImageLayout layout)
{
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
        return 0;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
               VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        return VK_ACCESS_MEMORY_READ_BIT;
    default:
        return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    }
}

VkImageType VulkanRHI::toVk(rhi::TextureDimension type) {
    switch (type) {
        case rhi::TextureDimension::Tex1D:              return VK_IMAGE_TYPE_1D;
        case rhi::TextureDimension::Tex2D:              return VK_IMAGE_TYPE_2D;
        case rhi::TextureDimension::Tex3D:              return VK_IMAGE_TYPE_3D;
        default:                                   return VK_IMAGE_TYPE_MAX_ENUM;
    }
}

VkSampleCountFlagBits VulkanRHI::toVk(rhi::SampleCount count) {
    switch (count) {
        case rhi::SampleCount::x1:  return VK_SAMPLE_COUNT_1_BIT;
        case rhi::SampleCount::x2:  return VK_SAMPLE_COUNT_2_BIT;
        case rhi::SampleCount::x4:  return VK_SAMPLE_COUNT_4_BIT;
        case rhi::SampleCount::x8:  return VK_SAMPLE_COUNT_8_BIT;
        case rhi::SampleCount::x16: return VK_SAMPLE_COUNT_16_BIT;
        default:                    return VK_SAMPLE_COUNT_1_BIT;
    }
}

VkImageUsageFlags VulkanRHI::toVk(rhi::TextureUsage usage) {
    VkImageUsageFlags flags = 0;

    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(rhi::TextureUsage::Sampled))
        flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(rhi::TextureUsage::Storage))
        flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(rhi::TextureUsage::RenderTarget))
        flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(rhi::TextureUsage::DepthStencil))
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(rhi::TextureUsage::CopySrc))
        flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(rhi::TextureUsage::CopyDst))
        flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    return flags;
}

VkFormat VulkanRHI::toVk(rhi::VertexFormat format) {
    switch (format) {
    case rhi::VertexFormat::Float32:   return VK_FORMAT_R32_SFLOAT;
    case rhi::VertexFormat::Float32x2: return VK_FORMAT_R32G32_SFLOAT;
    case rhi::VertexFormat::Float32x3: return VK_FORMAT_R32G32B32_SFLOAT;
    case rhi::VertexFormat::Float32x4: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case rhi::VertexFormat::Int32:     return VK_FORMAT_R32_SINT;
    case rhi::VertexFormat::Int32x2:   return VK_FORMAT_R32G32_SINT;
    case rhi::VertexFormat::Int32x3:   return VK_FORMAT_R32G32B32_SINT;
    case rhi::VertexFormat::Int32x4:   return VK_FORMAT_R32G32B32A32_SINT;
    case rhi::VertexFormat::UInt32:    return VK_FORMAT_R32_UINT;
    case rhi::VertexFormat::UInt32x2:  return VK_FORMAT_R32G32_UINT;
    case rhi::VertexFormat::UInt32x3:  return VK_FORMAT_R32G32B32_UINT;
    case rhi::VertexFormat::UInt32x4:  return VK_FORMAT_R32G32B32A32_UINT;
    }
    return VK_FORMAT_UNDEFINED; // unreachable: all rhi::VertexFormat values covered above
}

VkPrimitiveTopology VulkanRHI::toVk(rhi::PrimitiveTopology topology) {
    switch (topology) {
    case rhi::PrimitiveTopology::PointList:     return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    case rhi::PrimitiveTopology::LineList:      return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    case rhi::PrimitiveTopology::LineStrip:     return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    case rhi::PrimitiveTopology::TriangleList:  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    case rhi::PrimitiveTopology::TriangleStrip: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    }
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // unreachable
}

VkCullModeFlags VulkanRHI::toVk(rhi::CullMode mode) {
    switch (mode) {
    case rhi::CullMode::None:  return VK_CULL_MODE_NONE;
    case rhi::CullMode::Front: return VK_CULL_MODE_FRONT_BIT;
    case rhi::CullMode::Back:  return VK_CULL_MODE_BACK_BIT;
    }
    return VK_CULL_MODE_NONE; // unreachable
}

VkFrontFace VulkanRHI::toVk(rhi::FrontFace face) {
    switch (face) {
    case rhi::FrontFace::CounterClockwise: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    case rhi::FrontFace::Clockwise:        return VK_FRONT_FACE_CLOCKWISE;
    }
    return VK_FRONT_FACE_COUNTER_CLOCKWISE; // unreachable
}

VkPolygonMode VulkanRHI::toVk(rhi::FillMode mode) {
    switch (mode) {
    case rhi::FillMode::Solid:     return VK_POLYGON_MODE_FILL;
    case rhi::FillMode::Wireframe: return VK_POLYGON_MODE_LINE;
    }
    return VK_POLYGON_MODE_FILL; // unreachable
}

VkBlendFactor VulkanRHI::toVk(rhi::BlendFactor factor) {
    switch (factor) {
    case rhi::BlendFactor::Zero:             return VK_BLEND_FACTOR_ZERO;
    case rhi::BlendFactor::One:              return VK_BLEND_FACTOR_ONE;
    case rhi::BlendFactor::SrcColor:         return VK_BLEND_FACTOR_SRC_COLOR;
    case rhi::BlendFactor::OneMinusSrcColor: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    case rhi::BlendFactor::DstColor:         return VK_BLEND_FACTOR_DST_COLOR;
    case rhi::BlendFactor::OneMinusDstColor: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    case rhi::BlendFactor::SrcAlpha:         return VK_BLEND_FACTOR_SRC_ALPHA;
    case rhi::BlendFactor::OneMinusSrcAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    case rhi::BlendFactor::DstAlpha:         return VK_BLEND_FACTOR_DST_ALPHA;
    case rhi::BlendFactor::OneMinusDstAlpha: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    }
    return VK_BLEND_FACTOR_ZERO; // unreachable
}

VkBlendOp VulkanRHI::toVk(rhi::BlendOp op) {
    switch (op) {
    case rhi::BlendOp::Add:             return VK_BLEND_OP_ADD;
    case rhi::BlendOp::Subtract:        return VK_BLEND_OP_SUBTRACT;
    case rhi::BlendOp::ReverseSubtract: return VK_BLEND_OP_REVERSE_SUBTRACT;
    case rhi::BlendOp::Min:             return VK_BLEND_OP_MIN;
    case rhi::BlendOp::Max:             return VK_BLEND_OP_MAX;
    }
    return VK_BLEND_OP_ADD;
}

VkColorComponentFlags VulkanRHI::toVk(rhi::ColorWriteMask mask) {
    // Bit values match 1:1 (R=1, G=2, B=4, A=8).
    return static_cast<VkColorComponentFlags>(static_cast<std::uint8_t>(mask));
}

VkPresentModeKHR VulkanRHI::toVk(rhi::PresentMode mode) {
    switch (mode) {
    case rhi::PresentMode::Fifo:        return VK_PRESENT_MODE_FIFO_KHR;
    case rhi::PresentMode::FifoRelaxed: return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
    case rhi::PresentMode::Mailbox:     return VK_PRESENT_MODE_MAILBOX_KHR;
    case rhi::PresentMode::Immediate:   return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VmaAllocationCreateInfo VulkanRHI::ToVmaAllocInfo(rhi::MemoryUsage memoryUsage)
{
    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    switch (memoryUsage) {
    case rhi::MemoryUsage::CpuToGpu:
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        break;
    case rhi::MemoryUsage::GpuToCpu:
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
        break;
    case rhi::MemoryUsage::GpuOnly:
        break;
    }
    return allocInfo;
}

std::uint32_t VulkanRHI::TexelBytes(rhi::TextureFormat format)
{
    using rhi::TextureFormat;
    switch (format) {
    case TextureFormat::R8Unorm:
    case TextureFormat::R8Snorm:
    case TextureFormat::R8Uint:
    case TextureFormat::R8Sint:
    case TextureFormat::Stencil8:
        return 1;
    case TextureFormat::RG8Unorm:
    case TextureFormat::RG8Snorm:
    case TextureFormat::RG8Uint:
    case TextureFormat::RG8Sint:
    case TextureFormat::R16Unorm:
    case TextureFormat::R16Snorm:
    case TextureFormat::R16Uint:
    case TextureFormat::R16Sint:
    case TextureFormat::R16Float:
    case TextureFormat::Depth16Unorm:
        return 2;
    case TextureFormat::RGBA8Unorm:
    case TextureFormat::RGBA8UnormSrgb:
    case TextureFormat::RGBA8Snorm:
    case TextureFormat::RGBA8Uint:
    case TextureFormat::RGBA8Sint:
    case TextureFormat::BGRA8Unorm:
    case TextureFormat::BGRA8UnormSrgb:
    case TextureFormat::RG16Unorm:
    case TextureFormat::RG16Snorm:
    case TextureFormat::RG16Uint:
    case TextureFormat::RG16Sint:
    case TextureFormat::RG16Float:
    case TextureFormat::R32Uint:
    case TextureFormat::R32Sint:
    case TextureFormat::R32Float:
    case TextureFormat::RGB10A2Unorm:
    case TextureFormat::RGB10A2Uint:
    case TextureFormat::RG11B10Float:
    case TextureFormat::RGB9E5Float:
    case TextureFormat::Depth24Plus:
    case TextureFormat::Depth32Float:
    case TextureFormat::Depth24PlusStencil8:
        return 4;
    case TextureFormat::RGBA16Unorm:
    case TextureFormat::RGBA16Snorm:
    case TextureFormat::RGBA16Uint:
    case TextureFormat::RGBA16Sint:
    case TextureFormat::RGBA16Float:
    case TextureFormat::RG32Uint:
    case TextureFormat::RG32Sint:
    case TextureFormat::RG32Float:
    case TextureFormat::Depth32FloatStencil8:
        return 8;
    case TextureFormat::RGB32Uint:
    case TextureFormat::RGB32Sint:
    case TextureFormat::RGB32Float:
        return 12;
    case TextureFormat::RGBA32Uint:
    case TextureFormat::RGBA32Sint:
    case TextureFormat::RGBA32Float:
        return 16;
    default:
        return 0;
    }
}

rhi::ShaderReflection VulkanRHI::ParseReflection(
    SpvReflectShaderModule* reflection,
    rhi::ShaderStageFlags stage)
{
    rhi::ShaderReflection result;

    uint32_t bindingCount = 0;

    spvReflectEnumerateDescriptorBindings(reflection, &bindingCount, nullptr);

    std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
    spvReflectEnumerateDescriptorBindings(reflection, &bindingCount, bindings.data());

    for (SpvReflectDescriptorBinding* binding : bindings) {
        rhi::ShaderBinding bindingResult{};

        bindingResult.set = binding->set;
        bindingResult.binding = binding->binding;
        bindingResult.arraySize = binding->count;
        bindingResult.stage = stage;

        switch (binding->descriptor_type)
        {
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                bindingResult.type = rhi::BindingType::UniformBuffer;
                break;

            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                bindingResult.type = rhi::BindingType::StorageBuffer;
                break;

            case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                bindingResult.type = rhi::BindingType::SampledTexture;
                break;

            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                bindingResult.type = rhi::BindingType::StorageTexture;
                break;

            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                bindingResult.type = rhi::BindingType::Sampler;
                break;

            default:
                continue;
        }

        result.bindings.push_back(bindingResult);
    }

    // Vertex inputs
    if (stage == rhi::ShaderStageFlags::Vertex)
    {
        uint32_t inputCount = 0;

        spvReflectEnumerateInputVariables(
            reflection,
            &inputCount,
            nullptr
        );

        std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);

        spvReflectEnumerateInputVariables(
            reflection,
            &inputCount,
            inputs.data()
        );

        for (SpvReflectInterfaceVariable* input : inputs)
        {
            // Built-in inputs such as gl_VertexIndex aren't vertex attributes.
            if (input->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN)
                continue;

            rhi::VertexInputAttr attr{};
            attr.location = input->location;

            switch (input->format)
            {
            case SPV_REFLECT_FORMAT_R32_SFLOAT:
                attr.format = rhi::VertexFormat::Float32;
                break;

            case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
                attr.format = rhi::VertexFormat::Float32x2;
                break;

            case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
                attr.format = rhi::VertexFormat::Float32x3;
                break;

            case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
                attr.format = rhi::VertexFormat::Float32x4;
                break;

            case SPV_REFLECT_FORMAT_R32_SINT:
                attr.format = rhi::VertexFormat::Int32;
                break;

            case SPV_REFLECT_FORMAT_R32G32_SINT:
                attr.format = rhi::VertexFormat::Int32x2;
                break;

            case SPV_REFLECT_FORMAT_R32G32B32_SINT:
                attr.format = rhi::VertexFormat::Int32x3;
                break;

            case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
                attr.format = rhi::VertexFormat::Int32x4;
                break;

            case SPV_REFLECT_FORMAT_R32_UINT:
                attr.format = rhi::VertexFormat::UInt32;
                break;

            case SPV_REFLECT_FORMAT_R32G32_UINT:
                attr.format = rhi::VertexFormat::UInt32x2;
                break;

            case SPV_REFLECT_FORMAT_R32G32B32_UINT:
                attr.format = rhi::VertexFormat::UInt32x3;
                break;

            case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
                attr.format = rhi::VertexFormat::UInt32x4;
                break;

            default:
                continue;
            }

            result.vertexInputs.push_back(attr);
        }
    }

    return result;
}
