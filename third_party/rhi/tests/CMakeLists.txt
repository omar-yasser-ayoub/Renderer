cmake_minimum_required(VERSION 3.20)

# Fetch Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# Create test executable. Backend sources come from the rhi lib;
# backend defines (RHI_VULKAN/...) propagate via PUBLIC compile defs.
add_executable(rhi_tests
    test_rhi.cpp
)

target_link_libraries(rhi_tests
    PRIVATE
        rhi
        gtest
        gtest_main
)

if(RHI_BACKEND STREQUAL "WEBGPU")
    # Dawn needs its backend DLLs next to the test binary.
    if(WIN32)
        set(DAWN_SDK_ROOT "C:/Dawn/SDK" CACHE PATH "Path to Dawn SDK")
        add_custom_command(TARGET rhi_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DAWN_SDK_ROOT}/bin/d3dcompiler_47.dll"
                "$<TARGET_FILE_DIR:rhi_tests>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${DAWN_SDK_ROOT}/bin/vulkan-1.dll"
                "$<TARGET_FILE_DIR:rhi_tests>"
        )
    endif()
elseif(NOT RHI_BACKEND STREQUAL "VULKAN" AND NOT RHI_BACKEND STREQUAL "DX12")
    message(FATAL_ERROR "Unknown RHI backend: ${RHI_BACKEND}")
endif()

copy_dxc_runtime(rhi_tests)

configure_rhi_shaders(rhi_tests)

# Enable testing
enable_testing()
add_test(NAME RHITests COMMAND rhi_tests)
