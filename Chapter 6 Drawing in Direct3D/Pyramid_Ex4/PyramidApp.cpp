#include "PyramidApp.h"

PyramidApp::PyramidApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

PyramidApp::~PyramidApp()
{
}

bool PyramidApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(),
		              nullptr)); // we are not drawing, pipeline state could be nullptr

	BuildInputLayout();
	BuildGeometry();
	BuildDescriptorHeaps();
	BuildCBVs();
	BuildRootSignature();
	BuildShaders();
	BuildPSO();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void PyramidApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void PyramidApp::Update(const GameTimer& gt)
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

	XMMATRIX world         = XMLoadFloat4x4(&mWorld);
	XMMATRIX proj          = XMLoadFloat4x4(&mProj);
	XMMATRIX worldViewProj = world * view * proj;

	// Update the constant buffer with the latest worldViewProj matrix.
	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
	mObjectCB->CopyData(0, objConstants);
}

void PyramidApp::Draw(const GameTimer& gt)
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

	D3D12_VERTEX_BUFFER_VIEW vertexBuffers[] = {mBoxGeo->VertexBufferView()};
	mCommandList->IASetVertexBuffers(0, 1, // we are binding buffers to input slot 0
	                                 vertexBuffers);

	mCommandList->IASetIndexBuffer(&mBoxGeo->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// bind the descriptor table to the pipeline
	mCommandList->SetGraphicsRootDescriptorTable(0,                                               // index of the root parameter we are setting
	                                             mCbvHeap->GetGPUDescriptorHandleForHeapStart()); // handle to a descriptor in the heap that specifies the first descriptor in the table being set

	mCommandList->DrawIndexedInstanced(mBoxGeo->DrawArgs["box"].IndexCount,
	                                   1,
	                                   mBoxGeo->DrawArgs["box"].StartIndexLocation,
	                                   mBoxGeo->DrawArgs["box"].BaseVertexLocation,
	                                   0);

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
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT;

	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame.
	FlushCommandQueue();
}

void PyramidApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void PyramidApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void PyramidApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if (d3dUtil::IsKeyDown(MK_LBUTTON))
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
	else if (d3dUtil::IsKeyDown(MK_RBUTTON))
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

void PyramidApp::OnKeyboardInput(const GameTimer& gt)
{
	if (d3dUtil::IsKeyDown('1'))
		mIsWireframe = true;
	else
		mIsWireframe = false;
}

/**
 * \brief Descriptor heap for constant buffer descriptor
 */
void PyramidApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = 1;                                         // we only have a single object, thus we only need a single descriptor to describe a single constant buffer
	cbvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;    // heap types for CBV, SRV, and UAV
	cbvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // descriptors from this heap will be accessed by shader programs
	cbvHeapDesc.NodeMask       = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		              IID_PPV_ARGS(&mCbvHeap)));
}

void PyramidApp::BuildCBVs()
{
	// constant buffer to store the constants of n object (in this case, we have a single object)
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	// address to start of the buffer (0th constant buffer)
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();

	// Offset to the ith object constant buffer in the buffer. (in this case, we have a single object)
	int boxCBufIndex = 0; // i
	cbAddress += boxCBufIndex * objCBByteSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes    = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants)); // must be a multiple of 256

	md3dDevice->CreateConstantBufferView(&cbvDesc,
	                                     mCbvHeap->GetCPUDescriptorHandleForHeapStart()); // Describes the CPU descriptor handle that represents the destination where the newly-created constant buffer view will reside
}

void PyramidApp::BuildRootSignature()
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

void PyramidApp::BuildInputLayout()
{
	mInputLayout =
	{
		{
			"POSITION", 0,                              // semanticName + semanticIndex: corresponds to "float3 PosL  : POSITION0;" in shader. 
			DXGI_FORMAT_R32G32B32_FLOAT,                // data type: float3
			0,                                          // input slot: this element comes from the input slot 0
			0,                                          // 0-byte offset from the start of the C++ Vertex struct of the input slot 0 to the start of the vertex component. could use D3D12_APPEND_ALIGNED_ELEMENT instead of manually specifying the offset
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, // Input data is per-vertex data
			0                                           // This value must be 0 for an element that contains per-vertex data
		},
		{
			"COLOR", 0,                                 // semanticName + semanticIndex: corresponds to "float4 Color : COLOR0;" in shader
			DXGI_FORMAT_R32G32B32A32_FLOAT,             // data type: float4
			0,                                          // input slot: this element comes from the input slot 0
			12,                                         // 12-byte offset from the start of the C++ Vertex struct of the input slot 0 to the start of the vertex component. could use D3D12_APPEND_ALIGNED_ELEMENT instead of manually specifying the offset
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, // Input data is per-vertex data
			0                                           // This value must be 0 for an element that contains per-vertex data
		}
	};
}

void PyramidApp::BuildShaders()
{
	// Offline .cso shader loading:
	// mvsByteCode = d3dUtil::LoadBinary(L"Shaders\\color2.cso");
	// mpsByteCode = d3dUtil::LoadBinary(L"Shaders\\color2.cso");

	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");
}

void PyramidApp::BuildGeometry()
{
	//!? Change vertex and index buffer to draw a pyramid
	//!? copy paste from OBJ to here: (remember to triangulate the geometry (in Maya) before exporting it as OBJ file)
	std::array<Vertex, 5> vertices =
	{
		Vertex({XMFLOAT3(0.0f, -0.35f, -0.71f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(-0.71f, -0.35f, 0.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(0.0f, -0.35f, 0.71f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(0.71f, -0.35f, 0.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(0.0f, 0.35f, 0.0f), XMFLOAT4(Colors::Red)}),
	};

	std::array<std::uint16_t, 18> indices =
	{
		1, 4, 2,
		2, 4, 3,
		1, 2, 5,
		2, 3, 5,
		3, 4, 5,
		4, 1, 5,
	};

	//!? Note: OBJ file has 1-based index array whereas our is zero-based
	for (auto& n : indices) { n -= 1; }

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mBoxGeo = std::make_unique<MeshGeometry>();

	mBoxGeo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mBoxGeo->VertexBufferCPU));
	CopyMemory(mBoxGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mBoxGeo->IndexBufferCPU));
	CopyMemory(mBoxGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                                        mCommandList.Get(),
	                                                        vertices.data(),
	                                                        vbByteSize,
	                                                        mBoxGeo->VertexBufferUploader);

	mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                                       mCommandList.Get(),
	                                                       indices.data(),
	                                                       ibByteSize,
	                                                       mBoxGeo->IndexBufferUploader);

	mBoxGeo->VertexByteStride     = sizeof(Vertex);
	mBoxGeo->VertexBufferByteSize = vbByteSize;
	mBoxGeo->IndexFormat          = DXGI_FORMAT_R16_UINT;
	mBoxGeo->IndexBufferByteSize  = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount         = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	mBoxGeo->DrawArgs["box"] = submesh;
}

void PyramidApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.InputLayout    = {mInputLayout.data(), (UINT)mInputLayout.size()}; // fill in D3D12_INPUT_LAYOUT_DESC: {an array of D3D12_INPUT_ELEMENT_DESC, # of elements}
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
	opaquePsoDesc.RasterizerState       = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState            = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState     = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask            = UINT_MAX; // do not disable any samples
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets      = 1;
	opaquePsoDesc.RTVFormats[0]         = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count      = 1;
	opaquePsoDesc.SampleDesc.Quality    = 0;
	opaquePsoDesc.DSVFormat             = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode           = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}
