cmake_minimum_required(VERSION 3.20)
project(NewRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Backend is chosen here, before the RHI configures. One backend per build:
#   cmake -DRHI_BACKEND=VULKAN|DX12|WEBGPU
set(RHI_BACKEND "VULKAN" CACHE STRING "RHI backend to use")
set_property(CACHE RHI_BACKEND PROPERTY STRINGS VULKAN DX12 WEBGPU)

if(EMSCRIPTEN)
    # Web build: its own target and its own build tree (see
    # scripts/build_web.bat). Everything the app loads at runtime (shaders,
    # models) is embedded into the wasm and mounted from the in-memory
    # filesystem, so the runtime paths are absolute VFS paths.
    set(APP_TARGET new_renderer_web)
    set(WEB_MODEL_DIR "/models")
    set(WEB_SHELL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")

    # contrib.glfw3 is the Emscripten port of GLFW: it provides the GLFW
    # headers and library, so the vendored native GLFW is not built here.
    add_compile_options(--use-port=contrib.glfw3)
    add_link_options(--use-port=contrib.glfw3)
else()
    set(APP_TARGET new_renderer)
endif()

add_subdirectory(third_party/rhi)
add_subdirectory(third_party/fastgltf)

# stb is header-only with no CMakeLists: expose it as an INTERFACE target.
# One TU in the app must #define STB_IMAGE_IMPLEMENTATION before including.
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE third_party/stb)

# Header-only GLM: no tests, no install targets, no compiled dummy lib.
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/glm)

# Dear ImGui: core + the GLFW platform backend only (input/events).
# The rendering side lives in src/imgui_backend.cpp, built on the RHI,
# so one backend file serves the Vulkan/DX12/WebGPU builds unchanged.
add_library(imgui STATIC
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_demo.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
)
target_include_directories(imgui
    PUBLIC
        third_party/imgui
        third_party/imgui/backends
)
if(NOT EMSCRIPTEN)
    # On the web the GLFW headers/library come from the contrib.glfw3 port.
    target_link_libraries(imgui PUBLIC glfw)
endif()

if(NOT EMSCRIPTEN)
    # No examples/tests/docs/install from GLFW: just the library.
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/glfw)
    if(WIN32)
        # GLFW's CMake leaves Win32 system libs to the consumer; Ninja+MSVC
        # doesn't get MSBuild's implicit user32/gdi32. PUBLIC so all
        # consumers of glfw inherit them.
        target_link_libraries(glfw PUBLIC gdi32 user32 shell32)
    endif()
endif()

add_executable(${APP_TARGET}
    src/main.cpp
    src/importer/import.cpp
    src/imgui_backend.cpp
)

target_include_directories(${APP_TARGET} PRIVATE src)

if(WIN32)
    # Windows.h defines min/max macros that break standard headers
    # (e.g. std::numeric_limits<>::max() inside fastgltf). Kill them.
    # (No WIN32_LEAN_AND_MEAN: dxcapi.h needs the full COM declarations.)
    target_compile_definitions(${APP_TARGET} PRIVATE NOMINMAX)
endif()
if(MSVC)
    target_compile_options(${APP_TARGET} PRIVATE /EHsc)
endif()

target_link_libraries(${APP_TARGET}
    PRIVATE
        rhi
        fastgltf::fastgltf
        stb
        glm::glm
        imgui
)
if(NOT EMSCRIPTEN)
    target_link_libraries(${APP_TARGET} PRIVATE glfw)
endif()

# fastgltf's public headers pull in simdjson/simdutf headers
# (core.hpp -> exporter.hpp -> simdjson.h) but fastgltf links them
# PRIVATE, so consumers need the helper targets for the include paths.
if(TARGET fastgltf::simdjson)
    target_link_libraries(${APP_TARGET} PRIVATE fastgltf::simdjson)
endif()
if(TARGET fastgltf::simdutf)
    target_link_libraries(${APP_TARGET} PRIVATE fastgltf::simdutf)
endif()

# RHI-owned deployment: copies shaders into the build tree + defines
# RHI_SHADER_DIR, and stages dxcompiler next to the binary.
rhi_add_shaders(${APP_TARGET} "${CMAKE_CURRENT_SOURCE_DIR}/shaders")

if(EMSCRIPTEN)
    # The app is a web page: the WebGPU surface is an HTML canvas, and the
    # frame loop is driven by requestAnimationFrame.
    set_target_properties(${APP_TARGET} PROPERTIES SUFFIX ".html")
    target_link_options(${APP_TARGET} PRIVATE
        "--shell-file=${WEB_SHELL_FILE}"
        # The frame loop is driven from JS (see newRendererWebFrame).
        "-sEXPORTED_FUNCTIONS=_main,_newRendererWebFrame"
        # Dawn's webgpu.h implementation needs a synchronous way to wait for
        # the browser's promise-based adapter/device requests.
        "-sASYNCIFY=1"
        "-sASYNCIFY_STACK_SIZE=65536"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sENVIRONMENT=web"
        "-sEXIT_RUNTIME=0"
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(${APP_TARGET} PRIVATE "--closure=1")
    endif()

    # Embed the models in the wasm: the browser has no filesystem, so they
    # are mounted into the in-memory filesystem at startup. The options go
    # into a link response file: the full list is far too long for the
    # Windows command line.
    set(embed_files ${RHI_SHADER_EMBED_FILES})
    file(GLOB_RECURSE model_files RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/models"
        "${CMAKE_CURRENT_SOURCE_DIR}/models/*")
    foreach(model_file IN LISTS model_files)
        if(NOT IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/models/${model_file}")
            list(APPEND embed_files
                "${CMAKE_CURRENT_SOURCE_DIR}/models/${model_file}@${WEB_MODEL_DIR}/${model_file}")
        endif()
    endforeach()

    set(embed_rsp "${CMAKE_CURRENT_BINARY_DIR}/web_embed_files.rsp")
    set(embed_rsp_content "")
    foreach(embed_file IN LISTS embed_files)
        string(APPEND embed_rsp_content "--embed-file=${embed_file}\n")
    endforeach()
    file(WRITE "${embed_rsp}" "${embed_rsp_content}")
    target_link_options(${APP_TARGET} PRIVATE "@${embed_rsp}")

    target_compile_definitions(${APP_TARGET} PRIVATE MODEL_DIR=\"${WEB_MODEL_DIR}\")
else()
    copy_dxc_runtime(${APP_TARGET})

    # Models are backend-agnostic, no transform needed: point at the source dir.
    target_compile_definitions(${APP_TARGET}
        PRIVATE MODEL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/models")
endif()
