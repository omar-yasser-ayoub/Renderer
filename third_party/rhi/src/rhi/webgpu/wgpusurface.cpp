#include "rhi/webgpu/wgpurhi.h"

#include <cstring>
#include <iostream>

rhi::SurfaceHandle WebGPURHI::CreateSurface(const rhi::SurfaceDesc& desc)
{
    if (!m_Instance || desc.window.type == rhi::NativeWindowType::Unknown) {
        return {};
    }

    wgpu::SurfaceDescriptor surfaceDesc{};
#if defined(_WIN32)
    wgpu::SurfaceSourceWindowsHWND windowsSource{};
#endif
#if defined(__linux__)
    wgpu::SurfaceSourceXlibWindow xlibSource{};
    wgpu::SurfaceSourceXCBWindow xcbSource{};
    wgpu::SurfaceSourceWaylandSurface waylandSource{};
#endif
#if defined(__EMSCRIPTEN__)
    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvasSource{};
#endif
    switch (desc.window.type) {
#if defined(_WIN32)
    case rhi::NativeWindowType::Win32:
        if (desc.window.handle == nullptr) {
            return {};
        }
        windowsSource.hinstance = desc.window.display;
        windowsSource.hwnd = desc.window.handle;
        surfaceDesc.nextInChain = &windowsSource;
        break;
#endif
#if defined(__linux__)
    case rhi::NativeWindowType::Xlib:
        xlibSource.display = desc.window.display;
        xlibSource.window = static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(desc.window.handle));
        surfaceDesc.nextInChain = &xlibSource;
        break;
    case rhi::NativeWindowType::Xcb:
        xcbSource.connection = desc.window.display;
        xcbSource.window = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(desc.window.handle));
        surfaceDesc.nextInChain = &xcbSource;
        break;
    case rhi::NativeWindowType::Wayland:
        waylandSource.display = desc.window.display;
        waylandSource.surface = desc.window.handle;
        surfaceDesc.nextInChain = &waylandSource;
        break;
#endif
#if defined(__EMSCRIPTEN__)
    case rhi::NativeWindowType::Canvas: {
        // The browser resolves the selector to a canvas element and takes a
        // "webgpu" context on it. The selector must be a NUL-terminated
        // string: the port reads it as a plain C string.
        const char* selector = desc.window.canvasSelector;
        if (selector == nullptr) {
            return {};
        }
        canvasSource.selector = wgpu::StringView(selector, std::strlen(selector));
        surfaceDesc.nextInChain = &canvasSource;
        break;
    }
#endif
    default:
        return {};
    }

    wgpu::Surface surface = m_Instance.CreateSurface(&surfaceDesc);
    if (!surface) {
        std::cerr << "Failed to create WebGPU surface.\n";
        return {};
    }

    rhi::SurfaceHandle handle;
    handle.desc = desc;
    if (!m_FreeSurfaces.empty()) {
        handle.id = m_FreeSurfaces.back();
        m_FreeSurfaces.pop_back();
        m_Surfaces[handle.id] = surface;
    } else {
        handle.id = static_cast<std::uint64_t>(m_Surfaces.size());
        m_Surfaces.push_back(surface);
    }
    return handle;
}

void WebGPURHI::DestroySurface(rhi::SurfaceHandle handle)
{
    if (!m_Instance || handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Surfaces.size() ||
        !m_Surfaces[handle.id]) {
        return;
    }
    m_Surfaces[handle.id] = {};
    m_FreeSurfaces.push_back(handle.id);
}
