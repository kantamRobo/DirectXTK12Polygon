// tekito-.h  (fixed minimal version)
#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <memory>
#include <string>
#include <cstring>

#include "d3dx12.h"
#include <DescriptorHeap.h>
#include <GraphicsMemory.h>
#include <DeviceResources.h>
#include "RenderTexture.h"

using Microsoft::WRL::ComPtr;

inline void ThrowIfFailed(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("hr failed"); }

class RenderPassTwoTriangles
{
public:
    void Create(DX::DeviceResources* DR, const wchar_t* shaderFile = L"tekito.hlsl")
    {
        shaderFile_ = shaderFile;
        auto device = DR->GetD3DDevice();

        // GraphicsMemory（頂点/定数に使用）
        graphicsMemory_ = std::make_unique<DirectX::GraphicsMemory>(device);

        // ルートシグネチャ & PSO
        BuildRootSig(device);
        BuildShadersAndPSO(device, DR->GetBackBufferFormat());

        // オフスクリーンRT用のディスクリプタ（CPU）
        srvCpu_ = std::make_unique<DirectX::DescriptorHeap>(
            device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 1);

        rtvOffCpu_ = std::make_unique<DirectX::DescriptorHeap>(
            device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 1);

        // オフスクリーンRT生成
        offscreenRT_ = std::make_unique<RenderTexture>(DR->GetBackBufferFormat());
        offRTV_ = rtvOffCpu_->GetCpuHandle(0);
        offscreenRT_->SetDevice(device, srvCpu_->GetCpuHandle(0), offRTV_);

        // サイズ合わせ
        OnSizeChanged(DR);

        // 頂点（2D三角×2）
        struct Vertex { float x, y; };
        const Vertex triA[3] = { {-0.8f, -0.5f}, {0.0f, 0.7f}, {0.8f, -0.5f} };
        const Vertex triB[3] = { {-0.3f, -0.6f}, {-0.9f, 0.6f}, {0.3f, 0.6f} };

        vbA_ = graphicsMemory_->Allocate(sizeof(triA));
        std::memcpy(vbA_.Memory(), triA, sizeof(triA));
        vbvA_.BufferLocation = vbA_.GpuAddress();
        vbvA_.StrideInBytes = sizeof(Vertex);
        vbvA_.SizeInBytes = sizeof(triA);

        vbB_ = graphicsMemory_->Allocate(sizeof(triB));
        std::memcpy(vbB_.Memory(), triB, sizeof(triB));
        vbvB_.BufferLocation = vbB_.GpuAddress();
        vbvB_.StrideInBytes = sizeof(Vertex);
        vbvB_.SizeInBytes = sizeof(triB);

        // 定数バッファ（色）
        cb_ = graphicsMemory_->Allocate(sizeof(ColorCB), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        cbCPU_ = reinterpret_cast<ColorCB*>(cb_.Memory());
    }

    void OnSizeChanged(DX::DeviceResources* DR)
    {
        auto r = DR->GetOutputSize();
        const size_t w = size_t(r.right - r.left);
        const size_t h = size_t(r.bottom - r.top);
        offscreenRT_->SizeResources(w, h);
        offscreenRT_->SetWindow(r);
       
    }

    void Render(DX::DeviceResources* DR)
    {
        // コマンドリスト取得 + CL4 へQI
        cmdList_ = DR->GetCommandList();
		
        cmdList4_.Reset();
        ThrowIfFailed(cmdList_->QueryInterface(IID_PPV_ARGS(cmdList4_.ReleaseAndGetAddressOf())));

        // 共通設定
        cmdList_->SetPipelineState(pso_.Get());
        cmdList_->SetGraphicsRootSignature(rootSig_.Get());
        cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        
        // ===== Pass 1: BackBuffer に緑 =====
        D3D12_RENDER_PASS_BEGINNING_ACCESS beg1{};
        beg1.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
        beg1.Clear.ClearValue.Format = DR->GetBackBufferFormat();
        beg1.Clear.ClearValue.Color[0] = 0.05f; beg1.Clear.ClearValue.Color[1] = 0.05f;
        beg1.Clear.ClearValue.Color[2] = 0.12f; beg1.Clear.ClearValue.Color[3] = 1.0f;

        D3D12_RENDER_PASS_ENDING_ACCESS end1{};
        end1.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

        D3D12_RENDER_PASS_RENDER_TARGET_DESC rtd1{};
        rtd1.cpuDescriptor = DR->GetRenderTargetView();   // ← バックバッファ
        rtd1.BeginningAccess = beg1;
        rtd1.EndingAccess = end1;

        cmdList4_->BeginRenderPass(1, &rtd1, nullptr, D3D12_RENDER_PASS_FLAG_NONE);

        // 赤
        cbCPU_->rgba[0] = 1.0f; cbCPU_->rgba[1] = 0.2f; cbCPU_->rgba[2] = 0.2f; cbCPU_->rgba[3] = 1.0f;
        cmdList_->SetGraphicsRootConstantBufferView(0, cb_.GpuAddress());
        cmdList_->IASetVertexBuffers(0, 1, &vbvA_);
        cmdList_->DrawInstanced(3, 1, 0, 0);
        cmdList4_->EndRenderPass();
        
        // ===== Pass 2: BackBuffer に緑 =====
        D3D12_RENDER_PASS_BEGINNING_ACCESS beg2{};
        beg2.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE; // ← ここがポイント（上書きクリアしない）

        D3D12_RENDER_PASS_ENDING_ACCESS end2{};
        end2.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

        D3D12_RENDER_PASS_RENDER_TARGET_DESC rtd2{};
        rtd2.cpuDescriptor = DR->GetRenderTargetView();
        rtd2.BeginningAccess = beg2;
        rtd2.EndingAccess = end2;

        cmdList4_->BeginRenderPass(1, &rtd2, nullptr, D3D12_RENDER_PASS_FLAG_NONE);

        // 緑
        cbCPU_->rgba[0] = 0.2f; cbCPU_->rgba[1] = 1.0f; cbCPU_->rgba[2] = 0.2f; cbCPU_->rgba[3] = 1.0f;
        cmdList_->SetGraphicsRootConstantBufferView(0, cb_.GpuAddress());
        cmdList_->IASetVertexBuffers(0, 1, &vbvB_);
        cmdList_->DrawInstanced(3, 1, 0, 0);
        cmdList4_->EndRenderPass();


        // ※ Present は Game.cpp 側（DeviceResources::Present）で実施
        // ※ GraphicsMemory は Game 側で Commit するのがセオリーだが、
        //   この最小版では毎フレーム新規確保は行っていないため省略
    }

private:
    // ルートシグネチャ: b0 に定数バッファ
    void BuildRootSig(ID3D12Device* device)
    {
        D3D12_ROOT_PARAMETER rp{};
        rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        rp.Descriptor.ShaderRegister = 0; // b0

        D3D12_ROOT_SIGNATURE_DESC rs{};
        rs.NumParameters = 1;
        rs.pParameters = &rp;
        rs.NumStaticSamplers = 0;
        rs.pStaticSamplers = nullptr;
        rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        ComPtr<ID3DBlob> blob, err;
        ThrowIfFailed(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err));
        ThrowIfFailed(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&rootSig_)));
    }

    // シェーダ & PSO
    void BuildShadersAndPSO(ID3D12Device* device, DXGI_FORMAT rtvFormat)
    {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ThrowIfFailed(D3DCompileFromFile(L"VS.hlsl", nullptr, nullptr, "VSMain", "vs_5_1", flags, 0, &vs_, nullptr));
        ThrowIfFailed(D3DCompileFromFile(L"PS.hlsl", nullptr, nullptr, "PSMain", "ps_5_1", flags, 0, &ps_, nullptr));

        // 入力レイアウト: float2 POSITION
        D3D12_INPUT_ELEMENT_DESC il[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = rootSig_.Get();
        pso.VS = { vs_->GetBufferPointer(), vs_->GetBufferSize() };
        pso.PS = { ps_->GetBufferPointer(), ps_->GetBufferSize() };
        pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        pso.SampleMask = UINT_MAX;
        pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        pso.DepthStencilState.DepthEnable = FALSE; // 今回は深度なし
        pso.InputLayout = { il, _countof(il) };
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = rtvFormat;
        pso.SampleDesc = { 1, 0 };

        ThrowIfFailed(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pso_)));
    }

private:
    // パイプライン
    ComPtr<ID3D12RootSignature>     rootSig_;
    ComPtr<ID3D12PipelineState>     pso_;
    ComPtr<ID3DBlob>                vs_, ps_;

    // コマンドリスト
    ComPtr<ID3D12GraphicsCommandList>  cmdList_;
    ComPtr<ID3D12GraphicsCommandList4> cmdList4_;

    // オフスクリーン
    std::unique_ptr<RenderTexture>                 offscreenRT_;
    std::unique_ptr<DirectX::DescriptorHeap>       srvCpu_;
    std::unique_ptr<DirectX::DescriptorHeap>       rtvOffCpu_;
    D3D12_CPU_DESCRIPTOR_HANDLE                     offRTV_{};

    // ジオメトリ / 定数
    DirectX::SharedGraphicsResource vbA_, vbB_;
    D3D12_VERTEX_BUFFER_VIEW        vbvA_{}, vbvB_{};

    struct ColorCB { float rgba[4]; };
    DirectX::SharedGraphicsResource cb_;
    ColorCB* cbCPU_ = nullptr;

    std::unique_ptr<DirectX::GraphicsMemory> graphicsMemory_;
    std::wstring shaderFile_;
};


