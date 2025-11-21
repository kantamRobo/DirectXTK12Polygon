#include "pch.h"
#include "DirectXTK12_PolygonSceneTexture.h"
#include <DirectXMath.h>

#include <d3dx12.h>
#include <RenderTargetState.h>
#include <EffectPipelineStateDescription.h>
#include <d3dcompiler.h>
#include <DirectXHelpers.h>
#include <CommonStates.h>
#include <WICTextureLoader.h>
#include <ResourceUploadBatch.h>
// Create root signature.
enum RootParameterIndex
{
	ConstantBuffer = 0,
	TextureSRV = 1,
	TextureSampler = 2,
	RootParameterCount = 3
};
void DirectXTK12_PolygonSceneTexture::CreateTexture(DX::DeviceResources* DR)
{
    auto device = DR->GetD3DDevice();

   DirectX:: ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device, resourceUpload, ここに自由に画像パスを入力, tex.ReleaseAndGetAddressOf(), true
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
DirectXTK12_PolygonSceneTexture::DirectXTK12_PolygonSceneTexture(DX::DeviceResources* DR)
{
    vertices.resize(3);

    vertices[0].position.x = 0.0f;
    vertices[0].position.y = 0.5f;
    vertices[0].position.z = 0.0f;

    vertices[1].position.x = 0.5f;
    vertices[1].position.y = -0.5f;
    vertices[1].position.z = 0.0f;

    vertices[2].position.x = -0.5f;
    vertices[2].position.y = -0.5f;
    vertices[2].position.z = 0.0f;
    indices.resize(3);
    vertices[0].textureCoordinate = { 0.5f, 0.0f };               // テクスチャの中央上
    vertices[1].textureCoordinate = { 1.0f, 1.0f };               // テクスチャの右下
    vertices[2].textureCoordinate = { 0.0f, 1.0f };               // テクスチャの左下

    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
	auto device = DR->GetD3DDevice();
	m_graphicsMemory = std::make_unique<DirectX::GraphicsMemory>(device);
   
    m_vertexBuffer = m_graphicsMemory->Allocate(sizeof(DirectX::VertexPositionNormalTexture) * vertices.size());
    memcpy(m_vertexBuffer.Memory(), vertices.data(), sizeof(DirectX::VertexPositionNormalTexture) * vertices.size());

    m_indexBuffer = m_graphicsMemory->Allocate(sizeof(unsigned short) * indices.size());
    memcpy(m_indexBuffer.Memory(), indices.data(), sizeof(unsigned short) * indices.size());



    //(DirectXTK12Assimpで追加)
   // DirectXTK12Spehere.cpp
    m_vertexBufferView.BufferLocation = m_vertexBuffer.GpuAddress();
    m_vertexBufferView.StrideInBytes = sizeof(DirectX::VertexPositionNormalTexture);
    m_vertexBufferView.SizeInBytes = UINT(sizeof(DirectX::VertexPositionNormalTexture) * vertices.size()); // ←必ずVertexPositionNormalで揃える

    m_indexBufferView.BufferLocation = m_indexBuffer.GpuAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_indexBufferView.SizeInBytes = sizeof(unsigned short) * indices.size();

    auto sz = DR->GetOutputSize();
	
    DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);

    DirectX::XMVECTOR eye = DirectX::XMVectorSet(0.0f, 0.0f, -2.0f, 0.0f);  // 距離は好みで -1.5f～-5.0f くらい
    DirectX::XMVECTOR focus = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookAtLH(eye, focus, up);

    constexpr float fov = DirectX::XMConvertToRadians(45.0f);
    float aspect = float(sz.right - sz.left) / float(sz.bottom - sz.top);
    float    nearZ = 0.1f;
    float    farZ = 100.0f;
    DirectX::XMMATRIX projMatrix = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);

    SceneCB cb;
    XMStoreFloat4x4(&cb.world, XMMatrixTranspose(worldMatrix));
    XMStoreFloat4x4(&cb.view, XMMatrixTranspose(viewMatrix));
    XMStoreFloat4x4(&cb.projection, XMMatrixTranspose(projMatrix));



    //定数バッファの作成(DIrectXTK12Assimpで追加)

    //https://github.com/microsoft/DirectXTK12/wiki/GraphicsMemory


    SceneCBResource = m_graphicsMemory->AllocateConstant(cb);
    
    m_samplerDescriptor = std::make_unique<DirectX::DescriptorHeap>(
       device,
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        1
	);
    m_srvDescriptor = std::make_unique<DirectX::DescriptorHeap>(
        device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        1);
    CreateTexture(DR);
	m_pipelineState = CreateGraphicsPipelineState(
        DR,
        L"VertexShader.hlsl",
        L"PixelShader.hlsl"
	);



}

//(DIrectXTK12Assimpで追加)
// グラフィックパイプラインステートを作成する関数
Microsoft::WRL::ComPtr<ID3D12PipelineState> DirectXTK12_PolygonSceneTexture::CreateGraphicsPipelineState(
    DX::DeviceResources* deviceresources,

    const std::wstring& vertexShaderPath,
    const std::wstring& pixelShaderPath)
{
    // シェーダーをコンパイル
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
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

    // 入力レイアウトを定義
   m_layout = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D12_APPEND_ALIGNED_ELEMENT,      D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
   // Create root parameters and initialize first (constants)
   CD3DX12_DESCRIPTOR_RANGE ranges[2] = {};

   ranges[0].Init(
       D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // SRV
	   1,// number of descriptors
       0,                                // register(t0)
       0,                                // space 0
       D3D12_DESCRIPTOR_RANGE_FLAG_NONE
   );
   ranges[1].Init(
       D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
	   1,// number of descriptors
       0,     //register s0                                        
       0,                                    // register space
       D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

   CD3DX12_ROOT_PARAMETER rootParameters[3] = {};
   rootParameters[0].InitAsDescriptorTable(
	   1,// number of descriptor ranges
	   &ranges[0],// descriptor ranges
	   D3D12_SHADER_VISIBILITY_PIXEL// Shader visibility
   );
   rootParameters[1].InitAsDescriptorTable(
      
	   1,// number of descriptor ranges
	   &ranges[1],// descriptor ranges
	   D3D12_SHADER_VISIBILITY_PIXEL// Shader visibility
   );
   rootParameters[2].InitAsConstantBufferView(
	   0 // register b0
   );
  
  
   auto rootSignatureFlags =
       D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
       D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
       D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
	   D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    
   D3D12_ROOT_SIGNATURE_DESC rsigDesc = {};

	rsigDesc.Flags = rootSignatureFlags;
	rsigDesc.NumParameters = _countof(rootParameters);
	rsigDesc.pParameters = rootParameters;
	rsigDesc.NumStaticSamplers = 0;
	rsigDesc.pStaticSamplers = nullptr;

   
    DirectX::CreateRootSignature(
        deviceresources->GetD3DDevice(),
        &rsigDesc,
        m_rootSignature.ReleaseAndGetAddressOf()
	);

    //https://github.com/microsoft/DirectXTK12/wiki/PSOs,-Shaders,-and-Signatures
    // 
    // 
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
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

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
void DirectXTK12_PolygonSceneTexture::Render(DX::DeviceResources* DR)
{
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


    ID3D12DescriptorHeap* descriptorHeaps[] = {
        m_srvDescriptor->Heap(),
        m_samplerDescriptor->Heap()
	};
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    // ルートシグネチャ設定
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    commandList->SetGraphicsRootDescriptorTable(0, m_srvDescriptor->GetFirstGpuHandle());
    commandList->SetGraphicsRootDescriptorTable(1, m_samplerDescriptor->GetFirstGpuHandle());



    commandList->SetGraphicsRootConstantBufferView( 2, SceneCBResource.GpuAddress());
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