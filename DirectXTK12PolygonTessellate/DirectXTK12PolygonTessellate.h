#pragma once
#include <memory>
#include <DeviceResources.h>
#include <BufferHelpers.h>
#include <GraphicsMemory.h>
#include <DirectXMath.h>
#include <ResourceUploadBatch.h>


#include <d3dcompiler.h>
#include <d3dx12.h>

__declspec(align(16)) struct SceneCB {
	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 projection;
};

__declspec(align(16)) struct TessCB
{
	float Inner;    // SV_InsideTessFactor �ɑΉ�
	float Outer;    // SV_TessFactor �ɑΉ��i3 �v�f���ׂĂɓ����l�𗬂��z��j
};

// Create root signature.
enum RootParameterIndex
{
	ConstantBuffer,
	TextureSRV,
	TextureSampler,
	RootParameterCount
};
class DirectXTK12PolygonTessellate
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
	

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_layout;
	std::vector<DirectX::VertexPosition> vertices;
	std::vector<unsigned short> indices;
	std::unique_ptr<DirectX::GraphicsMemory> graphicsMemory;
	DirectX::XMMATRIX modelmat;
	//�V�F�[�_�[�̍쐬
	Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;//�V�K�ǉ�
	Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;//�V�K�ǉ�
	DirectX::GraphicsResource SceneCBResource;//�V�K�ǉ�
	DirectX::GraphicsResource TessCBResource;//�V�K�ǉ�
	const int SCENECBINDEX = 0;
	const int TESSCBINDEX = 1;
	DirectX::GraphicsResource m_vertexBuffer;
	DirectX::GraphicsResource m_indexBuffer;
	DirectX::GraphicsResource m_ConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;//�V�K�ǉ�};

};