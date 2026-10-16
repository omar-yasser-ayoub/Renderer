cmake_minimum_required(VERSION 3.20)
project(NewRenderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Backend is chosen here, before the RHI configures. One backend per build:
#   cmake -DRHI_BACKEND=VULKAN|DX12|WEBGPU
set(RHI_BACKEND "VULKAN" CACHE STRING "RHI backend to use")
set_property(CACHE RHI_BACKEND PROPERTY STRINGS VULKAN DX12 WEBGPU)

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

add_executable(new_renderer src/main.cpp src/importer/import.cpp)

target_include_directories(new_renderer PRIVATE src)

if(WIN32)
    # Windows.h defines min/max macros that break standard headers
    # (e.g. std::numeric_limits<>::max() inside fastgltf). Kill them.
    target_compile_definitions(new_renderer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(MSVC)
    target_compile_options(new_renderer PRIVATE /EHsc)
endif()

target_link_libraries(new_renderer
    PRIVATE
        rhi
        fastgltf::fastgltf
        stb
        glm::glm
        glfw
)

# fastgltf's public headers pull in simdjson/simdutf headers
# (core.hpp -> exporter.hpp -> simdjson.h) but fastgltf links them
# PRIVATE, so consumers need the helper targets for the include paths.
if(TARGET fastgltf::simdjson)
    target_link_libraries(new_renderer PRIVATE fastgltf::simdjson)
endif()
if(TARGET fastgltf::simdutf)
    target_link_libraries(new_renderer PRIVATE fastgltf::simdutf)
endif()

# RHI-owned deployment: copies shaders into the build tree + defines
# RHI_SHADER_DIR, and stages dxcompiler next to the binary.
rhi_add_shaders(new_renderer "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
copy_dxc_runtime(new_renderer)

# Models are backend-agnostic, no transform needed: point at the source dir.
target_compile_definitions(new_renderer
    PRIVATE MODEL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/models")
