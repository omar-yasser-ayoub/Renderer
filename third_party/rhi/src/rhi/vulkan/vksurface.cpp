#include "rhi/vulkan/vkrhi.h"

#include <vulkan/vulkan.h>
#include <cstdio>

#if defined(VK_USE_PLATFORM_XCB_KHR)
#include <xcb/xcb.h>
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include <X11/Xlib.h>
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <wayland-client.h>
#endif

rhi::SurfaceHandle VulkanRHI::CreateSurface(const rhi::SurfaceDesc& desc)
{
    if (m_Instance == VK_NULL_HANDLE) {
        return {};
    }
    if (desc.window.type == rhi::NativeWindowType::Unknown ||
        desc.window.handle == nullptr) {
        return {};
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkResult result = VK_ERROR_EXTENSION_NOT_PRESENT;
    switch (desc.window.type) {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    case rhi::NativeWindowType::Win32: {
        VkWin32SurfaceCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        info.hinstance = static_cast<HINSTANCE>(desc.window.display);
        info.hwnd = static_cast<HWND>(desc.window.handle);
        result = vkCreateWin32SurfaceKHR(m_Instance, &info, nullptr, &surface);
        break;
    }
#endif
#if defined(VK_USE_PLATFORM_XCB_KHR)
    case rhi::NativeWindowType::Xcb: {
        VkXcbSurfaceCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
        info.connection = static_cast<xcb_connection_t*>(desc.window.display);
        info.window = static_cast<xcb_window_t>(
            reinterpret_cast<std::uintptr_t>(desc.window.handle));
        result = vkCreateXcbSurfaceKHR(m_Instance, &info, nullptr, &surface);
        break;
    }
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    case rhi::NativeWindowType::Xlib: {
        VkXlibSurfaceCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        info.dpy = static_cast<Display*>(desc.window.display);
        info.window = static_cast<Window>(
            reinterpret_cast<std::uintptr_t>(desc.window.handle));
        result = vkCreateXlibSurfaceKHR(m_Instance, &info, nullptr, &surface);
        break;
    }
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
    case rhi::NativeWindowType::Wayland: {
        VkWaylandSurfaceCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        info.display = static_cast<wl_display*>(desc.window.display);
        info.surface = static_cast<wl_surface*>(desc.window.handle);
        result = vkCreateWaylandSurfaceKHR(m_Instance, &info, nullptr, &surface);
        break;
    }
#endif
    default:
        break;
    }
    if (result != VK_SUCCESS) {
        printf("[ERROR] Failed to create Vulkan surface\n");
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

void VulkanRHI::DestroySurface(rhi::SurfaceHandle handle)
{
    if (m_Instance == VK_NULL_HANDLE || handle.id == 0xFFFFFFFFFFFFFFFF ||
        handle.id >= m_Surfaces.size() || m_Surfaces[handle.id] == VK_NULL_HANDLE) {
        return;
    }
    vkDestroySurfaceKHR(m_Instance, m_Surfaces[handle.id], nullptr);
    m_Surfaces[handle.id] = VK_NULL_HANDLE;
    m_FreeSurfaces.push_back(handle.id);
}
