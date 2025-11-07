#pragma once
#include <DeviceResources.h>
#include <DescriptorHeap.h>
#include <memory>
#include <GraphicsMemory.h>
struct SceneCB {
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
	float padding[4];
};
class DirectXTK12_PolygonSceneTexture
{
public:
	DirectXTK12_PolygonSceneTexture() {};
	DirectXTK12_PolygonSceneTexture(DX::DeviceResources* DR);
	void Render(DX::DeviceResources* DR);
	virtual ~DirectXTK12_PolygonSceneTexture();
	
	std::unique_ptr<DirectX::DescriptorHeap> m_srvDescriptor;
	std::unique_ptr<DirectX::DescriptorHeap> m_samplerDescriptor;
	DirectX::GraphicsResource SceneCBResource;//êVãKí«â¡
	std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory;
};

