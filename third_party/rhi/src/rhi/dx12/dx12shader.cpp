#include "rhi/dx12/dx12rhi.h"
#include "rhi/resources/shader.h"
#include "third_party/dxc/inc/dxcapi.h"
#include <d3dcommon.h>
#include <iterator>
#include <fstream>
#include <string>
#include <wrl/client.h>
#include <d3d12shader.h>

rhi::ShaderModuleHandle DX12RHI::CompileShader(rhi::ShaderSource source) {

    std::filesystem::path path = source.path;

    std::ifstream file(path, std::ios::binary);

    std::string shadersource{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };

    DxcBuffer buffer{};
    buffer.Ptr = shadersource.data();
    buffer.Size = shadersource.size();
    buffer.Encoding = DXC_CP_UTF8;

    std::wstring entryPoint(source.entryPoint.begin(), source.entryPoint.end());

    std::vector<LPCWSTR> args = {
        L"-E",
        entryPoint.c_str(),
        L"-Wno-ignored-attributes",
    };

    if (source.stage == rhi::ShaderStageFlags::Vertex) {
        args.push_back(L"-T");
        args.push_back(L"vs_6_6");
    }
    else if (source.stage == rhi::ShaderStageFlags::Fragment) {
        args.push_back(L"-T");
        args.push_back(L"ps_6_6");
    }
    else if (source.stage == rhi::ShaderStageFlags::Compute) {
        args.push_back(L"-T");
        args.push_back(L"cs_6_6");
    }

    Microsoft::WRL::ComPtr<IDxcUtils> utils;

    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.ReleaseAndGetAddressOf()));

    if (FAILED(hr)) {
        printf("Failed to create DXC utils: 0x%x\n", hr);
        return {};
    }

    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;

    hr = utils->CreateDefaultIncludeHandler(includeHandler.ReleaseAndGetAddressOf());

    if (FAILED(hr)) {
        printf("Failed to create DXC include handler: 0x%x\n", hr);
        return {};
    }

    Microsoft::WRL::ComPtr<IDxcCompiler3> compiler3;

    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler3));

    if (FAILED(hr)) {
        printf("Failed to create DXC compiler: 0x%x\n", hr);
        return {};
    }

    Microsoft::WRL::ComPtr<IDxcResult> result;

    hr = compiler3->Compile(
        &buffer,
        args.data(),
        static_cast<UINT32>(args.size()),
        includeHandler.Get(),
        IID_PPV_ARGS(&result)
    );

    if (FAILED(hr) || !result) {
        printf("Failed to compile shader: 0x%x\n", hr);
        return {};
    }

    result->GetStatus(&hr);

    if (FAILED(hr)) {
        return {};
    }

    Microsoft::WRL::ComPtr<IDxcBlobUtf8> errorMsgs;

    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorMsgs), nullptr);

    if (errorMsgs && errorMsgs->GetStringLength()) {
        printf("Shader compilation errors:\n%s\n", errorMsgs->GetStringPointer());
    }

    Microsoft::WRL::ComPtr<IDxcBlob> shaderobj;

    hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderobj), nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> reflection;

    hr = result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflection), nullptr);

    // now, we have to convert from dxc blob to rhi shader module
    rhi::ShaderModule module;
    module.bytecode.resize(shaderobj->GetBufferSize());
    memcpy(module.bytecode.data(), shaderobj->GetBufferPointer(), shaderobj->GetBufferSize());

    if (reflection) {
        DxcBuffer reflectionData{};
        reflectionData.Ptr = reflection->GetBufferPointer();
        reflectionData.Size = reflection->GetBufferSize();
        reflectionData.Encoding = 0;

        Microsoft::WRL::ComPtr<ID3D12ShaderReflection> shaderReflection;

        utils->CreateReflection(
            &reflectionData,
            IID_PPV_ARGS(&shaderReflection)
        );

        if (shaderReflection) {
            module.reflection = ParseReflection(shaderReflection.Get(), source.stage);
        }
    }
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

void DX12RHI::DestroyShaderModule(rhi::ShaderModuleHandle handle)
{
    if (handle.id >= m_ShaderModules.size() ||
        m_ShaderModules[handle.id].bytecode.empty()) {
        return;
    }
    m_ShaderModules[handle.id] = {};
    m_FreeShaderModules.push_back(handle.id);
}

std::optional<rhi::ShaderModule> DX12RHI::GetShaderModule(rhi::ShaderModuleHandle handle) const
{
    if (handle.id >= m_ShaderModules.size() ||
        m_ShaderModules[handle.id].bytecode.empty()) {
        return std::nullopt;
    }
    return m_ShaderModules[handle.id];
}
