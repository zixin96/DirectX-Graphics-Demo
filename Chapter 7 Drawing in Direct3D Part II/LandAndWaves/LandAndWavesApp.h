#pragma once

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "../../common/imgui.h"
#include "FrameResource.h"
#include "Waves.h"

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount         = 0;
	UINT StartIndexLocation = 0;
	int  BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Count
};

class LandAndWavesApp : public D3DApp
{
public:
	LandAndWavesApp(HINSTANCE hInstance);
	LandAndWavesApp(const LandAndWavesApp& rhs)            = delete;
	LandAndWavesApp& operator=(const LandAndWavesApp& rhs) = delete;
	~LandAndWavesApp() override;

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
	void UpdateWaves(const GameTimer& gt);

	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildLandGeometry();
	void BuildWavesGeometryBuffers();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	float             GetHillsHeight(float x, float z) const;
	DirectX::XMFLOAT3 GetHillsNormal(float x, float z) const;

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource*                              mCurrFrameResource      = nullptr;
	int                                         mCurrFrameResourceIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>>               mGeometries;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>>            mShaders;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// save a reference to the wave render item so that we can set its vertex buffer on the fly
	// we need to do this because its vertex buffer is a dynamic buffer and changes every frame
	RenderItem*            mWavesRitem = nullptr;
	std::unique_ptr<Waves> mWaves;

	std::vector<std::unique_ptr<RenderItem>> mAllRitems;                           // List of all the render items.
	std::vector<RenderItem*>                 mRitemLayer[(int)RenderLayer::Count]; //! Render items divided by PSO. declare an array consisting of "Count" vectors of RenderItem*

	PassConstants mMainPassCB;

	bool mIsWireframe = false;

	DirectX::XMFLOAT3   mEyePos = {0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT4X4 mView   = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 mProj   = MathHelper::Identity4x4();

	float mTheta  = 1.5f * DirectX::XM_PI;
	float mPhi    = DirectX::XM_PIDIV2 - 0.1f;
	float mRadius = 50.0f;

	// Imgui State
	bool   show_demo_window    = true;
	bool   show_another_window = false;
	ImVec4 clear_color         = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	POINT mLastMousePos;
};
