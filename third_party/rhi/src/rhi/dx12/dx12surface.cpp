#include "rhi/dx12/dx12rhi.h"

#include <Windows.h>
rhi::SurfaceHandle DX12RHI::CreateSurface(const rhi::SurfaceDesc& desc)
{
    if (desc.window.type != rhi::NativeWindowType::Win32 ||
        desc.window.handle == nullptr) {
        return {};
    }

    HWND hwnd = static_cast<HWND>(desc.window.handle);
    if (hwnd == nullptr) {
        return {};
    }

    rhi::SurfaceHandle handle;
    handle.desc = desc;
    if (!m_FreeSurfaces.empty()) {
        handle.id = m_FreeSurfaces.back();
        m_FreeSurfaces.pop_back();
        m_Surfaces[handle.id] = hwnd;
    } else {
        handle.id = static_cast<std::uint64_t>(m_Surfaces.size());
        m_Surfaces.push_back(hwnd);
    }
    return handle;
}

void DX12RHI::DestroySurface(rhi::SurfaceHandle handle)
{
    if (handle.id == 0xFFFFFFFFFFFFFFFF || handle.id >= m_Surfaces.size() ||
        m_Surfaces[handle.id] == nullptr) {
        return;
    }
    m_Surfaces[handle.id] = nullptr;
    m_FreeSurfaces.push_back(handle.id);
}
