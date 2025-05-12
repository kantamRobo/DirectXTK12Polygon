#include <ResourceUploadBatch.h>
#include "DirectXTK12Polygon.h"
enum Descriptors
{
    WindowsLogo,
    CourierFont,
    ControllerFont,
    GamerPic,
    Count
};
HRESULT DirectXTK12Polygon::CreateBuffer(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResources* deviceResources, int height, int width)
{
    DirectX::ResourceUploadBatch resourceUpload(deviceResources->GetD3DDevice());
  
    resourceUpload.Begin();
    resourceHeap = std::make_unique<DirectX::DescriptorHeap>(deviceResources->GetD3DDevice(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);
    // 頂点バッファの作成
    DX::ThrowIfFailed(
        DirectX::CreateStaticBuffer(
            deviceResources->GetD3DDevice(),
            resourceUpload,
            vertices.data(),
            static_cast<int>(vertices.size()),
            sizeof(DirectX::VertexPositionNormalColorTexture),
            D3D12_RESOURCE_STATE_COMMON,
            m_vertexBuffer.GetAddressOf()
        )
    );
    // インデックスバッファの作成
    DX::ThrowIfFailed(
        DirectX::CreateStaticBuffer(
            deviceResources->GetD3DDevice(),
            resourceUpload,
            indices.data(),
            static_cast<int>(indices.size()),
            sizeof(unsigned short),
            D3D12_RESOURCE_STATE_COMMON,
            m_indexBuffer.GetAddressOf()
        )
    );


    //]
    D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    viewDesc.Format = DXGI_FORMAT_UNKNOWN;
    viewDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    viewDesc.Buffer.FirstElement = 0;
    viewDesc.Buffer.NumElements = 3;
    viewDesc.Buffer.StructureByteStride = sizeof(DirectX::VertexPositionColor);
    viewDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;


    vertexshaderresource = std::make_unique<DirectX::DescriptorHeap>(deviceResources->GetD3DDevice(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        Descriptors::Count);
    deviceResources->GetD3DDevice()->CreateShaderResourceView(m_vertexBuffer.GetAddressOf(), &viewDesc, vertexshaderresource->GetFirstCpuHandle());
   
    //ここはハンドルを動かさなくていいのか?
    
    
    // リソースのアップロードを終了
    auto uploadResourcesFinished = resourceUpload.End(deviceResources->GetCommandQueue());
    return S_OK;
}



void DirectXTK12Polygon::Render(DX::DeviceResources* DR)
{
    DirectX::ResourceUploadBatch resourceUpload(DR->GetD3DDevice());
    auto commandlist = DR->GetCommandList();
    resourceUpload.Begin();
    // ディスクリプタヒープを設定.
    ID3D12DescriptorHeap* heaps = resourceHeap->Heap();
    commandlist->SetDescriptorHeaps(1,&heaps);

    // ルートシグニチャを設定.
    commandlist->SetGraphicsRootSignature(m_pRootSignature.GetPtr());

    // ディスクリプタヒープテーブルを設定.
    commandlist->SetGraphicsRootShaderResourceView(0, m_pVertexBuffer->GetGPUVirtualAddress());
    commandlist->SetGraphicsRootShaderResourceView(1, m_pIndexBuffer->GetGPUVirtualAddress());

 // メッシュ描画.
    commandlist->DispatchMesh(1, 1, 1);

  
    // コマンドの記録を終了.
    m_pCmdList->Close();

    // コマンド実行.
    ID3D12CommandList* ppCmdLists[] = { m_pCmdList.GetPtr() };
    DR->GetCommandQueue()->ExecuteCommandLists(_countof(ppCmdLists), ppCmdLists);

    // Upload the resources to the GPU.
    auto finish = resourceUpload.End(DR->GetCommandQueue());

    // Wait for the upload thread to terminate
    finish.wait();
}
