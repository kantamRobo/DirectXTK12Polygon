#pragma once
#include "pch.h"
#include "DeviceResources.h"
#include <GraphicsMemory.h>
#include <memory>
#include <EffectPipelineStateDescription.h>
#include <DescriptorHeap.h>
#include <DirectXTex.h>
// Vertex structure matching the HLSL StructuredBuffer<Vertex>
struct Vertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT2 uv;
};

// Constant buffer data matching the HLSL cbuffer ConstantBuffer
struct CBData {
    DirectX::XMMATRIX worldViewProj;
    DirectX::XMFLOAT2 screenSize;
    uint32_t triangleCount;
    float padding;
};

// DX12 compute shader-based software rasterizer.
// Renders triangles using a compute shader and copies the result to the back buffer.
class DirectXTK12_ComputeRasterizer
{
public:
    void Initialize(DirectX::GraphicsMemory* graphicsMemory,
                    DX::DeviceResources* deviceResources,
                    int width, int height);

    void Resize(DX::DeviceResources* deviceResources, int width, int height);

    void Render(DX::DeviceResources* deviceResources);

    void CreateTexture(DX::DeviceResources* DR);
private:
    int    m_width         = 0;
    int    m_height        = 0;
    UINT   m_triangleCount = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature>   m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>   m_pipelineState;

    // UAV output texture (written by compute shader, copied to back buffer)
    Microsoft::WRL::ComPtr<ID3D12Resource>        m_outputTexture;

    // Structured buffer holding triangle vertices (SRV t0)
    Microsoft::WRL::ComPtr<ID3D12Resource>        m_vertexBuffer;

    // 1x1 white fallback texture (SRV t1)
    Microsoft::WRL::ComPtr<ID3D12Resource>        m_fallbackTexture;

    // Combined CBV_SRV_UAV descriptor heap
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>  m_descriptorHeap;
    UINT m_descriptorSize = 0;

    DirectX::GraphicsMemory* m_graphicsMemory = nullptr;
	std::unique_ptr<DirectX::DescriptorHeap> resourceDescriptors;

    // Descriptor heap slot indices
    enum DescriptorIndex : int
    {
        UAV_Output       = 0,  // u0 - RWTexture2D output
        SRV_VertexBuffer = 1,  // t0 - StructuredBuffer<Vertex>
        SRV_Texture      = 2,  // t1 - Texture2D base texture
        DescriptorCount  = 3
    };
};
