#include "rhi/webgpu/wgpurhi.h"

#include <iostream>

rhi::SurfaceHandle WebGPURHI::CreateSurface(const rhi::SurfaceDesc& desc)
{
    if (!m_Instance || desc.window.type == rhi::NativeWindowType::Unknown ||
        desc.window.handle == nullptr) {
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
    switch (desc.window.type) {
#if defined(_WIN32)
    case rhi::NativeWindowType::Win32:
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
