cmake_minimum_required(VERSION 3.20)
project(RHI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RHI_BACKEND "VULKAN" CACHE STRING "RHI backend to use")
set_property(CACHE RHI_BACKEND PROPERTY STRINGS VULKAN DX12 WEBGPU)

set(RHI_SHADER_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/src/shaders")

function(rhi_add_shaders target source_dir)
  set(binary_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
  file(GLOB shader_sources CONFIGURE_DEPENDS "${source_dir}/*.hlsl")
  if(NOT shader_sources)
    message(WARNING "No HLSL shaders found in ${source_dir}")
    return()
  endif()

  set(copy_target "${target}_shaders")
  if(NOT TARGET ${copy_target})
    set(shader_outputs)
    # On the web the shaders are embedded into the wasm and mounted from the
    # in-memory filesystem, so the runtime paths differ from the build tree.
    if(EMSCRIPTEN)
      set(runtime_shader_dir "/shaders")
      set(runtime_shader_tools_dir "/shader_tools")
    else()
      set(runtime_shader_dir "${binary_dir}")
      set(runtime_shader_tools_dir "${CMAKE_CURRENT_BINARY_DIR}/shader_tools")
    endif()

    if(RHI_BACKEND STREQUAL "WEBGPU")
      # The shader tools run on the build host, not on the target platform:
      # a cross build (e.g. Emscripten) still uses the host's DXC and Tint.
      if(CMAKE_HOST_WIN32)
        set(dxc_exe "${DXC_ROOT}/bin/${DXC_ARCH}/dxc.exe")
        # FUNCTION_LIST_DIR, not SOURCE_DIR: this function runs in the
        # caller's scope, which is a different project when consumed via
        # add_subdirectory. Same reason DXC_ROOT/DXC_ARCH are cache vars.
        set(tint_exe "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/src/third_party/tint/tint.exe")
      elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
        set(RHI_TINT_EXE "RHI_TINT_EXE-NOTFOUND" CACHE FILEPATH "Tint executable for shaders (build from Dawn sources)")
        if(NOT EXISTS "${RHI_TINT_EXE}")
          message(FATAL_ERROR "WebGPU shader compilation needs Tint at RHI_TINT_EXE=${RHI_TINT_EXE}")
        endif()
        file(COPY "${DXC_ROOT}/bin/linux-x64/dxc" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/shader_tools"
          FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
        set(dxc_exe "${CMAKE_CURRENT_BINARY_DIR}/shader_tools/dxc")
        set(tint_exe "${RHI_TINT_EXE}")
      else()
        message(FATAL_ERROR "WebGPU shader compilation needs DXC and Tint tool paths for this platform")
      endif()

      set(shader_tool_dir "${CMAKE_CURRENT_BINARY_DIR}/shader_tools")

      foreach(shader IN LISTS shader_sources)
        get_filename_component(shader_filename "${shader}" NAME)

        if(shader_filename MATCHES "^(.+)\\.vert\\.hlsl$")
          set(shader_stem "${CMAKE_MATCH_1}.vert")
          set(shader_profile vs_6_6)
        elseif(shader_filename MATCHES "^(.+)\\.frag\\.hlsl$")
          set(shader_stem "${CMAKE_MATCH_1}.frag")
          set(shader_profile ps_6_6)
        elseif(shader_filename MATCHES "^(.+)\\.comp\\.hlsl$")
          set(shader_stem "${CMAKE_MATCH_1}.comp")
          set(shader_profile cs_6_6)
        else()
          message(FATAL_ERROR
            "Shader '${shader_filename}' must use .vert.hlsl, .frag.hlsl, or .comp.hlsl")
        endif()

        set(hlsl_output "${binary_dir}/${shader_filename}")
        set(spirv_output "${shader_tool_dir}/${shader_stem}.spv")
        set(wgsl_output "${binary_dir}/${shader_stem}.wgsl")
        set(reflection_output "${binary_dir}/${shader_stem}.reflection.txt")

        add_custom_command(
          OUTPUT "${wgsl_output}" "${reflection_output}"
          BYPRODUCTS "${spirv_output}"
          COMMAND ${CMAKE_COMMAND} -E make_directory "${binary_dir}"
          COMMAND ${CMAKE_COMMAND} -E make_directory "${shader_tool_dir}"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${shader}" "${hlsl_output}"
          COMMAND "${dxc_exe}"
            -spirv -E main -T ${shader_profile} -fvk-use-dx-layout
            -Fo "${spirv_output}" "${shader}"
          COMMAND "${tint_exe}"
            --format wgsl -o "${wgsl_output}" "${spirv_output}"
          COMMAND "${tint_exe}"
            --dump-inspector-bindings true "${spirv_output}"
            > "${reflection_output}"
          DEPENDS "${shader}" "${dxc_exe}" "${tint_exe}"
          VERBATIM
        )
        list(APPEND shader_outputs "${wgsl_output}" "${reflection_output}")
        if(EMSCRIPTEN)
          list(APPEND shader_embed_files
            "${wgsl_output}@${runtime_shader_dir}/${shader_stem}.wgsl"
            "${spirv_output}@${runtime_shader_tools_dir}/${shader_stem}.spv")
        endif()
      endforeach()
    else()
      foreach(shader IN LISTS shader_sources)
        get_filename_component(shader_filename "${shader}" NAME)
        set(hlsl_output "${binary_dir}/${shader_filename}")
        add_custom_command(
          OUTPUT "${hlsl_output}"
          COMMAND ${CMAKE_COMMAND} -E make_directory "${binary_dir}"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${shader}" "${hlsl_output}"
          DEPENDS "${shader}"
          VERBATIM
        )
        list(APPEND shader_outputs "${hlsl_output}")
      endforeach()
    endif()

    add_custom_target(${copy_target} DEPENDS ${shader_outputs})
    if(TARGET rhi_shader_cleanup)
      add_dependencies(${copy_target} rhi_shader_cleanup)
    endif()
  endif()

  add_dependencies(${target} ${copy_target})
  target_compile_definitions(${target}
    PRIVATE RHI_SHADER_DIR=\"${runtime_shader_dir}\")
  if(EMSCRIPTEN AND shader_embed_files)
    # The compiled shaders are embedded in the wasm and unpacked into the
    # in-memory filesystem before main() runs. The caller turns the list
    # into a link response file: the full list of embed options is too long
    # for the Windows command line.
    set(RHI_SHADER_EMBED_FILES "${shader_embed_files}" PARENT_SCOPE)
  endif()
endfunction()

# Backwards-compatible wrapper for the RHI's own shaders.
function(configure_rhi_shaders target)
  rhi_add_shaders(${target} "${RHI_SHADER_SOURCE_DIR}")
endfunction()

add_library(rhi STATIC)

target_include_directories(rhi
    PUBLIC
        src
)

set(DXC_ROOT "${CMAKE_CURRENT_LIST_DIR}/src/third_party/dxc" CACHE PATH "DXC SDK root")
if(CMAKE_HOST_WIN32)
  # The DXC tools run on the build host: a cross build (e.g. Emscripten on
  # Windows) still uses the host's x64 DXC.
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DXC_ARCH x64 CACHE INTERNAL "DXC binary architecture")
  else()
    set(DXC_ARCH x86 CACHE INTERNAL "DXC binary architecture")
  endif()

  if(NOT EMSCRIPTEN)
    add_library(dxc SHARED IMPORTED GLOBAL)
    set_target_properties(dxc PROPERTIES
      IMPORTED_IMPLIB "${DXC_ROOT}/lib/${DXC_ARCH}/dxcompiler.lib"
      IMPORTED_LOCATION "${DXC_ROOT}/bin/${DXC_ARCH}/dxcompiler.dll"
      INTERFACE_INCLUDE_DIRECTORIES "${DXC_ROOT}/inc"
    )
  endif()
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
  if(NOT EMSCRIPTEN)
    add_library(dxc SHARED IMPORTED GLOBAL)
    set_target_properties(dxc PROPERTIES
      IMPORTED_LOCATION "${DXC_ROOT}/lib/linux-x64/libdxcompiler.so"
      INTERFACE_INCLUDE_DIRECTORIES "${DXC_ROOT}/inc"
    )
  endif()
endif()

function(copy_dxc_runtime target)
  if(WIN32)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXC_ROOT}/bin/${DXC_ARCH}/dxcompiler.dll"
        "$<TARGET_FILE_DIR:${target}>"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXC_ROOT}/bin/${DXC_ARCH}/dxil.dll"
        "$<TARGET_FILE_DIR:${target}>"
    )
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXC_ROOT}/lib/linux-x64/libdxcompiler.so"
        "$<TARGET_FILE_DIR:${target}>"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXC_ROOT}/lib/linux-x64/libdxil.so"
        "$<TARGET_FILE_DIR:${target}>"
    )
    set_target_properties(${target} PROPERTIES BUILD_RPATH "$ORIGIN")
  endif()
endfunction()

if(RHI_BACKEND STREQUAL "VULKAN")
  message(STATUS "Configuring RHI for Vulkan")
  find_package(Vulkan REQUIRED)
  target_sources(rhi
        PRIVATE
            src/rhi/vulkan/vkrhi.cpp
            src/rhi/vulkan/vktexture.cpp
            src/rhi/vulkan/vkbuffer.cpp
            src/rhi/vulkan/vksampler.cpp
            src/rhi/vulkan/vkconversions.cpp
            src/rhi/vulkan/vkshader.cpp
            src/rhi/vulkan/vkbindgroup.cpp
            src/rhi/vulkan/vkpipeline.cpp
            src/rhi/vulkan/vksurface.cpp
            src/rhi/vulkan/vkswapchain.cpp
            src/rhi/vulkan/vkrenderpass.cpp
            src/third_party/spirv-reflect/spirv_reflect.cpp
    )
  target_link_libraries(rhi
        PUBLIC
            Vulkan::Vulkan
            dxc
    )
  target_compile_definitions(rhi
        PUBLIC
            RHI_VULKAN
    )
elseif(RHI_BACKEND STREQUAL "DX12")
  if(NOT WIN32)
    message(FATAL_ERROR "DirectX 12 is only supported on Windows.")
  endif()
  message(STATUS "Configuring RHI for DirectX 12")
  target_sources(rhi
        PRIVATE
            src/rhi/dx12/dx12rhi.cpp
            src/rhi/dx12/dx12sampler.cpp
            src/rhi/dx12/dx12texture.cpp
            src/rhi/dx12/dx12buffer.cpp
            src/rhi/dx12/dx12conversions.cpp
            src/rhi/dx12/dx12shader.cpp
            src/rhi/dx12/dx12bindgroup.cpp
            src/rhi/dx12/dx12pipeline.cpp
            src/rhi/dx12/dx12surface.cpp
            src/rhi/dx12/dx12swapchain.cpp
            src/rhi/dx12/dx12renderpass.cpp
    )
  target_link_libraries(rhi
        PUBLIC
            d3d12
            dxgi
            dxc
    )
  target_compile_definitions(rhi
        PUBLIC
            RHI_DX12
    )
elseif(RHI_BACKEND STREQUAL "WEBGPU")
  target_sources(rhi
            PRIVATE
                src/rhi/webgpu/wgpurhi.cpp
                src/rhi/webgpu/wgpubuffer.cpp
                src/rhi/webgpu/wgpuconversions.cpp
                src/rhi/webgpu/wgpusampler.cpp
                src/rhi/webgpu/wgputexture.cpp
                src/rhi/webgpu/wgpushader.cpp
                src/rhi/webgpu/wgpubindgroup.cpp
                src/rhi/webgpu/wgpupipeline.cpp
                src/rhi/webgpu/wgpusurface.cpp
                src/rhi/webgpu/wgpuswapchain.cpp
                src/rhi/webgpu/wgpurenderpass.cpp
                src/third_party/spirv-reflect/spirv_reflect.cpp
        )
  if(EMSCRIPTEN)
    # Web build: Dawn's own webgpu.h/webgpu_cpp.h implementation on top of the
    # browser's WebGPU API (the emdawnwebgpu Emscripten port). Same C++ API as
    # the native Dawn build, so the same sources compile unchanged.
    message(STATUS "Configuring RHI for WebGPU (web / Emscripten)")
    target_compile_options(rhi PUBLIC --use-port=emdawnwebgpu)
    target_link_options(rhi PUBLIC --use-port=emdawnwebgpu)
    target_compile_definitions(rhi
      PUBLIC
        RHI_WEBGPU
        RHI_WEBGPU_WEB
    )
  else()
    message(STATUS "Configuring RHI for WebGPU (native / Dawn)")
    set(DAWN_SDK_ROOT "C:/Dawn/SDK" CACHE PATH "Path to Dawn SDK")
    find_package(Threads REQUIRED)
    find_package(Dawn REQUIRED PATHS ${DAWN_SDK_ROOT})
    target_link_libraries(rhi
        PUBLIC
            dawn::webgpu_dawn
        )
    target_compile_definitions(rhi
        PUBLIC
            RHI_WEBGPU
    )
  endif()

else()
  message(FATAL_ERROR "Unknown RHI backend: ${RHI_BACKEND}")
endif()

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(RHI_BUILD_TESTS_DEFAULT ON)
else()
  set(RHI_BUILD_TESTS_DEFAULT OFF)
endif()
option(RHI_BUILD_TESTS "Build RHI tests" ${RHI_BUILD_TESTS_DEFAULT})

if(RHI_BUILD_TESTS)
  add_subdirectory(tests)
endif()
