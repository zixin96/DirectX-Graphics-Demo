#include "MultiTexturingApp.h"

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

MultiTexturingApp::MultiTexturingApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

MultiTexturingApp::~MultiTexturingApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool MultiTexturingApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	BuildTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void MultiTexturingApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 0.1f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

void MultiTexturingApp::Update(const GameTimer& gt)
{
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

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void MultiTexturingApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mOpaquePSO.Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
	                                                                       D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = {mSrvDescriptorHeap.Get()};
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
	                                                                       D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

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

void MultiTexturingApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void MultiTexturingApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void MultiTexturingApp::OnMouseMove(WPARAM btnState, int x, int y)
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
		mRadius = MathHelper::Clamp(mRadius, 1.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void MultiTexturingApp::UpdateCamera(const GameTimer& gt)
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

void MultiTexturingApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world        = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void MultiTexturingApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0     = mat->FresnelR0;
			matConstants.Roughness     = mat->Roughness;
			//!? remember to transpose the matrix before passing into the shader
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void MultiTexturingApp::UpdateMainPassCB(const GameTimer& gt)
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
	mMainPassCB.AmbientLight        = {0.25f, 0.25f, 0.35f, 1.0f};
	mMainPassCB.Lights[0].Direction = {0.57735f, -0.57735f, 0.57735f};
	mMainPassCB.Lights[0].Strength  = {0.6f, 0.6f, 0.6f};
	mMainPassCB.Lights[1].Direction = {-0.57735f, -0.57735f, 0.57735f};
	mMainPassCB.Lights[1].Strength  = {0.3f, 0.3f, 0.3f};
	mMainPassCB.Lights[2].Direction = {0.0f, -0.707f, -0.707f};
	mMainPassCB.Lights[2].Strength  = {0.15f, 0.15f, 0.15f};

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void MultiTexturingApp::AnimateMaterials(const GameTimer& gt)
{
	//!? rotate the fireball texture as a function of time 
	auto fireball = mMaterials["fireballCrate"].get();
	//!? note: we want to rotate the texture around the middle point. Thus, move the pivot to the center, rotate, and move it back
	XMStoreFloat4x4(&fireball->MatTransform, XMMatrixTranslation(-0.5f, -0.5f, 0.0f) * XMMatrixRotationZ(gt.TotalTime()) * XMMatrixTranslation(0.5f, 0.5f, 0.0f));

	fireball->NumFramesDirty = gNumFrameResources;
}

void MultiTexturingApp::BuildTextures()
{
	auto flare      = std::make_unique<Texture>();
	flare->Name     = "flare";
	flare->Filename = L"../../Textures/flare.dds";
	flare->Resource = d3dUtil::CreateTexture(md3dDevice.Get(),
	                                         mCommandList.Get(),
	                                         flare->Filename.c_str(),
	                                         flare->UploadHeap);

	auto flareAlpha      = std::make_unique<Texture>();
	flareAlpha->Name     = "flareAlpha";
	flareAlpha->Filename = L"../../Textures/flarealpha.dds";
	flareAlpha->Resource = d3dUtil::CreateTexture(md3dDevice.Get(),
	                                              mCommandList.Get(),
	                                              flareAlpha->Filename.c_str(),
	                                              flareAlpha->UploadHeap);

	mTextures[flare->Name]      = std::move(flare);
	mTextures[flareAlpha->Name] = std::move(flareAlpha);
}

void MultiTexturingApp::BuildRootSignature()
{
	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0); // t0, t1

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4,
	                                        slotRootParameter,
	                                        (UINT)staticSamplers.size(),
	                                        staticSamplers.data(),
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
		              IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void MultiTexturingApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors             = 2;
	srvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE srvHeapHandle(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto flareTex = mTextures["flare"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format                          = flareTex->GetDesc().Format;
	srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // do not reorder rgba components 
	// filling in D3D12_TEX2D_SRV for 2D textures:
	srvDesc.Texture2D.MostDetailedMip     = 0;                             // index of the most detailed mipmap level to view
	srvDesc.Texture2D.MipLevels           = flareTex->GetDesc().MipLevels; // # of mipmap levels to view
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;                          // all the mipmap levels can be accessed: [0, MipCount - 1] can be accessed

	md3dDevice->CreateShaderResourceView(flareTex.Get(), &srvDesc, srvHeapHandle);

	// --------------------------------------------------------------

	srvHeapHandle.Offset(1, mCbvSrvUavDescriptorSize);

	// --------------------------------------------------------------

	auto flareAlphaTex          = mTextures["flareAlpha"]->Resource;
	srvDesc.Format              = flareAlphaTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = flareAlphaTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(flareAlphaTex.Get(), &srvDesc, srvHeapHandle);
}

void MultiTexturingApp::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"]   = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};
}

void MultiTexturingApp::BuildShapeGeometry()
{
	GeometryGenerator           geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount         = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = 0;
	boxSubmesh.BaseVertexLocation = 0;


	std::vector<Vertex> vertices(box.Vertices.size());

	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		vertices[i].Pos    = box.Vertices[i].Position;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC   = box.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices = box.GetIndices16();

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo  = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                                    mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	                                                   mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride     = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat          = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize  = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void MultiTexturingApp::BuildPSOs()
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
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mOpaquePSO)));
}

void MultiTexturingApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
		                                                          1,
		                                                          (UINT)mAllRitems.size(),
		                                                          (UINT)mMaterials.size()));
	}
}

void MultiTexturingApp::BuildMaterials()
{
	auto fireballCrate                 = std::make_unique<Material>();
	fireballCrate->Name                = "fireballCrate";
	fireballCrate->MatCBIndex          = 0;
	fireballCrate->DiffuseSrvHeapIndex = 0;
	fireballCrate->DiffuseAlbedo       = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	fireballCrate->FresnelR0           = XMFLOAT3(0.05f, 0.05f, 0.05f);
	fireballCrate->Roughness           = 0.2f;

	mMaterials["fireballCrate"] = std::move(fireballCrate);
}

void MultiTexturingApp::BuildRenderItems()
{
	auto boxRitem                = std::make_unique<RenderItem>();
	boxRitem->ObjCBIndex         = 0;
	boxRitem->Mat                = mMaterials["fireballCrate"].get();
	boxRitem->Geo                = mGeometries["boxGeo"].get();
	boxRitem->PrimitiveType      = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount         = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem));

	// All the render items are opaque.
	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}

void MultiTexturingApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB    = mCurrFrameResource->MaterialCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvUavDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex); // bind to root parameter 0. The root parameter description specifies which shader register slot this corresponds to .
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> MultiTexturingApp::GetStaticSamplers()
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
