#pragma once

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;

/**
 * \brief Define our custom vertex format
 * We inform D3D about this structure of our Vertex using a vector of D3D12_INPUT_ELEMENT_DESC.
 * Each element in our structure corresponds to one element in this vector. 
 */
struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

/**
 * \brief Constant data per-object
 */
struct ObjectConstants
{
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

class PyramidApp : public D3DApp
{
public:
	PyramidApp(HINSTANCE hInstance);
	PyramidApp(const PyramidApp& rhs)            = delete;
	PyramidApp& operator=(const PyramidApp& rhs) = delete;
	~PyramidApp() override;

	bool Initialize() override;

private:
	void OnResize() override;
	void Update(const GameTimer& gt) override;
	void Draw(const GameTimer& gt) override;

	void OnMouseDown(WPARAM btnState, int x, int y) override;
	void OnMouseUp(WPARAM btnState, int x, int y) override;
	void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);

	void BuildDescriptorHeaps();
	void BuildCBVs();
	void BuildRootSignature();
	void BuildInputLayout();
	void BuildShaders();
	void BuildGeometry();
	void BuildPSO();

private:
	ComPtr<ID3D12RootSignature>                    mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap>                   mCbvHeap       = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB      = nullptr;
	std::unique_ptr<MeshGeometry>                  mBoxGeo        = nullptr;
	ComPtr<ID3DBlob>                               mvsByteCode    = nullptr;
	ComPtr<ID3DBlob>                               mpsByteCode    = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout; // a description of our custom vertex structure

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	bool mIsWireframe = false;

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView  = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj  = MathHelper::Identity4x4();

	float mTheta  = 1.5f * XM_PI; // 3 * pi / 2
	float mPhi    = XM_PIDIV4;    // pi / 4
	float mRadius = 5.0f;

	POINT mLastMousePos;
};
