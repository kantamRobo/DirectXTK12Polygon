#include "pch.h"

#include <ResourceUploadBatch.h>
#include "DirectXTK12PolygonTessellate.h"














#include <DirectXMath.h>
#include "pch.h"

#include <d3dcompiler.h>
#include <d3dx12.h>

enum Descriptors
{
    WindowsLogo,
    CourierFont,
    ControllerFont,
    GamerPic,
    Count
};


struct SceneCB {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;
};
// Create root signature.
enum RootParameterIndex
{
    ConstantBuffer,
    TextureSRV,
    TextureSampler,
    RootParameterCount
};


using namespace DirectX;
HRESULT DirectXTK12PolygonTessellate::CreateBuffer(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResources* deviceResources, int height, int width)
{
    auto device = deviceResources->GetD3DDevice();
    graphicsMemory = std::make_unique<GraphicsMemory>(device);
    // 三角形の頂点データ

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

    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    DirectX::ResourceUploadBatch resourceUpload(deviceResources->GetD3DDevice());

    resourceUpload.Begin();
    // 頂点バッファの作成

    SceneCB scenecb;
    m_vertexBuffer = graphicsMemory->Allocate(sizeof(DirectX::VertexPosition) * vertices.size());
    memcpy(m_vertexBuffer.Memory(), vertices.data(), sizeof(DirectX::VertexPosition) * vertices.size());

    m_indexBuffer = graphicsMemory->Allocate(sizeof(DirectX::VertexPosition) * vertices.size());
    memcpy(m_vertexBuffer.Memory(), vertices.data(), sizeof(DirectX::VertexPosition) * vertices.size());

    SceneCBResource = graphicsMemory->Allocate(sizeof(SceneCB));
    memcpy(SceneCBResource.Memory(), &scenecb, sizeof(scenecb));
    TessCB tesscb;
    TessCBResource = graphicsMemory->Allocate(sizeof(TessCB));
    memcpy(TessCBResource.Memory(), &tesscb, sizeof(tesscb));
    m_indexBuffer = graphicsMemory->Allocate(sizeof(unsigned short) * indices.size());
    memcpy(m_indexBuffer.Memory(), indices.data(), sizeof(unsigned short) * indices.size());

    //(DirectXTK12Assimpで追加)
    m_vertexBufferView.BufferLocation = m_vertexBuffer.GpuAddress();
    m_vertexBufferView.StrideInBytes = sizeof(DirectX::VertexPosition);
    m_vertexBufferView.SizeInBytes = sizeof(DirectX::VertexPosition) * vertices.size();

    m_indexBufferView.BufferLocation = m_indexBuffer.GpuAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_indexBufferView.SizeInBytes = sizeof(unsigned short) * indices.size();



    DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);

    DirectX::XMVECTOR eye = DirectX::XMVectorSet(2.0f, 2.0f, -2.0f, 0.0f);
    DirectX::XMVECTOR focus = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookAtLH(eye, focus, up);

    constexpr float fov = DirectX::XMConvertToRadians(45.0f);
    float    aspect = (1200 / 600);
    float    nearZ = 0.1f;
    float    farZ = 100.0f;
    DirectX::XMMATRIX projMatrix = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);

    SceneCB cb;
    XMStoreFloat4x4(&cb.world, XMMatrixTranspose(worldMatrix));
    XMStoreFloat4x4(&cb.view, XMMatrixTranspose(viewMatrix));
    XMStoreFloat4x4(&cb.projection, XMMatrixTranspose(projMatrix));



    //定数バッファの作成(DIrectXTK12Assimpで追加)

    //https://github.com/microsoft/DirectXTK12/wiki/GraphicsMemory

    TessCB tesscb;
    SceneCBResource = graphicsmemory->AllocateConstant(cb);
    TessCBResource = graphicsmemory->AllocateConstant(tesscb);



    //定数バッファの作成(DIrectXTK12Assimpで追加)

    //https://github.com/microsoft/DirectXTK12/wiki/GraphicsMemory

    m_pipelineState = CreateGraphicsPipelineState(deviceResources, L"VertexShader.hlsl", L"PixelShader.hlsl");

    // リソースのアップロードを終了
    auto uploadResourcesFinished = resourceUpload.End(deviceResources->GetCommandQueue());
    return S_OK;
}




//(DIrectXTK12Assimpで追加)
void DirectXTK12PolygonTessellate::Draw(const DX::DeviceResources* DR) {


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
    commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST);



    // ルートシグネチャ設定
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    //2024/12/30/9:42
    commandList->SetGraphicsRootConstantBufferView(0, SceneCBResource.GpuAddress());
    commandList->SetGraphicsRootConstantBufferView(1, TessCBResource.GpuAddress());



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
Microsoft::WRL::ComPtr<ID3D12PipelineState> DirectXTK12PolygonTessellate::CreateGraphicsPipelineState(
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
        nullptr,
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
        nullptr,
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
    };

    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

    // Create root parameters and initialize first (constants)
    CD3DX12_ROOT_PARAMETER rootParameters[1] = {};
    rootParameters[RootParameterIndex::ConstantBuffer].InitAsConstantBufferView(0); 

        rootParameters[RootParameterIndex::ConstantBuffer].InitAsConstantBufferView(1);



        // Root parameter descriptor
        CD3DX12_ROOT_SIGNATURE_DESC rsigDesc = {};

    // use all parameters
    rsigDesc.Init(static_cast<UINT>(std::size(rootParameters)), rootParameters, 0, nullptr, rootSignatureFlags);

    DX::ThrowIfFailed(DirectX::CreateRootSignature(deviceresources->GetD3DDevice(), &rsigDesc, m_rootSignature.ReleaseAndGetAddressOf()));
    /*
    // ラスタライザーステート
    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // ブレンドステート
    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    */
    /*
    // 深度/ステンシルステート
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthStencilDesc.StencilEnable = FALSE;
    */
    /*
    // グラフィックパイプラインステートの設定
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState = depthStencilDesc;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = rtState.rtvFormats[0];
    psoDesc.DSVFormat = rtState.dsvFormat;
    psoDesc.SampleDesc.Count = 1;
    */
https://github.com/Microsoft/DirectXTK12/wiki/EffectPipelineStateDescription

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

    auto patchpd = pd.GetDesc();
    D3D12_SHADER_BYTECODE vertexshaderBCode = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };


    D3D12_SHADER_BYTECODE pixelShaderBCode = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };


    D3D12_SHADER_BYTECODE HullShaderBCode = { HullShader->GetBufferPointer(), HullShader->GetBufferSize() };

    D3D12_SHADER_BYTECODE DomainShaderBCode = { DomainShader->GetBufferPointer(), DomainShader->GetBufferSize() };

    patchpd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
    patchpd.VS = vertexshaderBCode;
    patchpd.PS = pixelShaderBCode;
    patchpd.HS = HullShaderBCode;
    patchpd.DS = DomainShaderBCode;
    // パイプラインステートオブジェクトを作成
    ComPtr<ID3D12PipelineState> pipelineState;
    
    device->CreateGraphicsPipelineState(&patchpd, IID_PPV_ARGS(pipelineState.ReleaseAndGetAddressOf()));
    
  

   
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create pipeline state");
    }

    return pipelineState;
}
