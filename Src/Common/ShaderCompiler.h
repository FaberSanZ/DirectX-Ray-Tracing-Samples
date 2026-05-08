#pragma once

#include <wrl/client.h>
#include <string>
#include <vector>
#include <dxcapi.h>
#include <d3dcommon.h>
#include <filesystem>

#ifndef DX12_SAMPLE_ASSET_DIR
#define DX12_SAMPLE_ASSET_DIR L"../../Assets"
#endif

inline std::wstring ResolveShaderPath(const std::wstring& filename)
{
    if (std::filesystem::exists(filename))
        return filename;

    const size_t assetsPos = filename.find(L"Assets");
    if (assetsPos != std::wstring::npos)
    {
        std::wstring relative = filename.substr(assetsPos + 6);
        while (!relative.empty() && (relative.front() == L'/' || relative.front() == L'\\'))
            relative.erase(relative.begin());

        std::filesystem::path resolved = std::filesystem::path(DX12_SAMPLE_ASSET_DIR) / relative;
        if (std::filesystem::exists(resolved))
            return resolved.wstring();
    }

    return filename;
}

class ShaderCompiler
{
public:
    ShaderCompiler()
    {

    }

    ID3DBlob* Compile(const WCHAR* filename, const WCHAR* targetString)
    {
        const std::wstring resolvedFilename = ResolveShaderPath(filename);
        IDxcCompiler* pCompiler = nullptr;
        IDxcLibrary* pLibrary = nullptr;

        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
        DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&pLibrary));

        IDxcBlobEncoding* pTextBlob = nullptr;
        HRESULT hr = pLibrary->CreateBlobFromFile(resolvedFilename.c_str(), nullptr, &pTextBlob);

        if (FAILED(hr))
        {
            OutputDebugStringA("Error\n");
            return nullptr;
        }

        IDxcIncludeHandler* pIncludeHandler = nullptr;
        pLibrary->CreateIncludeHandler(&pIncludeHandler);

        IDxcOperationResult* pResult = nullptr;

        pCompiler->Compile(
            pTextBlob,
            resolvedFilename.c_str(),
            L"",
            targetString,
            nullptr,
            0,
            nullptr,
            0,
            pIncludeHandler,
            &pResult
        );

        HRESULT resultCode;
        pResult->GetStatus(&resultCode);

        if (FAILED(resultCode))
        {
            IDxcBlobEncoding* pErrors;
            pResult->GetErrorBuffer(&pErrors);

            if (pErrors)
            {
                OutputDebugStringA((char*)pErrors->GetBufferPointer());
            }

            return nullptr;
        }

        IDxcBlob* pBlob = nullptr;
        pResult->GetResult(&pBlob);

        return reinterpret_cast<ID3DBlob*>(pBlob);
    }

private:


};

namespace Core
{
    class ShaderCompilerDXC
    {
    public:
        ShaderCompilerDXC()
        {
            DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
            DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
            DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
        }

        IDxcBlob* Compile(const std::wstring& shaderPath, const std::wstring& entryPoint, const std::wstring& targetProfile)
        {
            const std::wstring resolvedPath = ResolveShaderPath(shaderPath);

            IDxcBlobEncoding* source = nullptr;
            utils->LoadFile(resolvedPath.c_str(), nullptr, &source);

            IDxcIncludeHandler* includeHandler = nullptr;
            utils->CreateDefaultIncludeHandler(&includeHandler);

            LPCWSTR args[] =
            {
                resolvedPath.c_str(),
                L"-E", entryPoint.c_str(),
                L"-T", targetProfile.c_str(),
                L"-Zi", L"-Qembed_debug",
                L"-Od"
            };

            Microsoft::WRL::ComPtr<IDxcOperationResult> result;
            compiler->Compile(
                source,
                resolvedPath.c_str(),
                entryPoint.c_str(),
                targetProfile.c_str(),
                args,
                _countof(args),
                nullptr,
                0,
                includeHandler,
                &result
            );

            HRESULT status;
            result->GetStatus(&status);
            if (FAILED(status))
            {
                IDxcBlobEncoding* errors = nullptr;
                result->GetErrorBuffer(&errors);
                if (errors)
                    OutputDebugStringA(static_cast<char*>(errors->GetBufferPointer()));
                return nullptr;
            }

            IDxcBlob* shaderBlob = nullptr;
            result->GetResult(&shaderBlob);
            return shaderBlob;
        }

    private:
        IDxcCompiler* compiler = nullptr;
        IDxcLibrary* library = nullptr;
        IDxcUtils* utils = nullptr;
    };
}


