#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>

#include "rhi/resources/bindgroup.h"
#include "rhi/resources/buffer.h"
#include "rhi/resources/pipeline.h"
#include "rhi/resources/renderpass.h"
#include "rhi/resources/surface.h"
#include "rhi/resources/swapchain.h"
#include "rhi/resources/texture.h"
#include "rhi/rhi.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "imgui.h"
#include "importer/import.h"
#include "imgui_backend.h"
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

    int w, h;
    glfwGetWindowSize(window, &w, &h);
    int fbw, fbh;
    glfwGetFramebufferSize(window, &fbw, &fbh);

    rhi::SwapchainDesc swapchainDesc;
    swapchainDesc.surface = surface;
    swapchainDesc.width = w;
    swapchainDesc.height = h;
    swapchainDesc.format = rhi::TextureFormat::RGBA8Unorm;

    auto swapchain = rhi.CreateSwapchain(swapchainDesc);

    ImGuiRenderer imgui;
    if (!imgui.Init(rhi, window, shaderDir, swapchainDesc.format,
                    static_cast<uint32_t>(fbw), static_cast<uint32_t>(fbh),
                    swapchain)) {
        std::cerr << "Failed to initialize ImGui.\n";
        return 1;
    }
    auto lastFrameTime = std::chrono::steady_clock::now();

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

    lightData.lights[0].dirRadius = glm::vec4(glm::normalize(glm::vec3(0.0f, -1.0f, 0.0f)), 0.0f);
    lightData.lights[0].colorIntensity = glm::vec4(1.0f, 0.93f, 0.82f, 3.4f);

    uint32_t lightCount = 1;

    auto addPointLight = [&](glm::vec3 position, glm::vec3 color, float intensity, float range) {
        Light& light = lightData.lights[lightCount++];
        light.posType        = glm::vec4(position, 1.0f);
        light.dirRadius      = glm::vec4(0.0f, 0.0f, 0.0f, range);
        light.colorIntensity = glm::vec4(color, intensity);
    };

    addPointLight(glm::vec3( 0.0f, 3.0f,  0.0f), glm::vec3(0.3f, 0.20f, 0.85f), 1.0f, 16.0f); // hot pink

    lightData.lightCount = lightCount;

    // build one orthographic light matrix per shadow-casting directional light
    const glm::vec3 sceneCenter(0.0f, 0.0f, 0.0f);
    const float sceneRadius = 40.0f;
    const float lightDistance = 1000.0f;

    ShadowData shadowData{};
    uint32_t shadowCount = 0;
    for (uint32_t i = 0; i < lightData.lightCount && shadowCount < MAX_SHADOWS; ++i) {
        if (lightData.lights[i].posType.w != 0.0f) {
            continue; // only directional lights cast shadows for now
        }

        const glm::vec3 lightDir = glm::normalize(glm::vec3(lightData.lights[i].dirRadius));
        const glm::vec3 lightPos = sceneCenter - lightDir * lightDistance;

        glm::vec3 up(0.0f, 1.0f, 0.0f);
        if (glm::abs(glm::dot(up, lightDir)) > 0.99f) {
            up = glm::vec3(1.0f, 0.0f, 0.0f);
        }

        const glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, up);
        const glm::mat4 lightProj = glm::orthoRH_ZO(
            -sceneRadius, sceneRadius,
            -sceneRadius, sceneRadius,
            lightDistance - sceneRadius,
            lightDistance + sceneRadius);

        shadowData.lightViewProj[shadowCount] = lightProj * lightView;
        lightData.lights[i].shadowIndex = static_cast<int>(shadowCount);
        ++shadowCount;
    }

    rhi.WriteBuffer(lightUbo, &lightData, sizeof(LightData));

    rhi::BindGroupEntry lightEntry;
    lightEntry.binding = 1;
    lightEntry.buffer = lightUbo;
    cameraBindGroupDesc.entries.push_back(lightEntry);

    // one depth texture, one layer per shadow-casting light
    rhi::TextureDesc shadowMapDesc;
    shadowMapDesc.format = rhi::TextureFormat::Depth32Float;
    shadowMapDesc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::DepthStencil;
    shadowMapDesc.dimension = rhi::TextureDimension::Tex2D;
    shadowMapDesc.width = 1024;
    shadowMapDesc.height = 1024;
    shadowMapDesc.mipLevels = 1;
    shadowMapDesc.arrayLayers = MAX_SHADOWS;
    rhi::TextureHandle shadowMapTexture = rhi.CreateTexture(shadowMapDesc);

    // single-layer views: the render target for each light's depth pass
    std::vector<rhi::TextureViewHandle> shadowLayerViews;
    for (uint32_t s = 0; s < MAX_SHADOWS; ++s) {
        rhi::TextureViewDesc layerViewDesc;
        layerViewDesc.dimension = rhi::TextureViewDimension::Tex2D;
        layerViewDesc.format = shadowMapDesc.format;
        layerViewDesc.aspect = rhi::TextureAspect::Depth;
        layerViewDesc.baseMipLevel = 0;
        layerViewDesc.baseArrayLayer = s;
        layerViewDesc.mipLevels = 1;
        layerViewDesc.arrayLayers = 1;
        shadowLayerViews.push_back(rhi.CreateTextureView(layerViewDesc, shadowMapTexture));
    }

    // full-array view: what the fragment shader samples
    rhi::TextureViewDesc shadowArrayViewDesc;
    shadowArrayViewDesc.dimension = rhi::TextureViewDimension::Tex2DArray;
    shadowArrayViewDesc.format = shadowMapDesc.format;
    shadowArrayViewDesc.aspect = rhi::TextureAspect::Depth;
    shadowArrayViewDesc.baseMipLevel = 0;
    shadowArrayViewDesc.baseArrayLayer = 0;
    shadowArrayViewDesc.mipLevels = 1;
    shadowArrayViewDesc.arrayLayers = MAX_SHADOWS;
    rhi::TextureViewHandle shadowArrayView = rhi.CreateTextureView(shadowArrayViewDesc, shadowMapTexture);

    const rhi::SamplerHandle shadowSampler = rhi.CreateSampler({
        .addressU = rhi::AddressMode::ClampToEdge,
        .addressV = rhi::AddressMode::ClampToEdge,
        .compareEnabled = true,
        .compareOp = rhi::CompareOp::LessEqual,
    });

    rhi::BufferDesc shadowDataBufferDesc;
    shadowDataBufferDesc.size = sizeof(ShadowData);
    shadowDataBufferDesc.usage = rhi::BufferUsage::Uniform;
    shadowDataBufferDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
    rhi::BufferHandle shadowDataBuffer = rhi.CreateBuffer(shadowDataBufferDesc);
    rhi.WriteBuffer(shadowDataBuffer, &shadowData, sizeof(ShadowData));

    rhi::BindGroupEntry shadowDataEntry;
    shadowDataEntry.binding = 2;
    shadowDataEntry.buffer = shadowDataBuffer;
    cameraBindGroupDesc.entries.push_back(shadowDataEntry);

    rhi::BindGroupEntry shadowMapEntry;
    shadowMapEntry.binding = 3;
    shadowMapEntry.textureView = shadowArrayView;
    cameraBindGroupDesc.entries.push_back(shadowMapEntry);

    rhi::BindGroupEntry shadowSamplerEntry;
    shadowSamplerEntry.binding = 4;
    shadowSamplerEntry.sampler = shadowSampler;
    cameraBindGroupDesc.entries.push_back(shadowSamplerEntry);

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

    // compile the shadermodule for the shadow prepass vertex shader
    rhi::ShaderSource prepassvsSource;

    prepassvsSource.path = shaderDir / "shadow.vert.hlsl";
    prepassvsSource.stage = rhi::ShaderStageFlags::Vertex;
    prepassvsSource.entryPoint = "main";

    const rhi::ShaderModuleHandle prepassvs = rhi.CompileShader(prepassvsSource);

    //build the bind group layout for the shadow prepass
    std::vector<rhi::ShaderReflection> prepassReflection;
    prepassReflection.push_back(rhi.GetShaderModule(prepassvs)->reflection);
    auto prepassBindGroupLayoutDescs = rhi::BuildBindGroupLayoutDescs(prepassReflection);

    std::sort(prepassBindGroupLayoutDescs.begin(), prepassBindGroupLayoutDescs.end(),
        [](const rhi::BindGroupLayoutDesc& a, const rhi::BindGroupLayoutDesc& b) {
            return a.shaderBindings[0].set < b.shaderBindings[0].set;
        });

    std::vector<rhi::BindGroupLayoutHandle> prepassBindGroupLayouts;
    rhi::PipelineLayoutDesc prepassLayoutDesc;

    for (const auto &desc : prepassBindGroupLayoutDescs) {
        rhi::BindGroupLayoutHandle handle = rhi.CreateBindGroupLayout(desc);
        prepassBindGroupLayouts.push_back(handle);
        prepassLayoutDesc.layouts.push_back(handle);
    }

    rhi::PipelineLayoutHandle prepassLayout = rhi.CreatePipelineLayout(prepassLayoutDesc);

    // create the pipeline for the shadow prepass
    rhi::GraphicsPipelineDesc shadowPrepassPipelineDesc;
    shadowPrepassPipelineDesc.vertexShader = prepassvs;
    shadowPrepassPipelineDesc.layout = prepassLayout;
    shadowPrepassPipelineDesc.vertexBuffers.push_back({
        .stride = 32,
        .attributes = {
            {.bufferSlot = 0, .location = 0, .format = rhi::VertexFormat::Float32x3, .offset = 0},  //pos
        },
    });
    shadowPrepassPipelineDesc.depthStencil = {
        .format = rhi::TextureFormat::Depth32Float,
        .depthTestEnabled = true,
        .depthWriteEnabled = true,
        .depthFunc = rhi::CompareOp::Less,
    };

    auto shadowPrepassPipeline = rhi.CreateGraphicsPipeline(shadowPrepassPipelineDesc);

    // one small per-light UBO + bind group for the depth passes (matrix at offset 0)
    std::vector<rhi::BindGroupHandle> shadowPrepassBindGroups;
    for (uint32_t s = 0; s < shadowCount; ++s) {
        rhi::BufferDesc shadowPrepassBufferDesc;
        shadowPrepassBufferDesc.size = sizeof(ShadowPrepassUBO);
        shadowPrepassBufferDesc.usage = rhi::BufferUsage::Uniform;
        shadowPrepassBufferDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
        rhi::BufferHandle shadowPrepassBuffer = rhi.CreateBuffer(shadowPrepassBufferDesc);

        ShadowPrepassUBO shadowPrepassData{};
        shadowPrepassData.lightViewProjMatrix = shadowData.lightViewProj[s];
        rhi.WriteBuffer(shadowPrepassBuffer, &shadowPrepassData, sizeof(ShadowPrepassUBO));

        rhi::BindGroupDesc shadowPrepassBindGroupDesc;
        rhi::BindGroupEntry shadowPrepassEntry;
        shadowPrepassEntry.binding = 0;
        shadowPrepassEntry.buffer = shadowPrepassBuffer;
        shadowPrepassBindGroupDesc.entries.push_back(shadowPrepassEntry);
        shadowPrepassBindGroupDesc.layout = prepassBindGroupLayouts[0];
        shadowPrepassBindGroups.push_back(rhi.CreateBindGroup(shadowPrepassBindGroupDesc));
    }

    // AO depth prepass
    // Create depth prepass texture and view
    rhi::TextureDesc AODepthPrepassDesc;
    AODepthPrepassDesc.width = w;
    AODepthPrepassDesc.height = h;
    AODepthPrepassDesc.format = rhi::TextureFormat::Depth32Float;
    AODepthPrepassDesc.usage = rhi::TextureUsage::DepthStencil | rhi::TextureUsage::Sampled;
    rhi::TextureHandle AODepthPrepassTexture = rhi.CreateTexture(AODepthPrepassDesc);

    rhi::TextureViewDesc AODepthPrepassViewDesc;
    AODepthPrepassViewDesc.format = rhi::TextureFormat::Depth32Float;
    AODepthPrepassViewDesc.aspect = rhi::TextureAspect::Depth;
    const rhi::TextureViewHandle AODepthPrepassView = rhi.CreateTextureView(AODepthPrepassViewDesc, AODepthPrepassTexture);

    // Create the result AOTexture for use in the main lighting pass
    rhi::TextureDesc AOResultDesc;
    AOResultDesc.width = w;
    AOResultDesc.height = h;
    AOResultDesc.format = rhi::TextureFormat::R32Float;
    AOResultDesc.usage = rhi::TextureUsage::Sampled | rhi::TextureUsage::Storage;
    rhi::TextureHandle AOResultTexture = rhi.CreateTexture(AOResultDesc);

    rhi::TextureViewDesc AOResultViewDesc;
    AOResultViewDesc.format = rhi::TextureFormat::R32Float;
    const rhi::TextureViewHandle AOResultView = rhi.CreateTextureView(AOResultViewDesc, AOResultTexture);

    // the main lighting pass samples the AO result
    rhi::BindGroupEntry AOResultEntry;
    AOResultEntry.binding = 5;
    AOResultEntry.textureView = AOResultView;
    cameraBindGroupDesc.entries.push_back(AOResultEntry);

    cameraBindGroupDesc.layout = bindGroupLayouts[0];
    rhi::BindGroupHandle cameraBindGroup = rhi.CreateBindGroup(cameraBindGroupDesc);

    // Create the buffer for the camera data
    rhi::BufferDesc AOCameraBufferDesc;
    AOCameraBufferDesc.size = sizeof(ShadowPrepassUBO);
    AOCameraBufferDesc.usage = rhi::BufferUsage::Uniform;
    AOCameraBufferDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
    rhi::BufferHandle AOCameraBuffer = rhi.CreateBuffer(AOCameraBufferDesc);

    ShadowPrepassUBO AOPrepassData{};
    AOPrepassData.lightViewProjMatrix = cameraData.viewProjMatrix;
    rhi.WriteBuffer(AOCameraBuffer, &AOPrepassData, sizeof(ShadowPrepassUBO));

    // Create the bind group for the camera
    rhi::BindGroupDesc AOcameraBindGroupDesc;
    AOcameraBindGroupDesc.layout = prepassBindGroupLayouts[0];
    rhi::BindGroupEntry AOcameraEntry;
    AOcameraEntry.buffer = AOCameraBuffer;
    AOcameraEntry.binding = 0;
    AOcameraBindGroupDesc.entries.push_back(AOcameraEntry);
    const rhi::BindGroupHandle AOcameraBindGroup = rhi.CreateBindGroup(AOcameraBindGroupDesc);

    // AO compute pipeline
    rhi::ShaderSource AOComputeSource;
    AOComputeSource.path = shaderDir / "ssao.comp.hlsl";
    AOComputeSource.stage = rhi::ShaderStageFlags::Compute;
    AOComputeSource.entryPoint = "main";
    const rhi::ShaderModuleHandle AOComputeShader = rhi.CompileShader(AOComputeSource);

    std::vector<rhi::ShaderReflection> AOReflections;
    AOReflections.push_back(rhi.GetShaderModule(AOComputeShader)->reflection);

    auto AOBindGroupLayoutDescs = rhi::BuildBindGroupLayoutDescs(AOReflections);

    std::sort(AOBindGroupLayoutDescs.begin(), AOBindGroupLayoutDescs.end(),
        [](const rhi::BindGroupLayoutDesc& a, const rhi::BindGroupLayoutDesc& b) {
            return a.shaderBindings[0].set < b.shaderBindings[0].set;
        });

    std::vector<rhi::BindGroupLayoutHandle> AOBindGroupLayouts;
    rhi::PipelineLayoutDesc AOLayoutDesc;

    for (const auto &desc : AOBindGroupLayoutDescs) {
        rhi::BindGroupLayoutHandle handle = rhi.CreateBindGroupLayout(desc);
        AOBindGroupLayouts.push_back(handle);
        AOLayoutDesc.layouts.push_back(handle);
    }

    rhi::PipelineLayoutHandle AOLayout = rhi.CreatePipelineLayout(AOLayoutDesc);

    rhi::ComputePipelineDesc AOPipelineDesc;
    AOPipelineDesc.layout = AOLayout;
    AOPipelineDesc.computeShader = AOComputeShader;
    auto AOPipeline = rhi.CreateComputePipeline(AOPipelineDesc);

    rhi::BufferDesc AOUBOBufferDesc;
    AOUBOBufferDesc.size = sizeof(AOUBO);
    AOUBOBufferDesc.usage = rhi::BufferUsage::Uniform;
    AOUBOBufferDesc.memoryUsage = rhi::MemoryUsage::CpuToGpu;
    rhi::BufferHandle AOUBOBuffer = rhi.CreateBuffer(AOUBOBufferDesc);

    AOUBO AOData{};
    AOData.invProjection = glm::inverse(proj);
    AOData.resolution = glm::vec2(static_cast<float>(w), static_cast<float>(h));
    AOData.radius = 0.5f;
    AOData.strength = 1.5f;
    rhi.WriteBuffer(AOUBOBuffer, &AOData, sizeof(AOUBO));

    rhi::BindGroupDesc AOBindGroupDesc;
    AOBindGroupDesc.layout = AOBindGroupLayouts[0];

    rhi::BindGroupEntry AOUBOEntry;
    AOUBOEntry.binding = 0;
    AOUBOEntry.buffer = AOUBOBuffer;
    AOBindGroupDesc.entries.push_back(AOUBOEntry);

    rhi::BindGroupEntry AODepthEntry;
    AODepthEntry.binding = 1;
    AODepthEntry.textureView = AODepthPrepassView;
    AOBindGroupDesc.entries.push_back(AODepthEntry);

    rhi::BindGroupEntry AOOutputEntry;
    AOOutputEntry.binding = 2;
    AOOutputEntry.textureView = AOResultView;
    AOBindGroupDesc.entries.push_back(AOOutputEntry);

    const rhi::BindGroupHandle AOBindGroup = rhi.CreateBindGroup(AOBindGroupDesc);

    // AO debug view pipeline
    rhi::ShaderSource AODebugVSSource;
    AODebugVSSource.path = shaderDir / "ao_debug.vert.hlsl";
    AODebugVSSource.stage = rhi::ShaderStageFlags::Vertex;
    AODebugVSSource.entryPoint = "main";
    const rhi::ShaderModuleHandle AODebugVS = rhi.CompileShader(AODebugVSSource);

    rhi::ShaderSource AODebugFSSource;
    AODebugFSSource.path = shaderDir / "ao_debug.frag.hlsl";
    AODebugFSSource.stage = rhi::ShaderStageFlags::Fragment;
    AODebugFSSource.entryPoint = "main";
    const rhi::ShaderModuleHandle AODebugFS = rhi.CompileShader(AODebugFSSource);

    std::vector<rhi::ShaderReflection> AODebugReflections;
    AODebugReflections.push_back(rhi.GetShaderModule(AODebugVS)->reflection);
    AODebugReflections.push_back(rhi.GetShaderModule(AODebugFS)->reflection);

    auto AODebugBindGroupLayoutDescs = rhi::BuildBindGroupLayoutDescs(AODebugReflections);

    std::sort(AODebugBindGroupLayoutDescs.begin(), AODebugBindGroupLayoutDescs.end(),
        [](const rhi::BindGroupLayoutDesc& a, const rhi::BindGroupLayoutDesc& b) {
            return a.shaderBindings[0].set < b.shaderBindings[0].set;
        });

    std::vector<rhi::BindGroupLayoutHandle> AODebugBindGroupLayouts;
    rhi::PipelineLayoutDesc AODebugLayoutDesc;

    for (const auto& desc : AODebugBindGroupLayoutDescs) {
        rhi::BindGroupLayoutHandle handle = rhi.CreateBindGroupLayout(desc);
        AODebugBindGroupLayouts.push_back(handle);
        AODebugLayoutDesc.layouts.push_back(handle);
    }

    rhi::PipelineLayoutHandle AODebugLayout = rhi.CreatePipelineLayout(AODebugLayoutDesc);

    rhi::GraphicsPipelineDesc AODebugPipelineDesc;
    AODebugPipelineDesc.layout = AODebugLayout;
    AODebugPipelineDesc.vertexShader = AODebugVS;
    AODebugPipelineDesc.fragmentShader = AODebugFS;
    AODebugPipelineDesc.colorTargets.push_back({ .format = rhi::TextureFormat::RGBA8Unorm });
    auto AODebugPipeline = rhi.CreateGraphicsPipeline(AODebugPipelineDesc);

    rhi::BindGroupDesc AODebugBindGroupDesc;
    AODebugBindGroupDesc.layout = AODebugBindGroupLayouts[0];

    rhi::BindGroupEntry AODebugTextureEntry;
    AODebugTextureEntry.binding = 0;
    AODebugTextureEntry.textureView = AOResultView;
    AODebugBindGroupDesc.entries.push_back(AODebugTextureEntry);

    const rhi::BindGroupHandle AODebugBindGroup = rhi.CreateBindGroup(AODebugBindGroupDesc);

    bool showAO = false;
    bool enableAO = true;
    bool lastEnableAO = enableAO;

    while (!glfwWindowShouldClose(window)) {
        auto frameStart = std::chrono::steady_clock::now();
        const double deltaTime = std::chrono::duration<double>(
            frameStart - lastFrameTime).count();
        lastFrameTime = frameStart;

        auto acquired = rhi.AcquireNextImage(swapchain);

        if (acquired.status != rhi::AcquireStatus::Success
            && acquired.status != rhi::AcquireStatus::Suboptimal
        ) {
            continue;
        }

        auto cmd = rhi.BeginCommandList();

        // AO depth prepass
        rhi::RenderPassDesc AODepthPrepassPass{};
        AODepthPrepassPass.depth.enabled = true;
        AODepthPrepassPass.depth.view = AODepthPrepassView;
        AODepthPrepassPass.depth.clearDepth = true;
        AODepthPrepassPass.depth.clearDepthValue = 1.0f;
        rhi.BeginRenderPass(cmd, AODepthPrepassPass);
        rhi.SetPipeline(cmd, shadowPrepassPipeline);
        rhi.SetViewport(cmd, 0, 0, w, h);
        rhi.SetScissor(cmd, 0, 0, w, h);
        rhi.SetBindGroup(cmd, 0, AOcameraBindGroup);
        rhi.SetVertexBuffer(cmd, 0, model.vertexBuffer);
        rhi.SetIndexBuffer(cmd, model.indexBuffer, rhi::IndexFormat::Uint32);
        for (size_t d = 0; d < model.drawables.size(); ++d) {
            const auto& submesh = model.primitivesList[model.drawables[d].submeshIdx];
            rhi.SetBindGroup(cmd, 1, modelBindGroups[d]);
            rhi.DrawIndexed(cmd, submesh.indexCount, 1, submesh.firstIndex, submesh.baseVertex, 0);
        }
        rhi.EndPass(cmd);

        if (enableAO != lastEnableAO) {
            // strength 0 makes the pass write 1.0 everywhere, i.e. no occlusion
            AOData.strength = enableAO ? 1.5f : 0.0f;
            rhi.WriteBuffer(AOUBOBuffer, &AOData, sizeof(AOUBO));
            lastEnableAO = enableAO;
        }

        // The AO Pass itself
        rhi.BeginComputePass(cmd);
        rhi.SetPipeline(cmd, AOPipeline);
        rhi.SetBindGroup(cmd, 0, AOBindGroup);
        rhi.Dispatch(cmd, (w + 7) / 8, (h + 7) / 8, 1);
        rhi.EndPass(cmd);

        // shadow prepass: one depth-only pass per shadow-casting light
        for (size_t s = 0; s < shadowPrepassBindGroups.size(); ++s) {
            rhi::RenderPassDesc shadowpass{};
            shadowpass.depth.enabled = true;
            shadowpass.depth.view = shadowLayerViews[s];
            shadowpass.depth.clearDepth = true;
            shadowpass.depth.clearDepthValue = 1.0f;
            rhi.BeginRenderPass(cmd, shadowpass);
            rhi.SetPipeline(cmd, shadowPrepassPipeline);
            rhi.SetViewport(cmd, 0, 0, 1024.0f, 1024.0f);
            rhi.SetScissor(cmd, 0, 0, 1024, 1024);
            rhi.SetBindGroup(cmd, 0, shadowPrepassBindGroups[s]);
            rhi.SetVertexBuffer(cmd, 0, model.vertexBuffer);
            rhi.SetIndexBuffer(cmd, model.indexBuffer, rhi::IndexFormat::Uint32);
            for (size_t d = 0; d < model.drawables.size(); ++d) {
                const auto& submesh = model.primitivesList[model.drawables[d].submeshIdx];
                rhi.SetBindGroup(cmd, 1, modelBindGroups[d]);
                rhi.DrawIndexed(cmd, submesh.indexCount, 1, submesh.firstIndex, submesh.baseVertex, 0);
            }
            rhi.EndPass(cmd);
        }

        // lighting pass
        rhi::RenderPassDesc imagepass{};
        imagepass.swapchain = swapchain;
        imagepass.imageIndex = acquired.imageIndex;
        imagepass.colors.push_back({
            .useSwapchain = true,
            .clearColor = {0.0f, 0.0f, 0.0f, 1.0f},
            .clear = true
        });
        imagepass.depth.enabled = true;
        imagepass.depth.view = depthView;
        imagepass.depth.clearDepth = true;
        imagepass.depth.clearDepthValue = 1.0f;
        rhi.BeginRenderPass(cmd, imagepass);
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

        if (showAO) {
            rhi::RenderPassDesc aodebugpass{};
            aodebugpass.swapchain = swapchain;
            aodebugpass.imageIndex = acquired.imageIndex;
            aodebugpass.colors.push_back({
                .useSwapchain = true,
                .clearColor = {0.0f, 0.0f, 0.0f, 1.0f},
                .clear = true
            });
            rhi.BeginRenderPass(cmd, aodebugpass);
            rhi.SetPipeline(cmd, AODebugPipeline);
            rhi.SetViewport(cmd, 0, 0, (float)w, (float)h);
            rhi.SetScissor(cmd, 0, 0, w, h);
            rhi.SetBindGroup(cmd, 0, AODebugBindGroup);
            rhi.Draw(cmd, 3, 1, 0, 0);
            rhi.EndPass(cmd);
        }

        // Screen-space UI pass on the same swapchain image, then submit both.
        imgui.NewFrame(deltaTime, static_cast<uint32_t>(fbw), static_cast<uint32_t>(fbh));

        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Renderer", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Drawables: %d", static_cast<int>(model.drawables.size()));
            ImGui::Text("Frame time: %.2f ms (%.1f FPS)", deltaTime * 1000.0, deltaTime > 0.0 ? 1.0 / deltaTime : 0.0);
            ImGui::Text("Lights: %d | Shadows: %d", lightCount, shadowCount);
            ImGui::Checkbox("Enable AO", &enableAO);
            ImGui::Checkbox("Show AO texture", &showAO);
        }
        ImGui::End();

        imgui.Render(rhi, cmd, acquired.imageIndex);

        rhi.Submit({&cmd, 1});
        rhi.Present(swapchain);
        glfwPollEvents();
    }

    imgui.Shutdown(rhi);
    glfwTerminate();
    rhi.Shutdown();
    return 0;
}
