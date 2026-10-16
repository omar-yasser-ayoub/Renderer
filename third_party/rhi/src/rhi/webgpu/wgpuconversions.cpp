#include "rhi/webgpu/wgpurhi.h"
#include "third_party/spirv-reflect/spirv_reflect.h"

#include <cstdint>
#include <vector>

wgpu::FilterMode WebGPURHI::toWebGPU(rhi::Filter f) {
    switch (f) {
        case rhi::Filter::Nearest: return wgpu::FilterMode::Nearest;
        case rhi::Filter::Linear:  return wgpu::FilterMode::Linear;
        default: return wgpu::FilterMode::Nearest;
    }
}

wgpu::MipmapFilterMode WebGPURHI::toWebGPU(rhi::MipmapFilter f) {
    switch (f) {
        case rhi::MipmapFilter::Nearest: return wgpu::MipmapFilterMode::Nearest;
        case rhi::MipmapFilter::Linear:  return wgpu::MipmapFilterMode::Linear;
        default: return wgpu::MipmapFilterMode::Nearest;
    }
}

wgpu::AddressMode WebGPURHI::toWebGPU(rhi::AddressMode f) {
    switch (f) {
        case rhi::AddressMode::Repeat: return wgpu::AddressMode::Repeat;
        case rhi::AddressMode::MirrorRepeat: return wgpu::AddressMode::MirrorRepeat;
        case rhi::AddressMode::ClampToEdge: return wgpu::AddressMode::ClampToEdge;
        case rhi::AddressMode::ClampToBorder:
            printf("[RHI] Warning: ClampToBorder unsupported in WebGPU, falling back to ClampToEdge\n");
            return wgpu::AddressMode::ClampToEdge;
        case rhi::AddressMode::MirrorClampToEdge:
            printf("[RHI] Warning: MirrorClampToEdge unsupported in WebGPU, falling back to MirrorRepeat\n");
            return wgpu::AddressMode::MirrorRepeat;
        default: return wgpu::AddressMode::Repeat;
    }
}

wgpu::CompareFunction WebGPURHI::toWebGPU(rhi::CompareOp op) {
    switch (op) {
        case rhi::CompareOp::Never: return wgpu::CompareFunction::Never;
        case rhi::CompareOp::Less: return wgpu::CompareFunction::Less;
        case rhi::CompareOp::Equal: return wgpu::CompareFunction::Equal;
        case rhi::CompareOp::LessEqual: return wgpu::CompareFunction::LessEqual;
        case rhi::CompareOp::Greater: return wgpu::CompareFunction::Greater;
        case rhi::CompareOp::NotEqual: return wgpu::CompareFunction::NotEqual;
        case rhi::CompareOp::GreaterEqual: return wgpu::CompareFunction::GreaterEqual;
        case rhi::CompareOp::Always: return wgpu::CompareFunction::Always;
        default: return wgpu::CompareFunction::Undefined;
    }
}

wgpu::BufferUsage WebGPURHI::toWebGPU(rhi::BufferUsage usage) {
    wgpu::BufferUsage result = wgpu::BufferUsage::None;
    if (HasFlag(usage, rhi::BufferUsage::Vertex)) result |= wgpu::BufferUsage::Vertex;
    if (HasFlag(usage, rhi::BufferUsage::Index)) result |= wgpu::BufferUsage::Index;
    if (HasFlag(usage, rhi::BufferUsage::Uniform)) result |= wgpu::BufferUsage::Uniform;
    if (HasFlag(usage, rhi::BufferUsage::Storage)) result |= wgpu::BufferUsage::Storage;
    if (HasFlag(usage, rhi::BufferUsage::Indirect)) result |= wgpu::BufferUsage::Indirect;
    if (HasFlag(usage, rhi::BufferUsage::CopySrc)) result |= wgpu::BufferUsage::CopySrc;
    if (HasFlag(usage, rhi::BufferUsage::CopyDst)) result |= wgpu::BufferUsage::CopyDst;
    return result;
}

wgpu::BufferUsage WebGPURHI::toWebGPU(rhi::MemoryUsage usage) {
    switch (usage) {
        case rhi::MemoryUsage::GpuOnly:
            return wgpu::BufferUsage::CopyDst;
        case rhi::MemoryUsage::CpuToGpu:
            return wgpu::BufferUsage::MapWrite | wgpu::BufferUsage::CopySrc;
        case rhi::MemoryUsage::GpuToCpu:
            return wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        default:
            return wgpu::BufferUsage::None;
    }
}

wgpu::TextureDimension WebGPURHI::toWebGPU(rhi::TextureDimension dim) {
    switch (dim) {
        case rhi::TextureDimension::Tex1D: return wgpu::TextureDimension::e1D;
        case rhi::TextureDimension::Tex2D: return wgpu::TextureDimension::e2D;
        case rhi::TextureDimension::Tex3D: return wgpu::TextureDimension::e3D;
        default: return wgpu::TextureDimension::e2D;
    }
}

wgpu::TextureViewDimension WebGPURHI::toWebGPU(rhi::TextureViewDimension dim) {
    switch (dim) {
        case rhi::TextureViewDimension::Tex1D: return wgpu::TextureViewDimension::e1D;
        case rhi::TextureViewDimension::Tex2D: return wgpu::TextureViewDimension::e2D;
        case rhi::TextureViewDimension::Tex3D: return wgpu::TextureViewDimension::e3D;
        case rhi::TextureViewDimension::Tex2DArray: return wgpu::TextureViewDimension::e2DArray;
        case rhi::TextureViewDimension::TexCube: return wgpu::TextureViewDimension::Cube;
        case rhi::TextureViewDimension::TexCubeArray: return wgpu::TextureViewDimension::CubeArray;
        // WebGPU has no 1D-array views.
        case rhi::TextureViewDimension::Tex1DArray: break;
        default: break;
    }
    return wgpu::TextureViewDimension::Undefined;
}

wgpu::TextureAspect WebGPURHI::toWebGPU(rhi::TextureAspect aspect) {
    switch (aspect) {
        case rhi::TextureAspect::Depth: return wgpu::TextureAspect::DepthOnly;
        case rhi::TextureAspect::Stencil: return wgpu::TextureAspect::StencilOnly;
        default: return wgpu::TextureAspect::All;
    }
}

bool WebGPURHI::HasStencilAspect(rhi::TextureFormat format) {
    switch (format) {
        case rhi::TextureFormat::Depth24PlusStencil8:
        case rhi::TextureFormat::Depth32FloatStencil8:
        case rhi::TextureFormat::Stencil8:
            return true;
        default:
            return false;
    }
}

bool WebGPURHI::IsStorageCompatible(rhi::TextureFormat format) {
    switch (format) {
        case rhi::TextureFormat::R32Float:
        case rhi::TextureFormat::R32Uint:
        case rhi::TextureFormat::R32Sint:
        case rhi::TextureFormat::RG32Float:
        case rhi::TextureFormat::RG32Uint:
        case rhi::TextureFormat::RG32Sint:
        case rhi::TextureFormat::RGBA8Unorm:
        case rhi::TextureFormat::RGBA8Snorm:
        case rhi::TextureFormat::RGBA8Uint:
        case rhi::TextureFormat::RGBA8Sint:
        case rhi::TextureFormat::RGBA16Float:
        case rhi::TextureFormat::RGBA16Uint:
        case rhi::TextureFormat::RGBA16Sint:
        case rhi::TextureFormat::RGBA32Float:
        case rhi::TextureFormat::RGBA32Uint:
        case rhi::TextureFormat::RGBA32Sint:
        case rhi::TextureFormat::RGB10A2Uint:
            return true;
        default:
            return false;
    }
}

rhi::TextureFormat WebGPURHI::fromWebGPU(wgpu::TextureFormat format) {
    switch (format) {
        case wgpu::TextureFormat::R8Unorm: return rhi::TextureFormat::R8Unorm;
        case wgpu::TextureFormat::RGBA8Unorm: return rhi::TextureFormat::RGBA8Unorm;
        case wgpu::TextureFormat::RGBA8UnormSrgb: return rhi::TextureFormat::RGBA8UnormSrgb;
        case wgpu::TextureFormat::BGRA8Unorm: return rhi::TextureFormat::BGRA8Unorm;
        case wgpu::TextureFormat::BGRA8UnormSrgb: return rhi::TextureFormat::BGRA8UnormSrgb;
        case wgpu::TextureFormat::Depth24Plus: return rhi::TextureFormat::Depth24Plus;
        case wgpu::TextureFormat::Depth32Float: return rhi::TextureFormat::Depth32Float;
        default: return rhi::TextureFormat::Unknown;
    }
}

rhi::PresentMode WebGPURHI::fromWebGPU(wgpu::PresentMode mode) {
    switch (mode) {
        case wgpu::PresentMode::Mailbox: return rhi::PresentMode::Mailbox;
        case wgpu::PresentMode::Immediate: return rhi::PresentMode::Immediate;
        case wgpu::PresentMode::FifoRelaxed: return rhi::PresentMode::FifoRelaxed;
        default: return rhi::PresentMode::Fifo;
    }
}

rhi::AlphaMode WebGPURHI::fromWebGPU(wgpu::CompositeAlphaMode mode) {
    switch (mode) {
        case wgpu::CompositeAlphaMode::Premultiplied: return rhi::AlphaMode::Premultiplied;
        default: return rhi::AlphaMode::Opaque;
    }
}

wgpu::TextureFormat WebGPURHI::toWebGPU(rhi::TextureFormat format) {
    switch (format) {
        case rhi::TextureFormat::Unknown:              return wgpu::TextureFormat::Undefined;

        // 8-bit
        case rhi::TextureFormat::R8Unorm:               return wgpu::TextureFormat::R8Unorm;
        case rhi::TextureFormat::R8Snorm:               return wgpu::TextureFormat::R8Snorm;
        case rhi::TextureFormat::R8Uint:                return wgpu::TextureFormat::R8Uint;
        case rhi::TextureFormat::R8Sint:                return wgpu::TextureFormat::R8Sint;
        case rhi::TextureFormat::RG8Unorm:              return wgpu::TextureFormat::RG8Unorm;
        case rhi::TextureFormat::RG8Snorm:              return wgpu::TextureFormat::RG8Snorm;
        case rhi::TextureFormat::RG8Uint:               return wgpu::TextureFormat::RG8Uint;
        case rhi::TextureFormat::RG8Sint:               return wgpu::TextureFormat::RG8Sint;
        case rhi::TextureFormat::RGBA8Unorm:            return wgpu::TextureFormat::RGBA8Unorm;
        case rhi::TextureFormat::RGBA8UnormSrgb:        return wgpu::TextureFormat::RGBA8UnormSrgb;
        case rhi::TextureFormat::RGBA8Snorm:            return wgpu::TextureFormat::RGBA8Snorm;
        case rhi::TextureFormat::RGBA8Uint:             return wgpu::TextureFormat::RGBA8Uint;
        case rhi::TextureFormat::RGBA8Sint:             return wgpu::TextureFormat::RGBA8Sint;
        case rhi::TextureFormat::BGRA8Unorm:            return wgpu::TextureFormat::BGRA8Unorm;
        case rhi::TextureFormat::BGRA8UnormSrgb:        return wgpu::TextureFormat::BGRA8UnormSrgb;

        // 16-bit
        case rhi::TextureFormat::R16Unorm:              return wgpu::TextureFormat::R16Unorm;
        case rhi::TextureFormat::R16Snorm:              return wgpu::TextureFormat::R16Snorm;
        case rhi::TextureFormat::R16Uint:               return wgpu::TextureFormat::R16Uint;
        case rhi::TextureFormat::R16Sint:               return wgpu::TextureFormat::R16Sint;
        case rhi::TextureFormat::R16Float:              return wgpu::TextureFormat::R16Float;
        case rhi::TextureFormat::RG16Unorm:             return wgpu::TextureFormat::RG16Unorm;
        case rhi::TextureFormat::RG16Snorm:             return wgpu::TextureFormat::RG16Snorm;
        case rhi::TextureFormat::RG16Uint:              return wgpu::TextureFormat::RG16Uint;
        case rhi::TextureFormat::RG16Sint:              return wgpu::TextureFormat::RG16Sint;
        case rhi::TextureFormat::RG16Float:             return wgpu::TextureFormat::RG16Float;
        case rhi::TextureFormat::RGBA16Unorm:           return wgpu::TextureFormat::RGBA16Unorm;
        case rhi::TextureFormat::RGBA16Snorm:           return wgpu::TextureFormat::RGBA16Snorm;
        case rhi::TextureFormat::RGBA16Uint:            return wgpu::TextureFormat::RGBA16Uint;
        case rhi::TextureFormat::RGBA16Sint:            return wgpu::TextureFormat::RGBA16Sint;
        case rhi::TextureFormat::RGBA16Float:           return wgpu::TextureFormat::RGBA16Float;

        // 32-bit
        case rhi::TextureFormat::R32Uint:               return wgpu::TextureFormat::R32Uint;
        case rhi::TextureFormat::R32Sint:               return wgpu::TextureFormat::R32Sint;
        case rhi::TextureFormat::R32Float:              return wgpu::TextureFormat::R32Float;
        case rhi::TextureFormat::RG32Uint:              return wgpu::TextureFormat::RG32Uint;
        case rhi::TextureFormat::RG32Sint:              return wgpu::TextureFormat::RG32Sint;
        case rhi::TextureFormat::RG32Float:             return wgpu::TextureFormat::RG32Float;
        case rhi::TextureFormat::RGBA32Uint:            return wgpu::TextureFormat::RGBA32Uint;
        case rhi::TextureFormat::RGBA32Sint:            return wgpu::TextureFormat::RGBA32Sint;
        case rhi::TextureFormat::RGBA32Float:           return wgpu::TextureFormat::RGBA32Float;

        // No 3-channel 32-bit formats in WebGPU — falls through to default.
        case rhi::TextureFormat::RGB32Uint:
        case rhi::TextureFormat::RGB32Sint:
        case rhi::TextureFormat::RGB32Float:
            break;

        // Packed
        case rhi::TextureFormat::RGB10A2Unorm:          return wgpu::TextureFormat::RGB10A2Unorm;
        case rhi::TextureFormat::RGB10A2Uint:           return wgpu::TextureFormat::RGB10A2Uint;
        case rhi::TextureFormat::RG11B10Float:          return wgpu::TextureFormat::RG11B10Ufloat;
        case rhi::TextureFormat::RGB9E5Float:           return wgpu::TextureFormat::RGB9E5Ufloat;

        // Depth / Stencil
        case rhi::TextureFormat::Depth16Unorm:          return wgpu::TextureFormat::Depth16Unorm;
        case rhi::TextureFormat::Depth24Plus:           return wgpu::TextureFormat::Depth24Plus;
        case rhi::TextureFormat::Depth24PlusStencil8:   return wgpu::TextureFormat::Depth24PlusStencil8;
        case rhi::TextureFormat::Depth32Float:          return wgpu::TextureFormat::Depth32Float;
        case rhi::TextureFormat::Depth32FloatStencil8:  return wgpu::TextureFormat::Depth32FloatStencil8;
        case rhi::TextureFormat::Stencil8:              return wgpu::TextureFormat::Stencil8;

        // BC
        case rhi::TextureFormat::BC1RGBAUnorm:          return wgpu::TextureFormat::BC1RGBAUnorm;
        case rhi::TextureFormat::BC1RGBAUnormSrgb:      return wgpu::TextureFormat::BC1RGBAUnormSrgb;
        case rhi::TextureFormat::BC2RGBAUnorm:          return wgpu::TextureFormat::BC2RGBAUnorm;
        case rhi::TextureFormat::BC2RGBAUnormSrgb:      return wgpu::TextureFormat::BC2RGBAUnormSrgb;
        case rhi::TextureFormat::BC3RGBAUnorm:          return wgpu::TextureFormat::BC3RGBAUnorm;
        case rhi::TextureFormat::BC3RGBAUnormSrgb:      return wgpu::TextureFormat::BC3RGBAUnormSrgb;
        case rhi::TextureFormat::BC4RUnorm:             return wgpu::TextureFormat::BC4RUnorm;
        case rhi::TextureFormat::BC4RSnorm:             return wgpu::TextureFormat::BC4RSnorm;
        case rhi::TextureFormat::BC5RGUnorm:            return wgpu::TextureFormat::BC5RGUnorm;
        case rhi::TextureFormat::BC5RGSnorm:            return wgpu::TextureFormat::BC5RGSnorm;
        case rhi::TextureFormat::BC6HRGBUFloat:         return wgpu::TextureFormat::BC6HRGBUfloat;
        case rhi::TextureFormat::BC6HRGBFloat:          return wgpu::TextureFormat::BC6HRGBFloat;
        case rhi::TextureFormat::BC7RGBAUnorm:          return wgpu::TextureFormat::BC7RGBAUnorm;
        case rhi::TextureFormat::BC7RGBAUnormSrgb:      return wgpu::TextureFormat::BC7RGBAUnormSrgb;

        // ETC2 / EAC
        case rhi::TextureFormat::ETC2RGB8Unorm:         return wgpu::TextureFormat::ETC2RGB8Unorm;
        case rhi::TextureFormat::ETC2RGB8UnormSrgb:     return wgpu::TextureFormat::ETC2RGB8UnormSrgb;
        case rhi::TextureFormat::ETC2RGB8A1Unorm:       return wgpu::TextureFormat::ETC2RGB8A1Unorm;
        case rhi::TextureFormat::ETC2RGB8A1UnormSrgb:   return wgpu::TextureFormat::ETC2RGB8A1UnormSrgb;
        case rhi::TextureFormat::ETC2RGBA8Unorm:        return wgpu::TextureFormat::ETC2RGBA8Unorm;
        case rhi::TextureFormat::ETC2RGBA8UnormSrgb:    return wgpu::TextureFormat::ETC2RGBA8UnormSrgb;
        case rhi::TextureFormat::EACR11Unorm:           return wgpu::TextureFormat::EACR11Unorm;
        case rhi::TextureFormat::EACR11Snorm:           return wgpu::TextureFormat::EACR11Snorm;
        case rhi::TextureFormat::EACRG11Unorm:          return wgpu::TextureFormat::EACRG11Unorm;
        case rhi::TextureFormat::EACRG11Snorm:          return wgpu::TextureFormat::EACRG11Snorm;

        // ASTC
        case rhi::TextureFormat::ASTC4x4Unorm:          return wgpu::TextureFormat::ASTC4x4Unorm;
        case rhi::TextureFormat::ASTC4x4UnormSrgb:      return wgpu::TextureFormat::ASTC4x4UnormSrgb;
        case rhi::TextureFormat::ASTC5x4Unorm:          return wgpu::TextureFormat::ASTC5x4Unorm;
        case rhi::TextureFormat::ASTC5x4UnormSrgb:      return wgpu::TextureFormat::ASTC5x4UnormSrgb;
        case rhi::TextureFormat::ASTC5x5Unorm:          return wgpu::TextureFormat::ASTC5x5Unorm;
        case rhi::TextureFormat::ASTC5x5UnormSrgb:      return wgpu::TextureFormat::ASTC5x5UnormSrgb;
        case rhi::TextureFormat::ASTC6x5Unorm:          return wgpu::TextureFormat::ASTC6x5Unorm;
        case rhi::TextureFormat::ASTC6x5UnormSrgb:      return wgpu::TextureFormat::ASTC6x5UnormSrgb;
        case rhi::TextureFormat::ASTC6x6Unorm:          return wgpu::TextureFormat::ASTC6x6Unorm;
        case rhi::TextureFormat::ASTC6x6UnormSrgb:      return wgpu::TextureFormat::ASTC6x6UnormSrgb;
        case rhi::TextureFormat::ASTC8x5Unorm:          return wgpu::TextureFormat::ASTC8x5Unorm;
        case rhi::TextureFormat::ASTC8x5UnormSrgb:      return wgpu::TextureFormat::ASTC8x5UnormSrgb;
        case rhi::TextureFormat::ASTC8x6Unorm:          return wgpu::TextureFormat::ASTC8x6Unorm;
        case rhi::TextureFormat::ASTC8x6UnormSrgb:      return wgpu::TextureFormat::ASTC8x6UnormSrgb;
        case rhi::TextureFormat::ASTC8x8Unorm:          return wgpu::TextureFormat::ASTC8x8Unorm;
        case rhi::TextureFormat::ASTC8x8UnormSrgb:      return wgpu::TextureFormat::ASTC8x8UnormSrgb;
        case rhi::TextureFormat::ASTC10x5Unorm:         return wgpu::TextureFormat::ASTC10x5Unorm;
        case rhi::TextureFormat::ASTC10x5UnormSrgb:     return wgpu::TextureFormat::ASTC10x5UnormSrgb;
        case rhi::TextureFormat::ASTC10x6Unorm:         return wgpu::TextureFormat::ASTC10x6Unorm;
        case rhi::TextureFormat::ASTC10x6UnormSrgb:     return wgpu::TextureFormat::ASTC10x6UnormSrgb;
        case rhi::TextureFormat::ASTC10x8Unorm:         return wgpu::TextureFormat::ASTC10x8Unorm;
        case rhi::TextureFormat::ASTC10x8UnormSrgb:     return wgpu::TextureFormat::ASTC10x8UnormSrgb;
        case rhi::TextureFormat::ASTC10x10Unorm:        return wgpu::TextureFormat::ASTC10x10Unorm;
        case rhi::TextureFormat::ASTC10x10UnormSrgb:    return wgpu::TextureFormat::ASTC10x10UnormSrgb;
        case rhi::TextureFormat::ASTC12x10Unorm:        return wgpu::TextureFormat::ASTC12x10Unorm;
        case rhi::TextureFormat::ASTC12x10UnormSrgb:    return wgpu::TextureFormat::ASTC12x10UnormSrgb;
        case rhi::TextureFormat::ASTC12x12Unorm:        return wgpu::TextureFormat::ASTC12x12Unorm;
        case rhi::TextureFormat::ASTC12x12UnormSrgb:    return wgpu::TextureFormat::ASTC12x12UnormSrgb;

        default: break;
    }
    return wgpu::TextureFormat::Undefined;
}

uint32_t WebGPURHI::toWebGPU(rhi::SampleCount count) {
    return static_cast<uint32_t>(count);
}

wgpu::TextureUsage WebGPURHI::toWebGPU(rhi::TextureUsage usage) {
    wgpu::TextureUsage result = wgpu::TextureUsage::None;

    if ((usage & rhi::TextureUsage::CopySrc) != rhi::TextureUsage::None)
        result |= wgpu::TextureUsage::CopySrc;
    if ((usage & rhi::TextureUsage::CopyDst) != rhi::TextureUsage::None)
        result |= wgpu::TextureUsage::CopyDst;
    if ((usage & rhi::TextureUsage::Sampled) != rhi::TextureUsage::None)
        result |= wgpu::TextureUsage::TextureBinding;
    if ((usage & rhi::TextureUsage::Storage) != rhi::TextureUsage::None)
        result |= wgpu::TextureUsage::StorageBinding;
    if ((usage & rhi::TextureUsage::RenderTarget) != rhi::TextureUsage::None)
        result |= wgpu::TextureUsage::RenderAttachment;
    if ((usage & rhi::TextureUsage::DepthStencil) != rhi::TextureUsage::None)
        result |= wgpu::TextureUsage::RenderAttachment;

    return result;
}

wgpu::IndexFormat WebGPURHI::toWebGPU(rhi::IndexFormat format) {
    switch (format) {
    case rhi::IndexFormat::Uint16: return wgpu::IndexFormat::Uint16;
    case rhi::IndexFormat::Uint32: return wgpu::IndexFormat::Uint32;
    default: return wgpu::IndexFormat::Uint32;
    }
}

wgpu::PresentMode WebGPURHI::toWebGPU(rhi::PresentMode mode) {
    switch (mode) {
    case rhi::PresentMode::Fifo: return wgpu::PresentMode::Fifo;
    case rhi::PresentMode::FifoRelaxed: return wgpu::PresentMode::FifoRelaxed;
    case rhi::PresentMode::Mailbox: return wgpu::PresentMode::Mailbox;
    case rhi::PresentMode::Immediate: return wgpu::PresentMode::Immediate;
    default: return wgpu::PresentMode::Fifo;
    }
}

wgpu::CompositeAlphaMode WebGPURHI::toWebGPU(rhi::AlphaMode mode) {
    switch (mode) {
    case rhi::AlphaMode::Opaque: return wgpu::CompositeAlphaMode::Opaque;
    case rhi::AlphaMode::Premultiplied: return wgpu::CompositeAlphaMode::Premultiplied;
    default: return wgpu::CompositeAlphaMode::Auto;
    }
}

wgpu::VertexFormat WebGPURHI::toWebGPU(rhi::VertexFormat format) {
    switch (format) {
    case rhi::VertexFormat::Float32:   return wgpu::VertexFormat::Float32;
    case rhi::VertexFormat::Float32x2: return wgpu::VertexFormat::Float32x2;
    case rhi::VertexFormat::Float32x3: return wgpu::VertexFormat::Float32x3;
    case rhi::VertexFormat::Float32x4: return wgpu::VertexFormat::Float32x4;
    case rhi::VertexFormat::Int32:     return wgpu::VertexFormat::Sint32;
    case rhi::VertexFormat::Int32x2:   return wgpu::VertexFormat::Sint32x2;
    case rhi::VertexFormat::Int32x3:   return wgpu::VertexFormat::Sint32x3;
    case rhi::VertexFormat::Int32x4:   return wgpu::VertexFormat::Sint32x4;
    case rhi::VertexFormat::UInt32:    return wgpu::VertexFormat::Uint32;
    case rhi::VertexFormat::UInt32x2:  return wgpu::VertexFormat::Uint32x2;
    case rhi::VertexFormat::UInt32x3:  return wgpu::VertexFormat::Uint32x3;
    case rhi::VertexFormat::UInt32x4:  return wgpu::VertexFormat::Uint32x4;
    }
    return wgpu::VertexFormat::Float32;
}

wgpu::PrimitiveTopology WebGPURHI::toWebGPU(rhi::PrimitiveTopology topology) {
    switch (topology) {
    case rhi::PrimitiveTopology::PointList:    return wgpu::PrimitiveTopology::PointList;
    case rhi::PrimitiveTopology::LineList:     return wgpu::PrimitiveTopology::LineList;
    case rhi::PrimitiveTopology::LineStrip:    return wgpu::PrimitiveTopology::LineStrip;
    case rhi::PrimitiveTopology::TriangleList: return wgpu::PrimitiveTopology::TriangleList;
    case rhi::PrimitiveTopology::TriangleStrip: return wgpu::PrimitiveTopology::TriangleStrip;
    default: return wgpu::PrimitiveTopology::Undefined;
    }
}

wgpu::CullMode WebGPURHI::toWebGPU(rhi::CullMode mode) {
    switch (mode) {
    case rhi::CullMode::None:  return wgpu::CullMode::None;
    case rhi::CullMode::Front: return wgpu::CullMode::Front;
    case rhi::CullMode::Back:  return wgpu::CullMode::Back;
    default: return wgpu::CullMode::Undefined;
    }
}

wgpu::FrontFace WebGPURHI::toWebGPU(rhi::FrontFace face) {
    switch (face) {
    case rhi::FrontFace::CounterClockwise: return wgpu::FrontFace::CCW;
    case rhi::FrontFace::Clockwise:        return wgpu::FrontFace::CW;
    default: return wgpu::FrontFace::Undefined;
    }
}

wgpu::BlendFactor WebGPURHI::toWebGPU(rhi::BlendFactor factor) {
    switch (factor) {
    case rhi::BlendFactor::Zero:             return wgpu::BlendFactor::Zero;
    case rhi::BlendFactor::One:              return wgpu::BlendFactor::One;
    case rhi::BlendFactor::SrcColor:         return wgpu::BlendFactor::Src;
    case rhi::BlendFactor::OneMinusSrcColor: return wgpu::BlendFactor::OneMinusSrc;
    case rhi::BlendFactor::DstColor:         return wgpu::BlendFactor::Dst;
    case rhi::BlendFactor::OneMinusDstColor: return wgpu::BlendFactor::OneMinusDst;
    case rhi::BlendFactor::SrcAlpha:         return wgpu::BlendFactor::SrcAlpha;
    case rhi::BlendFactor::OneMinusSrcAlpha: return wgpu::BlendFactor::OneMinusSrcAlpha;
    case rhi::BlendFactor::DstAlpha:         return wgpu::BlendFactor::DstAlpha;
    case rhi::BlendFactor::OneMinusDstAlpha: return wgpu::BlendFactor::OneMinusDstAlpha;
    default: return wgpu::BlendFactor::Undefined;
    }
}

wgpu::BlendOperation WebGPURHI::toWebGPU(rhi::BlendOp op) {
    switch (op) {
    case rhi::BlendOp::Add:             return wgpu::BlendOperation::Add;
    case rhi::BlendOp::Subtract:        return wgpu::BlendOperation::Subtract;
    case rhi::BlendOp::ReverseSubtract: return wgpu::BlendOperation::ReverseSubtract;
    case rhi::BlendOp::Min:             return wgpu::BlendOperation::Min;
    case rhi::BlendOp::Max:             return wgpu::BlendOperation::Max;
    default: return wgpu::BlendOperation::Undefined;
    }
}

wgpu::ColorWriteMask WebGPURHI::toWebGPU(rhi::ColorWriteMask mask) {
    // Bit values match 1:1 (R=1, G=2, B=4, A=8).
    return static_cast<wgpu::ColorWriteMask>(static_cast<std::uint8_t>(mask));
}

uint32_t WebGPURHI::TexelBytes(rhi::TextureFormat format)
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

rhi::ShaderReflection WebGPURHI::ParseReflection(
    SpvReflectShaderModule* reflection,
    rhi::ShaderStageFlags stage)
{
    rhi::ShaderReflection result;

    uint32_t bindingCount = 0;
    spvReflectEnumerateDescriptorBindings(reflection, &bindingCount, nullptr);

    std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
    spvReflectEnumerateDescriptorBindings(
        reflection,
        &bindingCount,
        bindings.data());

    for (const auto* binding : bindings) {
        rhi::ShaderBinding bindingResult{};
        bindingResult.set = binding->set;
        bindingResult.binding = binding->binding;
        bindingResult.arraySize = binding->count;
        bindingResult.stage = stage;

        switch (binding->descriptor_type) {
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

    if (stage != rhi::ShaderStageFlags::Vertex) {
        return result;
    }

    uint32_t inputCount = 0;
    spvReflectEnumerateInputVariables(reflection, &inputCount, nullptr);
    std::vector<SpvReflectInterfaceVariable*> inputs(inputCount);
    spvReflectEnumerateInputVariables(reflection, &inputCount, inputs.data());

    for (const auto* input : inputs) {
        if (input->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) {
            continue;
        }

        rhi::VertexInputAttr attr{};
        attr.location = input->location;

        switch (input->format) {
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

    return result;
}

wgpu::ShaderModule WebGPURHI::MakeShaderModule(const rhi::ShaderModule& module)
{
    if (module.bytecode.empty()) {
        return nullptr;
    }

    wgpu::ShaderSourceWGSL wgsl{};
    wgsl.code = wgpu::StringView(
        reinterpret_cast<const char*>(module.bytecode.data()),
        module.bytecode.size());

    wgpu::ShaderModuleDescriptor modDesc{};
    modDesc.nextInChain = &wgsl;
    return m_Device.CreateShaderModule(&modDesc);
}
