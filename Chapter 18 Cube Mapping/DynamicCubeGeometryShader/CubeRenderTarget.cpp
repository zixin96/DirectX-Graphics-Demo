//***************************************************************************************
// CubeRenderTarget.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "CubeRenderTarget.h"

CubeRenderTarget::CubeRenderTarget(ID3D12Device* device,
                                   UINT          width,
                                   UINT          height,
                                   DXGI_FORMAT   format)
{
	md3dDevice = device;

	mWidth  = width;
	mHeight = height;
	mFormat = format;

	// Because the cube map faces will have a different resolution than the main back buffer,
	// we need to define a new viewport and scissor rectangle that covers a cube map face:
	mViewport    = {0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
	mScissorRect = {0, 0, (int)width, (int)height};

	BuildResource();
}

ID3D12Resource* CubeRenderTarget::Resource()
{
	return mCubeMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CubeRenderTarget::Srv()
{
	return mhGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeRenderTarget::Rtv()
{
	return mhCpuRtv;
}

D3D12_VIEWPORT CubeRenderTarget::Viewport() const
{
	return mViewport;
}

D3D12_RECT CubeRenderTarget::ScissorRect() const
{
	return mScissorRect;
}

void CubeRenderTarget::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
                                        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
                                        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv)
{
	// Save references to the descriptors (to the whole cube map)
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;
	mhCpuRtv = hCpuRtv;

	// Create the descriptors
	BuildDescriptors();
}

void CubeRenderTarget::BuildDescriptors()
{
	// Create SRV to the entire cubemap resource so that we can sample it in a pixel shader after it is built
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format                          = mFormat;
	srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip     = 0;
	srvDesc.TextureCube.MipLevels           = 1;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(mCubeMap.Get(), &srvDesc, mhCpuSrv);

	//!? create a render target view to the entire texture array
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.ViewDimension                  = D3D12_RTV_DIMENSION_TEXTURE2DARRAY; // The resource will be accessed as an array of 2D textures
	rtvDesc.Format                         = mFormat;
	rtvDesc.Texture2DArray.MipSlice        = 0;
	rtvDesc.Texture2DArray.PlaneSlice      = 0;
	rtvDesc.Texture2DArray.FirstArraySlice = 0; // The index of the first texture to use in an array of textures
	rtvDesc.Texture2DArray.ArraySize       = 6; // Number of textures in the array to use in the render target view, starting from FirstArraySlice.

	// Create RTV to ith cubemap face.
	md3dDevice->CreateRenderTargetView(mCubeMap.Get(), &rtvDesc, mhCpuRtv);
}

// build cube map resource
void CubeRenderTarget::BuildResource()
{
	// creating a cube map texture is done by creating a texture array with 6 elements (one for each face)

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment          = 0;
	texDesc.Width              = mWidth;
	texDesc.Height             = mHeight;
	texDesc.DepthOrArraySize   = 6; // Cube texture array contains 6 elements (one for each face)
	texDesc.MipLevels          = 1;
	texDesc.Format             = mFormat;
	texDesc.SampleDesc.Count   = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // we're going to render to the cube map

	// Get rid of optimization warning:
	D3D12_CLEAR_VALUE optValue;
	optValue.Format   = mFormat;
	optValue.Color[0] = DirectX::Colors::LightSteelBlue[0];
	optValue.Color[1] = DirectX::Colors::LightSteelBlue[1];
	optValue.Color[2] = DirectX::Colors::LightSteelBlue[2];
	optValue.Color[3] = DirectX::Colors::LightSteelBlue[3];

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		              &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		              D3D12_HEAP_FLAG_NONE,
		              &texDesc,
		              D3D12_RESOURCE_STATE_GENERIC_READ, // initially, cube map is in READ state (for reading in the shader)
		              &optValue,
		              IID_PPV_ARGS(&mCubeMap)));
}
