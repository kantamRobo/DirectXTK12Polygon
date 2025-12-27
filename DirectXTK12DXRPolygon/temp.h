#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <d3dx12.h>
#include <DeviceResources.h>
#include <DirectXMath.h>
#include <dxcapi.h> // DXCを使うために必須
#include <d3dcompiler.h>
#include <d3dx12.h>
using Microsoft::WRL::ComPtr;

struct Vertex
{

	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 color;

};
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
	uint8_t* mappedShaderTableBuffer = nullptr;
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
        ComPtr<IDxcBlobEncoding> pSourceBlob;
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
    void Initialize(DX::DeviceResources* deviceResources)
    {
		// 初期化コード (必要に応じて追加)
		dxilLib = CompileShaderLibrary(L"RaytracingShaders.hlsl");
        CreateVertexBuffer(deviceResources->GetD3DDevice());
		BuildAccelerationStructures(deviceResources->GetD3DDevice(), deviceResources->GetCommandList(), vertexBuffer.Get(), static_cast<UINT>(triangleVertices.size()));
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
        ID3D12Device5* device,
        ID3D12GraphicsCommandList4* commandList,
        ID3D12Resource* vertexBuffer,
        UINT vertexCount)
    {
        // --- 1. BLAS (Geometry) の定義 ---
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

        // --- 2. TLAS (Instance) の定義 ---
        D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
        instanceDesc.Transform[0][0] = instanceDesc.Transform[1][1] = instanceDesc.Transform[2][2] = 1; // Identity
        instanceDesc.InstanceMask = 1;
        instanceDesc.AccelerationStructure = blasResultBuffer->GetGPUVirtualAddress();
        // ※ 実際にはBLAS構築後にGPUアドレスを取得して設定します

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
        tlasInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        tlasInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        tlasInputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
        tlasInputs.NumDescs = 1; // インスタンス数

        // --- 3. サイズ要件の取得とバッファ確保 (省略形) ---
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blasInfo, tlasInfo;
        device->GetRaytracingAccelerationStructurePrebuildInfo(&blasInputs, &blasInfo);
        device->GetRaytracingAccelerationStructurePrebuildInfo(&tlasInputs, &tlasInfo);

        // ...ここでスクラッチバッファとAS用バッファ(Result)をCreateCommittedResourceで作成してください...
        device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(blasInfo.ScratchDataSizeInBytes > tlasInfo.ScratchDataSizeInBytes ? blasInfo.ScratchDataSizeInBytes : tlasInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
			IID_PPV_ARGS(&scratchBuffer));
        device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(blasInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
			nullptr,
			IID_PPV_ARGS(&blasResultBuffer));
        device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(tlasInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
			D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,nullptr, IID_PPV_ARGS(&tlasResultBuffer));
        

        // --- 4. ASのビルドコマンド発行 ---

        // BLASビルド
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC blasBuildDesc = {};
        blasBuildDesc.Inputs = blasInputs;
        blasBuildDesc.DestAccelerationStructureData = blasResultBuffer->GetGPUVirtualAddress();
        blasBuildDesc.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress();
        commandList->BuildRaytracingAccelerationStructure(&blasBuildDesc, 0, nullptr);

        // バリア (BLAS完了待ち)
        auto uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(blasResultBuffer.Get());
        commandList->ResourceBarrier(1, &uavBarrier);

        // TLASビルド (BLASのアドレスをInstanceDescに設定してUploadBuffer経由でGPUに送った後に行う)
        // ...InstanceDescの転送処理...

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlasBuildDesc = {};
        tlasBuildDesc.Inputs = tlasInputs; // InstanceDescsのアドレスをセットしたinputs
        tlasBuildDesc.DestAccelerationStructureData = tlasResultBuffer->GetGPUVirtualAddress();
        tlasBuildDesc.ScratchAccelerationStructureData = scratchBuffer->GetGPUVirtualAddress();
        commandList->BuildRaytracingAccelerationStructure(&tlasBuildDesc, 0, nullptr);
    }

    void CreateRaytracingPipeline(ID3D12Device5* device)
    {
        CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

        // 1. DXILライブラリ (コンパイル済みシェーダー)
        auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        D3D12_SHADER_BYTECODE libdxil = { dxilLib->GetBufferPointer(), dxilLib->GetBufferSize() };
        lib->SetDXILLibrary(&libdxil);
        // エクスポートするシンボル (シェーダー関数名)
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

        uint32_t shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES; // 32 bytes

        // UploadHeap等のバッファにmemcpyします
        // RayGen Table
        uint8_t* pData = mappedShaderTableBuffer;
        memcpy(pData, rayGenID, shaderIdSize);

        // Miss Table (少しオフセット)
        pData += Align(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
        memcpy(pData, missID, shaderIdSize);

        // HitGroup Table
        pData += Align(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
        memcpy(pData, hitGroupID, shaderIdSize);

		// 各テーブルバッファの作成
        device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(Align(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT)),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
			IID_PPV_ARGS(&rayGenShaderTableBuffer));
        device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(Align(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT)),
            D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,IID_PPV_ARGS(&missShaderTableBuffer));
        device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(Align(shaderIdSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT)),
			D3D12_RESOURCE_STATE_GENERIC_READ,nullptr, IID_PPV_ARGS(&hitGroupShaderTableBuffer));
      
    }


    void Render(ID3D12GraphicsCommandList4* commandList)
    {
        // ... バリアの設定、RootSignatureの設定 ...
		auto tlasDescriptorGpuHandle = tlasDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		auto uavDescriptorGpuHandle = uavDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

        commandList->SetComputeRootSignature(m_globalRootSig.Get());
        commandList->SetComputeRootDescriptorTable(0, tlasDescriptorGpuHandle);
        commandList->SetComputeRootDescriptorTable(1, uavDescriptorGpuHandle);

        D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
		auto rayGenTableGPUAddr = rayGenShaderTableBuffer->GetGPUVirtualAddress();
		auto missTableGPUAddr = missShaderTableBuffer->GetGPUVirtualAddress();
		auto hitGroupTableGPUAddr = hitGroupShaderTableBuffer->GetGPUVirtualAddress();
		auto shaderIdSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        // Shader Tableのアドレスとサイズを設定
        dispatchDesc.RayGenerationShaderRecord.StartAddress = rayGenTableGPUAddr;
        
            
        dispatchDesc.RayGenerationShaderRecord.SizeInBytes = shaderIdSize;

        dispatchDesc.MissShaderTable.StartAddress = missTableGPUAddr;
        dispatchDesc.MissShaderTable.SizeInBytes = shaderIdSize;
        dispatchDesc.MissShaderTable.StrideInBytes = shaderIdSize; // レコード間隔

        dispatchDesc.HitGroupTable.StartAddress = hitGroupTableGPUAddr;
        dispatchDesc.HitGroupTable.SizeInBytes = shaderIdSize;
        dispatchDesc.HitGroupTable.StrideInBytes = shaderIdSize;

        dispatchDesc.Width = ScreenWidth;
            dispatchDesc.Height = ScreenHeight;
        dispatchDesc.Depth = 1;

        commandList->SetPipelineState1(m_rtStateObject.Get());
        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        commandList->DispatchRays(&dispatchDesc);
    }
    

};