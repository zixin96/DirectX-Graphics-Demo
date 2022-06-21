# DirectX 12 Graphics Techniques

We hardcode values and define things in the source code that might normally be data-driven. 

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





- Introduction to DirectX Math
- 

## TODO

How does DirectX 12 fit into a well-designed rendering engine? 

Output float values to debug window: 

```c++
std::wostringstream woss;
woss << mLightRotationAngle;
OutputDebugString(woss.str().c_str());
OutputDebugString(L"\n");
```

