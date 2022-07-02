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

	UINT                          Width() const;
	UINT                          Height() const;
	ID3D12Resource*               Resource();
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;
	D3D12_VIEWPORT                Viewport() const;
	D3D12_RECT                    ScissorRect() const;

	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	                      CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);
private:
	void BuildResource();

	ID3D12Device*  md3dDevice = nullptr;
	D3D12_VIEWPORT mViewport;
	D3D12_RECT     mScissorRect;
	UINT           mWidth  = 0;
	UINT           mHeight = 0;

	// Since we will create 2 views (DSV and SRV) on the same resource,
	// we will use a typeless format here: 
	DXGI_FORMAT mFormat = DXGI_FORMAT_R24G8_TYPELESS;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMap = nullptr;
};
