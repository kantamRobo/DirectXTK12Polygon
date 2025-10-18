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
using Microsoft::WRL::ComPtr;
using namespace DirectX;
inline void ThrowIfFailed(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("HRESULT failed"); }

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

    void Initialize(const InitDesc& d) {
        device_ = d.device;
        queue_ = d.queue;
        swapchain_ = d.swapchain;
        rtvFormat_ = d.rtvFormat;
        width_ = d.width;
        height_ = d.height;
        shaderFile_ = d.shaderFile;

        CreateRTVHeaps();
        CreateOffscreenRT();
        BuildRootSig();
        BuildShadersAndPSO();
        BuildGeometry();
        BuildCB();
        BuildFrameObjects();
    }

    // リサイズ対応（バックバッファのサイズ変更時に呼ぶ）
    void OnResize(UINT w, UINT h) {
        width_ = w; height_ = h;
        CreateOffscreenRT(); // サイズに合わせて作り直し
    }

    // 1フレーム描画：2 RenderPass を連続実行
    void Render() {
        // フレームリソース（シンプルに都度リセット）
        ThrowIfFailed(cmdAlloc_->Reset());
        ThrowIfFailed(cmdList_->Reset(cmdAlloc_.Get(), pso_.Get()));

        // 共通セット
        cmdList_->SetGraphicsRootSignature(rootSig_.Get());
        cmdList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // ===== Pass 1 : Offscreen (赤い三角) =====
        {
            ResourceUploadBatch resourceUpload(device);

            resourceUpload.Begin();
            // COMMON -> RENDER_TARGET
              // 必要ならリソースバリア（COMMON→RENDER_TARGET）
            CD3DX12_RESOURCE_BARRIER toRT = CD3DX12_RESOURCE_BARRIER::Transition(
                g_offscreenRT.Get(),
                D3D12_RESOURCE_STATE_COMMON,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            cmdList4->ResourceBarrier(1, &toRT);


            D3D12_RENDER_PASS_BEGINNING_ACCESS beg = {};
            beg.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
            beg.Clear.ClearValue.Format = rtvFormat_;
            beg.Clear.ClearValue.Color[0] = 0.1f;
            beg.Clear.ClearValue.Color[1] = 0.1f;
            beg.Clear.ClearValue.Color[2] = 0.1f;
            beg.Clear.ClearValue.Color[3] = 1.0f;

            D3D12_RENDER_PASS_ENDING_ACCESS end = {};
            end.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_STORE;

            D3D12_RENDER_PASS_RENDER_TARGET_DESC rtd = {};
            rtd.cpuDescriptor = offscreenRTV_;
            rtd.BeginningAccess = beg;
            rtd.EndingAccess = end;

            cmdList4_->BeginRenderPass(1, &rtd, nullptr, D3D12_RENDER_PASS_FLAG_NONE);

            D3D12_VIEWPORT vp{ 0,0,(float)width_,(float)height_,0,1 };
            D3D12_RECT     sc{ 0,0,(LONG)width_,(LONG)height_ };
            cmdList_->RSSetViewports(1, &vp);
            cmdList_->RSSetScissorRects(1, &sc);

            // 色：赤
            colorCBMapped_->rgba[0] = 1.0f; colorCBMapped_->rgba[1] = 0.2f;
            colorCBMapped_->rgba[2] = 0.2f; colorCBMapped_->rgba[3] = 1.0f;
            cmdList_->SetGraphicsRootConstantBufferView(0, colorCB_->GetGPUVirtualAddress());

            cmdList_->IASetVertexBuffers(0, 1, &vbvA_);
            cmdList_->DrawInstanced(3, 1, 0, 0);

            cmdList4_->EndRenderPass();

            // RENDER_TARGET -> COMMON（今回は結果は読み出さないが整合性のため戻す）
            resourceUpload.Transition(
                g_offscreenRT.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_COMMON));

        }

        // ===== Pass 2 : BackBuffer (緑の三角) =====
        UINT backIdx = swapchain_->GetCurrentBackBufferIndex();
        ComPtr<ID3D12Resource> backBuffer;
        ThrowIfFailed(swapchain_->GetBuffer(backIdx, IID_PPV_ARGS(&backBuffer)));
        auto backRTV = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapSwap_->GetCPUDescriptorHandleForHeapStart(),
            backIdx, rtvStride_);

        // PRESENT -> RENDER_TARGET
        {
            resourceUpload.Transition(
                backBuffer, ,
                D3D12_RESOURCE_STATE_COMMON
                D3D12_RESOURCE_STATE_RENDER_TARGET));

        }

        {
            D3D12_RENDER_PASS_BEGINNING_ACCESS beg = {};
            beg.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
            beg.Clear.ClearValue.Format = rtvFormat_;
            beg.Clear.ClearValue.Color[0] = 0.05f;
            beg.Clear.ClearValue.Color[1] = 0.05f;
            beg.Clear.ClearValue.Color[2] = 0.12f;
            beg.Clear.ClearValue.Color[3] = 1.0f;

            D3D12_RENDER_PASS_ENDING_ACCESS end = {};
            end.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_STORE;

            D3D12_RENDER_PASS_RENDER_TARGET_DESC rtd = {};
            rtd.cpuDescriptor = backRTV;
            rtd.BeginningAccess = beg;
            rtd.EndingAccess = end;

            cmdList4_->BeginRenderPass(1, &rtd, nullptr, D3D12_RENDER_PASS_FLAG_NONE);

            D3D12_VIEWPORT vp{ 0,0,(float)width_,(float)height_,0,1 };
            D3D12_RECT     sc{ 0,0,(LONG)width_,(LONG)height_ };
            cmdList_->RSSetViewports(1, &vp);
            cmdList_->RSSetScissorRects(1, &sc);

            // 色：緑
            colorCBMapped_->rgba[0] = 0.2f; colorCBMapped_->rgba[1] = 1.0f;
            colorCBMapped_->rgba[2] = 0.2f; colorCBMapped_->rgba[3] = 1.0f;
            cmdList_->SetGraphicsRootConstantBufferView(0, colorCB_->GetGPUVirtualAddress());

            cmdList_->IASetVertexBuffers(0, 1, &vbvB_);
            cmdList_->DrawInstanced(3, 1, 0, 0);

            cmdList4_->EndRenderPass();
        }

        // RENDER_TARGET -> PRESENT
        {
            resourceUpload.Transition(
                backBuffer, ,
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT));

        }

        ThrowIfFailed(cmdList_->Close());
        ID3D12CommandList* lists[] = { cmdList_.Get() };
        queue_->ExecuteCommandLists(1, lists);

        ThrowIfFailed(swapchain_->Present(1, 0));
        WaitGPU(); // 単純同期
    }



    // スワップチェーンのRTV（各バックバッファぶん）を作り直したい時に呼ぶ
    void RebuildSwapchainRTVs() {
        CreateSwapRTVs();
    }

private:
    // ---- リソース定義 ----
    ID3D12Device* device_ = nullptr;
    ID3D12CommandQueue* queue_ = nullptr;
    IDXGISwapChain3* swapchain_ = nullptr;

    DXGI_FORMAT rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    UINT width_ = 0, height_ = 0;
    std::wstring shaderFile_;

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
    std::unique_ptr<DX::RenderTexture> m_renderTexture;

    std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;
    std::unique_ptr<DirectX::DescriptorHeap> m_renderDescriptors;

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
    struct ColorCB { float rgba[4]; };
    ComPtr<ID3D12Resource> colorCB_;
    ColorCB* colorCBMapped_ = nullptr;

private:
    // ---- 構築系 ----
    void CreateRTVHeaps() {
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

            //DirectXTK12の追加ラッパーであるRenderTextureでリソースは生成される
            (CommitedResourse)

            m_renderTexture = std::make_unique<DX::RenderTexture>(
                m_deviceResources->GetBackBufferFormat());

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

        g_rtvHeapOffDescriptors = std::make_unique<DescriptorHeap>(device,
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            RTDescriptors::RTCount);

        device->CreateRenderTargetView(sceneTex.Get(),
            nullptr,
            renderDescriptors->GetCpuHandle(RTDescriptors::SceneRT));

        auto rtvDescriptor = renderDescriptors->GetCpuHandle(RTDescriptors::SceneRT);
        commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);



    }

    void CreateSwapRTVs() {
        UINT n = swapchain_->GetDesc1().BufferCount;
        for (UINT i = 0; i < n; ++i) {
            ComPtr<ID3D12Resource> buf;
            ThrowIfFailed(swapchain_->GetBuffer(i, IID_PPV_ARGS(&buf)));
            auto h = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvHeapSwap_->GetCPUDescriptorHandleForHeapStart(),
                i, rtvStride_);
            device_->CreateRenderTargetView(buf.Get(), nullptr, h);

            //DeviceResourcesを使う
        }
    }

    void CreateOffscreenRT() {
        // テクスチャ
        g_rtvHeapOffDescriptors = std::make_unique<DescriptorHeap>(device,
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
            D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            RTDescriptors::RTCount);

        // RTV
        device->CreateRenderTargetView(sceneTex.Get(),
            nullptr,
            renderDescriptors->GetCpuHandle(RTDescriptors::SceneRT));
        auto rtvDescriptor = renderDescriptors->GetCpuHandle(RTDescriptors::SceneRT);
        commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);

    }

    void BuildRootSig() {
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
        ThrowIfFailed(device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&rootSig_)));
    }

    void BuildShadersAndPSO() {
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
        pso.RTVFormats[0] = rtvFormat_;
        pso.SampleDesc = { 1,0 };
        ThrowIfFailed(device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pso_)));
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

    void BuildFrameObjects() {
        ThrowIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc_)));
        ThrowIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc_.Get(), pso_.Get(), IID_PPV_ARGS(&cmdList_)));
        ThrowIfFailed(cmdList_->Close()); // 初回は閉じておく
        ThrowIfFailed(cmdList_->QueryInterface(IID_PPV_ARGS(&cmdList4_)));

        ThrowIfFailed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
        fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        fenceValue_ = 1;
    }

    void WaitGPU() {
        ThrowIfFailed(queue_->Signal(fence_.Get(), fenceValue_));
        if (fence_->GetCompletedValue() < fenceValue_) {
            ThrowIfFailed(fence_->SetEventOnCompletion(fenceValue_, fenceEvent_));
            WaitForSingleObject(fenceEvent_, INFINITE);
        }
        fenceValue_++;
    }
};

