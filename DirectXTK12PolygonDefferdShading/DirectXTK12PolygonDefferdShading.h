#pragma once
#include <DescriptorHeap.h>
#include <memory>
#include <RenderTargetState.h>
#include "RenderTexture.h"
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
class DirectXTK12PolygonDefferdShading
{
public:
	bool CreateDescriptorHeap(GraphicsEngine& ge, ID3D12Device5*& d3dDevice);
	std::unique_ptr<RenderTexture> m_albedoRT;
	std::unique_ptr<RenderTexture> m_NormalRT;
	std::unique_ptr<DirectX::DescriptorHeap>  m_resourceDescriptors;
	std::unique_ptr<DirectX::DescriptorHeap> m_renderDescriptors;
};

