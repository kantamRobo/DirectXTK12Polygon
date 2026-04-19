// DirectXTK12_ComputeRasterizer.cpp
// DX12 port of the DirectXTKComputeRasterizer compute shader rasterizer.

#include "pch.h"
#include "DirectXTK12_ComputeRasterizer.h"
#include <d3dcompiler.h>
#include <d3dx12.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

void DirectXTK12_ComputeRasterizer::Initialize(
    DirectX::GraphicsMemory* graphicsMemory,
    DX::DeviceResources*     deviceResources,
    int width, int height)
{
    m_graphicsMemory = graphicsMemory;
    m_width          = width;
    m_height         = height;

    auto device       = deviceResources->GetD3DDevice();
    auto commandQueue = deviceResources->GetCommandQueue();

    // ------------------------------------------------------------------
    // 1. Compile compute shader at runtime
    // ------------------------------------------------------------------
    ComPtr<ID3DBlob> csBlob;
    ComPtr<ID3DBlob> errorBlob;
    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(
        L"TriangleRasterizer.hlsl",
        nullptr, nullptr,
        "CSMain", "cs_5_1",
        compileFlags, 0,
        &csBlob, &errorBlob);
    if (FAILED(hr))
    {
        if (errorBlob)
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        DX::ThrowIfFailed(hr);
    }

    // ------------------------------------------------------------------
    // 2. Create root signature
    //    [0] Descriptor table: 1 UAV  (u0)
    //    [1] Descriptor table: 2 SRVs (t0, t1)
    //    [2] Root CBV          (b0)
    //    Static sampler        (s0)
    // ------------------------------------------------------------------
    CD3DX12_DESCRIPTOR_RANGE uavRange;
    uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE srvRange;
    srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

    CD3DX12_ROOT_PARAMETER rootParams[3];
    rootParams[0].InitAsDescriptorTable(1, &uavRange);
    rootParams[1].InitAsDescriptorTable(1, &srvRange);
    rootParams[2].InitAsConstantBufferView(0);

    D3D12_STATIC_SAMPLER_DESC staticSampler = {};
    staticSampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.MipLODBias       = 0.0f;
    staticSampler.MaxAnisotropy    = 1;
    staticSampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    staticSampler.MinLOD           = 0.0f;
    staticSampler.MaxLOD           = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister   = 0;
    staticSampler.RegisterSpace    = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc(3, rootParams, 1, &staticSampler);

    ComPtr<ID3DBlob> serializedRS;
    ComPtr<ID3DBlob> rsError;
    hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                     &serializedRS, &rsError);
    if (FAILED(hr))
    {
        if (rsError)
            OutputDebugStringA(static_cast<const char*>(rsError->GetBufferPointer()));
        DX::ThrowIfFailed(hr);
    }
    DX::ThrowIfFailed(device->CreateRootSignature(
        0,
        serializedRS->GetBufferPointer(), serializedRS->GetBufferSize(),
        IID_PPV_ARGS(&m_rootSignature)));

    // ------------------------------------------------------------------
    // 3. Create compute pipeline state object
    // ------------------------------------------------------------------
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
    DX::ThrowIfFailed(device->CreateComputePipelineState(&psoDesc,
                                                          IID_PPV_ARGS(&m_pipelineState)));

    // ------------------------------------------------------------------
    // 4. Create UAV output texture (same format as back buffer)
    // ------------------------------------------------------------------
    {
        DXGI_FORMAT fmt = deviceResources->GetBackBufferFormat();
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            fmt,
            static_cast<UINT>(width), static_cast<UINT>(height),
            1, 1, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        DX::ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr,
            IID_PPV_ARGS(&m_outputTexture)));
        m_outputTexture->SetName(L"ComputeRasterizerOutput");
    }

    // ------------------------------------------------------------------
    // 5. Create CBV_SRV_UAV descriptor heap (3 descriptors)
    // ------------------------------------------------------------------
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = DescriptorCount;
        heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        DX::ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc,
                                                        IID_PPV_ARGS(&m_descriptorHeap)));
        m_descriptorSize = device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // ------------------------------------------------------------------
    // 6. Create vertex buffer (UPLOAD heap) and fill with a test triangle
    // ------------------------------------------------------------------
    // Vertices use counter-clockwise winding (area > 0 in screen space)
    Vertex triangleVertices[] = {
        { {  0.0f,  0.5f, 0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.0f } }, // top,          red
        { { -0.5f, -0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } }, // bottom-left,  green
        { {  0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }, // bottom-right, blue
    };
    m_triangleCount = 1;
    const UINT vbSize = sizeof(triangleVertices);
    {
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC bufDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
        DX::ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        void* mapped = nullptr;
        D3D12_RANGE readRange = {};
        DX::ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, &mapped));
        memcpy(mapped, triangleVertices, vbSize);
        m_vertexBuffer->Unmap(0, nullptr);
        m_vertexBuffer->SetName(L"ComputeRasterizerVertexBuffer");
    }

    // ------------------------------------------------------------------
    // 7. Create 1x1 white fallback texture (DEFAULT heap via temp cmd list)
    // ------------------------------------------------------------------
    {
        // Create dedicated command allocator/list for initialization upload
        ComPtr<ID3D12CommandAllocator>    initCmdAlloc;
        ComPtr<ID3D12GraphicsCommandList> initCmdList;
        DX::ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                          IID_PPV_ARGS(&initCmdAlloc)));
        DX::ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                     initCmdAlloc.Get(), nullptr,
                                                     IID_PPV_ARGS(&initCmdList)));

        // Texture resource in DEFAULT heap
        CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1);
        DX::ThrowIfFailed(device->CreateCommittedResource(
            &defaultHeap,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_fallbackTexture)));
        m_fallbackTexture->SetName(L"ComputeRasterizerFallbackTexture");

        // Get footprint so we know the upload buffer size
        UINT64 uploadSize = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
        device->GetCopyableFootprints(&texDesc, 0, 1, 0,
                                      &footprint, nullptr, nullptr, &uploadSize);

        // Upload buffer in UPLOAD heap
        CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
        ComPtr<ID3D12Resource> uploadBuffer;
        DX::ThrowIfFailed(device->CreateCommittedResource(
            &uploadHeap,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer)));

        // Map and fill with white (R=255 G=255 B=255 A=255)
        const uint32_t whitePixel = 0xFFFFFFFFu;
        BYTE* mapped = nullptr;
        D3D12_RANGE readRange = {};
        DX::ThrowIfFailed(uploadBuffer->Map(0, &readRange,
                                             reinterpret_cast<void**>(&mapped)));
        memcpy(mapped, &whitePixel, sizeof(whitePixel));
        uploadBuffer->Unmap(0, nullptr);

        // Record copy
        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.pResource        = m_fallbackTexture.Get();
        dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource       = uploadBuffer.Get();
        srcLoc.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint = footprint;

        initCmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        // Transition to NON_PIXEL_SHADER_RESOURCE for the compute stage
        D3D12_RESOURCE_BARRIER toSRV = CD3DX12_RESOURCE_BARRIER::Transition(
            m_fallbackTexture.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        initCmdList->ResourceBarrier(1, &toSRV);

        DX::ThrowIfFailed(initCmdList->Close());

        // Execute and wait for GPU to finish before releasing the upload buffer
        ID3D12CommandList* cmdLists[] = { initCmdList.Get() };
        commandQueue->ExecuteCommandLists(1, cmdLists);
        deviceResources->WaitForGpu();
        // uploadBuffer is released here (after GPU is done)
    }

    // ------------------------------------------------------------------
    // 8. Create views in the descriptor heap
    // ------------------------------------------------------------------
    auto cpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();

    // Slot 0: UAV for output texture (u0)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuBase;
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format        = deviceResources->GetBackBufferFormat();
        device->CreateUnorderedAccessView(m_outputTexture.Get(), nullptr, &uavDesc, handle);
    }

    // Slot 1: SRV for vertex buffer (t0 — StructuredBuffer<Vertex>)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuBase;
        handle.ptr += static_cast<SIZE_T>(m_descriptorSize) * SRV_VertexBuffer;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension               = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Format                      = DXGI_FORMAT_UNKNOWN;
        srvDesc.Shader4ComponentMapping     = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement         = 0;
        srvDesc.Buffer.NumElements          = static_cast<UINT>(m_triangleCount * 3);
        srvDesc.Buffer.StructureByteStride  = sizeof(Vertex);
        device->CreateShaderResourceView(m_vertexBuffer.Get(), &srvDesc, handle);
    }

    // Slot 2: SRV for fallback texture (t1)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuBase;
        handle.ptr += static_cast<SIZE_T>(m_descriptorSize) * SRV_Texture;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension               = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Format                      = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.Shader4ComponentMapping     = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels         = 1;
        device->CreateShaderResourceView(m_fallbackTexture.Get(), &srvDesc, handle);
    }
}

void DirectXTK12_ComputeRasterizer::Resize(
    DX::DeviceResources* deviceResources,
    int width, int height)
{
    if (m_width == width && m_height == height)
        return;

    m_width  = width;
    m_height = height;

    auto device = deviceResources->GetD3DDevice();

    // Release old output texture (GPU is idle at this point, called after WaitForGpu)
    m_outputTexture.Reset();

    // Recreate UAV output texture with the new dimensions
    {
        DXGI_FORMAT fmt = deviceResources->GetBackBufferFormat();
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            fmt,
            static_cast<UINT>(width), static_cast<UINT>(height),
            1, 1, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        DX::ThrowIfFailed(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &texDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr,
            IID_PPV_ARGS(&m_outputTexture)));
        m_outputTexture->SetName(L"ComputeRasterizerOutput");
    }

    // Update the UAV descriptor in slot 0 to point at the new texture
    {
        auto cpuBase = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format        = deviceResources->GetBackBufferFormat();
        device->CreateUnorderedAccessView(m_outputTexture.Get(), nullptr, &uavDesc, cpuBase);
    }
}

void DirectXTK12_ComputeRasterizer::Render(DX::DeviceResources* deviceResources)
{
    auto commandList = deviceResources->GetCommandList();
    auto backBuffer  = deviceResources->GetRenderTarget();

    // ------------------------------------------------------------------
    // Update constant buffer via GraphicsMemory (upload heap, per-frame)
    // ------------------------------------------------------------------
    CBData cbData = {};
    cbData.worldViewProj = DirectX::XMMatrixIdentity();
    cbData.screenSize    = DirectX::XMFLOAT2(static_cast<float>(m_width),
                                              static_cast<float>(m_height));
    cbData.triangleCount = m_triangleCount;
    cbData.padding       = 0.0f;

    auto cbAlloc = m_graphicsMemory->Allocate(sizeof(CBData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    memcpy(cbAlloc.Memory(), &cbData, sizeof(CBData));

    // ------------------------------------------------------------------
    // Bind descriptor heap and set pipeline state
    // ------------------------------------------------------------------
    ID3D12DescriptorHeap* heaps[] = { m_descriptorHeap.Get() };
    commandList->SetDescriptorHeaps(1, heaps);

    commandList->SetComputeRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pipelineState.Get());

    // Root parameter 0: UAV descriptor table (heap slot 0)
    auto gpuBase = m_descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    commandList->SetComputeRootDescriptorTable(0, gpuBase);

    // Root parameter 1: SRV descriptor table (heap slots 1-2)
    D3D12_GPU_DESCRIPTOR_HANDLE srvStart = gpuBase;
    srvStart.ptr += static_cast<UINT64>(m_descriptorSize) * SRV_VertexBuffer;
    commandList->SetComputeRootDescriptorTable(1, srvStart);

    // Root parameter 2: root CBV (GPU virtual address from GraphicsMemory alloc)
    commandList->SetComputeRootConstantBufferView(2, cbAlloc.GpuAddress());

    // ------------------------------------------------------------------
    // Dispatch: cover every pixel in a 16x16 tile grid
    // ------------------------------------------------------------------
    const UINT dispatchX = (static_cast<UINT>(m_width)  + 15) / 16;
    const UINT dispatchY = (static_cast<UINT>(m_height) + 15) / 16;
    commandList->Dispatch(dispatchX, dispatchY, 1);

    // UAV barrier to ensure all compute writes are visible before the copy
    D3D12_RESOURCE_BARRIER uavBarrier =
        CD3DX12_RESOURCE_BARRIER::UAV(m_outputTexture.Get());
    commandList->ResourceBarrier(1, &uavBarrier);

    // ------------------------------------------------------------------
    // Copy output texture to back buffer
    // ------------------------------------------------------------------
    // Transition output texture: UNORDERED_ACCESS -> COPY_SOURCE
    D3D12_RESOURCE_BARRIER outToSrc = CD3DX12_RESOURCE_BARRIER::Transition(
        m_outputTexture.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList->ResourceBarrier(1, &outToSrc);

    // Transition back buffer: RENDER_TARGET -> COPY_DEST
    D3D12_RESOURCE_BARRIER bbToDest = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_COPY_DEST);
    commandList->ResourceBarrier(1, &bbToDest);

    commandList->CopyResource(backBuffer, m_outputTexture.Get());

    // Transition back buffer: COPY_DEST -> RENDER_TARGET (required by Present)
    D3D12_RESOURCE_BARRIER bbToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer,
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &bbToRT);

    // Transition output texture: COPY_SOURCE -> UNORDERED_ACCESS (ready for next frame)
    D3D12_RESOURCE_BARRIER outToUAV = CD3DX12_RESOURCE_BARRIER::Transition(
        m_outputTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    commandList->ResourceBarrier(1, &outToUAV);
}
