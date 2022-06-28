/*
 * Vector Add Compute Shader Application sums the corresponding vector components stored in two structured buffers
 */

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

struct Data
{
	XMFLOAT3 v1;
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material*     Mat = nullptr;
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
	Transparent,
	AlphaTested,
	Count
};

class TypedBufferCSApp : public D3DApp
{
public:
	TypedBufferCSApp(HINSTANCE hInstance);
	TypedBufferCSApp(const TypedBufferCSApp& rhs)            = delete;
	TypedBufferCSApp& operator=(const TypedBufferCSApp& rhs) = delete;
	~TypedBufferCSApp() override;

	bool Initialize() override;

private:
	void OnResize() override;
	void Update(const GameTimer& gt) override;
	void Draw(const GameTimer& gt) override;

	void OnMouseDown(WPARAM btnState, int x, int y) override;
	void OnMouseUp(WPARAM btnState, int x, int y) override;
	void OnMouseMove(WPARAM btnState, int x, int y) override;

	void DoComputeWork();

	void BuildBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildDescriptorHeaps();

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource*                              mCurrFrameResource      = nullptr;
	int                                         mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

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

	const int mNumDataElements = 64;

	ComPtr<ID3D12DescriptorHeap> mCbvSrvUavDescriptorHeap = nullptr; //!? typed buffer needs a descriptor heap

	ComPtr<ID3D12Resource> mInputBufferA       = nullptr;
	ComPtr<ID3D12Resource> mInputUploadBufferA = nullptr;

	ComPtr<ID3D12Resource> mOutputBuffer   = nullptr; // the buffer that our compute shader will write to 
	ComPtr<ID3D12Resource> mReadBackBuffer = nullptr; // the buffer where we will copy the GPU resource to (which will later be read by CPU)

	PassConstants mMainPassCB;

	XMFLOAT3   mEyePos = {0.0f, 0.0f, 0.0f};
	XMFLOAT4X4 mView   = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj   = MathHelper::Identity4x4();

	float mTheta  = 1.5f * XM_PI;
	float mPhi    = XM_PIDIV2 - 0.1f;
	float mRadius = 50.0f;

	POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
                   PSTR      cmdLine, int         showCmd)
{
	// Enable run-time memory check for debug builds.
	#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	#endif

	try
	{
		TypedBufferCSApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

TypedBufferCSApp::TypedBufferCSApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

TypedBufferCSApp::~TypedBufferCSApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool TypedBufferCSApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	BuildBuffers();
	//!? typed buffer needs a descriptor heap
	BuildDescriptorHeaps();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	DoComputeWork();

	return true;
}


void TypedBufferCSApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors             = 2;
	srvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mCbvSrvUavDescriptorHeap)));

	//
	// Fill out the heap with texture descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mCbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format                          = DXGI_FORMAT_R32G32B32_FLOAT; // Buffer<float3>
	srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement             = 0;
	srvDesc.Buffer.NumElements              = mNumDataElements;
	md3dDevice->CreateShaderResourceView(mInputBufferA.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format                  = DXGI_FORMAT_R32_FLOAT; // RWBuffer<float>
	uavDesc.ViewDimension           = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement     = 0;
	uavDesc.Buffer.NumElements      = mNumDataElements;
	md3dDevice->CreateUnorderedAccessView(mOutputBuffer.Get(), nullptr, &uavDesc, hDescriptor);
}

void TypedBufferCSApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void TypedBufferCSApp::Update(const GameTimer& gt)
{
	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource      = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void TypedBufferCSApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
	                                                                       D3D12_RESOURCE_STATE_PRESENT,
	                                                                       D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), (float*)&mMainPassCB.FogColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
	                                                                       D3D12_RESOURCE_STATE_RENDER_TARGET,
	                                                                       D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void TypedBufferCSApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void TypedBufferCSApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void TypedBufferCSApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.2f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.2f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void TypedBufferCSApp::DoComputeWork()
{
	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSOs["StructBuffer"].Get()));

	ID3D12DescriptorHeap* descriptorHeaps[] = {mCbvSrvUavDescriptorHeap.Get()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetComputeRootSignature(mRootSignature.Get());

	CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mCbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	mCommandList->SetComputeRootDescriptorTable(0, tex);
	tex.Offset(1, mCbvSrvUavDescriptorSize);
	mCommandList->SetComputeRootDescriptorTable(1, tex);

	mCommandList->Dispatch(1, 1, 1);

	// Schedule to copy the data from the default buffer to the readback buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
	                                                                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
	                                                                       D3D12_RESOURCE_STATE_COPY_SOURCE));

	mCommandList->CopyResource(mReadBackBuffer.Get(), // the destination resource
	                           mOutputBuffer.Get());  // represents the source resource.

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
	                                                                       D3D12_RESOURCE_STATE_COPY_SOURCE,
	                                                                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait for the work to finish.
	FlushCommandQueue();

	// Map the data so we can read it on CPU.
	float* mappedData = nullptr;
	ThrowIfFailed(mReadBackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));

	std::ofstream fout("results.txt");

	for (int i = 0; i < mNumDataElements; ++i)
	{
		fout << "(" << mappedData[i] << ")" << std::endl;
	}

	mReadBackBuffer->Unmap(0, nullptr);
}

void TypedBufferCSApp::BuildBuffers()
{
	// Generate some data.
	std::vector<Data> dataA(mNumDataElements);
	for (int i = 0; i < mNumDataElements; ++i)
	{
		XMVECTOR randUnitVec = MathHelper::RandUnitVec3();
		XMStoreFloat3(&dataA[i].v1, randUnitVec * MathHelper::RandF(1.0f, 10.f));
	}

	UINT64 inputyteSize = dataA.size() * sizeof(Data);

	// Create some buffers to be used as SRVs.
	mInputBufferA = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                             mCommandList.Get(),
	                                             dataA.data(),
	                                             inputyteSize,
	                                             mInputUploadBufferA);

	UINT64 outputByteSize = dataA.size() * sizeof(float);

	// Create the buffer that will be a UAV.
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		              &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		              D3D12_HEAP_FLAG_NONE,
		              &CD3DX12_RESOURCE_DESC::Buffer(outputByteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS), //! This resource will be accessed by UAV 
		              D3D12_RESOURCE_STATE_UNORDERED_ACCESS,                                                      //! This resource is in UAV access state by default                                                
		              nullptr,
		              IID_PPV_ARGS(&mOutputBuffer)));

	// create the buffer that will be read by CPU 
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		              &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK), //! "readback" buffer
		              D3D12_HEAP_FLAG_NONE,
		              &CD3DX12_RESOURCE_DESC::Buffer(outputByteSize),
		              D3D12_RESOURCE_STATE_COPY_DEST, //! Initial state is copy destination b/c we will use ID3D12GraphicsCommandList::CopyResource to copy the GPU resource to this resource
		              nullptr,
		              IID_PPV_ARGS(&mReadBackBuffer)));
}

void TypedBufferCSApp::BuildRootSignature()
{
	//!? typed buffer needs descriptor tables
	CD3DX12_DESCRIPTOR_RANGE srvInputTable;
	srvInputTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &srvInputTable, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[1].InitAsDescriptorTable(1, &uavTable, D3D12_SHADER_VISIBILITY_ALL);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2,
	                                        slotRootParameter,
	                                        0,
	                                        nullptr,
	                                        D3D12_ROOT_SIGNATURE_FLAG_NONE);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob         = nullptr;
	HRESULT          hr                = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
	                                                                 serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		              0,
		              serializedRootSig->GetBufferPointer(),
		              serializedRootSig->GetBufferSize(),
		              IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void TypedBufferCSApp::BuildShadersAndInputLayout()
{
	mShaders["StructBufferCS"] = d3dUtil::CompileShader(L"Shaders\\Buffer.hlsl", nullptr, "CS", "cs_5_0");
}

void TypedBufferCSApp::BuildPSOs()
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature                    = mRootSignature.Get();
	computePsoDesc.CS                                =
	{
		reinterpret_cast<BYTE*>(mShaders["StructBufferCS"]->GetBufferPointer()),
		mShaders["StructBufferCS"]->GetBufferSize()
	};
	computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["StructBuffer"])));
}

void TypedBufferCSApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
		                                                          1));
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> TypedBufferCSApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
	                                            0,                                // shaderRegister
	                                            D3D12_FILTER_MIN_MAG_MIP_POINT,   // filter
	                                            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
	                                            D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
	                                            D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
	                                             1,                                 // shaderRegister
	                                             D3D12_FILTER_MIN_MAG_MIP_POINT,    // filter
	                                             D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
	                                             D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
	                                             D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
	                                             2,                                // shaderRegister
	                                             D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // filter
	                                             D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
	                                             D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
	                                             D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
	                                              3,                                 // shaderRegister
	                                              D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // filter
	                                              D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
	                                              D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
	                                              D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
	                                                  4,                               // shaderRegister
	                                                  D3D12_FILTER_ANISOTROPIC,        // filter
	                                                  D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
	                                                  D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
	                                                  D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressW
	                                                  0.0f,                            // mipLODBias
	                                                  8);                              // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
	                                                   5,                                // shaderRegister
	                                                   D3D12_FILTER_ANISOTROPIC,         // filter
	                                                   D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
	                                                   D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
	                                                   D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressW
	                                                   0.0f,                             // mipLODBias
	                                                   8);                               // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp
	};
}
