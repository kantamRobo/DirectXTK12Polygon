#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <d3dx12.h>
#include <DeviceResources.h>
#include <DirectXMath.h>
#include <dxcapi.h> // DXCを使うために必須
#include <d3dcompiler.h>
#include <vector>

struct Vertex
{

	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 color;

};
struct Constants
{
    DirectX::XMMATRIX view;
    DirectX::XMMATRIX proj;
};
/*
DXRにおいて、AS（Acceleration Structure）のバッファアドレスは 256バイトアライメント (D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT) に沿っている必要があります。
*/
// 必須のアライメント
#define D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT 256


// 頂点データ (例: 三角形)
std::vector<Vertex> triangleVertices = {
    { { 0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
    { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
    { {-0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
};
class HelloDXR
{
public:
    //グローバルルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_globalRootSig;
    // レイトレーシングパイプラインステートオブジェクト
    Microsoft::WRL::ComPtr<ID3D12StateObject> m_rtPipelineState;
public:
    //頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    //TLASのリザルトバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> tlasResultBuffer;
    //BLASのリザルトバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> blasResultBuffer;
    //スクラッチバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> scratchBuffer;
    //インスタンスのアップロードバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> instanceUploadBuffer;
    //レイトレステートオブジェクト
    Microsoft::WRL::ComPtr<ID3D12StateObject> m_rtStateObject;
    //tlasディスクリプタハンドル
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> tlasDescriptorHeap;
    //uavディスクリプタハンドル
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> uavDescriptorHeap;
    //レイ生成シェーダーテーブルバッファ 
    Microsoft::WRL::ComPtr<ID3D12Resource> rayGenShaderTableBuffer;

    //ミスシェーダーテーブルバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> missShaderTableBuffer;
    //ヒットグループシェーダーテーブルバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> hitGroupShaderTableBuffer;

    Microsoft::WRL::ComPtr<ID3DBlob> dxilLib;
    //スクリーンの幅と高さ
    UINT ScreenWidth = 0;
    UINT ScreenHeight = 0;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

    // 出力用リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> m_outputResource;

public:
    // UINT版
    UINT Align(UINT size, UINT alignment)
    {
        return (size + (alignment - 1)) & ~(alignment - 1);
    }

    // D3DBlob生成用ヘルパー関数
// hlslFileName: シェーダーファイルパス (例: L"Shaders.hlsl")
    Microsoft::WRL::ComPtr<ID3DBlob> CompileShaderLibrary(const std::wstring& hlslFileName)
    {
        // 1. DXCのユーティリティとコンパイラのインスタンス作成
        Microsoft::WRL::ComPtr<IDxcUtils> pUtils;
        Microsoft::WRL::ComPtr<IDxcCompiler3> pCompiler;

        HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
        if (FAILED(hr)) return nullptr;

        hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
        if (FAILED(hr)) return nullptr;

        // 2. インクルードハンドラの作成 (ヘッダーファイルをincludeする場合に必要)
        Microsoft::WRL::ComPtr<IDxcIncludeHandler> pIncludeHandler;
        pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

        // 3. ソースファイルの読み込み
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> pSourceBlob;
        hr = pUtils->LoadFile(hlslFileName.c_str(), nullptr, &pSourceBlob);
        if (FAILED(hr)) {
            OutputDebugStringA("Failed to load shader file.\n");
            return nullptr;
        }

        // コンパイル用バッファ設定
        DxcBuffer sourceBuffer;
        sourceBuffer.Ptr = pSourceBlob->GetBufferPointer();
        sourceBuffer.Size = pSourceBlob->GetBufferSize();
        sourceBuffer.Encoding = DXC_CP_ACP; // デフォルト (ANSI/UTF-8)

        // 4. コンパイル引数の設定
        std::vector<LPCWSTR> args;

        // ターゲットプロファイル: レイトレーシングは "lib_6_3" 以上が必須
        args.push_back(L"-T");
        args.push_back(L"lib_6_3");

        // デバッグ情報 (必要に応じて変更)
#if defined(_DEBUG)
        args.push_back(L"-Zi");          // デバッグ情報有効化
        args.push_back(L"-Qembed_debug"); // デバッグ情報を埋め込む
        args.push_back(L"-Od");          // 最適化なし
#else
        args.push_back(L"-O3");          // 最大最適化
#endif

        // 5. コンパイル実行
        Microsoft::WRL::ComPtr<IDxcResult> pResult;
        hr = pCompiler->Compile(
            &sourceBuffer,          // ソース
            args.data(),            // 引数配列
            (UINT32)args.size(),    // 引数の数
            pIncludeHandler.Get(),  // インクルードハンドラ
            IID_PPV_ARGS(&pResult)  // 結果出力先
        );

        if (FAILED(hr)) return nullptr;

        // 6. エラー確認
        Microsoft::WRL::ComPtr<IDxcBlobUtf8> pErrors = nullptr;
        pResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
        if (pErrors != nullptr && pErrors->GetStringLength() > 0)
        {
            // エラーメッセージをデバッグ出力に出す
            OutputDebugStringA((char*)pErrors->GetBufferPointer());
            OutputDebugStringA("\n");

            // エラーがあった場合は失敗とする
            // (警告だけの場合は成功させても良いが、ここでは厳密にする)
            // DXCは成功しても空のエラーBlobを返すことがあるため長さをチェック
        }

        // コンパイル自体のステータス確認
        HRESULT compileStatus;
        pResult->GetStatus(&compileStatus);
        if (FAILED(compileStatus)) {
            OutputDebugStringA("Shader Compilation Failed.\n");
            return nullptr;
        }

        // 7. コンパイル済みバイナリ (DXIL) の取得
        Microsoft::WRL::ComPtr<IDxcBlob> pShaderBlob;
        hr = pResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShaderBlob), nullptr);
        if (FAILED(hr)) return nullptr;

        // 8. IDxcBlob -> ID3DBlob への変換
        // IDxcBlobは直接ID3DBlobと互換性がないため、D3DCreateBlobで作成してコピーするか、
        // IDxcBlob自体を保持して GetBufferPointer() を使うのが一般的です。
        // ここでは、リクエストされた関数の引数型 (ID3DBlob*) に合わせるため、ID3DBlobを生成してコピーします。

        // 注: D3DCreateBlobを使うには d3dcompiler.lib のリンクと #include <d3dcompiler.h> が必要
        // もし d3dcompiler を使いたくない場合は、自作クラスでラップするか、
        // 呼び出し元の引数を IDxcBlob* に変えるのがモダンな設計です。

        Microsoft::WRL::ComPtr<ID3DBlob> pD3DBlob;
        // D3DCreateBlob関数を使用 (要: #include <d3dcompiler.h>)
        if (FAILED(D3DCreateBlob(pShaderBlob->GetBufferSize(), &pD3DBlob))) {
            return nullptr;
        }

        memcpy(pD3DBlob->GetBufferPointer(), pShaderBlob->GetBufferPointer(), pShaderBlob->GetBufferSize());

        return pD3DBlob;
    }
    void CreateGlobalRootSignature(ID3D12Device5* device)
    {
        // ルートパラメータの定義
        CD3DX12_DESCRIPTOR_RANGE ranges[2];
        // パラメータ0: TLAS用のSRV (t0)
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0, space0
        // パラメータ1: 出力UAV用 (u0)
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0, space0

        CD3DX12_ROOT_PARAMETER rootParameters[2];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0]); // TLAS
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1]); // Output UAV

        // ルートシグネチャの作成
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(
            2,                      // パラメータ数
            rootParameters,         // パラメータ配列
            0,                      // StaticSampler数
            nullptr,                // StaticSampler配列
            D3D12_ROOT_SIGNATURE_FLAG_NONE
        );

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;
        HRESULT hr = D3D12SerializeRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signature,
            &error
        );

        if (FAILED(hr))
        {
            if (error)
            {
                OutputDebugStringA((char*)error->GetBufferPointer());
            }
            return;
        }

        hr = device->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&m_globalRootSig)
        );
    }

    void CreateOutputResource(DX::DeviceResources* deviceResources)
    {
        auto format = deviceResources->GetBackBufferFormat();
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            format,
            ScreenWidth,
            ScreenHeight,
            1, 1, 1, 0,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        DX::ThrowIfFailed(
            deviceResources->GetD3DDevice()->CreateCommittedResource(
                &defaultHeap,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                nullptr,
                IID_PPV_ARGS(&m_outputResource)));
    }

    void CreateDescriptorHeaps(ID3D12Device5* device)
    {
        // SRV/UAV両方を格納する単一のディスクリプタヒープを作成
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = 2;  // TLASとUAV両方分
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&tlasDescriptorHeap));

        UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // TLAS用のSRVを作成 (インデックス0)
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.RaytracingAccelerationStructure.Location = tlasResultBuffer->GetGPUVirtualAddress();
        device->CreateShaderResourceView(nullptr, &srvDesc, tlasDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        // 出力用のUAVを作成 (インデックス1)
        CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(tlasDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, descriptorSize);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc, uavHandle);
    }
    void Initialize(DX::DeviceResources* deviceResources)
    {
        // 初期化コード (必要に応じて追加)
        dxilLib = CompileShaderLibrary(L"MyRaytracing.hlsl");
        CreateVertexBuffer(deviceResources->GetD3DDevice());

        // グローバルルートシグネチャの作成
        CreateGlobalRootSignature(deviceResources->GetD3DDevice());

        // 出力リソースの作成
        CreateOutputResource(deviceResources);

        BuildAccelerationStructures(deviceResources, static_cast<UINT>(triangleVertices.size()));

        // ディスクリプタヒープの作成（ASビルド後に作成）
        CreateDescriptorHeaps(deviceResources->GetD3DDevice());

        CreateRaytracingPipeline(deviceResources->GetD3DDevice());
        BuildShaderTables(deviceResources->GetD3DDevice());

    }

    void CreateVertexBuffer(ID3D12Device5* device)
    {


        const UINT vertexBufferSize = static_cast<UINT>(sizeof(Vertex) * triangleVertices.size());
        // 2. リソースの作成 (Upload Heap)
// CPUから書き込み可能で、GPUからも読み取り可能なヒープタイプを指定
        auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);

        HRESULT hr = device->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &resDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, // Upload Heapの初期状態はこれにする必要があります
            nullptr,
            IID_PPV_ARGS(&vertexBuffer)
        );

        if (FAILED(hr)) {
            // エラーハンドリング
        }

        // 3. データの転送 (Map -> Copy -> Unmap)
        UINT8* pVertexDataBegin = nullptr;
        CD3DX12_RANGE readRange(0, 0); // CPUから読み込むつもりはないので範囲を0に指定

        // GPUメモリをCPUのアドレス空間にマッピング
        hr = vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));

        if (SUCCEEDED(hr)) {
            // メモリコピー
            memcpy(pVertexDataBegin, triangleVertices.data(), vertexBufferSize);

            // マッピング解除
            vertexBuffer->Unmap(0, nullptr);
        }

        // 4. 頂点バッファビュー (VBV) の作成
        // ディスクリプタヒープは不要で、構造体を保持しておくだけでOK

        vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress(); // GPU上のアドレス
        vertexBufferView.StrideInBytes = sizeof(Vertex);                        // 1頂点あたりのサイズ
        vertexBufferView.SizeInBytes = vertexBufferSize;
    }

    //幅と高さをセット
    void SetScreenSize(const UINT& width, const UINT& height)
    {
        ScreenWidth = width;
        ScreenHeight = height;
    }
    // 必要なヘッダー: d3dx12.h
    void BuildAccelerationStructures(

        DX::DeviceResources* deviceResources,
        UINT vertexCount)
    {
        auto commandList = deviceResources->GetCommandList();
        auto device = deviceResources->GetD3DDevice();
        auto commandAllocator = deviceResources->GetCommandAllocator();

        // コマンドリストのリセット (必要に応じて)
        commandList->Reset(commandAllocator, nullptr);

        // ==========================================
        // 1. BLAS (Geometry) の設定
        // ==========================================
        D3D12_RAYTRACING_GEOMETRY_DESC geomDesc = {};
        geomDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        geomDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress();
        geomDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
        geomDesc.Triangles.VertexCount = vertexCount;
        geomDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
        geomDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
        blasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        blasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        blasInputs.pGeometryDescs = &geomDesc;
        blasInputs.NumDescs = 1;
        blasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

        // ==========================================
        // 2. TLAS (Instance) の設定 (初期段階)
        // ==========================================
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
        tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        tlasInputs.NumDescs = 1; // インスタンス数
        // ※ InstanceDescs (アドレス) はバッファ作成後にセットします

        // ==========================================
        // 3. サイズ計算と AS用バッファ確保
        // ==========================================
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasInfo, tlasInfo;
        device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasInfo);
        device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasInfo);

        // ヒーププロパティ
        auto defaultHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD); // インスタンス記述子転送用

        // バッファ記述子の作成
        auto scratchBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
            std::max(blasInfo.ScratchDataSizeInBytes, tlasInfo.ScratchDataSizeInBytes),
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        );
        // ★ここ修正済み: ResultDataMaxSizeInBytesを使用
        auto blasBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(blasInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto tlasBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(tlasInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        // リソース生成 (BLAS/TLAS/Scratch)
        device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &scratchBufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&scratchBuffer));
        device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &blasBufferDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&blasResultBuffer));
        device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &tlasBufferDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, nullptr, IID_PPV_ARGS(&tlasResultBuffer));

        // ==========================================
        // 4. インスタンスバッファの作成とデータ転送 (★ここが重要★)
        // ==========================================

        // インスタンス記述子を格納するUploadバッファを作成
        const UINT64 instanceBufferSize = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * tlasInputs.NumDescs;
        auto instanceBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(instanceBufferSize);

        Microsoft::WRL::ComPtr<ID3D12Resource> instanceBuffer;
        device->CreateCommittedResource(
            &uploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &instanceBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&instanceBuffer)
        );

        // インスタンス記述子の定義 (CPU側)
        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
        instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1.0f;
        instanceDesc.InstanceMask = 0xFF; // 全て表示
        instanceDesc.InstanceContributionToHitGroupIndex = 0;
        instanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        // ★ここで確保したBLASのアドレスをセット
        instanceDesc.AccelerationStructure = blasResultBuffer->GetGPUVirtualAddress();

        // バッファにマップして書き込み
        D3D12_RAYTRACING_INSTANCE_DESC* pData;
        instanceBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));
        pData[0] = instanceDesc; // 配列コピー
        instanceBuffer->Unmap(0, nullptr);

        // ★TLASの入力にインスタンスバッファのアドレスを設定
        tlasInputs.InstanceDescs = instanceBuffer->GetGPUVirtualAddress();

        // ==========================================
        // 5. BLAS ビルド
        // ==========================================
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuildDesc = {};
        blasBuildDesc.Inputs = blasInputs;
        blasBuildDesc.DestAccelerationStructureData = blasResultBuffer->GetGPUVirtualAddress();
        blasBuildDesc.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress();

        commandList->BuildRaytracingAccelerationStructure(&blasBuildDesc, 0, nullptr);

        // BLASビルド完了待ちバリア
        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(blasResultBuffer.Get());
        commandList->ResourceBarrier(1, &uavBarrier);

        // ==========================================
        // 6. TLAS ビルド
        // ==========================================
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuildDesc = {};
        tlasBuildDesc.Inputs = tlasInputs; // InstanceDescsが設定済みのものを使用
        tlasBuildDesc.DestAccelerationStructureData = tlasResultBuffer->GetGPUVirtualAddress();
        tlasBuildDesc.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress();

        commandList->BuildRaytracingAccelerationStructure(&tlasBuildDesc, 0, nullptr);

        // コマンドリストを閉じて実行
        DX::ThrowIfFailed(commandList->Close());
        ID3D12CommandList* cmdLists[] = { commandList };
        deviceResources->GetCommandQueue()->ExecuteCommandLists(1, cmdLists);

        // GPU完了を待つ
        deviceResources->WaitForGpu();
    }

    void CreateRaytracingPipeline(ID3D12Device5* device)
    {
        CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

        // 1. DXILライブラリ (コンパイル済みシェーダー)
        auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        D3D12_SHADER_BYTECODE libdxil = { dxilLib->GetBufferPointer(), dxilLib->GetBufferSize() };
        lib->SetDXILLibrary(&libdxil);
        // エクスポートするシンボル (シェーダー関数名)f
        lib->DefineExport(L"RayGen");
        lib->DefineExport(L"Miss");
        lib->DefineExport(L"ClosestHit");

        // 2. ヒットグループ (ClosestHit と AnyHit/Intersection をまとめる)
        auto hitGroup = raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
        hitGroup->SetClosestHitShaderImport(L"ClosestHit");
        hitGroup->SetHitGroupExport(L"HitGroup0");
        hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

        // 3. シェーダー構成 (Payloadサイズなど)
        auto shaderConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
        UINT payloadSize = 4 * sizeof(float); // float4 color
        UINT attributeSize = 2 * sizeof(float); // barycentrics
        shaderConfig->Config(payloadSize, attributeSize);

        // 4. パイプライン構成 (再帰深度)
        auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
        pipelineConfig->Config(1); // 1バウンスのみなら1

        // 5. グローバルルートシグネチャ
        auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
        globalRootSignature->SetRootSignature(m_globalRootSig.Get()); // TLASとOutputUAVを設定したもの

        // パイプライン生成
        HRESULT hr = device->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_rtStateObject));
    }

    // 概念図: | RayGenRecord | MissRecord | HitGroupRecord |
    void BuildShaderTables(ID3D12Device5* device)
    {
        ID3D12StateObjectProperties* props;
        m_rtStateObject->QueryInterface(IID_PPV_ARGS(&props));

        void* rayGenID = props->GetShaderIdentifier(L"RayGen");
        void* missID = props->GetShaderIdentifier(L"Miss");
        void* hitGroupID = props->GetShaderIdentifier(L"HitGroup0");

        uint32_t shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        auto uploadheap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto Buffer = CD3DX12_RESOURCE_DESC::Buffer(Align(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT));

        // バッファを先に作成
        device->CreateCommittedResource(&uploadheap, D3D12_HEAP_FLAG_NONE, &Buffer,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&rayGenShaderTableBuffer));
        device->CreateCommittedResource(&uploadheap, D3D12_HEAP_FLAG_NONE, &Buffer,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&missShaderTableBuffer));
        device->CreateCommittedResource(&uploadheap, D3D12_HEAP_FLAG_NONE, &Buffer,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&hitGroupShaderTableBuffer));

        // Map -> memcpy -> Unmap
        uint8_t* pData = nullptr;
        rayGenShaderTableBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));
        memcpy(pData, rayGenID, shaderIdSize);
        rayGenShaderTableBuffer->Unmap(0, nullptr);

        missShaderTableBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));
        memcpy(pData, missID, shaderIdSize);
        missShaderTableBuffer->Unmap(0, nullptr);

        hitGroupShaderTableBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));
        memcpy(pData, hitGroupID, shaderIdSize);
        hitGroupShaderTableBuffer->Unmap(0, nullptr);
    }


    void Render(ID3D12GraphicsCommandList4* commandList, ID3D12Device* device, ID3D12Resource* renderTarget)
    {
        auto tlasDescriptorGpuHandle = tlasDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

        // UAVは同じヒープ内のインデックス1にあるので、オフセットを計算
        UINT descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        CD3DX12_GPU_DESCRIPTOR_HANDLE uavDescriptorGpuHandle(tlasDescriptorGpuHandle, 1, descriptorSize);

        // 単一のディスクリプタヒープのみを設定
        ID3D12DescriptorHeap* heaps[] = { tlasDescriptorHeap.Get() };
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);

        commandList->SetComputeRootSignature(m_globalRootSig.Get());
        commandList->SetComputeRootDescriptorTable(0, tlasDescriptorGpuHandle);
        commandList->SetComputeRootDescriptorTable(1, uavDescriptorGpuHandle);

        D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
        auto rayGenTableGPUAddr = rayGenShaderTableBuffer->GetGPUVirtualAddress();
        auto missTableGPUAddr = missShaderTableBuffer->GetGPUVirtualAddress();
        auto hitGroupTableGPUAddr = hitGroupShaderTableBuffer->GetGPUVirtualAddress();
        auto shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

        dispatchDesc.RayGenerationShaderRecord.StartAddress = rayGenTableGPUAddr;
        dispatchDesc.RayGenerationShaderRecord.SizeInBytes = shaderIdSize;

        dispatchDesc.MissShaderTable.StartAddress = missTableGPUAddr;
        dispatchDesc.MissShaderTable.SizeInBytes = shaderIdSize;
        dispatchDesc.MissShaderTable.StrideInBytes = shaderIdSize;

        dispatchDesc.HitGroupTable.StartAddress = hitGroupTableGPUAddr;
        dispatchDesc.HitGroupTable.SizeInBytes = shaderIdSize;
        dispatchDesc.HitGroupTable.StrideInBytes = shaderIdSize;

        dispatchDesc.Width = ScreenWidth;
        dispatchDesc.Height = ScreenHeight;
        dispatchDesc.Depth = 1;

        commandList->SetPipelineState1(m_rtStateObject.Get());
        commandList->DispatchRays(&dispatchDesc);

        // レイトレーシング結果をバックバッファにコピー
        D3D12_RESOURCE_BARRIER preCopyBarriers[2] = {};
        preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(),
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
        commandList->ResourceBarrier(2, preCopyBarriers);

        commandList->CopyResource(renderTarget, m_outputResource.Get());

        D3D12_RESOURCE_BARRIER postCopyBarriers[2] = {};
        postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(m_outputResource.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget,
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList->ResourceBarrier(2, postCopyBarriers);
    }
};