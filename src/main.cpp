#include <filesystem>
#include <iostream>

#include "rhi/resources/bindgroup.h"
#include "rhi/resources/buffer.h"
#include "rhi/resources/pipeline.h"
#include "rhi/resources/renderpass.h"
#include "rhi/resources/surface.h"
#include "rhi/resources/swapchain.h"
#include "rhi/rhi.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "importer/import.h"


int main()
{

#ifdef RHI_SHADER_DIR
    const std::filesystem::path shaderDir(RHI_SHADER_DIR);
#else
    const std::filesystem::path shaderDir("shaders");
#endif

#ifdef MODEL_DIR
  const std::filesystem::path modelDir(MODEL_DIR);
#else
  const std::filesystem::path modelDir("models");
#endif

    RHI rhi;
    if (!rhi.Initialize()) {
        std::cerr << "Failed to initialize RHI.\n";
        return 1;
    }

    if (!glfwInit()) {
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window;

    window = glfwCreateWindow(2560, 1440, "Renderer", NULL, NULL);

    if (!window) {
        glfwTerminate();
        return -1;
    }

    rhi::ShaderSource vsSource;
    vsSource.path = shaderDir / "triangle.vert.hlsl";
    vsSource.stage = rhi::ShaderStageFlags::Vertex;
    vsSource.entryPoint = "main";
    const rhi::ShaderModuleHandle vs = rhi.CompileShader(vsSource);

    rhi::ShaderSource fsSource;
    fsSource.path = shaderDir / "triangle.frag.hlsl";
    fsSource.stage = rhi::ShaderStageFlags::Fragment;
    fsSource.entryPoint = "main";
    const rhi::ShaderModuleHandle fs = rhi.CompileShader(fsSource);

    const auto vsModule = rhi.GetShaderModule(vs);
    const auto fsModule = rhi.GetShaderModule(fs);

    rhi::BufferDesc vertexDesc;
    vertexDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
    vertexDesc.usage = rhi::BufferUsage::Vertex | rhi::BufferUsage::CopyDst;
    vertexDesc.size = 60;

    rhi::BufferHandle vb = rhi.CreateBuffer(vertexDesc);
    float data[] = {
      -0.5f, -0.5f, 0.0f,  0.0f, 0.0f,
       0.5f, -0.5f, 0.0f,  1.0f, 0.0f,
       0.0f,  0.5f, 0.0f,  0.5f, 1.0f,
    };

    rhi.WriteBuffer(vb, data, sizeof(data), 0);

    std::vector<rhi::ShaderReflection> reflections = {};
    reflections.push_back(vsModule->reflection);
    reflections.push_back(fsModule->reflection);

    auto bindGroupLayoutDescs = rhi::BuildBindGroupLayoutDescs(reflections);

    rhi::PipelineLayoutDesc layoutDesc;

    for (const auto &desc : bindGroupLayoutDescs) {
        layoutDesc.layouts.push_back(rhi.CreateBindGroupLayout(desc));
    }

    rhi::PipelineLayoutHandle layout = rhi.CreatePipelineLayout(layoutDesc);

    rhi::ColorTarget colorTarget;
    colorTarget.format = rhi::TextureFormat::RGBA8Unorm;

    rhi::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.layout = layout;
    pipelineDesc.vertexShader = vs;
    pipelineDesc.fragmentShader = fs;
    pipelineDesc.colorTargets.push_back(colorTarget);

    pipelineDesc.vertexBuffers.push_back({
        .stride = 20,
        .attributes = {
            {.bufferSlot = 0, .location = 0, .format = rhi::VertexFormat::Float32x3, .offset = 0},
            {.bufferSlot = 0, .location = 1, .format = rhi::VertexFormat::Float32x2, .offset = 12},
        },
    });

    auto pipeline = rhi.CreateGraphicsPipeline(pipelineDesc);

    rhi::SurfaceDesc surfaceDesc;
    surfaceDesc.window = {
        rhi::NativeWindowType::Win32,
        glfwGetWin32Window(window),
        GetModuleHandle(nullptr),
    };

    rhi::SurfaceHandle surface = rhi.CreateSurface(surfaceDesc);

    int w,h;
    glfwGetWindowSize(window, &w, &h);

    rhi::SwapchainDesc swapchainDesc;
    swapchainDesc.surface = surface;
    swapchainDesc.width = w;
    swapchainDesc.height = h;
    swapchainDesc.format = rhi::TextureFormat::RGBA8Unorm;

    auto swapchain = rhi.CreateSwapchain(swapchainDesc);

    while (!glfwWindowShouldClose(window)) {
        auto acquired = rhi.AcquireNextImage(swapchain);

        if (acquired.status != rhi::AcquireStatus::Success
            && acquired.status != rhi::AcquireStatus::Suboptimal
        ) {
            continue;
        }

        auto cmd = rhi.BeginCommandList();

        rhi::RenderPassDesc pass{};
        pass.swapchain = swapchain;
        pass.imageIndex = acquired.imageIndex;
        pass.colors.push_back({
            .useSwapchain = true,
            .clearColor = {0.0f, 0.0f, 0.0f, 1.0f},
            .clear = true
        });
        rhi.BeginRenderPass(cmd, pass);
        rhi.SetPipeline(cmd, pipeline);
        rhi.SetViewport(cmd, 0, 0, (float)w, (float)h);
        rhi.SetScissor(cmd, 0, 0, w, h);
        rhi.SetVertexBuffer(cmd, 0, vb);
        rhi.Draw(cmd, 3, 1, 0, 0);

        rhi.EndPass(cmd);
        rhi.Submit({&cmd, 1});
        rhi.Present(swapchain);
        glfwPollEvents();
    }

    glfwTerminate();
    rhi.Shutdown();
    return 0;
}
