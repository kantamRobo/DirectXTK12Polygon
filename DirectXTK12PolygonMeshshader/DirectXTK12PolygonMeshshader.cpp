#include "pch.h"
#include "DirectXTK12PolygonMeshshader.h"
#include <d3dx12.h>
#include "ReadData.h"










#include <dxcapi.h>    // ← DXC API
#include <DirectXMath.h>
#include <ResourceUploadBatch.h>
#include <d3dcompiler.h>


enum Descriptors
{
    WindowsLogo,
    CourierFont,
    ControllerFont,
    GamerPic,
    Count
};
using namespace DirectX;


void DirectXTK12MeshShader::Initialize(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResourcesMod* deviceResources, int height, int width)
{
    InitializeDXC();
    CreateBuffer(graphicsmemory, deviceResources, 1200, 600);
}
//-----------------------------------------------------------------------------
// ヘルパー: DXC の初期化（ライブラリ・コンパイラ・インクルードハンドラ）
//-----------------------------------------------------------------------------
void DirectXTK12MeshShader::InitializeDXC()
{
    
    if (m_dxcLibrary)
        return; // すでに初期化済み

    // ライブラリとコンパイラの生成
    DX::ThrowIfFailed(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&m_dxcLibrary)));
    DX::ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_dxcCompiler)));

    // デフォルトのインクルードハンドラ
    DX::ThrowIfFailed(m_dxcLibrary->CreateIncludeHandler(&m_dxcIncludeHandler));
}

HRESULT DirectXTK12MeshShader::CreateBuffer(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResourcesMod* deviceResources, int height, int width)
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
            sizeof(uint32_t),
            D3D12_RESOURCE_STATE_COMMON,
            m_indexBuffer.GetAddressOf()
        )
    );

    // バッファアップロードが終わる前後でOKですが、GPU可視のSRVヒープを先に用意しておくと楽です。
// DirectXTK12 の DescriptorHeap を既にメンバに持っているのでそれを利用:
   
        // CBV/SRV/UAV 用のシェーダ可視ヒープを 2 枚（頂点/インデックス）確保
        m_resourceDescriptors = std::make_unique<DirectX::DescriptorHeap>(
            deviceResources->GetD3DDevice(),

            Descriptors::Count);
        auto HeapsDesc = m_resourceDescriptors->Heap()->GetDesc();
        // 1) SRVヒープを1本だけ作る（2つ分の枠）
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.NumDescriptors = 2; // 頂点SRV + インデックスSRV
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        DX::ThrowIfFailed(deviceResources->GetD3DDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_heaps)));

        // 頂点バッファ：StructuredBuffer<VertexPosition>
        D3D12_SHADER_RESOURCE_VIEW_DESC vtxSrvDesc = {};
        vtxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        vtxSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        vtxSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
        vtxSrvDesc.Buffer.FirstElement = 0;
        vtxSrvDesc.Buffer.NumElements = static_cast<UINT>(vertices.size());
        vtxSrvDesc.Buffer.StructureByteStride = sizeof(DirectX::VertexPosition);
        vtxSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        auto cpu0 = m_resourceDescriptors->GetCpuHandle(0);
        deviceResources->GetD3DDevice()->CreateShaderResourceView(
            m_vertexBuffer.Get(), &vtxSrvDesc, cpu0);

        // インデックスバッファ：StructuredBuffer<uint>
        D3D12_SHADER_RESOURCE_VIEW_DESC idxSrvDesc = {};
        idxSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

        idxSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        idxSrvDesc.Format = DXGI_FORMAT_UNKNOWN;

        idxSrvDesc.Buffer.FirstElement = 0;
        idxSrvDesc.Buffer.NumElements = static_cast<UINT>(indices.size());
        idxSrvDesc.Buffer.StructureByteStride = sizeof(uint32_t);
        idxSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        const UINT inc = deviceResources->GetD3DDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        auto cpuStart = m_heaps->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE cpuVtx = cpuStart;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuIdx = { cpuStart.ptr + inc };

        // 2) それぞれのSRVを同ヒープの別スロットに作る
        deviceResources->GetD3DDevice()->CreateShaderResourceView(m_vertexBuffer.Get(), &vtxSrvDesc, cpuVtx);
        deviceResources->GetD3DDevice()->CreateShaderResourceView(m_indexBuffer.Get(), &idxSrvDesc, cpuIdx);


        auto sz = deviceResources->GetOutputSize();
        float aspect = float(sz.right - sz.left) / float(sz.bottom - sz.top);

   
        using namespace DirectX;
        XMMATRIX world = XMMatrixIdentity();
        XMVECTOR eye = XMVectorSet(0, 0, -2, 0);
        XMVECTOR focus = XMVectorSet(0, 0, 0, 0);
        XMVECTOR up = XMVectorSet(0, 1, 0, 0);

        XMMATRIX view = XMMatrixLookAtLH(eye, focus, up);
        XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), aspect, 0.1f, 100.0f);

        SceneCB cb;
        XMStoreFloat4x4(&cb.world, XMMatrixTranspose(world));
        XMStoreFloat4x4(&cb.view, XMMatrixTranspose(view));
        XMStoreFloat4x4(&cb.projection, XMMatrixTranspose(proj));

        SceneCBResource = graphicsmemory->AllocateConstant(cb);   // ← 古いCBの代わりに最新を確保

    //�萔�o�b�t�@�̍쐬(DIrectXTK12Assimp�Œǉ�)

    //https://github.com/microsoft/DirectXTK12/wiki/GraphicsMemory

    m_pipelineState = CreateGraphicsPipelineState(deviceResources, L"VertexShader.hlsl", L"SimpleTrianglePS.hlsl", L"SimpleTriangleMS.hlsl");

    // ���\�[�X�̃A�b�v���[�h���I��
    auto uploadResourcesFinished = resourceUpload.End(deviceResources->GetCommandQueue());
    return S_OK;
}


void DirectXTK12MeshShader::Draw(GraphicsMemory* graphic, DX::DeviceResourcesMod* DR)
{
    DirectX::ResourceUploadBatch resourceUpload(DR->GetD3DDevice());
    resourceUpload.Begin();

    if (vertices.empty() || indices.empty()) return;

    auto* commandList = DR->GetCommandList();

    // ルートシグネチャ
    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // CBV（b0）: 既存のまま
    commandList->SetGraphicsRootConstantBufferView(ConstantBuffer, SceneCBResource.GpuAddress());

    // パイプライン（MS/PS）
    commandList->SetPipelineState(m_pipelineState.Get());

    // ★ SRVヒープのセット
    ID3D12DescriptorHeap* heaps[] = { m_heaps.Get()};
    commandList->SetDescriptorHeaps(1, heaps);

    // ★ t0（頂点）からのテーブルをバインド（t0=頂点, t1=インデックス）// t0-t1 テーブルをセット
    auto gpuStart = m_heaps->GetGPUDescriptorHandleForHeapStart();
    commandList->SetGraphicsRootDescriptorTable(1, gpuStart);
    // メッシュを 1 グループだけディスパッチ
    commandList->DispatchMesh(1, 1, 1);
    
    auto fini = resourceUpload.End(DR->GetCommandQueue());
    fini.wait();
}

using Microsoft::WRL::ComPtr;
Microsoft::WRL::ComPtr<ID3D12PipelineState> DirectXTK12MeshShader::CreateGraphicsPipelineState(
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
        /* raster */ DirectX::CommonStates::CullNone,
        rtState);

 
    // 1) ルートシグネチャ -------------------------------------------------
    CD3DX12_ROOT_PARAMETER1 rootParams[2] = {};
    // CreateGraphicsPipelineState内
    CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, /*NumDescriptors*/2, /*baseShaderRegister*/0);

   
    rootParams[0].InitAsConstantBufferView(0);            // b0 : SceneCB
    rootParams[1].InitAsDescriptorTable(1, ranges);       // t0-t1 : SRVテーブル

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