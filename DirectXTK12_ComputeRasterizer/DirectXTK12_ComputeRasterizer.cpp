// DirectXTK12_ComputeRasterizer.cpp
// DX12 port of the DirectXTKComputeRasterizer compute shader rasterizer.

#include "pch.h"
#include "DirectXTK12_ComputeRasterizer.h"
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <DirectXHelpers.h>
enum Descriptors
{
    WindowsLogo,
    CourierFont,
    ControllerFont,
    GamerPic,
    Count
};
using namespace DirectX;
using Microsoft::WRL::ComPtr;


// 宣言の変更を前提とします: 
// void CreateTexture(D3D12MA::Allocator* allocator, DX::DeviceResources* DR);

void DirectXTK12_ComputeRasterizer::CreateTexture(D3D12MA::Allocator* allocator, DX::DeviceResources* DR)
{
    auto device = DR->GetD3DDevice();

    // --------------------------------------------------
    // 1. 絵を描く（データの準備：1x1の真っ白なピクセル）
    // --------------------------------------------------
    uint32_t whitePixel = 0xFFFFFFFF; // RGBA全て最大値(白)

    D3D12_SUBRESOURCE_DATA initData = {};
    initData.pData = &whitePixel;
    initData.RowPitch = sizeof(uint32_t);
    initData.SlicePitch = sizeof(uint32_t);

    // --------------------------------------------------
    // 2. キャンバスの用意（D3D12MAによるリソースの確保）
    // --------------------------------------------------
    CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1);

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    DX::ThrowIfFailed(allocator->CreateResource(
        &allocDesc,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, // まずは搬入先の状態として作成
        nullptr,
        m_fallbackTextureAllocation.ReleaseAndGetAddressOf(),
        IID_PPV_ARGS(m_fallbackTexture.ReleaseAndGetAddressOf())
    ));

    // --------------------------------------------------
    // 3. アトリエから美術館へ搬入（アップロードと状態遷移）
    // --------------------------------------------------
    DirectX::ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    // ① 作品の搬入指示
    resourceUpload.Upload(
        m_fallbackTexture.Get(),
        0,
        &initData,
        1
    );

    // ② 設営（状態遷移）の指示
    // ※注意: コンピュートシェーダーで読み取るため NON_PIXEL_SHADER_RESOURCE を指定します
    resourceUpload.Transition(
        m_fallbackTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
    );

    // ③ 指示書の提出と、作業完了の待機
    auto uploadFinished = resourceUpload.End(DR->GetCommandQueue());
    uploadFinished.wait();

    // --------------------------------------------------
    // 4. キャプションの設置（SRVの作成）
    // --------------------------------------------------
    // ※前提: m_descriptorHeap および m_descriptorSize が初期化済みであること
    if (m_descriptorHeap)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        // ヘッダで定義されている DescriptorIndex::SRV_Texture (t1) の位置にSRVを作成
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
            m_descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            DescriptorIndex::SRV_Texture,
            m_descriptorSize
        );

        device->CreateShaderResourceView(m_fallbackTexture.Get(), &srvDesc, srvHandle);
    }
    else
    {
        OutputDebugStringA("Warning: Descriptor heap is not initialized before CreateTexture.\n");
    }
}
//ディスクリプタヒープ内の配置
// UAV (u0) - 出力テクスチャ
// SRV (t0) - 頂点バッファ (StructuredBuffer<Vertex>)
// SRV (t1) - フォールバックテクスチャ (白黒のチェッカーボードなど)

// Helper function to create and serialize root signature
static ComPtr<ID3D12RootSignature> CreateRootSignature(
    ID3D12Device* device)
{
    // Define descriptor ranges
    CD3DX12_DESCRIPTOR_RANGE uavRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    CD3DX12_DESCRIPTOR_RANGE srvRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0);

    // Define root parameters
    CD3DX12_ROOT_PARAMETER rootParams[3];
    rootParams[0].InitAsDescriptorTable(1, &uavRange, D3D12_SHADER_VISIBILITY_ALL);
    rootParams[1].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_ALL);
    rootParams[2].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);

    // Define static sampler
    CD3DX12_STATIC_SAMPLER_DESC staticSampler(
        0,                                      // shader register
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,       // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,       // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,       // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,       // addressW
        0.0f,                                   // mipLODBias
        16,                                     // maxAnisotropy
        D3D12_COMPARISON_FUNC_NEVER,           // comparisonFunc
        D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
        0.0f,                                   // minLOD
        D3D12_FLOAT32_MAX,                     // maxLOD
        D3D12_SHADER_VISIBILITY_ALL);

    // Create root signature descriptor
    CD3DX12_ROOT_SIGNATURE_DESC rsDesc(
        3, rootParams,
        1, &staticSampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // Serialize root signature
    ComPtr<ID3DBlob> serializedRS;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rsDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRS,
        &errorBlob);

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA("Root Signature Error: ");
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        DX::ThrowIfFailed(hr);
    }

    // Create root signature
    ComPtr<ID3D12RootSignature> rootSignature;
    DX::ThrowIfFailed(device->CreateRootSignature(
        0,
        serializedRS->GetBufferPointer(),
        serializedRS->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)));

    rootSignature->SetName(L"ComputeRasterizerRootSignature");

    return rootSignature;
}
// Slot 0: UAV for output texture (u0)
// Slot 1: SRV for vertex buffer (t0 — StructuredBuffer<Vertex>)
// Slot 2: SRV for fallback texture (t1)
// Helper function to create descriptor heap views
static void CreateDescriptorViews(
    ID3D12Device* device,
    ID3D12DescriptorHeap* heap,
    UINT descriptorSize,
    ID3D12Resource* outputTexture,
    ID3D12Resource* vertexBuffer,
    ID3D12Resource* fallbackTexture,
    UINT triangleCount,
    DXGI_FORMAT backBufferFormat)
{
    auto cpuBase = heap->GetCPUDescriptorHandleForHeapStart();

    // Slot 0: UAV for output texture (u0)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = backBufferFormat;
        DirectX::CreateUnorderedAccessView(
            device,
            outputTexture,cpuBase);
    }

    // Slot 1: SRV for vertex buffer (t0 — StructuredBuffer<Vertex>)
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
            cpuBase,
            1,  // SRV_VertexBuffer slot
            descriptorSize);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>(triangleCount * 3);
        srvDesc.Buffer.StructureByteStride = sizeof(Vertex);
        DirectX:CreateShaderResourceView(
            device,
            vertexBuffer,
			srvHandle);
    }

    // Slot 2: SRV for fallback texture (t1)
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
            cpuBase,
            2,  // SRV_Texture slot
            descriptorSize);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
     
        DirectX::CreateShaderResourceView(
            device,
            fallbackTexture, srvHandle);
    }
}

void DirectXTK12_ComputeRasterizer::Initialize(
    D3D12MA::Allocator* allocator,
    DX::DeviceResources*     deviceResources,
    int width, int height)
{
   
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
    // 2. Create root signature using helper function
    // ------------------------------------------------------------------
    m_rootSignature = CreateRootSignature(device);

    // ------------------------------------------------------------------
    // 3. Create compute pipeline state object
    // ------------------------------------------------------------------
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };
    DX::ThrowIfFailed(device->CreateComputePipelineState(&psoDesc,
                                                          IID_PPV_ARGS(&m_pipelineState)));
    m_pipelineState->SetName(L"ComputeRasterizerPipeline");

	m_graphicsMemory = std::make_unique<DirectX::GraphicsMemory>(device, 0);
  

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
        
		m_vertexBuffer = m_graphicsMemory->Allocate(vbSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        
       

       
       
        memcpy(m_vertexBuffer.Memory(), triangleVertices, vbSize);
       
    }
    // ==================================================
        // 1x1 フォールバックテクスチャ（白）の作成 (D3D12MA使用)
        // ==================================================

        // 1. 絵を描く（データ準備：1x1の真っ白なピクセル）
    uint32_t whitePixel = 0xFFFFFFFF;
    D3D12_SUBRESOURCE_DATA initData = {};
    initData.pData = &whitePixel;
    initData.RowPitch = sizeof(uint32_t);
    initData.SlicePitch = sizeof(uint32_t);

    // 2. キャンバスの用意（D3D12MAによるリソース確保）
    CD3DX12_RESOURCE_DESC texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1);

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    DX::ThrowIfFailed(allocator->CreateResource(
        &allocDesc,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        m_fallbackTextureAllocation.ReleaseAndGetAddressOf(),
        IID_PPV_ARGS(m_fallbackTexture.ReleaseAndGetAddressOf())
    ));

    // 3. アトリエから美術館へ搬入（アップロードと状態遷移）
    DirectX::ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    // 搬入
    resourceUpload.Upload(m_fallbackTexture.Get(), 0, &initData, 1);

    // 設営（コンピュートシェーダー用のため NON_PIXEL_SHADER_RESOURCE）
    resourceUpload.Transition(
        m_fallbackTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
    );

    auto uploadFinished = resourceUpload.End(deviceResources->GetCommandQueue());
    uploadFinished.wait();

    // 4. キャプションの設置（SRVの作成）
    if (m_descriptorHeap)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        // t1レジスタ相当の SRV_Texture スロットに登録
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(
            m_descriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            DescriptorIndex::SRV_Texture,
            m_descriptorSize
        );

        device->CreateShaderResourceView(m_fallbackTexture.Get(), &srvDesc, srvHandle);
    }

	//コンピュートシェーダー・頂点・フォールバックテクスチャ用のディスクリプタヒープを作成
    resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        3);

  
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
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvStart(
        gpuBase,
        SRV_VertexBuffer,
        m_descriptorSize);
    commandList->SetComputeRootDescriptorTable(1, srvStart);

    // Root parameter 2: root CBV (GPU virtual address from GraphicsMemory alloc)
    commandList->SetComputeRootConstantBufferView(2, cbAlloc.GpuAddress());

    // ------------------------------------------------------------------
    // Dispatch: cover every pixel in a 16x16 tile grid
    // ------------------------------------------------------------------
    const UINT dispatchX = (static_cast<UINT>(m_width)  + 15) / 16;
    const UINT dispatchY = (static_cast<UINT>(m_height) + 15) / 16;
    commandList->Dispatch(dispatchX, dispatchY, 1);

    // ------------------------------------------------------------------
    // Resource Barriers - Optimized using DirectXTK12 patterns
    // Reference: https://github.com/microsoft/DirectXTK12/wiki/Resource-Barriers
    // ------------------------------------------------------------------

    // UAV barrier to ensure all compute writes are visible before the copy
    auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_outputTexture.Get());
    commandList->ResourceBarrier(1, &uavBarrier);

    // Batch transition barriers for better performance
    D3D12_RESOURCE_BARRIER transitionBarriers[2] = {
        // Transition output texture: UNORDERED_ACCESS -> COPY_SOURCE
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputTexture.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE),
        // Transition back buffer: RENDER_TARGET -> COPY_DEST
        CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST)
    };
    commandList->ResourceBarrier(2, transitionBarriers);

    // Perform the copy operation
    commandList->CopyResource(backBuffer, m_outputTexture.Get());

    // Batch transition barriers back to their original states
    D3D12_RESOURCE_BARRIER returnBarriers[2] = {
        // Transition back buffer: COPY_DEST -> RENDER_TARGET (required by Present)
        CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer,
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET),
        // Transition output texture: COPY_SOURCE -> UNORDERED_ACCESS (ready for next frame)
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputTexture.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    };
    commandList->ResourceBarrier(2, returnBarriers);
}
