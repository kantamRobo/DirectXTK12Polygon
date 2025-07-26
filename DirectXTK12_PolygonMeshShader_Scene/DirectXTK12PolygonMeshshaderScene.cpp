#include "pch.h"
#include "DirectXTK12PolygonMeshshaderScene.h"
#include <d3dx12.h>
#include "ReadData.h"
#include <BufferHelpers.h>
#include <EffectPipelineStateDescription.h>
#include <CommonStates.h>
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


void DirectXTK12PolygonMeshshaderScene::Initialize(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResourcesMod* deviceResources, int height, int width)
{
    InitializeDXC();
    CreateBuffer(graphicsmemory, deviceResources, 1200, 600);
}
//-----------------------------------------------------------------------------
// ヘルパー: DXC の初期化（ライブラリ・コンパイラ・インクルードハンドラ）
//-----------------------------------------------------------------------------
void DirectXTK12PolygonMeshshaderScene::InitializeDXC()
{

    if (m_dxcLibrary)
        return; // すでに初期化済み

    // ライブラリとコンパイラの生成
    DX::ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&m_dxcLibrary)));
    DX::ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_dxcCompiler)));

    // デフォルトのインクルードハンドラ
    DX::ThrowIfFailed(m_dxcLibrary->CreateIncludeHandler(&m_dxcIncludeHandler));
}

HRESULT DirectXTK12PolygonMeshshaderScene::CreateBuffer(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResourcesMod* deviceResources, int height, int width)
{

    // �O�p�`�̒��_�f�[�^

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
    // ���_�o�b�t�@�̍쐬
    DX::ThrowIfFailed(
        DirectX::CreateStaticBuffer(
            deviceResources->GetD3DDevice(),
            resourceUpload,
            vertices.data(),
            static_cast<int>(vertices.size()),
            sizeof(DirectX::VertexPosition),
            D3D12_RESOURCE_STATE_COMMON,
            m_vertexBuffer.GetAddressOf()
        )
    );
    // �C���f�b�N�X�o�b�t�@�̍쐬
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


    //(DirectXTK12Assimp�Œǉ�)
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(DirectX::VertexPositionNormal);
    m_vertexBufferView.SizeInBytes = sizeof(DirectX::VertexPositionNormal) * vertices.size();

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
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



    //�萔�o�b�t�@�̍쐬(DIrectXTK12Assimp�Œǉ�)

    //https://github.com/microsoft/DirectXTK12/wiki/GraphicsMemory


    //SceneCBResource = graphicsmemory->AllocateConstant(cb);

    //�萔�o�b�t�@�̍쐬(DIrectXTK12Assimp�Œǉ�)

    //https://github.com/microsoft/DirectXTK12/wiki/GraphicsMemory

    m_pipelineState = CreateGraphicsPipelineState(deviceResources, L"VertexShader.hlsl", L"SimpleTrianglePS.hlsl", L"SimpleTriangleMS.hlsl");

    // ���\�[�X�̃A�b�v���[�h���I��
    auto uploadResourcesFinished = resourceUpload.End(deviceResources->GetCommandQueue());
    return S_OK;
}


//(DIrectXTK12Assimp�Œǉ�)
void DirectXTK12PolygonMeshshaderScene::Draw(GraphicsMemory* graphic, DX::DeviceResourcesMod* DR) {


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



    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



    // ���[�g�V�O�l�`���ݒ�
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    //2024/12/30/9:42
    commandList->SetGraphicsRootConstantBufferView(0, SceneCBResource.GpuAddress());

    // �p�C�v���C���X�e�[�g�ݒ�
    commandList->SetPipelineState(m_pipelineState.Get());



    // Only need one threadgroup to draw a single triangle.
    commandList->DispatchMesh(1, 1, 1);

    auto uploadResourcesFinished = resourceUpload.End(
        DR->GetCommandQueue());


    PIXEndEvent();

    uploadResourcesFinished.wait();
}
using Microsoft::WRL::ComPtr;
Microsoft::WRL::ComPtr<ID3D12PipelineState> DirectXTK12PolygonMeshshaderScene::CreateGraphicsPipelineState(
    DX::DeviceResourcesMod* devResources,
    const std::wstring& vsPath,
    const std::wstring& psPath,
    const std::wstring& msPath)
{
    auto device = devResources->GetD3DDevice();

    // 1) DXC の初期化
    InitializeDXC();


    auto psBlob = CompileShaderDXC(
        m_dxcLibrary.Get(), m_dxcCompiler.Get(), m_dxcIncludeHandler.Get(),
        psPath, L"main", L"ps_6_5");

    auto msBlob = CompileShaderDXC(
        m_dxcLibrary.Get(), m_dxcCompiler.Get(), m_dxcIncludeHandler.Get(),
        msPath, L"main", L"ms_6_5");

    // 3) 既存のルートシグネチャ／レイアウト設定は省略（元のコードを流用）

    // --- 例: 共通の EffectPipelineStateDescription を再利用 ---
    DirectX::RenderTargetState rtState(
        DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT_D32_FLOAT);

    DirectX::EffectPipelineStateDescription pd(
        /* inputLayout */ nullptr,
        /* blend */ DirectX::CommonStates::Opaque,
        /* depth */ DirectX::CommonStates::DepthDefault,
        /* raster */ DirectX::CommonStates::CullCounterClockwise,
        rtState);


    // 1) ルートシグネチャ -------------------------------------------------
    CD3DX12_ROOT_PARAMETER1 rootParams[1] = {};
    rootParams[ConstantBuffer].InitAsConstantBufferView(0, 0);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rsDesc;
    rsDesc.Init_1_1(_countof(rootParams), rootParams,
        0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> rsBlob, rsErr;
    DX::ThrowIfFailed(D3D12SerializeVersionedRootSignature(
        &rsDesc, &rsBlob, &rsErr));

    DX::ThrowIfFailed(device->CreateRootSignature(
        0,
        rsBlob->GetBufferPointer(),
        rsBlob->GetBufferSize(),
        IID_PPV_ARGS(m_rootSignature.ReleaseAndGetAddressOf())));

    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC meshDesc = {};
    meshDesc.pRootSignature = m_rootSignature.Get();
    meshDesc.MS = { msBlob->GetBufferPointer(), msBlob->GetBufferSize() };
    meshDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    meshDesc.BlendState = pd.blendDesc;
    meshDesc.SampleMask = pd.renderTargetState.sampleMask;
    meshDesc.RasterizerState = pd.rasterizerDesc;
    meshDesc.DepthStencilState = pd.depthStencilDesc;
    meshDesc.PrimitiveTopologyType = pd.primitiveTopology;
    meshDesc.NumRenderTargets = pd.renderTargetState.numRenderTargets;
    memcpy(meshDesc.RTVFormats, pd.renderTargetState.rtvFormats,
        sizeof(DXGI_FORMAT) * D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);
    meshDesc.DSVFormat = pd.renderTargetState.dsvFormat;
    meshDesc.SampleDesc = pd.renderTargetState.sampleDesc;
    meshDesc.NodeMask = pd.renderTargetState.nodeMask;



    ComPtr<ID3D12PipelineState> pso;


    // meshDesc が D3DX12_MESH_SHADER_PIPELINE_STATE_DESC meshDesc; 定義済みとする
    CD3DX12_PIPELINE_MESH_STATE_STREAM pipelineStream(meshDesc);
    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc = {
        sizeof(pipelineStream),
        &pipelineStream
    };
    DX::ThrowIfFailed(device->CreatePipelineState(&streamDesc, IID_GRAPHICS_PPV_ARGS(&pso)));

    return pso;
}