#include "pch.h"
#include "DirectXTK12PolygonDefferdShading.h"




bool DirectXTK12PolygonDefferdShading::CreateDescriptorHeap(GraphicsEngine& ge, ID3D12Device5*& d3dDevice)
{

    m_resourceDescriptors = std::make_unique<DirectX::DescriptorHeap>(d3dDevice,
        Descriptors::Count);

    m_albedoRTD = std::make_unique<DirectX::DescriptorHeap>(d3dDevice,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        RTDescriptors::RTCount);

    m_NormalRTD = std::make_unique<DirectX::DescriptorHeap>(d3dDevice,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
        RTDescriptors::RTCount);


    m_albedoRT->SetDevice(d3dDevice,
        m_resourceDescriptors->GetCpuHandle(Descriptors::SceneTex),
        m_renderDescriptors->GetCpuHandle(RTDescriptors::OffScreenRT));






   m_NormalRT->SetDevice(d3dDevice,
        m_resourceDescriptors->GetCpuHandle(Descriptors::SceneTex),
        m_renderDescriptors->GetCpuHandle(RTDescriptors::OffScreenRT));


    if (m_rtvHeap == nullptr) {
        //RTV用のディスクリプタヒープの作成に失敗した。
        return false;
    }
    //ディスクリプタのサイズを取得。
   auto  m_rtvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


    return true;
}
