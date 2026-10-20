#pragma once

#include <cstdint>

namespace rhi
{

    enum class NativeWindowType : std::uint32_t
    {
        Unknown = 0,
        Win32,
        Xlib,
        Xcb,
        Wayland,
        MetalLayer,
        Android,
        Canvas,
    };

    struct NativeWindow
    {
        NativeWindowType type = NativeWindowType::Unknown;
        void* handle = nullptr;
        void* display = nullptr;
        const char* canvasSelector = nullptr;
    };

    struct SurfaceDesc
    {
        NativeWindow window{};
    };

    struct SurfaceHandle
    {
        std::uint64_t id = 0xFFFFFFFFFFFFFFFF;
        SurfaceDesc desc{};
    };
}
