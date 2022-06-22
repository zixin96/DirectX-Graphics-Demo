# DirectX 12 Graphics Techniques

We hardcode values and define things in the source code that might normally be data-driven. 

---

Built using C++ 14, Visual Studio 2022

C++ 20 is not supported. 

Reason: 

```c++
ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // Incorrect in C++ 20, Visual Studio 2022
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,          
        D3D12_RESOURCE_STATE_COMMON, 
        &optClear,                   
        IID_PPV_ARGS(&mDepthStencilBuffer)));
```

Fix: 

```c++
CD3DX12_HEAP_PROPERTIES defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &defaultHeap, // & operator requires L-value
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,          
        D3D12_RESOURCE_STATE_COMMON, 
        &optClear,                   
        IID_PPV_ARGS(&mDepthStencilBuffer)));
```

---

Output float values to debug window: 

```c++
std::wostringstream woss;
woss << mLightRotationAngle;
OutputDebugString(woss.str().c_str());
OutputDebugString(L"\n");
```

---

Major shift starting from Chapter 7: from synchronizing the CPU and GPU once per frame, to using a circular array of the resources the CPU needs to modify each frame (frame resources).

```c++
void App::Update(const GameTimer& gt)
{
	// Update things that are not in frame resources...

	// Get the next available frame resource
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource      = mFrameResources[mCurrFrameResourceIndex].get();
	
    // Is this frame resource being used by GPU? 
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
        // This frame resource is in use by GPU
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	
    // This frame resource is not in use by GPU
    // Update things that are in frame resources...
}
```

"This frame resource is not in use by GPU" means two things: 

1. The frame resource is in its initial state (`mCurrFrameResource->Fence == 0`). There is no data in it. We can safely upload data. **OR** 
2. Directx12 Fence has reached the frame resource's fence value (`mFence->GetCompletedValue() >= mCurrFrameResource->Fence`). GPU has finished using this frame resource's data. We can safely update the data.

---

Imgui: 

Subapplicaitons only need to add imgui code in Draw functions

See `LandAndWaves` project for more details

```c++
void XXApp::Draw(const GameTimer& gt)
{
	//--------imgui---------------
	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	//--------imgui---------------

...

	//--------imgui---------------
	ImGui::Render();
	mCommandList->SetDescriptorHeaps(1, mSrvHeap.GetAddressOf());
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());
	//--------imgui---------------

	...

	//--------imgui---------------
	//! Note: this line is only necessary if we enable multi viewport, which we have by default
	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault(NULL, (void*)mCommandList.Get());
	//--------imgui---------------

	...
}
```



## TODO

How does DirectX 12 fit into a well-designed rendering engine? 
