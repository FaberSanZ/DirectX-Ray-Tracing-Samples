// IndexBuffer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include <iostream>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <DirectXMath.h>
#include "../Common/ShaderCompiler.h"

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
    ID3D12Resource* mpIndexBuffer;
    ID3D12Resource* mpVertexBuffer;
    ID3D12Resource* mpTopLevelAS;
    ID3D12Resource* mpBottomLevelAS;
    ID3D12Resource* mpOutputResource;
    ID3D12Resource* mpShaderTable;

    uint64_t mTlasSize = 0;
    ID3D12StateObject* mpPipelineState;
    ID3D12RootSignature* m_RootSig;
    uint32_t mShaderTableEntrySize = 0;

    ID3D12DescriptorHeap* m_SrvUavHeap;
    ID3D12DescriptorHeap* m_RenderTargetViewHeap;

    struct AccelerationStructureBuffers
    {
        ID3D12Resource* pScratch;
        ID3D12Resource* pResult;
        ID3D12Resource* pInstanceDesc;    // Used only for top-level AS
    };

    const WCHAR* kRayGenShader = L"rayGen";
    const WCHAR* kMissShader = L"miss";
    const WCHAR* kClosestHitShader = L"chs";
    const WCHAR* kHitGroup = L"HitGroup";
    ShaderCompiler shaderCompiler{};


    struct Vertex
    {
        float position[3];
        float color[3];
    };

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
        m_SrvUavHeap = createDescriptorHeap(pDevice, 4, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);


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


        createAccelerationStructures(); // create the bottom-level AS for the triangle and the top-level AS that references it
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

    ID3D12Resource* createQuadVB(ID3D12Device5* pDevice)
    {
        const Vertex vertices[] =
        {
            { { -0.5f,  0.3f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
            { {  0.5f,  0.3f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
            { {  0.5f, -0.3f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
            { { -0.5f, -0.3f, 0.0f }, { 1.0f, 0.0f, 1.0f } },
        };

        D3D12_HEAP_PROPERTIES heapUpload;
        heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapUpload.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapUpload.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapUpload.CreationNodeMask = 0;
        heapUpload.VisibleNodeMask = 0;

        ID3D12Resource* pBuffer = createBuffer(pDevice, sizeof(vertices), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, heapUpload); // For simplicity, we create the vertex buffer on the upload heap, but that's not required

        uint8_t* pData;
        pBuffer->Map(0, nullptr, (void**)&pData);
        memcpy(pData, vertices, sizeof(vertices));
        pBuffer->Unmap(0, nullptr);

        return pBuffer;
    }

    ID3D12Resource* createQuadIB(ID3D12Device5* pDevice)
    {
        const uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };

        D3D12_HEAP_PROPERTIES heapUpload;
        heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapUpload.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapUpload.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapUpload.CreationNodeMask = 0;
        heapUpload.VisibleNodeMask = 0;

        ID3D12Resource* pBuffer = createBuffer(pDevice, sizeof(indices), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, heapUpload);

        uint8_t* pData;
        pBuffer->Map(0, nullptr, (void**)&pData);
        memcpy(pData, indices, sizeof(indices));
        pBuffer->Unmap(0, nullptr);

        return pBuffer;
    }

    AccelerationStructureBuffers createBottomLevelAS(ID3D12Device5* pDevice, ID3D12GraphicsCommandList4* pCmdList, ID3D12Resource* pVB, ID3D12Resource* pIB)
    {
        D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
        geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geomDesc.Triangles.VertexBuffer.StartAddress = pVB->GetGPUVirtualAddress();
        geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
        geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geomDesc.Triangles.VertexCount = 4;
        geomDesc.Triangles.IndexBuffer = pIB->GetGPUVirtualAddress();
        geomDesc.Triangles.IndexCount = 6;
        geomDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
        geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        // Get the size requirements for the scratch and AS buffers
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        inputs.NumDescs = 1;
        inputs.pGeometryDescs = &geomDesc;
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
        pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);


        D3D12_HEAP_PROPERTIES heapDefault;
        heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapDefault.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapDefault.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapDefault.CreationNodeMask = 0;
        heapDefault.VisibleNodeMask = 0;


        // Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state
        AccelerationStructureBuffers buffers;
        buffers.pScratch = createBuffer(pDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, heapDefault);
        buffers.pResult = createBuffer(pDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, heapDefault);

        // Create the bottom-level AS
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
        asDesc.Inputs = inputs;
        asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
        asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

        pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

        // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
        D3D12_RESOURCE_BARRIER uavBarrier = {};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.UAV.pResource = buffers.pResult;
        pCmdList->ResourceBarrier(1, &uavBarrier);

        return buffers;
    }

    AccelerationStructureBuffers createTopLevelAS(ID3D12Device5* pDevice, ID3D12GraphicsCommandList4* pCmdList, ID3D12Resource* pBottomLevelAS, uint64_t& tlasSize)
    {
        // First, get the size of the TLAS buffers and create them
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
        inputs.NumDescs = 1;
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
        pDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);


        D3D12_HEAP_PROPERTIES heapDefault;
        heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapDefault.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapDefault.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapDefault.CreationNodeMask = 0;
        heapDefault.VisibleNodeMask = 0;


        // Create the buffers
        AccelerationStructureBuffers buffers;
        buffers.pScratch = createBuffer(pDevice, info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, heapDefault);
        buffers.pResult = createBuffer(pDevice, info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, heapDefault);
        tlasSize = info.ResultDataMaxSizeInBytes;


        D3D12_HEAP_PROPERTIES heapUpload;
        heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapUpload.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapUpload.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapUpload.CreationNodeMask = 0;
        heapUpload.VisibleNodeMask = 0;


        // The instance desc should be inside a buffer, create and map the buffer
        buffers.pInstanceDesc = createBuffer(pDevice, sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, heapUpload);
        D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc;
        buffers.pInstanceDesc->Map(0, nullptr, (void**)&pInstanceDesc);

        // Initialize the instance desc. We only have a single instance
        pInstanceDesc->InstanceID = 0;                            // This value will be exposed to the shader via InstanceID()
        pInstanceDesc->InstanceContributionToHitGroupIndex = 0;   // This is the offset inside the shader-table. We only have a single geometry, so the offset 0
        pInstanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

        DirectX::XMMATRIX m = DirectX::XMMatrixIdentity();

        //mat4 m; // Identity matrix
        memcpy(pInstanceDesc->Transform, &m, sizeof(pInstanceDesc->Transform));
        pInstanceDesc->AccelerationStructure = pBottomLevelAS->GetGPUVirtualAddress();
        pInstanceDesc->InstanceMask = 0xFF;

        // Unmap
        buffers.pInstanceDesc->Unmap(0, nullptr);

        // Create the TLAS
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
        asDesc.Inputs = inputs;
        asDesc.Inputs.InstanceDescs = buffers.pInstanceDesc->GetGPUVirtualAddress();
        asDesc.DestAccelerationStructureData = buffers.pResult->GetGPUVirtualAddress();
        asDesc.ScratchAccelerationStructureData = buffers.pScratch->GetGPUVirtualAddress();

        pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

        // We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
        D3D12_RESOURCE_BARRIER uavBarrier = {};
        uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uavBarrier.UAV.pResource = buffers.pResult;
        pCmdList->ResourceBarrier(1, &uavBarrier);

        return buffers;
    }

    void createAccelerationStructures()
    {
        mpVertexBuffer = createQuadVB(pDevice);
        mpIndexBuffer = createQuadIB(pDevice);
        AccelerationStructureBuffers bottomLevelBuffers = createBottomLevelAS(pDevice, mpCmdList, mpVertexBuffer, mpIndexBuffer);
        AccelerationStructureBuffers topLevelBuffers = createTopLevelAS(pDevice, mpCmdList, bottomLevelBuffers.pResult, mTlasSize);

        // The tutorial doesn't have any resource lifetime management, so we flush and sync here. This is not required by the DXR spec - you can submit the list whenever you like as long as you take care of the resources lifetime.
        mFenceValue = submitCommandList(mpCmdList, mpCmdQueue, mpFence, mFenceValue);
        mpFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
        WaitForSingleObject(mFenceEvent, INFINITE);
        mpCmdList->Reset(pCmdAllocator, nullptr);

        // Store the AS buffers. The rest of the buffers will be released once we exit the function
        mpTopLevelAS = topLevelBuffers.pResult;
        mpBottomLevelAS = bottomLevelBuffers.pResult;


        UINT descriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle = m_SrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        srvCpuHandle.ptr += 0 * descriptorSize; // We only need one SRV for the TLAS, but we could create more if we wanted to bind the same TLAS multiple times in the shader with different indices in the shader-table


        // Create the SRV for the TLAS. The SRV doesn't have a resource associated, instead we specify the GPU address of the TLAS in the raytracing shader resource view desc
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = mpTopLevelAS->GetGPUVirtualAddress();
        pDevice->CreateShaderResourceView(nullptr, &srvDesc, srvCpuHandle);
    }


    void createRtPipelineState()
    {
        std::vector<D3D12_STATE_SUBOBJECT> subobjects;
        subobjects.reserve(12);

        // Create the DXIL library subobject, which contains the raytracing shaders. We will export all the shaders with the same names as the entry points in the HLSL code, but we could choose different export names if we wanted to
        auto pShaderBlob = shaderCompiler.Compile(L"../../Assets/Shaders/IndexBuffer/Mesh.hlsl", L"lib_6_3");

        const WCHAR* exports[] =
        {
            kRayGenShader,
            kMissShader,
            kClosestHitShader
        };

        D3D12_EXPORT_DESC exportDescs[3] = {};
        for (int i = 0; i < 3; i++)
        {
            exportDescs[i].Name = exports[i];
        }

        D3D12_DXIL_LIBRARY_DESC dxilLibDesc = {};
        dxilLibDesc.DXILLibrary.pShaderBytecode = pShaderBlob->GetBufferPointer();
        dxilLibDesc.DXILLibrary.BytecodeLength = pShaderBlob->GetBufferSize();
        dxilLibDesc.NumExports = 3;
        dxilLibDesc.pExports = exportDescs;

        D3D12_STATE_SUBOBJECT dxilLibSO = {};
        dxilLibSO.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
        dxilLibSO.pDesc = &dxilLibDesc;
        subobjects.push_back(dxilLibSO);


        // Create the hit group subobject, which defines the shaders that will be executed when a ray hits a geometry. In this case we only have a closest hit shader, but we could also define an any hit or intersection shader if we wanted to
        D3D12_HIT_GROUP_DESC hitGroupDesc = {};
        hitGroupDesc.HitGroupExport = kHitGroup;
        hitGroupDesc.ClosestHitShaderImport = kClosestHitShader;
        hitGroupDesc.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

        D3D12_STATE_SUBOBJECT hitGroupSO = {};
        hitGroupSO.Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
        hitGroupSO.pDesc = &hitGroupDesc;
        subobjects.push_back(hitGroupSO);


        //Global Root Signature for all shaders (empty in this case, but it needs to be defined since we are using resources)
        D3D12_DESCRIPTOR_RANGE srvRange = {};
        srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        srvRange.NumDescriptors = 3;
        srvRange.BaseShaderRegister = 0;  // t0
        srvRange.RegisterSpace = 0;
        srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_DESCRIPTOR_RANGE uavRange = {};
        uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRange.NumDescriptors = 1;
        uavRange.BaseShaderRegister = 0;  // u0
        uavRange.RegisterSpace = 0;
        uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER rootParams[2] = {};
        rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[0].DescriptorTable.pDescriptorRanges = &srvRange;
        rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
        rootParams[1].DescriptorTable.pDescriptorRanges = &uavRange;
        rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        D3D12_ROOT_SIGNATURE_DESC globalRSDesc = {};
        globalRSDesc.NumParameters = 2;
        globalRSDesc.pParameters = rootParams;
        globalRSDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

        ID3DBlob* rsBlob;
        ID3DBlob* errorBlob;
        HRESULT hr = D3D12SerializeRootSignature(&globalRSDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &errorBlob);

        ID3D12RootSignature* pGlobalRS;
        pDevice->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&m_RootSig));

        D3D12_GLOBAL_ROOT_SIGNATURE globalRS = {};
        globalRS.pGlobalRootSignature = m_RootSig;

        D3D12_STATE_SUBOBJECT globalRSSO = {};
        globalRSSO.Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
        globalRSSO.pDesc = &globalRS;
        subobjects.push_back(globalRSSO);


        // Create the shader config subobject, which defines the size of the ray payload and attribute structures. The payload is the data that the ray generation shader can pass to the hit and miss shaders, 
        // and the attributes are the data that the intersection or hit shaders can pass to the closest hit shader. 
        // In this case, we define a payload with a float3 for the color, and an attribute with a float2 for the barycentrics (since we are using triangle geometry)
        D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
        shaderConfig.MaxPayloadSizeInBytes = sizeof(float) * 3;      // float3 color
        shaderConfig.MaxAttributeSizeInBytes = sizeof(float) * 2;    // barycentrics

        D3D12_STATE_SUBOBJECT shaderConfigSO = {};
        shaderConfigSO.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
        shaderConfigSO.pDesc = &shaderConfig;

        UINT shaderConfigIndex = (UINT)subobjects.size();
        subobjects.push_back(shaderConfigSO);

        const WCHAR* shaderConfigExports[] =
        {
            kRayGenShader,
            kMissShader,
            kClosestHitShader,
        };

        D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderAssoc = {};
        shaderAssoc.pSubobjectToAssociate = &subobjects[shaderConfigIndex];
        shaderAssoc.NumExports = 3;
        shaderAssoc.pExports = shaderConfigExports;

        D3D12_STATE_SUBOBJECT shaderAssocSO = {};
        shaderAssocSO.Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
        shaderAssocSO.pDesc = &shaderAssoc;
        subobjects.push_back(shaderAssocSO);


        // Create the pipeline config subobject, which defines the maximum recursion depth for the raytracing pipeline. Since we are only doing primary rays in this tutorial, we can set it to 1
        D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
        pipelineConfig.MaxTraceRecursionDepth = 1;

        D3D12_STATE_SUBOBJECT pipelineConfigSO = {};
        pipelineConfigSO.Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
        pipelineConfigSO.pDesc = &pipelineConfig;
        subobjects.push_back(pipelineConfigSO);


        // Create the state object
        D3D12_STATE_OBJECT_DESC desc = {};
        desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
        desc.NumSubobjects = (UINT)subobjects.size();
        desc.pSubobjects = subobjects.data();

        hr = pDevice->CreateStateObject(&desc, IID_PPV_ARGS(&mpPipelineState));

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

        // Entry 0 - RayGen (SOLO el ID)
        memcpy(pData, pRtsoProps->GetShaderIdentifier(kRayGenShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Entry 1 - Miss (SOLO el ID)
        memcpy(pData + mShaderTableEntrySize, pRtsoProps->GetShaderIdentifier(kMissShader), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Entry 2 - HitGroup (SOLO el ID)
        memcpy(pData + mShaderTableEntrySize * 2, pRtsoProps->GetShaderIdentifier(kHitGroup), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

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
        uavCpuHandle.ptr += 3 * descriptorSize;

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        pDevice->CreateUnorderedAccessView(mpOutputResource, nullptr, &uavDesc, uavCpuHandle);




        // --- Vertex buffer SRV (índice 1) ---
        D3D12_CPU_DESCRIPTOR_HANDLE vertexSrvHandle = m_SrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        vertexSrvHandle.ptr += 1 * descriptorSize;

        D3D12_SHADER_RESOURCE_VIEW_DESC vertexSrvDesc = {};
        vertexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        vertexSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        vertexSrvDesc.Buffer.FirstElement = 0;
        vertexSrvDesc.Buffer.NumElements = 4;
        vertexSrvDesc.Buffer.StructureByteStride = sizeof(Vertex);
        vertexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        pDevice->CreateShaderResourceView(mpVertexBuffer, &vertexSrvDesc, vertexSrvHandle);

        D3D12_CPU_DESCRIPTOR_HANDLE indexSrvHandle = m_SrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        indexSrvHandle.ptr += 2 * descriptorSize;

        D3D12_SHADER_RESOURCE_VIEW_DESC indexSrvDesc = {};
        indexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        indexSrvDesc.Format = DXGI_FORMAT_R32_UINT;
        indexSrvDesc.Buffer.FirstElement = 0;
        indexSrvDesc.Buffer.NumElements = 6;
        indexSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        indexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        pDevice->CreateShaderResourceView(mpIndexBuffer, &indexSrvDesc, indexSrvHandle);


    }

    void Loop()
    {
        uint32_t rtvIndex = mpSwapChain->GetCurrentBackBufferIndex();
        mpCmdList->SetComputeRootSignature(m_RootSig);

        ID3D12DescriptorHeap* heaps[] = { m_SrvUavHeap };
        mpCmdList->SetDescriptorHeaps(1, heaps);

        UINT descriptorSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        D3D12_GPU_DESCRIPTOR_HANDLE heapStartGPU = m_SrvUavHeap->GetGPUDescriptorHandleForHeapStart();

        // Root Param 0: tabla de SRVs (índices 0 y 1 del heap: TLAS y vertex buffer)
        mpCmdList->SetComputeRootDescriptorTable(0, heapStartGPU);

        // Root Param 1: UAV (índice 2)
        D3D12_GPU_DESCRIPTOR_HANDLE uavGPU = heapStartGPU;
        uavGPU.ptr += 3 * descriptorSize;
        mpCmdList->SetComputeRootDescriptorTable(1, uavGPU);

        mpCmdList->SetPipelineState1(mpPipelineState); // Set the pipeline state


        D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
        raytraceDesc.Width = width;
        raytraceDesc.Height = height;
        raytraceDesc.Depth = 1;

        raytraceDesc.RayGenerationShaderRecord.StartAddress = mpShaderTable->GetGPUVirtualAddress() + 0 * mShaderTableEntrySize;
        raytraceDesc.RayGenerationShaderRecord.SizeInBytes = mShaderTableEntrySize;

        size_t missOffset = 1 * mShaderTableEntrySize;
        raytraceDesc.MissShaderTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + missOffset;
        raytraceDesc.MissShaderTable.StrideInBytes = mShaderTableEntrySize;
        raytraceDesc.MissShaderTable.SizeInBytes = mShaderTableEntrySize;   // Only a s single miss-entry

        size_t hitOffset = 2 * mShaderTableEntrySize;
        raytraceDesc.HitGroupTable.StartAddress = mpShaderTable->GetGPUVirtualAddress() + hitOffset;
        raytraceDesc.HitGroupTable.StrideInBytes = mShaderTableEntrySize;
        raytraceDesc.HitGroupTable.SizeInBytes = mShaderTableEntrySize;


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
    const wchar_t title[] = L"DX12 IndexBuffer";

    RenderSystem render{};

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

