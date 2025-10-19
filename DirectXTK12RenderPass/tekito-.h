// RenderPassTwoTriangles.h/cpp
#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <vector>
#include <stdexcept>
#include <string>
#include "d3dx12.h"   // DirectX-Graphics-Samples 同梱のヘルパー
#include <ResourceUploadBatch.h>
#include <DescriptorHeap.h>
#include <GraphicsMemory.h>
#include <memory>
#include <DeviceResources.h>
#include "RenderTexture.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;
inline void ThrowIfFailed(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("HRESULT failed"); }


enum Descriptors
{
    SceneTex,
    Count
};

enum RTDescriptors
{
    OffScreenRT,
    RTCount
};
class RenderPassTwoTriangles {
public:
    struct InitDesc {
        ID3D12Device* device = nullptr;
        ID3D12CommandQueue* queue = nullptr;
        IDXGISwapChain3* swapchain = nullptr;
        DXGI_FORMAT                   rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        UINT                          width = 0;
        UINT                          height = 0;
        D3D_FEATURE_LEVEL             featureLevel = D3D_FEATURE_LEVEL_12_1;
        std::wstring                  shaderFile = L"Triangle.hlsl";
    };

    // 1フレーム描画：2 RenderPass を連続実行
    void Render(DX::DeviceResources* DR) {
		auto device = DR->GetD3DDevice();
       auto cmdList4 = DR->GetCommandList();
       // フレームリソース（シンプルに都度リセット）
       ResourceUploadBatch resourceUpload(device);

       resourceUpload.Begin();

       // 共通セット
       cmdList_->SetGraphicsRootSignature(rootSig_.Get());
       cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
       // ===== Pass 1 : Offscreen (赤い三角) =====
       {
           // COMMON -> RENDER_TARGET
           resourceUpload.Transition(
               offscreenRT_->GetResource(), D3D12_RESOURCE_STATE_COMMON,
               D3D12_RESOURCE_STATE_RENDER_TARGET);
           
           D3D12_RENDER_PASS_BEGINNING_ACCESS beg = {};
           beg.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
           beg.Clear.ClearValue.Format = DR->GetBackBufferFormat();
           beg.Clear.ClearValue.Color[0] = 0.1f;
           beg.Clear.ClearValue.Color[1] = 0.1f;
           beg.Clear.ClearValue.Color[2] = 0.1f;
           beg.Clear.ClearValue.Color[3] = 1.0f;
           D3D12_RENDER_PASS_ENDING_ACCESS end = {};
           end.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
           D3D12_RENDER_PASS_RENDER_TARGET_DESC rtd = {};
           rtd.cpuDescriptor = m_renderDescriptors->GetCpuHandle(0);
           rtd.BeginningAccess = beg;
           rtd.EndingAccess = end;
           cmdList4_->BeginRenderPass(1, &rtd, nullptr, D3D12_RENDER_PASS_FLAG_NONE);
		   // ビューポート＆シザーはGame.cppのレンダー側で行うので、ここでは不要かも
           
           // 色：赤
           colorCBMapped_->rgba[0] = 1.0f; colorCBMapped_->rgba[1] = 0.2f;
           colorCBMapped_->rgba[2] = 0.2f; colorCBMapped_->rgba[3] = 1.0f;
           cmdList_->SetGraphicsRootConstantBufferView(0, colorCB_.GpuAddress());
           cmdList_->IASetVertexBuffers(0, 1, &vbvA_);
           cmdList_->DrawInstanced(3, 1, 0, 0);
           cmdList4_->EndRenderPass();
           // RENDER_TARGET -> COMMON（今回は結果は読み出さないが整合性のため戻す）
           resourceUpload.Transition(

               offscreenRT_->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);
       }
       // ===== Pass 2 : BackBuffer (緑の三角) =====
       UINT backIdx = DR->GetSwapChain()->GetCurrentBackBufferIndex();
       ComPtr<ID3D12Resource> backBuffer;
       ThrowIfFailed(DR->GetSwapChain()->GetBuffer(backIdx, IID_PPV_ARGS(&backBuffer)));
       auto rtvOffscreen = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		   m_rtvHeapOffDescriptors->GetCpuHandle(0));
       // PRESENT -> RENDER_TARGET
       {
           resourceUpload.Transition(
               backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
       }
       {
           D3D12_RENDER_PASS_BEGINNING_ACCESS beg = {};
           beg.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
           beg.Clear.ClearValue.Format = DR->GetBackBufferFormat();
           beg.Clear.ClearValue.Color[0] = 0.05f;
           beg.Clear.ClearValue.Color[1] = 0.05f;
           beg.Clear.ClearValue.Color[2] = 0.12f;
           beg.Clear.ClearValue.Color[3] = 1.0f;
           D3D12_RENDER_PASS_ENDING_ACCESS end = {};
           end.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
           D3D12_RENDER_PASS_RENDER_TARGET_DESC rtd = {};
           rtd.cpuDescriptor = DR->GetRenderTargetView();
           rtd.BeginningAccess = beg;
           rtd.EndingAccess = end;
           cmdList4_->BeginRenderPass(1, &rtd, nullptr, D3D12_RENDER_PASS_FLAG_NONE);
		   // ビューポート＆シザーはGame.cppのレンダー側で行うので、ここでは不要かも
           // 色：緑
           colorCBMapped_->rgba[0] = 0.2f; colorCBMapped_->rgba[1] = 1.0f;
           colorCBMapped_->rgba[2] = 0.2f; colorCBMapped_->rgba[3] = 1.0f;
           cmdList_->SetGraphicsRootConstantBufferView(0, colorCB_.GpuAddress());
           cmdList_->IASetVertexBuffers(0, 1, &vbvB_);
           cmdList_->DrawInstanced(3, 1, 0, 0);
           cmdList4_->EndRenderPass();
       }
       // RENDER_TARGET -> PRESENT
       {
           resourceUpload.Transition(

               backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
       }

       auto finish =  resourceUpload.End(DR->GetCommandQueue());
       ThrowIfFailed(DR->GetSwapChain()->Present(1, 0));
	   finish.wait();


    }



private:
  

    // Frame
    ComPtr<ID3D12CommandAllocator>         cmdAlloc_;
    ComPtr<ID3D12GraphicsCommandList>      cmdList_;
    ComPtr<ID3D12GraphicsCommandList4>     cmdList4_;
    ComPtr<ID3D12Fence>                    fence_;
    HANDLE                                 fenceEvent_ = nullptr;
    UINT64                                 fenceValue_ = 0;

    // RTV heaps
    ComPtr<ID3D12DescriptorHeap> rtvHeapSwap_;
    UINT                         rtvStride_ = 0;

    // Offscreen
        // Offscreen
    std::unique_ptr<RenderTexture>  offscreenRT_;

        std::unique_ptr<RenderTexture> m_renderTexture;
    std::unique_ptr<RenderTexture> m_offscreenRT;
	std::wstring shaderFile_;

    std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap> m_renderDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap>  m_rtvHeapOffDescriptors;
   
    // Pipeline
    ComPtr<ID3D12RootSignature>         rootSig_;
    ComPtr<ID3D12PipelineState>         pso_;
    ComPtr<ID3DBlob>                    vs_, ps_;

    // Geometry
    struct Vertex { float x, y; };
    DirectX::SharedGraphicsResource  vertexBufferA, vertexBufferB;
	std::unique_ptr<DirectX::GraphicsMemory> graphicsMemory_;
    D3D12_VERTEX_BUFFER_VIEW vbvA_{}, vbvB_{};

    // Constant buffer
    
    std::unique_ptr<RenderTexture>  backbuffer;
    // Constant buffer
    struct ColorCB { float rgba[4]; };
   SharedGraphicsResource colorCB_;
    ColorCB*                        colorCBMapped_ = nullptr;
	// ---- 内部処理系 ----
   

private:
    // ---- 構築系 ----
    void CreateRTVHeaps(DX::DeviceResources* DR) {
		auto device = DR->GetD3DDevice();
		auto commandList = DR->GetCommandList();
        enum Descriptors
        {
            SceneTex,
            Count
        };

        enum RTDescriptors
        {
            OffScreenRT,
            RTCount
        };

         

        // RTV ヒープ＆ハンドル（スワップチェーン RTV とは別に 1 枚）
        //これもラッパーを使う
        //使うのはDescriptorHeapラッパーで、それでRTVを作成する
        enum RTDescriptors
        {
            SceneRT,
            Blur1RT,
            Blur2RT,
            RTCount
        };
        // テクスチャ
        m_rtvHeapOffDescriptors = std::make_unique<DescriptorHeap>(device,
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            RTDescriptors::RTCount);

      
        device->CreateRenderTargetView(offscreenRT_->GetResource(),
            nullptr,
            m_renderDescriptors->GetCpuHandle(RTDescriptors::SceneRT));

        auto rtvDescriptor = m_renderDescriptors->GetCpuHandle(RTDescriptors::SceneRT);
        commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);



    }

    

    void BuildRootSig(DX::DeviceResources* DR) {
        //DirectXTK12Polygonから使う        

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
        ThrowIfFailed(DR->GetD3DDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&rootSig_)));
    }

    void BuildShadersAndPSO(DX::DeviceResources* DR) {
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ThrowIfFailed(D3DCompileFromFile(shaderFile_.c_str(), nullptr, nullptr, "VSMain", "vs_5_1", flags, 0, &vs_, nullptr));
        ThrowIfFailed(D3DCompileFromFile(shaderFile_.c_str(), nullptr, nullptr, "PSMain", "ps_5_1", flags, 0, &ps_, nullptr));

        D3D12_INPUT_ELEMENT_DESC ild[] = {
            { "POSITION",0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.InputLayout = { ild, _countof(ild) };
        pso.pRootSignature = rootSig_.Get();
        pso.VS = { vs_->GetBufferPointer(), vs_->GetBufferSize() };
        pso.PS = { ps_->GetBufferPointer(), ps_->GetBufferSize() };
        pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        pso.DepthStencilState.DepthEnable = FALSE;
        pso.DepthStencilState.StencilEnable = FALSE;
        pso.SampleMask = UINT_MAX;
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = DR->GetBackBufferFormat();
        pso.SampleDesc = { 1,0 };
        ThrowIfFailed(DR->GetD3DDevice()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pso_)));
    }

    void BuildGeometry() {
        Vertex triA[3] = { {-0.8f,-0.5f}, {-0.2f,-0.5f}, {-0.5f,0.5f} };
        Vertex triB[3] = { { 0.2f,-0.5f}, { 0.8f,-0.5f}, { 0.5f,0.5f} };


        //特に何か動的な事をするわけではないけど、StaticBufferは今後の拡張性を考えると微妙なのでSharedGraphicResourceにする

        vertexBufferA = graphicsMemory_->Allocate(sizeof(Vertex) * sizeof(triA));
        vertexBufferB = graphicsMemory_->Allocate(sizeof(Vertex) * sizeof(triB));


        // ここでは簡単のため UploadHeap を使用（実運用は DefaultHeap + Upload 経由が推奨）
        D3D12_VERTEX_BUFFER_VIEW  viewOutA{}, viewOutB{};



        memcpy(vertexBufferA.Memory(), triA, sizeof(Vertex) * sizeof(triA));
        memcpy(vertexBufferA.Memory(), triB, sizeof(Vertex) * sizeof(triB));
        viewOutA.BufferLocation = vertexBufferA.GpuAddress();
        viewOutA.StrideInBytes = sizeof(Vertex);
        viewOutA.SizeInBytes = (UINT)(sizeof(triA));
        viewOutB.BufferLocation = vertexBufferB.GpuAddress();
        viewOutB.StrideInBytes = sizeof(Vertex);
        viewOutB.SizeInBytes = (UINT)sizeof(triB);


    }

};

