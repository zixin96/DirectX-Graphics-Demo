//***************************************************************************************
// BoxApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//
// Shows how to draw a box in Direct3D 12.
//
// Controls:
//   Hold the left mouse button down and move the mouse to rotate.
//   Hold the right mouse button down and move the mouse to zoom in and out.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

/**
 * \brief Define our custom vertex format
 * We inform D3D about this structure of our Vertex using a vector of D3D12_INPUT_ELEMENT_DESC.
 * Each element in our structure corresponds to one element in this vector.
 */
struct Vertex
{
	XMFLOAT3 Pos;
	// if vertex memory is significant, then reducing from 128-bit color to 32-bit color may be worthwhile: 
	// XMFLOAT4 Color;
	XMCOLOR Color;
};

/**
 * \brief Constant data per-object
 */
struct ObjectConstants
{
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

class BoxPyramidApp : public D3DApp
{
	public:
		BoxPyramidApp(HINSTANCE hInstance);
		BoxPyramidApp(const BoxPyramidApp& rhs)            = delete;
		BoxPyramidApp& operator=(const BoxPyramidApp& rhs) = delete;
		~BoxPyramidApp() override;

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
		void BuildConstantBuffers();
		void BuildRootSignature();
		void BuildShadersAndInputLayout();
		void BuildBoxPyramidGeometry();
		void BuildPSO();

	private:
		ComPtr<ID3D12RootSignature>  mRootSignature = nullptr;
		ComPtr<ID3D12DescriptorHeap> mCbvHeap       = nullptr;

		std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;

		std::unique_ptr<MeshGeometry> mBoxPyramidGeo = nullptr;

		ComPtr<ID3DBlob> mvsByteCode = nullptr;
		ComPtr<ID3DBlob> mpsByteCode = nullptr;

		std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout; // a description of our custom vertex structure

		std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

		bool mIsWireframe = false;

		XMFLOAT4X4 mBoxWorld     = MathHelper::Identity4x4();
		XMFLOAT4X4 mPyramidWorld = MathHelper::Identity4x4();

		XMFLOAT4X4 mView = MathHelper::Identity4x4();
		XMFLOAT4X4 mProj = MathHelper::Identity4x4();

		float mTheta  = 1.5f * XM_PI; // 3 * pi / 2
		float mPhi    = XM_PIDIV4;    // pi / 4
		float mRadius = 15.0f;

		POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE prevInstance,
                   PSTR      cmdLine,
                   int       showCmd)
{
	// Enable run-time memory check for debug builds.
	#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	#endif

	try
	{
		BoxPyramidApp theApp(hInstance);
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

BoxPyramidApp::BoxPyramidApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
	// move pyramid to the right so it's visible
	XMStoreFloat4x4(&mPyramidWorld, XMMatrixTranslation(5.0f, 0.0f, 0.0f));
}

BoxPyramidApp::~BoxPyramidApp()
{
}

bool BoxPyramidApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(),
		              nullptr)); // we are not drawing, pipeline state could be nullptr

	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildBoxPyramidGeometry();
	BuildPSO();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void BoxPyramidApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void BoxPyramidApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	// Convert Spherical to Cartesian coordinates.
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos    = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up     = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);

	XMMATRIX ViewProj = view * XMLoadFloat4x4(&mProj);

	XMMATRIX boxWorldViewProj = XMLoadFloat4x4(&mBoxWorld) * ViewProj;

	XMMATRIX pyWorldViewProj = XMLoadFloat4x4(&mPyramidWorld) * ViewProj;

	// Update the constant buffer with the latest worldViewProj matrix.
	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(boxWorldViewProj));
	mObjectCB->CopyData(0, objConstants);

	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(pyWorldViewProj));
	mObjectCB->CopyData(1, objConstants);
}

void BoxPyramidApp::Draw(const GameTimer& gt)
{
	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	if (mIsWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
	                                                                       D3D12_RESOURCE_STATE_PRESENT,
	                                                                       D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = {mCbvHeap.Get()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps); // bind the descriptor heap to the pipeline

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get()); // bind root signature to the pipeline

	D3D12_VERTEX_BUFFER_VIEW vertexBuffers[] = {mBoxPyramidGeo->VertexBufferView()};
	mCommandList->IASetVertexBuffers(0, 1, // we are binding buffers to input slot 0
	                                 vertexBuffers);

	mCommandList->IASetIndexBuffer(&mBoxPyramidGeo->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// before issuing the individual draw call, we need to bind the correct descriptor in the descriptor table so that each draw call can receive the correct per-object constant data

	auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());

	{
		// bind first descriptor in the descriptor table to the pipeline
		mCommandList->SetGraphicsRootDescriptorTable(0,          // index of the root parameter we are setting
		                                             cbvHandle); // handle to a descriptor in the heap that specifies the first descriptor in the table being set

		mCommandList->DrawIndexedInstanced(mBoxPyramidGeo->DrawArgs["box"].IndexCount,                                                             // # of indices to draw
		                                   1,                                                                                                      // advanced ONLY
		                                   mBoxPyramidGeo->DrawArgs["box"].StartIndexLocation, mBoxPyramidGeo->DrawArgs["box"].BaseVertexLocation, // one geometry to draw
		                                   0);
	}

	// move on to the next descriptor in the heap
	cbvHandle.Offset(1, mCbvSrvUavDescriptorSize);

	{
		// bind second descriptor in the descriptor table to the pipeline
		mCommandList->SetGraphicsRootDescriptorTable(0,
		                                             cbvHandle);

		mCommandList->DrawIndexedInstanced(mBoxPyramidGeo->DrawArgs["pyramid"].IndexCount,
		                                   1,
		                                   mBoxPyramidGeo->DrawArgs["pyramid"].StartIndexLocation, mBoxPyramidGeo->DrawArgs["pyramid"].BaseVertexLocation,
		                                   0);
	}

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
	                                                                       D3D12_RESOURCE_STATE_RENDER_TARGET,
	                                                                       D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame.
	FlushCommandQueue();
}

void BoxPyramidApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void BoxPyramidApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void BoxPyramidApp::OnMouseMove(WPARAM btnState, int x, int y)
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
		// Make each pixel correspond to 0.005 unit in the scene.
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void BoxPyramidApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;
}

/**
 * \brief Descriptor heap for constant buffer descriptor
 */
void BoxPyramidApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 2;                                         // we only have 2 objects, thus we only need 2 descriptors to describe 2 constant buffers
	cbvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;    // heap types for CBV, SRV, and UAV
	cbvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // descriptors from this heap will be accessed by shader programs
	cbvHeapDesc.NodeMask       = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		              IID_PPV_ARGS(&mCbvHeap)));
}

void BoxPyramidApp::BuildConstantBuffers()
{
	// create a constant buffer to store the per-object constant data. We have 2 objects. 
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 2, true);

	// the following helper variables are used when creating CBVs
	UINT                          objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	D3D12_GPU_VIRTUAL_ADDRESS     cbAddress     = mObjectCB->Resource()->GetGPUVirtualAddress(); // address to start of the buffer (0th constant buffer)
	CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(mCbvHeap->GetCPUDescriptorHandleForHeapStart());     // Offset to the ith object constant buffer in the buffer.

	// Next, we need to create 2 views into this constant buffer (since we have two objects)
	for (int i = 0; i < 2; i++)
	{
		int boxCBufIndex = i;
		cbAddress += boxCBufIndex * objCBByteSize;

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes    = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants)); // must be a multiple of 256
		md3dDevice->CreateConstantBufferView(&cbvDesc,
		                                     cbvHandle); // Describes the CPU descriptor handle that represents the destination where the newly-created constant buffer view will reside
		cbvHandle.Offset(1, mCbvSrvUavDescriptorSize);
	}
}

void BoxPyramidApp::BuildRootSignature()
{
	// Shader programs typically require resources as input (constant buffers,
	// textures, samplers).  The root signature defines the resources the shader
	// programs expect.  If we think of the shader programs as a function, and
	// the input resources as function parameters, then the root signature can be
	// thought of as defining the function signature.  

	// Create a single descriptor table of CBVs.
	// a descriptor table specifies a contiguous range of descriptors in a descriptor heap
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, // table type
	              1,                               // number of descriptors in the table
	              0);                              // base shader register arguments are bound to for this root parameter

	// Create a root parameter that expects a descriptor table of 1 CBV that gets bound to constant buffer register 0 
	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	slotRootParameter[0].InitAsDescriptorTable(1,          // number of ranges
	                                           &cbvTable); // pointer to array of ranges

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1,
	                                        slotRootParameter,
	                                        0,
	                                        nullptr,
	                                        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob         = nullptr;
	HRESULT          hr                = D3D12SerializeRootSignature(&rootSigDesc,
	                                                                 D3D_ROOT_SIGNATURE_VERSION_1,
	                                                                 serializedRootSig.GetAddressOf(),
	                                                                 errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		              0,
		              serializedRootSig->GetBufferPointer(),
		              serializedRootSig->GetBufferSize(),
		              IID_PPV_ARGS(&mRootSignature)));
}

void BoxPyramidApp::BuildShadersAndInputLayout()
{
	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

	// Offline .cso shader loading:
	// mvsByteCode = d3dUtil::LoadBinary(L"Shaders\\color2.cso");
	// mpsByteCode = d3dUtil::LoadBinary(L"Shaders\\color2.cso");


	// struct Vertex
	// {
	// 	XMFLOAT3 Pos; // 0-byte offset
	// 	XMCOLOR Color; // 12-byte offset
	// };

	// Each element in mInputLayout corresponds to one element in our Vertex struct
	mInputLayout =
	{
		{
			"POSITION", 0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"COLOR", 0,
			DXGI_FORMAT_B8G8R8A8_UNORM, // notice that in the shader, this is interpreted as an unsigned normalized floating-point value in the range [0, 1]
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
	};
}

// -------------Use for combining two std::array----------------------
struct concatenater
{
	template <typename T, std::size_t N, std::size_t M>
	auto operator()(const std::array<T, N>& ar1, const std::array<T, M>& ar2) const
	{
		std::array<T, N + M> result;
		std::copy(ar1.cbegin(), ar1.cend(), result.begin());
		std::copy(ar2.cbegin(), ar2.cend(), result.begin() + N);
		return result;
	}
};

template <typename F, typename T, typename T2>
static auto func(F f, T&& t, T2&& t2)
{
	return f(std::forward<T>(t), std::forward<T2>(t2));
}

template <typename F, typename T, typename T2, typename ... Ts>
static auto func(F f, T&& t, T2&& t2, Ts&&...args)
{
	return func(f, f(std::forward<T>(t), std::forward<T2>(t2)), std::forward<Ts>(args)...);
}

// -------------End Use for combining two std::array----------------------

void BoxPyramidApp::BuildBoxPyramidGeometry()
{
	std::array<Vertex, 8> boxVertices =
	{
		// box
		Vertex({XMFLOAT3(-1.0f, -1.0f, -1.0f), XMCOLOR(Colors::White)}),
		Vertex({XMFLOAT3(-1.0f, +1.0f, -1.0f), XMCOLOR(Colors::Black)}),
		Vertex({XMFLOAT3(+1.0f, +1.0f, -1.0f), XMCOLOR(Colors::Red)}),
		Vertex({XMFLOAT3(+1.0f, -1.0f, -1.0f), XMCOLOR(Colors::Green)}),
		Vertex({XMFLOAT3(-1.0f, -1.0f, +1.0f), XMCOLOR(Colors::Blue)}),
		Vertex({XMFLOAT3(-1.0f, +1.0f, +1.0f), XMCOLOR(Colors::Yellow)}),
		Vertex({XMFLOAT3(+1.0f, +1.0f, +1.0f), XMCOLOR(Colors::Cyan)}),
		Vertex({XMFLOAT3(+1.0f, -1.0f, +1.0f), XMCOLOR(Colors::Magenta)}),
	};

	std::array<Vertex, 5> pyramidVertices =
	{
		// pyramid
		Vertex({XMFLOAT3(0.0f, -0.35f, -0.71f), XMCOLOR(Colors::Green)}),
		Vertex({XMFLOAT3(-0.71f, -0.35f, 0.0f), XMCOLOR(Colors::Green)}),
		Vertex({XMFLOAT3(0.0f, -0.35f, 0.71f), XMCOLOR(Colors::Green)}),
		Vertex({XMFLOAT3(0.71f, -0.35f, 0.0f), XMCOLOR(Colors::Green)}),
		Vertex({XMFLOAT3(0.0f, 0.35f, 0.0f), XMCOLOR(Colors::Red)}),
	};

	std::array<std::uint16_t, 36> boxIndices =
	{
		// box
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7,
	};

	std::array<std::uint16_t, 18> pyramidIndices =
	{
		1, 4, 2,
		2, 4, 3,
		1, 2, 5,
		2, 3, 5,
		3, 4, 5,
		4, 1, 5,
	};

	// Note: OBJ file has 1-based index array whereas our is zero-based
	for (auto& n : pyramidIndices) { n -= 1; }

	const auto indices  = func(concatenater{}, boxIndices, pyramidIndices);
	const auto vertices = func(concatenater{}, boxVertices, pyramidVertices);

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mBoxPyramidGeo       = std::make_unique<MeshGeometry>();
	mBoxPyramidGeo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxPyramidGeo->VertexBufferCPU));
	CopyMemory(mBoxPyramidGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxPyramidGeo->IndexBufferCPU));
	CopyMemory(mBoxPyramidGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mBoxPyramidGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                                               mCommandList.Get(),
	                                                               vertices.data(),
	                                                               vbByteSize,
	                                                               mBoxPyramidGeo->VertexBufferUploader);

	mBoxPyramidGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                                              mCommandList.Get(),
	                                                              indices.data(),
	                                                              ibByteSize,
	                                                              mBoxPyramidGeo->IndexBufferUploader);

	mBoxPyramidGeo->VertexByteStride     = sizeof(Vertex);
	mBoxPyramidGeo->VertexBufferByteSize = vbByteSize;
	mBoxPyramidGeo->IndexFormat          = DXGI_FORMAT_R16_UINT;
	mBoxPyramidGeo->IndexBufferByteSize  = ibByteSize;

	UINT boxVertexOffset     = 0;
	UINT pyramidVertexOffset = (UINT)boxVertices.size();

	UINT boxIndexOffset     = 0;
	UINT pyramidIndexOffset = (UINT)boxIndices.size();

	SubmeshGeometry submesh;
	submesh.IndexCount         = (UINT)boxIndices.size();
	submesh.StartIndexLocation = boxIndexOffset;
	submesh.BaseVertexLocation = boxVertexOffset;

	mBoxPyramidGeo->DrawArgs["box"] = submesh;

	submesh.IndexCount         = (UINT)pyramidIndices.size();
	submesh.StartIndexLocation = pyramidIndexOffset;
	submesh.BaseVertexLocation = pyramidVertexOffset;

	mBoxPyramidGeo->DrawArgs["pyramid"] = submesh;
}

void BoxPyramidApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout    = {mInputLayout.data(), (UINT)mInputLayout.size()}; // fill in D3D12_INPUT_LAYOUT_DESC: {an array of D3D12_INPUT_ELEMENT_DESC, # of elements}
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS             =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//opaquePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	//opaquePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	opaquePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	opaquePsoDesc.BlendState               = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState        = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask               = UINT_MAX; // do not disable any samples
	opaquePsoDesc.PrimitiveTopologyType    = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets         = 1;
	opaquePsoDesc.RTVFormats[0]            = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count         = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality       = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat                = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode           = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}
