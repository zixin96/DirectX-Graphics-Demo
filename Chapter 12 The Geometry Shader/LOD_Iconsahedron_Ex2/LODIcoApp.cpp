#include "LODIcoApp.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace PackedVector;

LODIcoApp::LODIcoApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

LODIcoApp::~LODIcoApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool LODIcoApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildDescriptors();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void LODIcoApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void LODIcoApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);

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

	// GPU has finished processing the commands of the current frame resource.
	// Now, we can update the current frame resources

	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
}

void LODIcoApp::Draw(const GameTimer& gt)
{
	// build and submit command lists for the current frame:

	// retrieve the command allocator for the current frame
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// old commands in this allocator have been processed, so we can safely reset it
	ThrowIfFailed(cmdListAlloc->Reset());

	// choose which PSO to use based on whether we render wireframe or not
	if (mIsWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
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
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	int  passCbvIndex  = mPassCbvOffset + mCurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle); // bind a single descriptor in the descriptor table starting from passCbvHandle

	DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

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

	// note that GPU could still be working on commands from previous frames,
	// but that is OK, because we are not touching any frame resources associated with those frames. 
}

void LODIcoApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void LODIcoApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void LODIcoApp::OnMouseMove(WPARAM btnState, int x, int y)
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
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void LODIcoApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;
}

void LODIcoApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos    = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up     = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

// only need to be updated when an object's world matrix changes
void LODIcoApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRenderItems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

// need to be called once per rendering pass
void LODIcoApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj    = XMMatrixMultiply(view, proj);
	XMMATRIX invView     = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj     = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW             = mEyePos;
	mMainPassCB.RenderTargetSize    = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ               = 1.0f;
	mMainPassCB.FarZ                = 1000.0f;
	mMainPassCB.TotalTime           = gt.TotalTime();
	mMainPassCB.DeltaTime           = gt.DeltaTime();

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

// See Notability
void LODIcoApp::BuildDescriptorHeaps()
{
	//
	// Create the CBV heap.
	//
	UINT objCount = (UINT)mOpaqueRitems.size();

	// Each frame needs (n + 1) CBVs and we have gNumFrameResources (3) frame resources
	// n: n objects. 1: one pass constant
	UINT numDescriptors = (objCount + 1) * gNumFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	mPassCbvOffset = objCount * gNumFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask       = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		              IID_PPV_ARGS(&mCbvHeap)));
}

// See Notability
void LODIcoApp::BuildDescriptors()
{
	UINT objCount      = (UINT)mOpaqueRitems.size();
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	auto handle        = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();
			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes    = objCBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);

			// move on to the next descriptor
			handle.Offset(1, mCbvSrvUavDescriptorSize);
		}
	}

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto                      passCB    = mFrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes    = passCBByteSize;

		md3dDevice->CreateConstantBufferView(&cbvDesc, handle);

		// move on to the next descriptor
		handle.Offset(1, mCbvSrvUavDescriptorSize);
	}
}

void LODIcoApp::BuildRootSignature()
{
	// root signature takes 2 root parameters of descriptor-tables type 
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	//? Why do we need two descriptor tables? Can we just put them in the same table?
	//! No, we need to bind these two CBVs at different frequencies. We must put them into separate tables.
	//! For example, we bind per-pass CBV once per frame whereas we bind per-object CBV once per object

	// Describes a descriptor range: https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#using-descriptor-tables
	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
	               1,  // each object draw call requires a single descriptor to be bound that describes a per-object constant buffer 
	               0); // register(b0)

	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV,
	               1,  // each render pass requires a single descriptor to be bound that describes a per-pass constant buffer
	               1); // register(b1)

	// Create root CBVs. (we need two tables because the CBVs will be set at different frequencies)
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0); // per-object CBV
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1); // per-pass CBV

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2,                                                             // The number of slots in the root signature. This number is also the number of elements in the pParameters array (second parameter)
	                                        slotRootParameter,                                             // An array of D3D12_ROOT_PARAMETER structures for the slots in the root signature.
	                                        0,                                                             // Specifies the number of static samplers
	                                        nullptr,                                                       // Pointer to one or more D3D12_STATIC_SAMPLER_DESC structures.
	                                        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT); // Input Assembler is used

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	// Serializes a root signature version 1.0 that can be passed to ID3D12Device::CreateRootSignature
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob         = nullptr;
	HRESULT          hr                = D3D12SerializeRootSignature(&rootSigDesc,
	                                                                 D3D_ROOT_SIGNATURE_VERSION_1,
	                                                                 serializedRootSig.GetAddressOf(),
	                                                                 errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		              0,                                     // For single GPU operation, set this to zero
		              serializedRootSig->GetBufferPointer(), // A pointer to the source data for the serialized signature
		              serializedRootSig->GetBufferSize(),    // its size
		              IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void LODIcoApp::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["standardGS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["opaquePS"]   = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_1");

	// corresponds to Vertex struct in FrameResource.h
	mInputLayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void LODIcoApp::BuildShapeGeometry()
{
	// we place all scene geometry in one big vertex and index buffer

	GeometryGenerator           geoGen;
	GeometryGenerator::MeshData sphere = geoGen.CreateGeosphere(3.0f, 0);

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount         = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = 0;
	sphereSubmesh.BaseVertexLocation = 0;

	auto geo                = std::make_unique<MeshGeometry>();
	geo->Name               = "shapeGeo";
	geo->DrawArgs["sphere"] = sphereSubmesh;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount = sphere.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos   = sphere.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(Colors::Crimson);
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                                    mCommandList.Get(),
	                                                    vertices.data(),
	                                                    vbByteSize,
	                                                    geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                                   mCommandList.Get(),
	                                                   indices.data(),
	                                                   ibByteSize,
	                                                   geo->IndexBufferUploader);

	geo->VertexByteStride     = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat          = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize  = ibByteSize;

	mGeometries[geo->Name] = std::move(geo);
}

void LODIcoApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout    = {mInputLayout.data(), (UINT)mInputLayout.size()};
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS             =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardGS"]->GetBufferPointer()),
		mShaders["standardGS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState       = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState            = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState     = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask            = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets      = 1;
	opaquePsoDesc.RTVFormats[0]         = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count      = 1;
	opaquePsoDesc.SampleDesc.Quality    = 0;
	opaquePsoDesc.DSVFormat             = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


	//
	// PSO for opaque wireframe objects.
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode           = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}

void LODIcoApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
		                                                          1,
		                                                          (UINT)mAllRenderItems.size()));
	}
}

void LODIcoApp::BuildRenderItems()
{
	auto geoSphere = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&geoSphere->World, XMMatrixScaling(1.f, 1.f, 1.f));
	geoSphere->ObjCBIndex         = 0;
	geoSphere->Geo                = mGeometries["shapeGeo"].get();
	geoSphere->PrimitiveType      = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	geoSphere->IndexCount         = geoSphere->Geo->DrawArgs["sphere"].IndexCount;
	geoSphere->StartIndexLocation = geoSphere->Geo->DrawArgs["sphere"].StartIndexLocation;
	geoSphere->BaseVertexLocation = geoSphere->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mAllRenderItems.push_back(std::move(geoSphere));

	// All the render items are opaque.
	for (auto& e : mAllRenderItems)
	{
		mOpaqueRitems.push_back(e.get());
	}
}

void LODIcoApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	// draw one render object at a time (as the world matrix needs to be changed between objects)
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex  = mCurrFrameResourceIndex * (UINT)mOpaqueRitems.size() + ri->ObjCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle); // bind a single descriptor in the descriptor table starting from cbvHandle

		// using instancing, we only draw a subset of the vertex and index buffers by using information stored in the RenderItem
		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}
