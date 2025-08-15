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
        //RTV�p�̃f�B�X�N���v�^�q�[�v�̍쐬�Ɏ��s�����B
        return false;
    }
    //�f�B�X�N���v�^�̃T�C�Y���擾�B
   auto  m_rtvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


    return true;
}
