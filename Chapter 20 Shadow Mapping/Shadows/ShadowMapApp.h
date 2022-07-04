#pragma once

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "../../Common/Camera.h"
#include "FrameResource.h"
#include "ShadowMap.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;

// Lightweight structure stores parameters to draw a shape.  This will vary from app-to-app.
struct RenderItem
{
	RenderItem()                      = default;
	RenderItem(const RenderItem& rhs) = delete;

	XMFLOAT4X4               World          = MathHelper::Identity4x4();
	XMFLOAT4X4               TexTransform   = MathHelper::Identity4x4();
	int                      NumFramesDirty = gNumFrameResources;
	UINT                     ObjCBIndex     = -1;
	Material*                Mat            = nullptr;
	MeshGeometry*            Geo            = nullptr;
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType  = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount         = 0;
	UINT StartIndexLocation = 0;
	int  BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Debug,
	Sky,
	Count
};

class ShadowMapApp : public D3DApp
{
public:
	ShadowMapApp(HINSTANCE hInstance);
	ShadowMapApp(const ShadowMapApp& rhs)            = delete;
	ShadowMapApp& operator=(const ShadowMapApp& rhs) = delete;
	~ShadowMapApp() override;

	bool Initialize() override;
private:
	void CreateRtvAndDsvDescriptorHeaps() override;
	void OnResize() override;
	void Update(const GameTimer& gt) override;
	void Draw(const GameTimer& gt) override;

	void OnMouseDown(WPARAM btnState, int x, int y) override;
	void OnMouseUp(WPARAM btnState, int x, int y) override;
	void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateLights(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateShadowTransform(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateShadowPassCB(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildSkullGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void DrawSceneToShadowMap();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource*                              mCurrFrameResource      = nullptr;
	int                                         mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>>     mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>>      mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>>              mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>   mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	UINT                       mSkyShadowTexStartHeapIndex = 0;
	PassConstants              mMainPassCB;   // index 0 of pass cbuffer.
	PassConstants              mShadowPassCB; // index 1 of pass cbuffer.
	Camera                     mCamera;
	std::unique_ptr<ShadowMap> mShadowMap;
	BoundingSphere             mSceneBounds; // the bounding sphere of the entire scene

	float      mLightNearZ = 0.0f;
	float      mLightFarZ  = 0.0f;
	XMFLOAT3   mLightPosW;
	XMFLOAT4X4 mLightView       = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj       = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();

	float    mLightRotationAngle     = 0.0f;
	XMFLOAT3 mBaseLightDirections[3] = {
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	XMFLOAT3 mRotatedLightDirections[3];

	POINT mLastMousePos;
};
