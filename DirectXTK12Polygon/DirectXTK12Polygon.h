#pragma once
#include "pch.h"
#include <DeviceResources.h>
#include <DescriptorHeap.h>
#include <memory>
class DirectXTK12Polygon
{
	HRESULT CreateBuffer(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResources* deviceResources, int height, int width);
	void Render(DX::DeviceResources* DR);
	std::unique_ptr<DirectX::DescriptorHeap> resourceHeap = nullptr;
	std::unique_ptr<DirectX::DescriptorHeap> vertexshaderresource = nullptr;
};

