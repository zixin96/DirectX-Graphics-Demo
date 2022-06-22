#pragma once

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"

// RenderItem stores the data needed to draw an object
//! Each instance of RenderItem corresponds to an actual object in the scene
struct RenderItem
{
	RenderItem() = default;

	// where to position the object in world space? 
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	// when drawing, this parameter is used to access vertex and index buffer view
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters that defined the geometry to draw 
	UINT IndexCount         = 0;
	UINT StartIndexLocation = 0;
	int  BaseVertexLocation = 0;

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify object data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;
};

class ShapesApp : public D3DApp
{
public:
	ShapesApp(HINSTANCE hInstance);
	ShapesApp(const ShapesApp& rhs)            = delete;
	ShapesApp& operator=(const ShapesApp& rhs) = delete;
	~ShapesApp() override;

	bool Initialize() override;

private:
	void OnResize() override;
	void Update(const GameTimer& gt) override;
	void Draw(const GameTimer& gt) override;

	void OnMouseDown(WPARAM btnState, int x, int y) override;
	void OnMouseUp(WPARAM btnState, int x, int y) override;
	void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void BuildDescriptors();
	void BuildDescriptorHeaps();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource*                              mCurrFrameResource      = nullptr;
	int                                         mCurrFrameResourceIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mCbvHeap       = nullptr;

	// it's cumbersome to create a new variable name for each geometry, PSO, texture, shader, etc, so we use hash maps:

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>               mGeometries;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>            mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRenderItems;
	std::vector<RenderItem*>                 mOpaqueRitems; // Render items divided by PSO.

	PassConstants mMainPassCB;

	bool mIsWireframe = false;

	DirectX::XMFLOAT3   mEyePos = {0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT4X4 mView   = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj   = MathHelper::Identity4x4();

	float mTheta  = 1.5f * DirectX::XM_PI;
	float mPhi    = 0.2f * DirectX::XM_PI;
	float mRadius = 15.0f;

	POINT mLastMousePos;
};
