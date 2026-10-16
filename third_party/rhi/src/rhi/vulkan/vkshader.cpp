#include "rhi/resources/shader.h"
#include "rhi/vulkan/vkrhi.h"
#include "third_party/spirv-reflect/spirv_reflect.h"

#include "third_party/dxc/inc/dxcapi.h"
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <fstream>
#include <string>

template <typename T>
struct DxPtr {
    T* ptr = nullptr;
    ~DxPtr() { if (ptr != nullptr) ptr->Release(); }
    DxPtr() = default;
    DxPtr(const DxPtr&) = delete;
    DxPtr& operator=(const DxPtr&) = delete;
    T* Get() const { return ptr; }
    T** AddressOf() { return &ptr; }
    T* operator->() const { return ptr; }
    explicit operator bool() const { return ptr != nullptr; }
};

std::wstring WidenUtf8(const std::string& text)
{
#ifdef _WIN32
    return std::wstring(text.begin(), text.end());
#else
    std::mbstate_t state{};
    const char* src = text.c_str();
    const std::size_t length = std::mbsrtowcs(nullptr, &src, 0, &state);
    std::wstring wide(length, L'\0');
    std::mbsrtowcs(wide.data(), &src, length + 1, &state);
    return wide;
#endif
}


rhi::ShaderModuleHandle VulkanRHI::CompileShader(rhi::ShaderSource source) {

    std::filesystem::path path = source.path;

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        printf("Failed to open shader file: %s\n", path.string().c_str());
        return {};
    }

    std::string shadersource{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };

    DxcBuffer buffer{};
    buffer.Ptr = shadersource.data();
    buffer.Size = shadersource.size();
    buffer.Encoding = DXC_CP_UTF8;

    std::wstring entryPoint = WidenUtf8(source.entryPoint);

    std::vector<LPCWSTR> args = {
        L"-E",
        entryPoint.c_str(),
        L"-spirv",
        L"-fspv-target-env=vulkan1.3",
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

    DxPtr<IDxcUtils> utils;

    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.AddressOf()));

    if (FAILED(hr)) {
        printf("Failed to create DXC utils: 0x%x\n", static_cast<unsigned>(hr));
        return {};
    }

    DxPtr<IDxcIncludeHandler> includeHandler;

    hr = utils->CreateDefaultIncludeHandler(includeHandler.AddressOf());

    if (FAILED(hr)) {
        printf("Failed to create DXC include handler: 0x%x\n", static_cast<unsigned>(hr));
        return {};
    }

    DxPtr<IDxcCompiler3> compiler3;

    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler3.AddressOf()));

    if (FAILED(hr)) {
        printf("Failed to create DXC compiler: 0x%x\n", static_cast<unsigned>(hr));
        return {};
    }

    DxPtr<IDxcResult> result;

    hr = compiler3->Compile(
        &buffer,
        args.data(),
        static_cast<UINT32>(args.size()),
        includeHandler.Get(),
        IID_PPV_ARGS(result.AddressOf())
    );

    if (FAILED(hr) || !result) {
        printf("Failed to compile shader: 0x%x\n", static_cast<unsigned>(hr));
        return {};
    }

    result->GetStatus(&hr);

    if (FAILED(hr)) {
        DxPtr<IDxcBlobUtf8> errors;
        if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS,         IID_PPV_ARGS(errors.AddressOf()), nullptr)) &&
            errors && errors->GetStringLength() > 0) {
            printf("Shader compilation failed:\n%s\n", errors->GetStringPointer());
        }
        return {};
    }

    DxPtr<IDxcBlobUtf8> errorMsgs;

    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errorMsgs.AddressOf()), nullptr);

    if (errorMsgs && errorMsgs->GetStringLength()) {
        printf("Shader compilation errors:\n%s\n", errorMsgs->GetStringPointer());
    }

    DxPtr<IDxcBlob> shaderobj;

    hr = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(shaderobj.AddressOf()), nullptr);
    if (FAILED(hr) || !shaderobj || shaderobj->GetBufferSize() == 0) {
        printf("Shader compilation produced no object\n");
        return {};
    }

    // now, we have to convert from dxc blob to rhi shader module
    rhi::ShaderModule module;
    module.bytecode.resize(shaderobj->GetBufferSize());
    memcpy(module.bytecode.data(), shaderobj->GetBufferPointer(), shaderobj->GetBufferSize());

    SpvReflectShaderModule reflectModule{};

    SpvReflectResult reflectionResult = spvReflectCreateShaderModule(
        module.bytecode.size(),
        module.bytecode.data(),
        &reflectModule);
    if (reflectionResult != SPV_REFLECT_RESULT_SUCCESS) {
        printf("Failed to reflect shader module\n");
        return {};
    }

    rhi::ShaderReflection shaderReflection = ParseReflection(&reflectModule, source.stage);

    module.reflection = shaderReflection;
    module.entryPoint = source.entryPoint.empty() ? "main" : source.entryPoint;

    spvReflectDestroyShaderModule(&reflectModule);

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

void VulkanRHI::DestroyShaderModule(rhi::ShaderModuleHandle handle)
{
    if (handle.id >= m_ShaderModules.size() ||
        m_ShaderModules[handle.id].bytecode.empty()) {
        return;
    }
    m_ShaderModules[handle.id] = {};
    m_FreeShaderModules.push_back(handle.id);
}

std::optional<rhi::ShaderModule> VulkanRHI::GetShaderModule(rhi::ShaderModuleHandle handle) const
{
    if (handle.id >= m_ShaderModules.size() ||
        m_ShaderModules[handle.id].bytecode.empty()) {
        return std::nullopt;
    }
    return m_ShaderModules[handle.id];
}
