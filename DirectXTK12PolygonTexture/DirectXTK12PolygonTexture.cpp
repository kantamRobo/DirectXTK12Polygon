#include "pch.h"
#include "DirectXTK12PolygonTexture.h"
#include <ResourceUploadBatch.h>
#include<WICTextureLoader.h>
#include <DeviceResources.h>
#include <EffectPipelineStateDescription.h>
#include <CommonStates.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
using namespace DirectX;

void DirectXTK12PolygonTexture::CreateTexture(DX::DeviceResources* DR)
{
    auto device = DR->GetD3DDevice();
 
    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device, resourceUpload,L"C:\\Users\\hatte\\OneDrive\\Pictures\\Screenshots\\スクリーンショット 2025-07-23 172311.png",            tex.ReleaseAndGetAddressOf(), true
    ));

    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = tex->GetDesc().Format; // tex は ID3D12Resource
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

   
    device->CreateShaderResourceView(
        tex.Get(),
        &srvDesc,
        m_srvDescriptor->GetCpuHandle(0)
    );

    // LinearClamp
    D3D12_SAMPLER_DESC desc = { D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        0, D3D12_MAX_MAXANISOTROPY, D3D12_COMPARISON_FUNC_NEVER,
        { 0, 0, 0, 0 }, 0, D3D12_FLOAT32_MAX };


    // ヒープの0番目に書き込む
    device->CreateSampler(
        &desc,
        m_samplerDescriptor->GetCpuHandle(0));
    auto uploadResourcesFinished = resourceUpload.End(DR->GetCommandQueue());

    // Wait for the upload thread to terminate
    uploadResourcesFinished.wait();
    
}
enum Descriptors
{
    WindowsLogo,
    CourierFont,
    ControllerFont,
    GamerPic,
    Count
};
HRESULT DirectXTK12PolygonTexture::CreateBuffer(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResources* deviceResources, int height, int width)
{

    // 頂点構造体は VertexPositionColorTexture
    
    // 頂点 0: 中央上
  
    vertices.resize(3);
    vertices[0].position = { 0.0f,  0.5f, 0.0f };
  
    vertices[0].textureCoordinate = { 0.5f, 0.0f };               // テクスチャの中央上

    // 頂点 1: 右下
    vertices[1].position = { 0.5f, -0.5f, 0.0f };
   
    vertices[1].textureCoordinate = { 1.0f, 1.0f };               // テクスチャの右下

    // 頂点 2: 左下
    vertices[2].position = { -0.5f, -0.5f, 0.0f };
  
    vertices[2].textureCoordinate = { 0.0f, 1.0f };               // テクスチャの左下
    indices.resize(3);

    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    DirectX::ResourceUploadBatch resourceUpload(deviceResources->GetD3DDevice());

    resourceUpload.Begin();
    // 頂点バッファの作成
    DX::ThrowIfFailed(
        DirectX::CreateStaticBuffer(
            deviceResources->GetD3DDevice(),
            resourceUpload,
            vertices.data(),
            static_cast<int>(vertices.size()),
            sizeof(DirectX::VertexPositionColorTexture),
            D3D12_RESOURCE_STATE_COMMON,
            m_vertexBuffer.GetAddressOf()
        )
    );
    // インデックスバッファの作成
    DX::ThrowIfFailed(
        DirectX::CreateStaticBuffer(
            deviceResources->GetD3DDevice(),
            resourceUpload,
            indices.data(),
            indices.size(),
            sizeof(unsigned short),
            D3D12_RESOURCE_STATE_COMMON,
            m_indexBuffer.GetAddressOf()
        )
    );


    //(DirectXTK12Assimpで追加)
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(DirectX::VertexPositionColorTexture);
    m_vertexBufferView.SizeInBytes = sizeof(DirectX::VertexPositionColorTexture) * vertices.size();

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_indexBufferView.SizeInBytes = sizeof(unsigned short) * indices.size();



    //定数バッファの作成(DIrectXTK12Assimpで追加)

    //https://github.com/microsoft/DirectXTK12/wiki/GraphicsMemory

    m_pipelineState = CreateGraphicsPipelineState(deviceResources, L"VertexShader.hlsl", L"PixelShader.hlsl");
    //              ↓ タイプをCBV_SRV_UAVに、フラグをSHADER_VISIBLEに変更
    m_srvDescriptor = std::make_unique<DescriptorHeap>(
        deviceResources->GetD3DDevice(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        1
    );
    // サンプラーヒープを作成するだけで、CreateSampler を呼んでいない
    m_samplerDescriptor = std::make_unique<DescriptorHeap>(
        deviceResources->GetD3DDevice(),
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        32);

   
    // リソースのアップロードを終了
    auto uploadResourcesFinished = resourceUpload.End(deviceResources->GetCommandQueue());
    return S_OK;
}


//(DIrectXTK12Assimpで追加)
void DirectXTK12PolygonTexture::Draw(const DX::DeviceResources* DR) {


    DirectX::ResourceUploadBatch resourceUpload(DR->GetD3DDevice());

    resourceUpload.Begin();

    if (vertices.empty() || indices.empty()) {
        OutputDebugStringA("Vertices or indices buffer is empty.\n");
        return;
    }


    auto commandList = DR->GetCommandList();
    auto renderTarget = DR->GetRenderTarget();
    if (!commandList) {
        OutputDebugStringA("Command list is null.\n");
        return;
    }

    // 入力アセンブラー設定
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


    ID3D12DescriptorHeap* DescriptorHeaps[] = {m_srvDescriptor->Heap(),m_samplerDescriptor->Heap()};
    // ルートシグネチャ設定
    commandList->SetDescriptorHeaps(_countof(DescriptorHeaps), DescriptorHeaps);
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->SetGraphicsRootDescriptorTable(1, m_srvDescriptor->GetFirstGpuHandle());
    commandList->SetGraphicsRootDescriptorTable(0,m_samplerDescriptor->GetFirstGpuHandle());

    // パイプラインステート設定
    commandList->SetPipelineState(m_pipelineState.Get());

    // 描画コール
    commandList->DrawIndexedInstanced(
        static_cast<UINT>(indices.size()), // インデックス数
        1,                                 // インスタンス数
        0,                                 // 開始インデックス
        0,                                 // 頂点オフセット
        0                                  // インスタンスオフセット
    );



    auto uploadResourcesFinished = resourceUpload.End(
        DR->GetCommandQueue());

    uploadResourcesFinished.wait();
}

using Microsoft::WRL::ComPtr;
//(DIrectXTK12Assimpで追加)
// グラフィックパイプラインステートを作成する関数
Microsoft::WRL::ComPtr<ID3D12PipelineState> DirectXTK12PolygonTexture::CreateGraphicsPipelineState(
    DX::DeviceResources* deviceresources,

    const std::wstring& vertexShaderPath,
    const std::wstring& pixelShaderPath)
{
    auto device = deviceresources->GetD3DDevice();
    // シェーダーをコンパイル
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ComPtr<ID3DBlob> errorBlob;
    DirectX::RenderTargetState rtState(DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT);
    HRESULT hr = D3DCompileFromFile(
        vertexShaderPath.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main", // エントリーポイント
        "vs_5_0", // シェーダーモデル
        0,
        0,
        &vertexShader,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        throw std::runtime_error("Failed to compile vertex shader");
    }

    hr = D3DCompileFromFile(
        pixelShaderPath.c_str(),
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "ps_5_0",
        0,
        0,
        &pixelShader,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        throw std::runtime_error("Failed to compile pixel shader");
    }

    m_layout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,     0, 0,                                 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, D3D12_APPEND_ALIGNED_ELEMENT,      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D12_APPEND_ALIGNED_ELEMENT,      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };


    // Create root signature.
    enum RootParameterIndex
    {
        ConstantBuffer,
        TextureSRV,
        TextureSampler,
        RootParameterCount
    };

        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }
        // “DATA_STATIC” → “NONE” に置き換え
        CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
        ranges[0].Init(
            D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,  // レンジタイプ：サンプラー
            1,                                    // デスクリプタ数
            1,                                    // シェーダーレジスタ（t1等）
            0,                                    // register space
            D3D12_DESCRIPTOR_RANGE_FLAG_NONE      // フラグは NONE
        ); 

        // SRV 用レンジ（Texture2D はここでテーブルにまとめる）
        ranges[1].Init(
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // SRV
            1,                                // デスクリプタ数
            0,                                // register(t0)
            0,                                // space 0
            D3D12_DESCRIPTOR_RANGE_FLAG_NONE
        );
        CD3DX12_ROOT_PARAMETER1 rootParameters[2] = {};
        // ← これまで InitAsShaderResourceView(0) だった箇所を descriptor table に
        rootParameters[0].InitAsDescriptorTable(
            1,             // レンジの数
            &ranges[0],     // 先ほど定義したレンジ
            D3D12_SHADER_VISIBILITY_PIXEL
        );
        // 他のパラメータ（例：CBV など）はそのまま
        rootParameters[1].InitAsDescriptorTable(
            1,             // レンジの数
            &ranges[1],     // 先ほど定義したレンジ
            D3D12_SHADER_VISIBILITY_PIXEL
        );
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
      auto root = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error);
        if (FAILED(root)) {
            if (error) {
                OutputDebugStringA((char*)error->GetBufferPointer());
            }
            throw std::runtime_error("Failed to serialize root signature");
        }
        DX::ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
 

    D3D12_INPUT_LAYOUT_DESC inputlayaout = { m_layout.data(), m_layout.size() };
   
    
    
    DirectX::EffectPipelineStateDescription pd(
        &inputlayaout,
        DirectX::CommonStates::Opaque,
        DirectX::CommonStates::DepthDefault,
        DirectX::CommonStates::CullCounterClockwise,
        rtState);
    D3D12_SHADER_BYTECODE vertexshaderBCode = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };


    D3D12_SHADER_BYTECODE pixelShaderBCode = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    // パイプラインステートオブジェクトを作成
    ComPtr<ID3D12PipelineState> pipelineState;
    pd.CreatePipelineState(
        deviceresources->GetD3DDevice(),
        m_rootSignature.Get(),
        vertexshaderBCode,

        pixelShaderBCode,

        pipelineState.GetAddressOf()
    );
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create pipeline state");
    }

    return pipelineState;
}