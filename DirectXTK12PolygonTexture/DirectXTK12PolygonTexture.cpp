#include "pch.h"
#include "DirectXTK12PolygonTexture.h"



void DirectXTK12PolygonTexture::CreateTexture()
{
    ComPtr<ID3D12Resource> tex;

    ResourceUploadBatch resourceUpload(device);

    resourceUpload.Begin();

    DX::ThrowIfFailed(
        CreateWICTextureFromFile(device, resourceUpload, L"texture.bmp",
            tex.ReleaseAndGetAddressOf(), true)
    );

    // Upload the resources to the GPU.
    auto uploadResourcesFinished = resourceUpload.End(m_deviceResources->GetCommandQueue());

    // Wait for the upload thread to terminate
    uploadResourcesFinished.wait();
    
}