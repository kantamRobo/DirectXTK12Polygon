#pragma once
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
    instanceDesc.AccelerationStructure = /* ここに構築後のBLASのアドレスが必要 */;
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

void CreateRaytracingPipeline(ID3D12Device5* device, ID3DBlob* dxilLib)
{
    CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

    // 1. DXILライブラリ (コンパイル済みシェーダー)
    auto lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(dxilLib);
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
void BuildShaderTables(ID3D12Device5* device, ID3D12StateObject* rtpso)
{
    ID3D12StateObjectProperties* props;
    rtpso->QueryInterface(IID_PPV_ARGS(&props));

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
}


void Render(ID3D12GraphicsCommandList4* commandList)
{
    // ... バリアの設定、RootSignatureの設定 ...
    commandList->SetComputeRootSignature(m_globalRootSig.Get());
    commandList->SetComputeRootDescriptorTable(0, tlasDescriptorGpuHandle);
    commandList->SetComputeRootDescriptorTable(1, uavDescriptorGpuHandle);

    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};

    // Shader Tableのアドレスとサイズを設定
    dispatchDesc.RayGenerationShaderRecord.StartAddress = rayGenTableGPUAddr;
    dispatchDesc.RayGenerationShaderRecord.SizeInBytes = shaderIdSize;

    dispatchDesc.MissShaderTable.StartAddress = missTableGPUAddr;
    dispatchDesc.MissShaderTable.SizeInBytes = shaderIdSize;
    dispatchDesc.MissShaderTable.StrideInBytes = shaderIdSize; // レコード間隔

    dispatchDesc.HitGroupTable.StartAddress = hitGroupTableGPUAddr;
    dispatchDesc.HitGroupTable.SizeInBytes = shaderIdSize;
    dispatchDesc.HitGroupTable.StrideInBytes = shaderIdSize;

    dispatchDesc.Width = screenWidth;
    dispatchDesc.Height = screenHeight;
    dispatchDesc.Depth = 1;

    commandList->SetPipelineState1(m_rtStateObject.Get());
    commandList->DispatchRays(&dispatchDesc);
}

