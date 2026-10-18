#include <algorithm>
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
#include "renderer/structs.h"
#include <glm/gtc/matrix_transform.hpp>


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
    vsSource.path = shaderDir / "sponza.vert.hlsl";
    vsSource.stage = rhi::ShaderStageFlags::Vertex;
    vsSource.entryPoint = "main";
    const rhi::ShaderModuleHandle vs = rhi.CompileShader(vsSource);

    rhi::ShaderSource fsSource;
    fsSource.path = shaderDir / "sponza.frag.hlsl";
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

    std::sort(bindGroupLayoutDescs.begin(), bindGroupLayoutDescs.end(),
        [](const rhi::BindGroupLayoutDesc& a, const rhi::BindGroupLayoutDesc& b) {
            return a.shaderBindings[0].set < b.shaderBindings[0].set;
        });

    std::vector<rhi::BindGroupLayoutHandle> bindGroupLayouts;
    rhi::PipelineLayoutDesc layoutDesc;

    for (const auto &desc : bindGroupLayoutDescs) {
        rhi::BindGroupLayoutHandle handle = rhi.CreateBindGroupLayout(desc);
        bindGroupLayouts.push_back(handle);
        layoutDesc.layouts.push_back(handle);
    }

    rhi::PipelineLayoutHandle layout = rhi.CreatePipelineLayout(layoutDesc);

    rhi::ColorTarget colorTarget;
    colorTarget.format = rhi::TextureFormat::RGBA8Unorm;

    rhi::GraphicsPipelineDesc pipelineDesc;
    pipelineDesc.layout = layout;
    pipelineDesc.vertexShader = vs;
    pipelineDesc.fragmentShader = fs;
    pipelineDesc.colorTargets.push_back(colorTarget);

    pipelineDesc.depthStencil.format = rhi::TextureFormat::Depth32Float;
    pipelineDesc.depthStencil.depthTestEnabled = true;
    pipelineDesc.depthStencil.depthWriteEnabled = true;
    pipelineDesc.depthStencil.depthFunc = rhi::CompareOp::Less;

    pipelineDesc.vertexBuffers.push_back({
        .stride = 32,
        .attributes = {
            {.bufferSlot = 0, .location = 0, .format = rhi::VertexFormat::Float32x3, .offset = 0}, //pos
            {.bufferSlot = 0, .location = 1, .format = rhi::VertexFormat::Float32x3, .offset = 12}, //normal
            {.bufferSlot = 0, .location = 2, .format = rhi::VertexFormat::Float32x2, .offset = 24} //uv
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

    rhi::TextureDesc depthDesc;
    depthDesc.width = w;
    depthDesc.height = h;
    depthDesc.format = rhi::TextureFormat::Depth32Float;
    depthDesc.usage = rhi::TextureUsage::DepthStencil;
    rhi::TextureHandle depthTexture = rhi.CreateTexture(depthDesc);

    rhi::TextureViewDesc depthViewDesc;
    depthViewDesc.format = rhi::TextureFormat::Depth32Float;
    depthViewDesc.aspect = rhi::TextureAspect::Depth;
    const rhi::TextureViewHandle depthView = rhi.CreateTextureView(depthViewDesc, depthTexture);

    ModelData model = loadModel(rhi, (modelDir / "sponza/Sponza.gltf").string());

    // default texture fallbacks
    rhi::TextureDesc whiteDesc;
    whiteDesc.width = 1;
    whiteDesc.height = 1;
    whiteDesc.format = rhi::TextureFormat::RGBA8UnormSrgb;
    whiteDesc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::CopyDst;
    rhi::TextureHandle whiteTex = rhi.CreateTexture(whiteDesc);
    const uint8_t whitePixel[4] = { 255, 255, 255, 255 };
    rhi.UploadTexture(whiteTex, 0, whitePixel, 1, 1);

    rhi::TextureViewDesc whiteViewDesc;
    whiteViewDesc.format = whiteDesc.format;
    const rhi::TextureViewHandle whiteView = rhi.CreateTextureView(whiteViewDesc, whiteTex);

    const rhi::SamplerHandle defaultSampler = rhi.CreateSampler({});

    // resolve texture views and samplers by index, using fallbacks for missing textures/samplers
    auto resolveView = [&](uint32_t idx) -> rhi::TextureViewHandle {
        return idx == 0xFFFFFFFF ? whiteView : model.textures[idx];
    };
    auto resolveSampler = [&](uint32_t idx) -> rhi::SamplerHandle {
        return idx == 0xFFFFFFFF ? defaultSampler : model.samplers[idx];
    };

    //create the bind groups for our materials, indexable by the material index
    std::vector<rhi::BindGroupHandle> materialBindGroups;
    for (size_t i = 0; i < model.materials.size(); ++i) {
        rhi::BindGroupDesc bindGroupDesc;

        Material material = model.materials[i];

        rhi::BindGroupEntry albedoTexture;
        albedoTexture.binding = 0;
        albedoTexture.textureView = resolveView(material.albedoTextureIdx);
        bindGroupDesc.entries.push_back(albedoTexture);

        rhi::BindGroupEntry normalTexture;
        normalTexture.binding = 1;
        normalTexture.textureView = resolveView(material.normalTextureIdx);
        bindGroupDesc.entries.push_back(normalTexture);

        rhi::BindGroupEntry rmTexture;
        rmTexture.binding = 2;
        rmTexture.textureView = resolveView(material.rmTextureIdx);
        bindGroupDesc.entries.push_back(rmTexture);

        rhi::BindGroupEntry albedoSampler;
        albedoSampler.binding = 3;
        albedoSampler.sampler = resolveSampler(material.albedoSamplerIdx);
        bindGroupDesc.entries.push_back(albedoSampler);

        rhi::BindGroupEntry normalSampler;
        normalSampler.binding = 4;
        normalSampler.sampler = resolveSampler(material.normalSamplerIdx);
        bindGroupDesc.entries.push_back(normalSampler);

        rhi::BindGroupEntry rmSampler;
        rmSampler.binding = 5;
        rmSampler.sampler = resolveSampler(material.rmSamplerIdx);
        bindGroupDesc.entries.push_back(rmSampler);

        rhi::BufferDesc uboDesc;
        uboDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
        uboDesc.size = sizeof(MaterialUBO);
        uboDesc.usage = rhi::BufferUsage::Uniform;
        rhi::BufferHandle ubo = rhi.CreateBuffer(uboDesc);
        MaterialUBO uboData {
            material.baseColor,
            material.metallicFactor,
            material.roughnessFactor,
            {0.0f, 0.0f}
        };
        rhi.WriteBuffer(ubo, &uboData, sizeof(MaterialUBO));
        rhi::BindGroupEntry uboEntry;
        uboEntry.binding = 6;
        uboEntry.buffer = ubo;
        bindGroupDesc.entries.push_back(uboEntry);

        bindGroupDesc.layout = bindGroupLayouts[1];

        rhi::BindGroupHandle bindGroup = rhi.CreateBindGroup(bindGroupDesc);

        materialBindGroups.push_back(bindGroup);
    }

    //create the bind group for the camera

    rhi::BindGroupDesc cameraBindGroupDesc;

    rhi::BufferDesc cameraBuffer;
    cameraBuffer.size = sizeof(CameraUBO);
    cameraBuffer.usage = rhi::BufferUsage::Uniform;
    cameraBuffer.memoryUsage = rhi::MemoryUsage::CpuToGpu;
    rhi::BufferHandle cameraUbo = rhi.CreateBuffer(cameraBuffer);

    rhi::BindGroupEntry cameraEntry;
    cameraEntry.binding = 0;
    cameraEntry.buffer = cameraUbo;
    cameraBindGroupDesc.entries.push_back(cameraEntry);

    //create the light data buffer
    rhi::BufferDesc lightBufferDesc;
    lightBufferDesc.size = sizeof(LightData);
    lightBufferDesc.usage = rhi::BufferUsage::Uniform;
    lightBufferDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
    rhi::BufferHandle lightUbo = rhi.CreateBuffer(lightBufferDesc);

    LightData lightData{};

    // directional light
    lightData.lights[0].dirRadius = glm::vec4(glm::normalize(glm::vec3(-0.5f, -1.0f, -0.4f)), 0.0f);
    lightData.lights[0].colorIntensity = glm::vec4(1.0f, 0.98f, 0.95f, 3.0f);

    // point light
    lightData.lights[1].posType = glm::vec4(0.0f, 6.0f, 0.0f, 1.0f);
    lightData.lights[1].dirRadius = glm::vec4(0.0f, 0.0f, 0.0f, 15.0f);
    lightData.lights[1].colorIntensity = glm::vec4(1.0f, 0.6f, 0.3f, 5.0f);
    lightData.lightCount = 2;
    rhi.WriteBuffer(lightUbo, &lightData, sizeof(LightData));

    rhi::BindGroupEntry lightEntry;
    lightEntry.binding = 1;
    lightEntry.buffer = lightUbo;
    cameraBindGroupDesc.entries.push_back(lightEntry);

    cameraBindGroupDesc.layout = bindGroupLayouts[0];

    rhi::BindGroupHandle cameraBindGroup = rhi.CreateBindGroup(cameraBindGroupDesc);

    const glm::vec3 cameraPos(6.0f, 1.0f, 0.0f);
    const glm::vec3 cameraTarget(-0.5f, 1.0f, -0.3f);
    const glm::mat4 view = glm::lookAt(
        cameraPos,
        cameraTarget,
        glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspectiveRH_ZO(
        glm::radians(60.0f),
        static_cast<float>(w) / static_cast<float>(h),
        0.1f,
        1000.0f);

    CameraUBO cameraData;
    cameraData.viewProjMatrix = proj * view;
    cameraData.position = glm::vec4(cameraPos, 1.0f);
    rhi.WriteBuffer(cameraUbo, &cameraData, sizeof(CameraUBO));


    // create a model bind group for each drawable object
    std::vector<rhi::BindGroupHandle> modelBindGroups;
    for (size_t i = 0; i < model.drawables.size(); ++i) {
        rhi::BindGroupDesc modelBindGroupDesc;
        rhi::BufferDesc modelBufferDesc;
        modelBufferDesc.size = sizeof(ModelUBO);
        modelBufferDesc.usage = rhi::BufferUsage::Uniform;
        modelBufferDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
        rhi::BufferHandle modelUbo = rhi.CreateBuffer(modelBufferDesc);
        rhi.WriteBuffer(modelUbo, &model.drawables[i].transform, sizeof(ModelUBO));

        rhi::BindGroupEntry modelEntry;
        modelEntry.binding = 0;
        modelEntry.buffer = modelUbo;
        modelBindGroupDesc.entries.push_back(modelEntry);
        modelBindGroupDesc.layout = bindGroupLayouts[2];
        modelBindGroups.push_back(rhi.CreateBindGroup(modelBindGroupDesc));
    }

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
        pass.depth.enabled = true;
        pass.depth.view = depthView;
        pass.depth.clearDepth = true;
        pass.depth.clearDepthValue = 1.0f;
        rhi.BeginRenderPass(cmd, pass);
        rhi.SetPipeline(cmd, pipeline);
        rhi.SetViewport(cmd, 0, 0, (float)w, (float)h);
        rhi.SetScissor(cmd, 0, 0, w, h);
        rhi.SetBindGroup(cmd, 0, cameraBindGroup);
        rhi.SetVertexBuffer(cmd, 0, model.vertexBuffer);
        rhi.SetIndexBuffer(cmd, model.indexBuffer, rhi::IndexFormat::Uint32);
        for (size_t d = 0; d < model.drawables.size(); ++d) {
            const auto& submesh = model.primitivesList[model.drawables[d].submeshIdx];
            rhi.SetBindGroup(cmd, 1, materialBindGroups[submesh.materialIdx]);
            rhi.SetBindGroup(cmd, 2, modelBindGroups[d]);
            rhi.DrawIndexed(cmd, submesh.indexCount, 1, submesh.firstIndex, submesh.baseVertex, 0);
        }

        rhi.EndPass(cmd);
        rhi.Submit({&cmd, 1});
        rhi.Present(swapchain);
        glfwPollEvents();
    }

    glfwTerminate();
    rhi.Shutdown();
    return 0;
}
