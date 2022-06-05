//***************************************************************************************
// d3dApp.cpp by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#include "d3dApp.h"
#include <WindowsX.h> // for GET_X/Y_LPARAM macros

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

/**
 * \brief Main window procedure where we write the code that we want to execute in response to a message our window receives.
 * \param hwnd The handle to the window receiving the message
 * \param msg A predefined constant value identifying the message
 * \param wParam Extra information about the message which is dependent upon the specific message
 * \param lParam Extra information about the message which is dependent upon the specific message
 * \return whether the procedure is a success or failure.
 */
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) // CALLBACK identifier: Windows will be calling this function outside of the code space of the program when it needs to process a message 
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return D3DApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp* D3DApp::mApp = nullptr;

D3DApp* D3DApp::GetApp()
{
	return mApp;
}

D3DApp::D3DApp(HINSTANCE hInstance)
	: mhAppInst(hInstance)
{
	// Only one D3DApp can be constructed.
	assert(mApp == nullptr);
	mApp = this;
}

D3DApp::~D3DApp()
{
	if (md3dDevice != nullptr)
	{
		// we need to wait until GPU is done processing the commands in the queue before we destroy any resources the GPU is still referencing.
		// otherwise, the GPU might crash when the application exits
		FlushCommandQueue();
	}
}

HINSTANCE D3DApp::AppInst() const
{
	return mhAppInst;
}

HWND D3DApp::MainWnd() const
{
	return mhMainWnd;
}

float D3DApp::AspectRatio() const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

bool D3DApp::Get4xMsaaState() const
{
	return m4xMsaaState;
}

void D3DApp::Set4xMsaaState(bool value)
{
	if (m4xMsaaState != value)
	{
		m4xMsaaState = value;

		// Recreate the swapchain and buffers with new multisample settings.
		CreateSwapChain();
		OnResize();
	}
}

int D3DApp::Run()
{
	MSG msg = {0};

	mTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			mTimer.Tick();

			if (!mAppPaused)
			{
				CalculateFrameStats();
				Update(mTimer);
				Draw(mTimer);
			}
			else
			{
				// if the app is paused, free CPU cycles back to the OS
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

bool D3DApp::Initialize()
{
	if (!InitMainWindow())
		return false;

	if (!InitDirect3D())
		return false;

	// Do the initial resize code.
	OnResize();

	return true;
}

/**
 * \brief Create the render target and depth stencil descriptor heaps to store the descriptors/views
 */
void D3DApp::CreateRtvAndDsvDescriptorHeaps()
{
	// RTV heap 
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount; // we need to store 2 render target descriptors in this heap to describe the buffer resources in the swap chain 
	rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask       = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		              &rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

	// DSV heap
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1; // we need to store 1 depth/stencil descriptor in this heap to describe the depth/stencil buffer resource for depth testing
	dsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask       = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		              &dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));
}

void D3DApp::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	// Flush before changing any resources.
	FlushCommandQueue();

	// reset the command list to prep for recreation commands
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(),
		              nullptr)); // we are not using this command list for drawing in D3DApp::OnResize, so specify pipeline init state is nullptr here

	// Release the previous resources we will be recreating. (You must release the swap chain resources before calling IDXGISwapChain::ResizeBuffers)
	for (int i = 0; i < SwapChainBufferCount; ++i)
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	// Resize the swap chain.
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		              SwapChainBufferCount,
		              mClientWidth,
		              mClientHeight,
		              mBackBufferFormat,
		              DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	mCurrBackBuffer = 0;

	// create an RTV to each buffer in the swap chain
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; i++)
	{
		// get the ith buffer in the swap chain
		ThrowIfFailed(mSwapChain->GetBuffer(i, IID_PPV_ARGS(mSwapChainBuffer[i].GetAddressOf()))); // use GetBuffer to retrieve buffer resources that are stored in the swap chain

		// create an RTV to it 
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(),
		                                   nullptr,        // because we've specified our back buffer format when creating the swap chain (and resize), we can pass nullptr here
		                                   rtvHeapHandle); // Describes the CPU descriptor handle that represents the destination where the newly-created render target view will reside

		// next entry in heap (will change rtvHeapHandle)
		rtvHeapHandle.Offset(1,                   // The number of descriptors by which to increment.
		                     mRtvDescriptorSize); // The amount by which to increment for each descriptor, including padding.
	}

	// We need to manually create depth/stencil buffer resource because swap chain doesn't provide it

	// Create the depth/stencil buffer resource.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // the type of resource
	depthStencilDesc.Alignment        = 0;
	depthStencilDesc.Width            = mClientWidth;
	depthStencilDesc.Height           = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels        = 1;

	// Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
	// the depth buffer.  Therefore, because we need to create two views to the same resource:
	//   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
	//   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
	// we need to create the depth buffer resource with a typeless format.  
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

	depthStencilDesc.SampleDesc.Count   = m4xMsaaState ? 4 : 1;                    // both the back buffer and depth buffer must be created with the same multisampling settings
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0; // both the back buffer and depth buffer must be created with the same multisampling settings
	depthStencilDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;            // let driver to choose the most efficient layout
	depthStencilDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // flags for depth/stencil buffer

	D3D12_CLEAR_VALUE optClear;
	optClear.Format               = mDepthStencilFormat;
	optClear.DepthStencil.Depth   = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		              &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // resources that are solely accessed by GPU
		              D3D12_HEAP_FLAG_NONE,
		              &depthStencilDesc,           //  describes the resource 
		              D3D12_RESOURCE_STATE_COMMON, // initial state 
		              &optClear,                   // optimal clear values (if your clear value matches this, it's optimal)
		              IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf())));

	// before using the depth/stencil buffer, we must create an associated
	// depth/stencil view to be bound to the pipeline

	// Create the depth/stencil descriptor to mip level 0 of entire resource using the format of the resource.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags              = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format             = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(),
	                                   &dsvDesc,            // since our depth buffer resource is typeless, we must provide a D3D12_DEPTH_STENCIL_VIEW_DESC. 
	                                   DepthStencilView()); // Describes the CPU descriptor handle that represents the destination where the newly-created depth/stencil view will reside

	// Transition the resource from its initial state to be used as a depth buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
	                                                                       D3D12_RESOURCE_STATE_COMMON,
	                                                                       D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = {mCommandList.Get()};
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCommandQueue();

	// Update the viewport transform to cover the client area.
	// this specifies how we do viewport transform page 193
	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width    = static_cast<float>(mClientWidth);
	mScreenViewport.Height   = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	mScissorRect = {0, 0, mClientWidth, mClientHeight};
}

/**
 * \brief The window procedure function for the main application window.
 * Override this method if there is a message you need to handle that this function does not handle.
 * If you override this method, any message that you do not handle should be forwarded to this function.
 * \param hwnd The handle to the window receiving the message
 * \param msg A predefined constant value identifying the message
 * \param wParam Extra information about the message which is dependent upon the specific message
 * \param lParam Extra information about the message which is dependent upon the specific message
 * \return whether the procedure is a success or failure.
 */
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.  
		case WM_ACTIVATE:
			if (LOWORD(wParam) == WA_INACTIVE)
			{
				mAppPaused = true;
				mTimer.Stop();
			}
			else
			{
				mAppPaused = false;
				mTimer.Start();
			}
			return 0;

		// WM_SIZE is sent when the user resizes the window.  
		case WM_SIZE:
			// Save the new client area dimensions.
			mClientWidth = LOWORD(lParam);
			mClientHeight = HIWORD(lParam);
			if (md3dDevice)
			{
				if (wParam == SIZE_MINIMIZED)
				{
					mAppPaused = true;
					mMinimized = true;
					mMaximized = false;
				}
				else if (wParam == SIZE_MAXIMIZED)
				{
					mAppPaused = false;
					mMinimized = false;
					mMaximized = true;
					OnResize();
				}
				else if (wParam == SIZE_RESTORED)
				{
					// Restoring from minimized state?
					if (mMinimized)
					{
						mAppPaused = false;
						mMinimized = false;
						OnResize();
					}

					// Restoring from maximized state?
					else if (mMaximized)
					{
						mAppPaused = false;
						mMaximized = false;
						OnResize();
					}
					else if (mResizing)
					{
						// If user is dragging the resize bars, we do not resize 
						// the buffers here because as the user continuously 
						// drags the resize bars, a stream of WM_SIZE messages are
						// sent to the window, and it would be pointless (and slow)
						// to resize for each WM_SIZE message received from dragging
						// the resize bars.  So instead, we reset after the user is 
						// done resizing the window and releases the resize bars, which 
						// sends a WM_EXITSIZEMOVE message.
					}
					else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
					{
						OnResize();
					}
				}
			}
			return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
		case WM_ENTERSIZEMOVE:
			mAppPaused = true;
			mResizing = true;
			mTimer.Stop();
			return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
		case WM_EXITSIZEMOVE:
			mAppPaused = false;
			mResizing = false;
			mTimer.Start();
			OnResize(); // resize after the user has released the resize bar
			return 0;

		// WM_DESTROY is sent when the window is being destroyed.
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
		case WM_MENUCHAR:
			// Don't beep when we alt-enter.
			return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
		case WM_GETMINMAXINFO:
			((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
			((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
			return 0;

		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); // GET_X/Y_LPARAM retrieves the signed x/y-coordinate from LPARAM value
			return 0;
		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); // GET_X/Y_LPARAM retrieves the signed x/y-coordinate from LPARAM value
			return 0;
		case WM_MOUSEMOVE:
			OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)); // GET_X/Y_LPARAM retrieves the signed x/y-coordinate from LPARAM value
			return 0;
		case WM_KEYUP:
			if (wParam == VK_ESCAPE)
			{
				PostQuitMessage(0);
			}
			else if ((int)wParam == VK_F2)
			{
				Set4xMsaaState(!m4xMsaaState);
			}

			return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

/**
 * \brief Initializes a window 
 * \return whether the initialization is a success
 */
bool D3DApp::InitMainWindow()
{
	// describe basic properties of the window by filling out a WNDCLASS(window class) structure
	WNDCLASS wc;
	wc.style         = CS_HREDRAW | CS_VREDRAW;            // the class style: H(orizontal)REDRAW and V(ertical)REDRAW: the window is to be repainted when either the horizontal or vertical window size is changed. (doesn't seem to change anything, btw)
	wc.lpfnWndProc   = MainWndProc;                        // Windows that are created based on this WNDCLASS instance will use MainWndProc as the window procedure. 
	wc.cbClsExtra    = 0;                                  // our program does not require any extra memory slots
	wc.cbWndExtra    = 0;                                  // our program does not require any extra memory slots
	wc.hInstance     = mhAppInst;                          // a handle to the application instance 
	wc.hIcon         = LoadIcon(0, IDI_APPLICATION);       // a handle to an icon 
	wc.hCursor       = LoadCursor(0, IDC_ARROW);           // a handle to a cursor 
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH); // a handle to a brush which specifies the background color for the client area of the window
	wc.lpszMenuName  = 0;                                  // the window's menu
	wc.lpszClassName = L"MainWnd";                         // the name of the window class structure we are creating. This name is used to identify the class structure for later use

	// register the window class so that we can create windows based on it
	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = {0, 0, mClientWidth, mClientHeight};
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width  = R.right - R.left;
	int height = R.bottom - R.top;

	mhMainWnd = CreateWindow(L"MainWnd",
	                         mMainWndCaption.c_str(),
	                         WS_OVERLAPPEDWINDOW,
	                         CW_USEDEFAULT,
	                         CW_USEDEFAULT,
	                         width,
	                         height,
	                         0,
	                         0,
	                         mhAppInst,
	                         0);

	if (!mhMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd); // https://stackoverflow.com/questions/11118443/why-we-need-to-call-updatewindow-following-showwindow

	return true;
}

void D3DApp::Query4XMSAAQualityLevel()
{
	// Query the number of quality levels for a given texture format and sample count
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format           = mBackBufferFormat;
	msQualityLevels.SampleCount      = 4;
	msQualityLevels.Flags            = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(
		              D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		              &msQualityLevels, // this parameter is both an input (format, sample count, flags) and output (NumQualityLevels)
		              sizeof(msQualityLevels)));
	m4xMsaaQuality = msQualityLevels.NumQualityLevels; //! Note: the valid quality levels range from [0, NumQualityLevels-1]
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");
}

bool D3DApp::InitDirect3D()
{
	#if defined(DEBUG) || defined(_DEBUG)
	// Enable the D3D12 debug layer.
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();
	}
	#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));

	// Try to create hardware device.
	HRESULT hardwareResult = D3D12CreateDevice(
	                                           nullptr,                // use the default adapter
	                                           D3D_FEATURE_LEVEL_11_0, // minimum D3D_FEATURE_LEVEL
	                                           IID_PPV_ARGS(&md3dDevice));

	// Fallback to WARP device.
	if (FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(mdxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));

		ThrowIfFailed(D3D12CreateDevice(
			              pWarpAdapter.Get(),
			              D3D_FEATURE_LEVEL_11_0,
			              IID_PPV_ARGS(&md3dDevice)));
	}

	// create CPU/GPU synchronization object 
	ThrowIfFailed(md3dDevice->CreateFence(
		              0, // a fence point in time starts at value 0 
		              D3D12_FENCE_FLAG_NONE,
		              IID_PPV_ARGS(&mFence)));

	// query descriptors' size
	mRtvDescriptorSize       = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); // GetDescriptorHandleIncrementSize() above allows applications to manually offset handles into a heap (producing handles into anywhere in a descriptor heap).
	mDsvDescriptorSize       = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV); // https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Check 4X MSAA quality support for our back buffer format.
	// All Direct3D 11 capable devices support 4X MSAA for all render 
	// target formats, so we only need to check quality support.
	Query4XMSAAQualityLevel();

	#ifdef _DEBUG
	// LogAdapters();
	#endif

	CreateCommandObjects();
	CreateSwapChain();
	CreateRtvAndDsvDescriptorHeaps();

	return true;
}

void D3DApp::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue))); //! usage of IID_PPV_ARGS: page 109

	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		              D3D12_COMMAND_LIST_TYPE_DIRECT,
		              IID_PPV_ARGS(&mDirectCmdListAlloc)));

	ThrowIfFailed(md3dDevice->CreateCommandList(
		              0, // For single-GPU operation, set this to zero
		              D3D12_COMMAND_LIST_TYPE_DIRECT,
		              mDirectCmdListAlloc.Get(), // initially, we associate the command list with mDirectCmdListAlloc
		              nullptr,                   // mCommandList is not used to submit draw commands (the one in frame resource does), thus we specify nullptr here
		              IID_PPV_ARGS(&mCommandList)));

	// Start off in a closed state.  This is because the first time we refer 
	// to the command list we will Reset it (in D3DApp::OnResize), and it needs to be closed before
	// calling Reset.
	mCommandList->Close();
}

/**
 * \brief Create a swap chain. This function can be called multiple times,
 * which allows us to recreate the swap chain with different settings at runtime
 */
void D3DApp::CreateSwapChain()
{
	// describe the swap chain first
	DXGI_SWAP_CHAIN_DESC sd;
	// filling DXGI_MODE_DESC: description of back buffer
	sd.BufferDesc.Width                   = mClientWidth;
	sd.BufferDesc.Height                  = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator   = 60; // TODO: Changing this value doesn't affect refresh rate, investigate 
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format                  = mBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
	// filling DXGI_SAMPLE_DESC: Multisampling
	sd.SampleDesc.Count   = m4xMsaaState ? 4 : 1;                    // # of samples to take per pixel 
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0; // specify the desired quality level (what "quality level" means can vary across hardware manufacturers)
	// back buffer usage
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // we're going to render to the back buffer (use it as a render target)
	// back buffer count
	sd.BufferCount  = SwapChainBufferCount; // two for double buffering
	sd.OutputWindow = mhMainWnd;
	sd.Windowed     = true;
	sd.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags        = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	// then, create the swap chain
	ThrowIfFailed(mdxgiFactory->CreateSwapChain(
		              mCommandQueue.Get(), // we pass a pointer to the command queue b/c swap chain need to submit to the queue the actual commands to display a buffer to the screen
		              &sd,
		              &mSwapChain)); // will release the previous swap chain
}

/**
 * \brief Force the CPU to wait until the GPU has finished processing all the commands in the queue up to a specified fence point
 * Page 114, Figure 4.8 is the key to understand this function
 */
void D3DApp::FlushCommandQueue()
{
	// Advance the fence value to mark commands up to this fence point.
	mCurrentFence++;

	// Add an instruction to the command queue to set a new fence point.  Because we 
	// are on the GPU timeline, the new fence point won't be set until the GPU finishes
	// processing all the commands prior to this Signal().
	ThrowIfFailed(mCommandQueue->Signal(mFence.Get(), mCurrentFence));

	// Wait until the GPU has completed commands up to this fence point.
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		// Fire event when GPU hits current fence.  
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrentFence, eventHandle));

		// Wait until the GPU hits current fence event is fired.
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

ID3D12Resource* D3DApp::CurrentBackBuffer() const
{
	return mSwapChainBuffer[mCurrBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::CurrentBackBufferView() const
{
	// CD3DX12 constructor to offset to the RTV of the current back buffer
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(), // retrieve the heap start locationís handle 
	                                     mCurrBackBuffer,                                // index to offset
	                                     mRtvDescriptorSize);                            // byte size of descriptor
}

/**
 * \brief Return the CPU descriptor handle that represents the start of the heap that holds the depth-stencil view 
 * \return the CPU descriptor handle
 */
D3D12_CPU_DESCRIPTOR_HANDLE D3DApp::DepthStencilView() const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3DApp::CalculateFrameStats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.

	static int   frameCnt    = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if ((mTimer.TotalTime() - timeElapsed) >= 1.0f)
	{
		float fps  = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;   // ms per frame

		wstring fpsStr  = to_wstring(fps);
		wstring mspfStr = to_wstring(mspf);

		wstring windowText = mMainWndCaption +
		                     L"    fps: " + fpsStr +
		                     L"   mspf: " + mspfStr;

		SetWindowText(mhMainWnd, windowText.c_str());

		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

/**
 * \brief Enumerate all the adapters on a system
 */
void D3DApp::LogAdapters()
{
	UINT                       i       = 0; // The index of the adapter to enumerate
	IDXGIAdapter*              adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (mdxgiFactory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND) // Enumerates the adapters (video cards).
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc); // Gets a DXGI 1.0 description of an adapter (or video card).

		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";

		OutputDebugString(text.c_str());

		adapterList.push_back(adapter);

		++i;
	}

	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

/**
 * \brief Enumerate all the outputs (monitor) associated with this adapter
 * \param adapter Display adapter
 */
void D3DApp::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT         i      = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);

		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());

		LogOutputDisplayModes(output, mBackBufferFormat);

		ReleaseCom(output);

		++i;
	}
}

/**
 * \brief Fixing a display mode format, print out a list of all supported display modes an output supports in that format
 * \param output Display output
 * \param format Display mode format
 * TODO: Enumerated display modes will become important when we implement full-screen support.
 * The specified full-screen display mode must match exactly one of the enumerated ones here to get optimal performance. 
 */
void D3DApp::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;

	// Call with nullptr to get list count.
	output->GetDisplayModeList(format, flags, &count, nullptr);

	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count, &modeList[0]);

	for (auto& x : modeList)
	{
		UINT         n    = x.RefreshRate.Numerator;
		UINT         d    = x.RefreshRate.Denominator;
		std::wstring text =
				L"Width = " + std::to_wstring(x.Width) + L" " +
				L"Height = " + std::to_wstring(x.Height) + L" " +
				L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
				L"\n";

		::OutputDebugString(text.c_str());
	}
}
