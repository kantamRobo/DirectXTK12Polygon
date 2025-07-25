#pragma once

#include <dxcapi.h>    // ← 追加
#include <GraphicsMemory.h>
#include "DeviceResourcesMod.h"
#include <DescriptorHeap.h>
#include <fstream>
#include <vector>
#include <filesystem> // C++17以降推奨
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
    void Initialize(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResourcesMod* deviceResources, int height, int width);
    void InitializeDXC();
	
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShaderDXC(
        IDxcLibrary* lib,
        IDxcCompiler* comp,
        IDxcIncludeHandler* includeHandler,
        const std::wstring& path,
        LPCWSTR entryPoint,
        LPCWSTR targetProfile)
    {
        // --- ファイル読み込み ---
        std::ifstream shaderFile(path, std::ios::binary);
        if (!shaderFile) {
            throw std::runtime_error("Failed to open shader file");
        }

        std::vector<char> shaderSource(
            (std::istreambuf_iterator<char>(shaderFile)),
            std::istreambuf_iterator<char>());

        // --- DXC Blob 作成（UTF-8）---
        Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
        DX::ThrowIfFailed(lib->CreateBlobWithEncodingOnHeapCopy(
            shaderSource.data(),
            static_cast<UINT32>(shaderSource.size()),
            CP_UTF8,
            &sourceBlob));

        // --- コンパイル実行 ---
        Microsoft::WRL::ComPtr<IDxcOperationResult> result;
        DX::ThrowIfFailed(comp->Compile(
            sourceBlob.Get(),
            path.c_str(),
            entryPoint,
            targetProfile,
            nullptr, 0,
            nullptr, 0,
            includeHandler,
            &result));

        // --- コンパイルステータス確認 ---
        HRESULT hrStatus;
        DX::ThrowIfFailed(result->GetStatus(&hrStatus));
        if (FAILED(hrStatus)) {
            Microsoft::WRL::ComPtr<IDxcBlobEncoding> errors;
            result->GetErrorBuffer(&errors);
            OutputDebugStringA((char*)errors->GetBufferPointer());
            throw std::runtime_error("DXC shader compile failed");
        }

        // --- バイトコード取得 ---
        Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob;
        DX::ThrowIfFailed(result->GetResult(&shaderBlob));
        return shaderBlob;
    }

	HRESULT CreateBuffer(DirectX::GraphicsMemory* graphicsmemory, DX::DeviceResourcesMod* deviceResources, int height, int width);


	std::unique_ptr<DirectX::DescriptorHeap> m_resourceDescriptors;

    Microsoft::WRL::ComPtr<ID3D12PipelineState>CreateGraphicsPipelineState(
        DX::DeviceResourcesMod* devResources,
        const std::wstring& vsPath,
        const std::wstring& psPath,
        const std::wstring& msPath);
	void CreateDescriptors(DX::DeviceResourcesMod* DR);
	void Draw(DirectX::GraphicsMemory* graphic,DX::DeviceResourcesMod* DR);
    

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

	// DXC 用インターフェイス
	Microsoft::WRL::ComPtr<IDxcLibrary>           m_dxcLibrary;
	Microsoft::WRL::ComPtr<IDxcCompiler>          m_dxcCompiler;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler>    m_dxcIncludeHandler;

};


//https://simoncoenen.com/blog/programming/graphics/DxcCompiling
