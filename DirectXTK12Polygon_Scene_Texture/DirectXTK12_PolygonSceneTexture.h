#pragma once
#include <DeviceResources.h>
#include <DescriptorHeap.h>
#include <memory>
#include <GraphicsMemory.h>
#include <VertexTypes.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <string>
#include <vector>

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
	virtual ~DirectXTK12_PolygonSceneTexture() {};
	void CreateTexture(DX::DeviceResources* DR);
	// 追加: CreateGraphicsPipelineState メソッド宣言
	Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPipelineState(
		DX::DeviceResources* deviceresources,
		const std::wstring& vertexShaderPath,
		const std::wstring& pixelShaderPath);

	std::unique_ptr<DirectX::DescriptorHeap> m_srvDescriptor;
	std::unique_ptr<DirectX::DescriptorHeap> m_samplerDescriptor;
	DirectX::GraphicsResource SceneCBResource;//新規追加
	std::unique_ptr<DirectX::GraphicsMemory> m_graphicsMemory;

	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	DirectX::XMMATRIX modelmat;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_layout;
	std::vector<DirectX::VertexPositionNormalTexture> vertices;
	std::vector<unsigned short> indices;
	DirectX::SharedGraphicsResource m_vertexBuffer;
	DirectX::SharedGraphicsResource m_indexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> tex;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;//新規追加
};


