#pragma once
#include <BufferHelpers.h>
#include <VertexTypes.h>
#include <DescriptorHeap.h>
#include <GraphicsMemory.h>
#include <DeviceResources.h>
#include <DirectXHelpers.h>
class DirectXTK12PolygonTexture
{
public:
	void CreateTexture(DX::DeviceResources* DR);
	HRESULT CreateBuffer(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResources* deviceResources, int height, int width);


	std::unique_ptr<DirectX::DescriptorHeap> m_srvDescriptor;
	std::unique_ptr<DirectX::DescriptorHeap> m_samplerDescriptor;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPipelineState(DX::DeviceResources* deviceresources, const std::wstring& vertexShaderPath, const std::wstring& pixelShaderPath);
	void CreateDescriptors(DX::DeviceResources* DR);
	void Draw(const DX::DeviceResources* DR);


	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;



	//バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_ConstantBuffer;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_layout;
	std::vector<DirectX::VertexPositionColorTexture> vertices;
	std::vector<unsigned short> indices;

	DirectX::XMMATRIX modelmat;
	//シェーダーの作成
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;//新規追加
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;//新規追加
	Microsoft::WRL::ComPtr<ID3D12Resource> tex;

	DirectX::GraphicsResource SceneCBResource;//新規追加
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;

	//新規追加
};

