#pragma once

#include "DeviceResourcesMod.h"
struct SceneCB {
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
};
// Create root signature.
enum RootParameterIndex
{
	ConstantBuffer,
	TextureSRV,
	TextureSampler,
	RootParameterCount
};
class DirectXTK12MeshShader
{





public:
	HRESULT CreateBuffer(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResourcesMod* deviceResources, int height, int width);


	std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPipelineState(DX::DeviceResourcesMod* deviceresources, const std::wstring& vertexShaderPath, const std::wstring& pixelShaderPath);
	void CreateDescriptors(DX::DeviceResourcesMod* DR);
	void Draw(const DX::DeviceResourcesMod* DR);


	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;



	//�o�b�t�@
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_ConstantBuffer;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_layout;
	std::vector<DirectX::VertexPosition> vertices;
	std::vector<unsigned short> indices;

	DirectX::XMMATRIX modelmat;
	//�V�F�[�_�[�̍쐬
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;//�V�K�ǉ�
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;//�V�K�ǉ�
	DirectX::GraphicsResource SceneCBResource;//�V�K�ǉ�
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;//�V�K�ǉ�
};
