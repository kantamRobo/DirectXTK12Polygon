#pragma once
#include "pch.h"
#include <DeviceResources.h>
#include <DescriptorHeap.h>
#include <memory>
#include <GraphicsMemory.h>
class DirectXTK12Polygon
{
public:
	HRESULT CreateBuffer(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResources* deviceResources, int height, int width);
	

	std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPipelineState(DX::DeviceResources* deviceresources, const std::wstring& vertexShaderPath, const std::wstring& pixelShaderPath);
	
	void Draw(const DX::DeviceResources* DR);


	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

	

	//�o�b�t�@
	
	DirectX::GraphicsResource m_VertexBuffer;
	DirectX::GraphicsResource m_IndexBuffer;
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

