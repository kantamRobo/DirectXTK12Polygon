#include "pch.h"
#include <DirectXMath.h>
#include "DirectXTK12_PolygonSceneTexture.h"
#include <d3dx12.h>



// Create root signature.
enum RootParameterIndex
{
	ConstantBuffer = 0,
	TextureSRV = 1,
	TextureSampler = 2,
	RootParameterCount = 3
};

const UINT TEXTURE_SRVROOTPARAM = 1;
const UINT TEXTURE_SAMPLERROOTPARAM = 2;
const UINT CONSTANTBUFFFER_ROOTPARAM = 0;

DirectXTK12_PolygonSceneTexture::DirectXTK12_PolygonSceneTexture(DX::DeviceResources* DR)
{
	auto device = DR->GetD3DDevice();
	m_graphicsMemory = std::make_unique<DirectX::GraphicsMemory>(device);
    DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);

    DirectX::XMVECTOR eye = DirectX::XMVectorSet(0.0f, 0.0f, -2.0f, 0.0f);  // 距離は好みで -1.5f～-5.0f くらい
    DirectX::XMVECTOR focus = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixLookAtLH(eye, focus, up);

    constexpr float fov = DirectX::XMConvertToRadians(45.0f);
    float aspect = float(sz.right - sz.left) / float(sz.bottom - sz.top);
    float    nearZ = 0.1f;
    float    farZ = 100.0f;
    DirectX::XMMATRIX projMatrix = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);

    SceneCB cb;
    XMStoreFloat4x4(&cb.world, XMMatrixTranspose(worldMatrix));
    XMStoreFloat4x4(&cb.view, XMMatrixTranspose(viewMatrix));
    XMStoreFloat4x4(&cb.projection, XMMatrixTranspose(projMatrix));



    //定数バッファの作成(DIrectXTK12Assimpで追加)

    //https://github.com/microsoft/DirectXTK12/wiki/GraphicsMemory


    SceneCBResource = m_graphicsMemory->AllocateConstant(cb);
    
    m_samplerDescriptor = std::make_unique<DirectX::DescriptorHeap>(
       device,
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        1
	);
    m_srvDescriptor = std::make_unique<DirectX::DescriptorHeap>(
        device,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        1);
	// Create root parameters and initialize first (constants)
    CD3DX12_DESCRIPTOR_RANGE1 ranges[2] = {};
    
    ranges[0].Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // SRV
        1,
        0,                                // register(t0)
        0,                                // space 0
        D3D12_DESCRIPTOR_RANGE_FLAG_NONE
    );
    ranges[1].Init(
        D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
        1,
        0,     //register s0                                        
        0,                                    // register space
        D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

    CD3DX12_ROOT_PARAMETER1 srvrootParameters[2] = {};
    srvrootParameters[0].InitAsDescriptorTable(
        1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL
    );
    srvrootParameters[1].InitAsDescriptorTable(
        1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL
    );

    // Create root parameters and initialize first (constants)
    CD3DX12_ROOT_PARAMETER constrootParameters[1] = {};
    constrootParameters[0].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);//b0





}


void DirectXTK12_PolygonSceneTexture::Render(DX::DeviceResources* DR)
{
    auto commandList = DR->GetCommandList();
    commandList->SetGraphicsRootDescriptorTable(0, m_srvDescriptor->GetFirstGpuHandle());
    commandList->SetGraphicsRootDescriptorTable(1, m_samplerDescriptor->GetFirstGpuHandle());



    commandList->SetGraphicsRootConstantBufferView( 0, SceneCBResource.GpuAddress());


}