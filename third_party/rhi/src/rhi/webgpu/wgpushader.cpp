#include "rhi/resources/shader.h"
#include "rhi/webgpu/wgpurhi.h"
#include "third_party/spirv-reflect/spirv_reflect.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

inline void MarkDepthComparisonSampling(
    const std::uint32_t* words, std::size_t wordCount,
    rhi::ShaderReflection& reflection)
{
    constexpr std::uint32_t Invalid = 0xFFFFFFFFu;
    constexpr std::uint16_t OpLoad = 61;
    constexpr std::uint16_t OpDecorate = 71;
    constexpr std::uint16_t OpSampledImage = 86;
    constexpr std::uint16_t OpImageSampleImplicit = 87;
    constexpr std::uint16_t OpImageSampleExplicit = 88;
    constexpr std::uint16_t OpDrefImplicit = 89;
    constexpr std::uint16_t OpDrefExplicit = 90;
    constexpr std::uint16_t OpImageFetch = 95;
    constexpr std::uint16_t OpImageGather = 96;
    constexpr std::uint16_t OpDrefGather = 97;
    constexpr std::uint16_t OpImageSparseGather = 98;
    constexpr std::uint32_t DecoBinding = 33;
    constexpr std::uint32_t DecoDescriptorSet = 34;

    if (words == nullptr || wordCount < 5 || words[0] != 0x07230203u)
        return;
    const std::uint32_t bound = words[3];
    if (bound == 0)
        return;

    std::vector<std::uint32_t> setOf(bound, Invalid);
    std::vector<std::uint32_t> bindingOf(bound, Invalid);
    std::vector<std::uint32_t> loadedFrom(bound, Invalid);
    std::vector<std::uint32_t> sampledImage(bound, Invalid);
    std::vector<std::uint32_t> sampledSampler(bound, Invalid);
    std::vector<std::uint32_t> drefUses;
    std::vector<std::uint32_t> filterUses;
    std::vector<std::uint32_t> fetchUses;

    for (int pass = 0; pass < 2; ++pass)
    {
        std::size_t i = 5;
        while (i < wordCount)
        {
            const std::uint16_t op = static_cast<std::uint16_t>(words[i] & 0xFFFFu);
            const std::uint16_t count = static_cast<std::uint16_t>(words[i] >> 16);
            if (count == 0 || i + count > wordCount)
                break;
            const std::uint32_t* ops = words + i + 1;
            const std::size_t n = static_cast<std::size_t>(count) - 1;

            if (pass == 0)
            {
                if (op == OpDecorate && n >= 2 && ops[0] < bound)
                {
                    if (ops[1] == DecoDescriptorSet)
                        setOf[ops[0]] = ops[2];
                    else if (ops[1] == DecoBinding)
                        bindingOf[ops[0]] = ops[2];
                }
                else if (op == OpLoad && n >= 3 && ops[1] < bound && ops[2] < bound)
                {
                    loadedFrom[ops[1]] = ops[2];
                }
                else if (op == OpSampledImage && n >= 4 && ops[1] < bound &&
                         ops[2] < bound && ops[3] < bound)
                {
                    sampledImage[ops[1]] = ops[2];
                    sampledSampler[ops[1]] = ops[3];
                }
                else if ((op == OpDrefImplicit || op == OpDrefExplicit) &&
                         n >= 3 && ops[2] < bound)
                {
                    drefUses.push_back(ops[2]);
                }
                else if ((op == OpImageSampleImplicit || op == OpImageSampleExplicit ||
                          op == OpImageGather || op == OpDrefGather ||
                          op == OpImageSparseGather) &&
                         n >= 3 && ops[2] < bound)
                {
                    if (op == OpDrefGather)
                        drefUses.push_back(ops[2]);
                    else
                        filterUses.push_back(ops[2]);
                }
                else if (op == OpImageFetch && n >= 3 && ops[2] < bound)
                {
                    fetchUses.push_back(ops[2]);
                }
            }
            i += count;
        }
    }

    auto resolveImage = [&](std::uint32_t sampledImageId) {
        std::uint32_t image = Invalid;
        if (sampledImageId < bound && sampledImage[sampledImageId] != Invalid)
            image = (loadedFrom[sampledImage[sampledImageId]] != Invalid)
                ? loadedFrom[sampledImage[sampledImageId]] : sampledImage[sampledImageId];
        return image;
    };

    for (std::uint32_t use : drefUses)
    {
        std::uint32_t image = resolveImage(use);
        std::uint32_t sampler = Invalid;
        if (sampledSampler[use] != Invalid)
            sampler = (loadedFrom[sampledSampler[use]] != Invalid)
                ? loadedFrom[sampledSampler[use]] : sampledSampler[use];

        for (auto& candidate : reflection.bindings)
        {
            if (image != Invalid && setOf[image] == candidate.set &&
                bindingOf[image] == candidate.binding &&
                candidate.type == rhi::BindingType::SampledTexture)
            {
                candidate.depthSampled = true;
            }
            if (sampler != Invalid && setOf[sampler] == candidate.set &&
                bindingOf[sampler] == candidate.binding &&
                candidate.type == rhi::BindingType::Sampler)
            {
                candidate.comparison = true;
            }
        }
    }

    std::vector<bool> imageFiltered(bound, false);
    for (std::uint32_t use : filterUses)
    {
        const std::uint32_t image = resolveImage(use);
        if (image != Invalid)
            imageFiltered[image] = true;
    }
    for (std::uint32_t use : fetchUses)
    {
        if (use >= bound)
            continue;
        const std::uint32_t image =
            (loadedFrom[use] != Invalid) ? loadedFrom[use] : use;
        if (imageFiltered[image])
            continue;
        for (auto& candidate : reflection.bindings)
        {
            if (setOf[image] == candidate.set &&
                bindingOf[image] == candidate.binding &&
                candidate.type == rhi::BindingType::SampledTexture &&
                !candidate.depthSampled)
            {
                candidate.unfilterableSampled = true;
            }
        }
    }
}

rhi::ShaderModuleHandle WebGPURHI::CompileShader(rhi::ShaderSource source)
{
    rhi::ShaderModule module;

    const std::filesystem::path inputName = source.path.filename();
    const std::string filename = inputName.string();
    const std::string suffix = ".hlsl";
    if (filename.size() <= suffix.size() ||
        filename.compare(filename.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return {};
    }

    const std::string stem = filename.substr(0, filename.size() - suffix.size());
    // Build-time artifacts (.wgsl + .spv) are siblings of the copied .hlsl:
    // <build>/shaders/<stem>.wgsl and <build>/shader_tools/<stem>.spv.
    // Derive them from source.path (absolute via RHI_SHADER_DIR) instead of
    // CWD-relative literals so the exe runs from any working directory.
    const std::filesystem::path dir = source.path.parent_path();
    const std::filesystem::path wgslPath = dir / (stem + ".wgsl");

    const std::filesystem::path spirvPath =
        dir.parent_path() / "shader_tools" / (stem + ".spv");

    std::ifstream wgslFile(wgslPath, std::ios::binary);
    if (!wgslFile) {
        return {};
    }

    module.bytecode.assign(
        std::istreambuf_iterator<char>(wgslFile),
        std::istreambuf_iterator<char>());

    std::ifstream spirvFile(spirvPath, std::ios::binary);
    if (!spirvFile) {
        module.bytecode.clear();
        return {};
    }

    std::vector<std::uint8_t> spirv{
        std::istreambuf_iterator<char>(spirvFile),
        std::istreambuf_iterator<char>()};
    if (spirv.empty()) {
        module.bytecode.clear();
        return {};
    }

    SpvReflectShaderModule reflection{};
    const SpvReflectResult result = spvReflectCreateShaderModule(
        spirv.size(),
        spirv.data(),
        &reflection);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        module.bytecode.clear();
        return {};
    }

    module.reflection = ParseReflection(&reflection, source.stage);
    MarkDepthComparisonSampling(
        reinterpret_cast<const std::uint32_t*>(spirv.data()),
        spirv.size() / sizeof(std::uint32_t),
        module.reflection);
    spvReflectDestroyShaderModule(&reflection);
    module.entryPoint = source.entryPoint.empty() ? "main" : source.entryPoint;

    rhi::ShaderModuleHandle handle;
    if (!m_FreeShaderModules.empty()) {
        handle.id = m_FreeShaderModules.back();
        m_FreeShaderModules.pop_back();
        m_ShaderModules[handle.id] = module;
    } else {
        handle.id = static_cast<uint64_t>(m_ShaderModules.size());
        m_ShaderModules.push_back(module);
    }
    return handle;
}

void WebGPURHI::DestroyShaderModule(rhi::ShaderModuleHandle handle)
{
    if (handle.id >= m_ShaderModules.size() ||
        m_ShaderModules[handle.id].bytecode.empty()) {
        return;
    }
    m_ShaderModules[handle.id] = {};
    m_FreeShaderModules.push_back(handle.id);
}

std::optional<rhi::ShaderModule> WebGPURHI::GetShaderModule(rhi::ShaderModuleHandle handle) const
{
    if (handle.id >= m_ShaderModules.size() ||
        m_ShaderModules[handle.id].bytecode.empty()) {
        return std::nullopt;
    }
    return m_ShaderModules[handle.id];
}
