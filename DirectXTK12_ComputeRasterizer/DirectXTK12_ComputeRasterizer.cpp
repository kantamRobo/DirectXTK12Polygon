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
        DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);  

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    DX::ThrowIfFailed(allocator->CreateResource(
        &allocDesc,
        &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, // まずは搬入先の状態として作成
        nullptr,
        m_outputTextureAllocation.ReleaseAndGetAddressOf(),
        IID_PPV_ARGS(m_outputTexture.ReleaseAndGetAddressOf())
    ));
	m_outputTexture->SetName(L"OutputTexture");
    CD3DX12_RESOURCE_DESC backbufferDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1, 1, 0,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

    //バックバッファの用意
	D3D12MA::ALLOCATION_DESC backBufferAllocDesc = {};
	backBufferAllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
	//バックバッファはレンダーターゲットとして使用するため、ALLOW_RENDER_TARGETフラグを追加

    //バックバッファはテクスチャの書き込み先として使用する
    DX::ThrowIfFailed(allocator->CreateResource(
        &backBufferAllocDesc,
        &backbufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        m_backBufferAllocation.ReleaseAndGetAddressOf(),
        IID_PPV_ARGS(m_backBuffer.ReleaseAndGetAddressOf())
	));
	m_backBuffer->SetName(L"BackBuffer");
    // --------------------------------------------------
    // 3. アトリエから美術館へ搬入（アップロードと状態遷移）
    // --------------------------------------------------
    DirectX::ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    // ① 作品の搬入指示
    resourceUpload.Upload(
        m_outputTexture.Get(),
        0,
        &initData,
        1
    );

    // ② 設営（状態遷移）の指示
    // ※注意: コンピュートシェーダーで読み取るため NON_PIXEL_SHADER_RESOURCE を指定します
    resourceUpload.Transition(
        m_outputTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
    );

    // ③ 指示書の提出と、作業完了の待機
    auto uploadFinished = resourceUpload.End(DR->GetCommandQueue());
    uploadFinished.wait();
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

    // Define root parameters - Initialize to zero first to avoid C6001 warning
    CD3DX12_ROOT_PARAMETER rootParams[3] = {};
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
	DirectX::DescriptorHeap* heap,
    ID3D12Device* device,
    UINT descriptorSize,
    ID3D12Resource* outputTexture,
    ID3D12Resource* vertexBuffer,
    ID3D12Resource* fallbackTexture,
    UINT triangleCount,
    DXGI_FORMAT backBufferFormat)
{
	auto cpuBase = heap->GetCpuHandle(0);

    // Slot 0: UAV for output texture (u0)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = backBufferFormat;
        DirectX::CreateUnorderedAccessView(
            device,
            outputTexture,
            cpuBase);
    }

    // Slot 1: SRV for vertex buffer (t0 — StructuredBuffer<Vertex>)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>(triangleCount * 3);
        srvDesc.Buffer.StructureByteStride = sizeof(Vertex);
        DirectX::CreateBufferShaderResourceView(
            device,
            vertexBuffer,
            heap->GetCpuHandle(1),
            sizeof(Vertex));
    }

    // Slot 2: SRV for fallback texture (t1)
    {
        auto srvHandle = heap->GetCpuHandle(2);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
     
        device->CreateShaderResourceView(
            fallbackTexture,
            &srvDesc,
            srvHandle);
    }
}

void DirectXTK12_ComputeRasterizer::Initialize(
   
	
    DX::DeviceResources*     deviceResources,
    int width, int height)
{
   
    m_width          = width;
    m_height         = height;

    auto device       = deviceResources->GetD3DDevice();
    auto commandQueue = deviceResources->GetCommandQueue();
	auto adapter = deviceResources->adapter.Get(); // 追加: IDXGIAdapterを取得
    m_graphicsMemory = std::make_unique<DirectX::GraphicsMemory>(device);

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
        "CSMain", "cs_5_0",
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



    D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
    allocatorDesc.pDevice = device;
    allocatorDesc.pAdapter = adapter;
    allocatorDesc.Flags = D3D12MA_RECOMMENDED_ALLOCATOR_FLAGS;

    D3D12MA::Allocator* allocator;
    HRESULT createallocator = D3D12MA::CreateAllocator(&allocatorDesc, &allocator);
    // ------------------------------------------------------------------
 // 6. Create vertex buffer (UPLOAD heap) and fill with a test triangle
 // ------------------------------------------------------------------
    Vertex triangleVertices[] = {
        { {  0.0f,  0.5f, 0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 0.0f } },
        { { -0.5f, -0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
        { {  0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },
    };
    m_triangleCount = 1;
    const UINT vbSize = sizeof(triangleVertices);

    // --- 頂点バッファの確保 (D3D12MA) ---
    D3D12MA::ALLOCATION_DESC vbAllocDesc = {};
    vbAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD; // 頻繁に書き換えないならDEFAULT推奨ですが、今回は簡略化のためUPLOAD

    CD3DX12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);
    DX::ThrowIfFailed(allocator->CreateResource(
        &vbAllocDesc,
        &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        m_vertexBufferAllocation.ReleaseAndGetAddressOf(),
        IID_PPV_ARGS(m_vertexBufferResource.ReleaseAndGetAddressOf())
    ));

    //頂点バッファに名前を付ける
    m_vertexBufferResource->SetName(L"VertexBufferResource");



    // 頂点データの書き込み (Mapしてコピー)
    void* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0); // 読み取りはしない
    DX::ThrowIfFailed(m_vertexBufferResource->Map(0, &readRange, &pVertexDataBegin));
    memcpy(pVertexDataBegin, triangleVertices, vbSize);
    m_vertexBufferResource->Unmap(0, nullptr);


    // ------------------------------------------------------------------
    // 7. 定数バッファの作成とマップ (追加部分)
    // ------------------------------------------------------------------
    // 256バイトの倍数にアライメントする
    UINT cbSizeAligned = (sizeof(CBData) + 255) & ~255;
    CD3DX12_RESOURCE_DESC cbDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSizeAligned);

    D3D12MA::ALLOCATION_DESC cbAllocDesc = {};
    cbAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD; // 毎フレーム更新するためUPLOADヒープ

    DX::ThrowIfFailed(allocator->CreateResource(
        &cbAllocDesc,
        &cbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        m_constantBufferAllocation.ReleaseAndGetAddressOf(),
        IID_PPV_ARGS(m_constantBufferResource.ReleaseAndGetAddressOf())
    ));
    m_constantBufferResource->SetName(L"ConstantBufferResource");
    // CPU書き込み用のポインタを永続的に取得しておく
    DX::ThrowIfFailed(m_constantBufferResource->Map(0, &readRange, &m_cbvDataBegin));
    //バックバッファ用のテクスチャの作成
	CreateTexture(allocator, deviceResources);

    // ==================================================
    // フォールバックテクスチャ（白）の作成 (D3D12MA使用)
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

    // ==================================================
    // ディスクリプタヒープの作成（フォールバックテクスチャの準備後）
    // ==================================================
    resourceDescriptors = std::make_unique<DescriptorHeap>(device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        3);

    // ==================================================
    // ディスクリプタビューの作成
    // ==================================================
    CreateDescriptorViews(
        resourceDescriptors.get(),
        device,
        resourceDescriptors->Count(),
        m_outputTexture.Get(),
        m_vertexBufferResource.Get(),
        m_fallbackTexture.Get(),
        m_triangleCount,
        deviceResources->GetBackBufferFormat()
    );

  
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
		auto cpuBase = resourceDescriptors->GetCpuHandle(0);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format        = deviceResources->GetBackBufferFormat();
        device->CreateUnorderedAccessView(m_outputTexture.Get(), nullptr, &uavDesc, cpuBase);
    }
}

void DirectXTK12_ComputeRasterizer::Render(DX::DeviceResources* deviceResources)
{
    auto commandList = deviceResources->GetCommandList();

    // ------------------------------------------------------------------
    // 定数バッファの更新
    // ------------------------------------------------------------------
    CBData cbData = {};
    cbData.worldViewProj = DirectX::XMMatrixIdentity();
    cbData.screenSize = DirectX::XMFLOAT2(static_cast<float>(m_width),
        static_cast<float>(m_height));
    cbData.triangleCount = m_triangleCount;
    cbData.padding = 0.0f;

    // 正解：Map済みのCPUポインタに対してmemcpyを行う
    memcpy(m_cbvDataBegin, &cbData, sizeof(CBData));

    // ------------------------------------------------------------------
    // Bind descriptor heap and set pipeline state
    // ------------------------------------------------------------------
    ID3D12DescriptorHeap* heaps[] = { resourceDescriptors->Heap() };
    commandList->SetDescriptorHeaps(1, heaps);

    commandList->SetComputeRootSignature(m_rootSignature.Get());
    commandList->SetPipelineState(m_pipelineState.Get());

    auto gpuBase = resourceDescriptors->GetGpuHandle(0);
    commandList->SetComputeRootDescriptorTable(0, gpuBase);
    commandList->SetComputeRootDescriptorTable(1, resourceDescriptors->GetGpuHandle(SRV_VertexBuffer));

    // ルートパラメータ2: 紐付けは「GPUの仮想アドレス」を渡す
    commandList->SetComputeRootConstantBufferView(2, m_constantBufferResource->GetGPUVirtualAddress());

    // ------------------------------------------------------------------
    // Dispatch & Resource Barriers (以降は前回修正済みの正しいバリア処理)
    // ------------------------------------------------------------------
    const UINT dispatchX = (static_cast<UINT>(m_width) + 15) / 16;
    const UINT dispatchY = (static_cast<UINT>(m_height) + 15) / 16;
    commandList->Dispatch(dispatchX, dispatchY, 1);

    auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_outputTexture.Get());
    commandList->ResourceBarrier(1, &uavBarrier);

    D3D12_RESOURCE_BARRIER transitionBarriers[2] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputTexture.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_backBuffer.Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COPY_DEST)
    };
    commandList->ResourceBarrier(2, transitionBarriers);

    commandList->CopyResource(m_backBuffer.Get(), m_outputTexture.Get());

    D3D12_RESOURCE_BARRIER returnBarriers[2] = {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_backBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_RENDER_TARGET),
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_outputTexture.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    };
    commandList->ResourceBarrier(2, returnBarriers);
}