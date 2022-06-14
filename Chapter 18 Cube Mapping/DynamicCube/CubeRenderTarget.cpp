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

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeRenderTarget::Rtv(int faceIndex)
{
	return mhCpuRtv[faceIndex];
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
                                        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv[6])
{
	// Save references to the descriptors (to the whole cube map)
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;

	for (int i = 0; i < 6; ++i)
	{
		mhCpuRtv[i] = hCpuRtv[i];
	}

	// Create the descriptors
	BuildDescriptors();
}

void CubeRenderTarget::OnResize(UINT newWidth, UINT newHeight)
{
	if ((mWidth != newWidth) || (mHeight != newHeight))
	{
		mWidth  = newWidth;
		mHeight = newHeight;

		BuildResource();

		// New resource, so we need new descriptors to that resource.
		BuildDescriptors();
	}
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

	// create a RTV to each element in the cube map texture array so that we can render onto each cube map face one-by-one
	for (int i = 0; i < 6; ++i)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.ViewDimension             = D3D12_RTV_DIMENSION_TEXTURE2DARRAY; // The resource will be accessed as an array of 2D textures
		rtvDesc.Format                    = mFormat;
		rtvDesc.Texture2DArray.MipSlice   = 0;
		rtvDesc.Texture2DArray.PlaneSlice = 0;

		// Render target to ith element.
		rtvDesc.Texture2DArray.FirstArraySlice = i; // The index of the first texture to use in an array of textures

		// Only view one element of the array.
		rtvDesc.Texture2DArray.ArraySize = 1; // Number of textures in the array to use in the render target view, starting from FirstArraySlice.

		// Create RTV to ith cubemap face.
		md3dDevice->CreateRenderTargetView(mCubeMap.Get(), &rtvDesc, mhCpuRtv[i]);
	}
}

// build cube map resource
void CubeRenderTarget::BuildResource()
{
	// Note, compressed formats cannot be used for UAV.  We get error like:
	// ERROR: ID3D11Device::CreateTexture2D: The format (0x4d, BC3_UNORM) 
	// cannot be bound as an UnorderedAccessView, or cast to a format that
	// could be bound as an UnorderedAccessView.  Therefore this format 
	// does not support D3D11_BIND_UNORDERED_ACCESS.

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
