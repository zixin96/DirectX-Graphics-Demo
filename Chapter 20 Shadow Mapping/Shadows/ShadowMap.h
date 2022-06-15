//***************************************************************************************
// ShadowMap.h by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#pragma once

#include "../../Common/d3dUtil.h"

/**
 * \brief A utility class that helps us store the scene depth from the perspective of the light source.
 * It encapsulates a depth/stencil buffer, necessary views, and viewport.
 * A depth/stencil buffer used for shadow mapping is called a shadow map. 
 */
class ShadowMap
{
public:
	ShadowMap(ID3D12Device* device,
	          UINT          width,
	          UINT          height);

	ShadowMap(const ShadowMap& rhs)            = delete;
	ShadowMap& operator=(const ShadowMap& rhs) = delete;
	~ShadowMap()                               = default;

	UINT Width() const;
	UINT Height() const;

	ID3D12Resource* Resource();

	// The shadow mapping algorithm requires 2 render passes. In the first one,
	// we render the scene depth from the viewpoint of the light into the shadow map (DSV).
	// In the second one, we render the scene as normal to the back buffer from our "player" camera,
	// but use the shadow map as a shader input (SRV) to implement the shadow mapping algorithm.
	// Thus, the shadow map will be viewed as both depth/stencil (shadow pass) and shader resource (rendering pass)
	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;

	D3D12_VIEWPORT Viewport() const;
	D3D12_RECT     ScissorRect() const;

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	                      CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
	                      CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	void OnResize(UINT newWidth, UINT newHeight);

private:
	void BuildDescriptors();
	void BuildResource();

private:
	ID3D12Device* md3dDevice = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT     mScissorRect;

	UINT        mWidth  = 0;
	UINT        mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R24G8_TYPELESS;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap = nullptr;
};
