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


void DirectXTK12_ComputeRasterizer::CreateTexture(DX::DeviceResources* DR)
{
    auto device = DR->GetD3DDevice();
    auto commandQueue = DR->GetCommandQueue();

    // 1. DirectXTex を用いて画像ファイルを読み込む
    DirectX::TexMetadata metadata;
    DirectX::ScratchImage image;
    /*
      struct DIRECTX_TEX_API TexMetadata
    {
        size_t          width;
        size_t          height;     // Should be 1 for 1D textures
        size_t          depth;      // Should be 1 for 1D or 2D textures
        size_t          arraySize;  // For cubemap, this is a multiple of 6
        size_t          mipLevels;
        uint32_t        miscFlags;
        uint32_t        miscFlags2;
        DXGI_FORMAT     format;
        TEX_DIMENSION   dimension;

        size_t __cdecl ComputeIndex(size_t mip, size_t item, size_t slice) const noexcept;
            // Returns size_t(-1) to indicate an out-of-range error

        bool __cdecl IsCubemap() const noexcept { return (miscFlags & TEX_MISC_TEXTURECUBE) != 0; }
            // Helper for miscFlags

        bool __cdecl IsPMAlpha() const noexcept { return ((miscFlags2 & TEX_MISC2_ALPHA_MODE_MASK) == TEX_ALPHA_MODE_PREMULTIPLIED) != 0; }
        void __cdecl SetAlphaMode(TEX_ALPHA_MODE mode) noexcept { miscFlags2 = (miscFlags2 & ~static_cast<uint32_t>(TEX_MISC2_ALPHA_MODE_MASK)) | static_cast<uint32_t>(mode); }
        TEX_ALPHA_MODE __cdecl GetAlphaMode() const noexcept { return static_cast<TEX_ALPHA_MODE>(miscFlags2 & TEX_MISC2_ALPHA_MODE_MASK); }
            // Helpers for miscFlags2

        bool __cdecl IsVolumemap() const noexcept { return (dimension == TEX_DIMENSION_TEXTURE3D); }
            // Helper for dimension

        uint32_t __cdecl CalculateSubresource(size_t mip, size_t item) const noexcept;
        uint32_t __cdecl CalculateSubresource(size_t mip, size_t item, size_t plane) const noexcept;
            // Returns size_t(-1) to indicate an out-of-range error
    };
    */
  /*
  struct Image
{
    size_t      width;
    size_t      height;
    DXGI_FORMAT format;
    size_t      rowPitch;
    size_t      slicePitch;
    uint8_t*    pixels;
}
  */
	metadata.width = 512;
	metadata.height = 512;
	metadata.depth = 1;
	metadata.arraySize = 1;
	metadata.mipLevels = 1;
	metadata.miscFlags = 0;
	metadata.miscFlags2 = 0;
	metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM;
	metadata.dimension = TEX_DIMENSION_TEXTURE2D;
	metadata.SetAlphaMode(TEX_ALPHA_MODE_OPAQUE);

    // 2. テクスチャリソース(ID3D12Resource)の作成
    DX::ThrowIfFailed(
        DirectX::CreateTexture(
            device,
            metadata,
            m_outputTexture.ReleaseAndGetAddressOf()
        )
    );

    // 3. アップロード用のサブリソースデータを準備する
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DX::ThrowIfFailed(
        DirectX::PrepareUpload(
            device,
            image.GetImages(),
            image.GetImageCount(),
            metadata,
            subresources
        )
    );

    // 4. ResourceUploadBatch を使用して GPU へデータをアップロード
    ResourceUploadBatch resourceUpload(device);
    resourceUpload.Begin();

    resourceUpload.Upload(
		m_outputTexture.Get(),
        0,
        subresources.data(),
        static_cast<UINT>(subresources.size())
    );

    // リソースステートをシェーダーリソースに移行
    resourceUpload.Transition(
        tex.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );

    // アップロードを実行し、完了まで待機
    auto uploadResourcesFinished = resourceUpload.End(commandQueue);
    uploadResourcesFinished.wait();

    // 5. SRV (Shader Resource View) の作成
    // DirectXTK12の DirectXHelpers にある CreateShaderResourceView ヘルパー関数を利用して記述を簡略化
    DirectX::CreateShaderResourceView(
        device,
        tex.Get(),
        resourceDescriptors->GetCpuHandle(0)
    );


    // 6. サンプラーの作成 (既存コードのまま)
    D3D12_SAMPLER_DESC desc = {
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0,
        D3D12_MAX_MAXANISOTROPY,
        D3D12_COMPARISON_FUNC_NEVER,
        { 0, 0, 0, 0 },
        0,
        D3D12_FLOAT32_MAX
    };

    // ヒープの0番目に書き込む
    device->CreateSampler(
        &desc,
        resourceDescriptors->GetCpuHandle(0)
    );
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

    // ------------------------------------------------------------------
    // 4. Create UAV output texture (same format as back buffer)
    // ------------------------------------------------------------------
    {
    
      
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
        
		m_VertexBuffer = graphicsMemory->Allocate(vbSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        
       

       
       
        memcpy(m_VertexBuffer.Memory(), triangleVertices, vbSize);
       
    }

    // ------------------------------------------------------------------
    // 7. Create 1x1 white fallback texture using DirectXTex CreateTexture + TexMetadata
    // ------------------------------------------------------------------
    {
        // Prepare metadata describing a 1x1 RGBA8 texture
        DirectX::TexMetadata fbMeta = {};
        fbMeta.width = 1;
        fbMeta.height = 1;
        fbMeta.depth = 1;
        fbMeta.arraySize = 1;
        fbMeta.mipLevels = 1;
        fbMeta.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        fbMeta.dimension = TEX_DIMENSION_TEXTURE2D;
        fbMeta.SetAlphaMode(TEX_ALPHA_MODE_OPAQUE);

        // Create the GPU resource via DirectXTex helper
        DX::ThrowIfFailed(
            DirectX::CreateTexture(
                device,
                fbMeta,
                m_fallbackTexture.ReleaseAndGetAddressOf()
            )
        );
        m_fallbackTexture->SetName(L"ComputeRasterizerFallbackTexture");

        // Prepare a single D3D12_SUBRESOURCE_DATA containing a white pixel
        const uint32_t whitePixel = 0xFFFFFFFFu; // RGBA = 255,255,255,255
        D3D12_SUBRESOURCE_DATA subresource = {};
        subresource.pData = &whitePixel;
        subresource.RowPitch = sizeof(whitePixel);
        subresource.SlicePitch = sizeof(whitePixel);

        // Upload using ResourceUploadBatch (handles upload buffer creation internally)
        ResourceUploadBatch resourceUpload(device);
        resourceUpload.Begin();

        resourceUpload.Upload(
            m_fallbackTexture.Get(),
            0,
            &subresource,
            1
        );

        // Transition to NON_PIXEL_SHADER_RESOURCE for compute usage (matching previous code)
        resourceUpload.Transition(
            m_fallbackTexture.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
        );

        auto uploadDone = resourceUpload.End(commandQueue);
        uploadDone.wait();
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
