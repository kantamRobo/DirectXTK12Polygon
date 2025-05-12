#pragma once
#include "pch.h"
#include <DeviceResources.h>
#include <DescriptorHeap.h>
#include <memory>
class DirectXTK12Polygon
{
	HRESULT CreateBuffer(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResources* deviceResources, int height, int width);
	void Draw(const DX::DeviceResources* DR);
	void Render(DX::DeviceResources* DR);
	Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateGraphicsPipelineState(DX::DeviceResources* deviceresources, const std::wstring& vertexShaderPath, const std::wstring& pixelShaderPath);
	std::unique_ptr<DirectX::DescriptorHeap> resourceHeap = nullptr;
	std::unique_ptr<DirectX::DescriptorHeap> vertexshaderresource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
	std::vector<DirectX::VertexPositionColor> vertices;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	std::vector<unsigned short> indices;
};

