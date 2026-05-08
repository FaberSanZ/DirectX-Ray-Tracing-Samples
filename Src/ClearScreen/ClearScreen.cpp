// ClearScreen.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include <iostream>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include "../Common/ShaderCompiler.h"
#include <DirectXMath.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

#define align_to(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)


class RenderSystem
{
public:

    uint32_t width = 1280;
    uint32_t height = 820;
    uint32_t frameCount{ 2 };

    ID3D12Device5* pDevice;
    ID3D12CommandQueue* mpCmdQueue;
    IDXGISwapChain3* mpSwapChain;

    ID3D12CommandAllocator* pCmdAllocator;
    ID3D12GraphicsCommandList4* mpCmdList;

    ID3D12Fence* mpFence;
    HANDLE mFenceEvent;
    uint64_t mFenceValue = 0;

    ID3D12Resource* pSwapChainBuffer[3];
    ID3D12Resource* mpOutputResource;
    ID3D12Resource* mpShaderTable;

    ID3D12StateObject* mpPipelineState;
    ID3D12RootSignature* m_RootSig;
    uint32_t mShaderTableEntrySize = 0;

    ID3D12DescriptorHeap* m_SrvUavHeap;
    ID3D12DescriptorHeap* m_RenderTargetViewHeap;

    const WCHAR* kRayGenShader = L"rayGen";
    ShaderCompiler shaderCompiler{};


    void Initialize(HWND handle, uint32_t w, uint32_t h)
    {

        // Create the DXGI factory
        IDXGIFactory4* pDxgiFactory;
        CreateDXGIFactory1(IID_PPV_ARGS(&pDxgiFactory));

        D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&pDevice));

        D3D12_COMMAND_QUEUE_DESC cqDesc = {};
        cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        pDevice->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&mpCmdQueue));

        // Create the command allocator
        pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pCmdAllocator));

        // Describe and create the swap chain.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.BufferCount = frameCount;
        swapChainDesc.Width = width;
        swapChainDesc.Height = height;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.SampleDesc.Count = 1;

        IDXGISwapChain1* pSwapChain;
        HRESULT hr = pDxgiFactory->CreateSwapChainForHwnd(mpCmdQueue, handle, &swapChainDesc, nullptr, nullptr, &pSwapChain);
        pSwapChain->QueryInterface(IID_PPV_ARGS(&mpSwapChain));


        // Create descriptor heap for render target views
        m_RenderTargetViewHeap = createDescriptorHeap(pDevice, frameCount, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, false);

        // Create descriptor heap for shader resource views and unordered access views. We need at least 2 descriptors: one for the output texture (UAV) and one for the TLAS (SRV)
        m_SrvUavHeap = createDescriptorHeap(pDevice, 1, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);


        auto m_DescriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        for (uint32_t i = 0; i < frameCount; ++i)
        {
            ID3D12Resource* backBuffer = nullptr;
            mpSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RenderTargetViewHeap->GetCPUDescriptorHandleForHeapStart();
            rtvHandle.ptr += i * m_DescriptorSize;

            pDevice->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);
            pSwapChainBuffer[i] = backBuffer;
        }

        // Create the command-list
        pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pCmdAllocator, nullptr, IID_PPV_ARGS(&mpCmdList));

        // Create a fence and the event
        pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mpFence));
        mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);


        createRtPipelineState(); // create the raytracing pipeline state object with the raygen, miss and hit shaders
        createShaderResourceUAV(); // create the output resource and the UAV for it
        createShaderTable(); // create the shader table with the required entries for raygen, miss and hit shaders, and fill it with the shader identifiers and the root arguments
    }

    ID3D12DescriptorHeap* createDescriptorHeap(ID3D12Device5* pDevice, uint32_t count, D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible)
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = count;
        desc.Type = type;
        desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        ID3D12DescriptorHeap* pHeap;
        pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap));
        return pHeap;
    }

    uint64_t submitCommandList(ID3D12GraphicsCommandList4* pCmdList, ID3D12CommandQueue* pCmdQueue, ID3D12Fence* pFence, uint64_t fenceValue)
    {
        pCmdList->Close();
        ID3D12CommandList* cmds = pCmdList;
        pCmdQueue->ExecuteCommandLists(1, &cmds);
        fenceValue++;
        pCmdQueue->Signal(pFence, fenceValue);
        return fenceValue;
    }

    ID3D12Resource* createBuffer(ID3D12Device5* pDevice, uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps)
    {
        D3D12_RESOURCE_DESC bufDesc = {};
        bufDesc.Alignment = 0;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Flags = flags;
        bufDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufDesc.Height = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        bufDesc.MipLevels = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.SampleDesc.Quality = 0;
        bufDesc.Width = size;

        ID3D12Resource* pBuffer;
        pDevice->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer));
        return pBuffer;
    }




    void createRtPipelineState()
    {
        std::vector<D3D12_STATE_SUBOBJECT> subobjects;

        auto pShaderBlob = shaderCompiler.Compile(L"../../../../Assets/Shaders/ClearScreen/Mesh.hlsl", L"lib_6_3");

        const WCHAR* exports[] = { kRayGenShader };

        D3D12_EXPORT_DESC exportDesc = {};
        exportDesc.Name = kRayGenShader;

        D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
        dxilLibDesc.DXILLibrary.pShaderBytecode = pShaderBlob->GetBufferPointer();
        dxilLibDesc.DXILLibrary.BytecodeLength = pShaderBlob->GetBufferSize();
        dxilLibDesc.NumExports = 1;
        dxilLibDesc.pExports = &exportDesc;

        D3D12_STATE_SUBOBJECT dxilLibSO = {};
        dxilLibSO.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        dxilLibSO.pDesc = &dxilLibDesc;
        subobjects.push_back(dxilLibSO);

        // ROOT SIGNATURE (solo UAV)
        D3D12_DESCRIPTOR_RANGE uavRange = {};
        uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRange.NumDescriptors = 1;
        uavRange.BaseShaderRegister = 0;

        D3D12_ROOT_PARAMETER rootParam = {};
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParam.DescriptorTable.NumDescriptorRanges = 1;
        rootParam.DescriptorTable.pDescriptorRanges = &uavRange;

        D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
        rsDesc.NumParameters = 1;
        rsDesc.pParameters = &rootParam;

        ID3DBlob* rsBlob;
        ID3DBlob* errorBlob;
        D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &errorBlob);

        pDevice->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&m_RootSig));

        D3D12_GLOBAL_ROOT_SIGNATURE globalRS = {};
        globalRS.pGlobalRootSignature = m_RootSig;

        D3D12_STATE_SUBOBJECT rsSO = {};
        rsSO.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
        rsSO.pDesc = &globalRS;
        subobjects.push_back(rsSO);

        // SHADER CONFIG
        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
        shaderConfig.MaxPayloadSizeInBytes = 0;
        shaderConfig.MaxAttributeSizeInBytes = 0;

        D3D12_STATE_SUBOBJECT shaderConfigSO = {};
        shaderConfigSO.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        shaderConfigSO.pDesc = &shaderConfig;
        subobjects.push_back(shaderConfigSO);

        // PIPELINE CONFIG
        D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
        pipelineConfig.MaxTraceRecursionDepth = 1;

        D3D12_STATE_SUBOBJECT pipelineSO = {};
        pipelineSO.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        pipelineSO.pDesc = &pipelineConfig;
        subobjects.push_back(pipelineSO);

        // CREATE STATE OBJECT
        D3D12_STATE_OBJECT_DESC desc = {};
        desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        desc.NumSubobjects = (UINT)subobjects.size();
        desc.pSubobjects = subobjects.data();

        pDevice->CreateStateObject(&desc, IID_PPV_ARGS(&mpPipelineState));
    }

    void createShaderTable()
    {
        ID3D12StateObjectProperties* pRtsoProps;
        mpPipelineState->QueryInterface(IID_PPV_ARGS(&pRtsoProps));

        // Calcular tamaño
        mShaderTableEntrySize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        mShaderTableEntrySize = align_to(D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT, mShaderTableEntrySize);

        uint32_t shaderTableSize = mShaderTableEntrySize * 3; // RayGen, Miss, HitGroup

        D3D12_HEAP_PROPERTIES heapUpload;
        heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapUpload.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapUpload.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapUpload.CreationNodeMask = 0;
        heapUpload.VisibleNodeMask = 0;

        mpShaderTable = createBuffer(pDevice, shaderTableSize, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, heapUpload);

        uint8_t* pData;
        mpShaderTable->Map(0, nullptr, (void**)&pData);

        memcpy(pData, pRtsoProps->GetShaderIdentifier(kRayGenShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        mpShaderTable->Unmap(0, nullptr);
    }

    void createShaderResourceUAV()
    {
        UINT descriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // Create the output resource
        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.DepthOrArraySize = 1;
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        resDesc.Width = width;
        resDesc.Height = height;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resDesc.MipLevels = 1;
        resDesc.SampleDesc.Count = 1;

        D3D12_HEAP_PROPERTIES heapDefault;
        heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapDefault.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapDefault.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapDefault.CreationNodeMask = 0;
        heapDefault.VisibleNodeMask = 0;

        pDevice->CreateCommittedResource(&heapDefault, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&mpOutputResource));


        // Create UAV for the output resource in descriptor heap at index 1 (index 0 is for the TLAS SRV)
        D3D12_CPU_DESCRIPTOR_HANDLE uavCpuHandle = m_SrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        uavCpuHandle.ptr += 0 * descriptorSize;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        pDevice->CreateUnorderedAccessView(mpOutputResource, nullptr, &uavDesc, uavCpuHandle);
    }

    void Loop()
    {
        uint32_t rtvIndex = mpSwapChain->GetCurrentBackBufferIndex(); // Get the index of the current back buffer


        mpCmdList->SetComputeRootSignature(m_RootSig); // Set the global root signature. Since we only have a global root signature, we don't need to set a local root signature

        ID3D12DescriptorHeap* heaps[] = { m_SrvUavHeap };
        mpCmdList->SetDescriptorHeaps(1, heaps); // Set the descriptor heap. We only have one, but we could set multiple heaps if we wanted to

        D3D12_GPU_DESCRIPTOR_HANDLE uavHandle = m_SrvUavHeap->GetGPUDescriptorHandleForHeapStart();
        uavHandle.ptr += 0 * pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); // indice 1 for UAV (output)
        mpCmdList->SetComputeRootDescriptorTable(0, uavHandle); // Set the UAV for the output resource in the root signature. The shader will be able to access it using u0

        mpCmdList->SetPipelineState1(mpPipelineState); // Set the pipeline state


        D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
        raytraceDesc.Width = width;
        raytraceDesc.Height = height;
        raytraceDesc.Depth = 1;

        raytraceDesc.RayGenerationShaderRecord.StartAddress = mpShaderTable->GetGPUVirtualAddress();
        raytraceDesc.RayGenerationShaderRecord.SizeInBytes = mShaderTableEntrySize;

        raytraceDesc.MissShaderTable.StartAddress = 0;
        raytraceDesc.MissShaderTable.SizeInBytes = 0;
        raytraceDesc.MissShaderTable.StrideInBytes = 0;

        raytraceDesc.HitGroupTable.StartAddress = 0;
        raytraceDesc.HitGroupTable.SizeInBytes = 0;
        raytraceDesc.HitGroupTable.StrideInBytes = 0;

        mpCmdList->DispatchRays(&raytraceDesc);
        mpCmdList->CopyResource(pSwapChainBuffer[rtvIndex], mpOutputResource);

        mFenceValue = submitCommandList(mpCmdList, mpCmdQueue, mpFence, mFenceValue);
        mpSwapChain->Present(0, 0);


        pCmdAllocator->Reset();
        mpCmdList->Reset(pCmdAllocator, nullptr);
    }

    void Shutdown()
    {
        // Wait for the command queue to finish execution
        mFenceValue++;
        mpCmdQueue->Signal(mpFence, mFenceValue);
        mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
        WaitForSingleObject(mFenceEvent, INFINITE);
    }
};

int main()
{
    uint32_t width = 1280;
    uint32_t height = 820;
    const wchar_t title[] = L"DX12 ClearScreen";

    RenderSystem render {};

    WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_CLASSDC, DefWindowProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, title, nullptr };
    RegisterClassEx(&wcex);

    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hWnd = CreateWindow(title, title, WS_OVERLAPPEDWINDOW, 100, 100, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, nullptr, nullptr, wcex.hInstance, nullptr);
    ShowWindow(hWnd, SW_SHOW);

    render.Initialize(hWnd, width, height);

    MSG msg = { 0 };
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            render.Loop();
        }
    }

    render.Shutdown();
    return 0;
}
