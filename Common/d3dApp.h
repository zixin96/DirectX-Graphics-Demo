#pragma once

#if defined(DEBUG) || defined(_DEBUG)
// Find memory leaks with the CRT library: https://docs.microsoft.com/en-us/visualstudio/debugger/finding-memory-leaks-using-the-crt-library?view=vs-2022
#define _CRTDBG_MAP_ALLOC 
#include <crtdbg.h>
#endif

#include "d3dUtil.h"
#include "GameTimer.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

/**
 * \brief Base D3D application class
 */
class D3DApp
{
protected:
	D3DApp(HINSTANCE hInstance);
	D3DApp(const D3DApp& rhs)            = delete;
	D3DApp& operator=(const D3DApp& rhs) = delete;
	virtual ~D3DApp();

public:
	static D3DApp* GetApp();
	HINSTANCE      AppInst() const;
	HWND           MainWnd() const;
	float          AspectRatio() const;
	int            Run();

	// This method does basic initialization for a D3D app.
	// Clients may need to override this: D3DApp::Initialize() + application-specific init code
	virtual bool Initialize();

	// This method implements the window procedure for the main application window.
	// You rarely need to override this function unless there is a message you need to handle that this function doesn't handle.
	// If you override this function, any messages that you do not handle should be forwarded to this function.
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	// Create the render target and depth stencil descriptor heaps to store the descriptors/views
	// Clients may need to override this for more advanced rendering techniques
	virtual void CreateRtvAndDsvDescriptorHeaps();

	// Resize the swap chain if the window is resized
	// Clients may need to override this: D3DApp::OnResize() + application-specific init code
	virtual void OnResize();

	// this method is called every frame to update the 3D application over time:
	// perform animations, move the camera, do collision detection, check for user-input, etc
	virtual void Update(const GameTimer& gt) =0;

	// this method is called every frame to issue rendering commands
	virtual void Draw(const GameTimer& gt) =0;

	// Convenience overrides for handling mouse input.
	virtual void OnMouseDown(WPARAM btnState, int x, int y)
	{
	}

	virtual void OnMouseUp(WPARAM btnState, int x, int y)
	{
	}

	virtual void OnMouseMove(WPARAM btnState, int x, int y)
	{
	}

protected:
	bool            InitMainWindow();
	bool            InitDirect3D();
	void            CreateCommandObjects();
	void            CreateSwapChain();
	void            FlushCommandQueue();
	ID3D12Resource* CurrentBackBuffer() const;

	// access descriptors in the heap
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	void CalculateFrameStats();

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);
protected:
	static D3DApp* mApp;
	HINSTANCE      mhAppInst        = nullptr; // application instance handle
	HWND           mhMainWnd        = nullptr; // main window handle
	bool           mAppPaused       = false;   // is the application paused?
	bool           mMinimized       = false;   // is the application minimized?
	bool           mMaximized       = false;   // is the application maximized?
	bool           mResizing        = false;   // are the resize bars being dragged?
	bool           mFullscreenState = false;   // fullscreen enabled

	// GameTimer is used to keep track of the “delta-time” and game time
	GameTimer mTimer;

	// IDXGIFactory4 is used to create our swap chain and a WARP adapter if necessary
	Microsoft::WRL::ComPtr<IDXGIFactory4> mdxgiFactory;

	// ID3D12Device is used to create D3D objects
	Microsoft::WRL::ComPtr<ID3D12Device> md3dDevice;

	// --------------D3D12 Sync CPU/GPU---------------
	Microsoft::WRL::ComPtr<ID3D12Fence> mFence;            // fence is used to sync GPU and CPU
	UINT64                              mCurrentFence = 0; // a fence object maintains a UINT64 value, which is an integer to identify a fence point in time. Every time we need to mark a new fence point, we increment the integer. 
	// -----------End D3D12 Sync CPU/GPU---------------

	// --------------D3D12 Command Objects---------------
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>        mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
	// -----------End D3D12 Command Objects---------------

	Microsoft::WRL::ComPtr<IDXGISwapChain>       mSwapChain; // swap chain stores the front and back buffer textures, and provides methods for resizing and presenting. 
	static const int                             SWAP_CHAIN_BUFFER_COUNT = 2;
	int                                          mCurrBackBuffer         = 0; // we need to track which buffer is the current back buffer so we know which one to render to 
	Microsoft::WRL::ComPtr<ID3D12Resource>       mSwapChainBuffer[SWAP_CHAIN_BUFFER_COUNT];
	Microsoft::WRL::ComPtr<ID3D12Resource>       mDepthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap;
	UINT                                         mRtvDescriptorSize       = 0;
	UINT                                         mDsvDescriptorSize       = 0;
	UINT                                         mCbvSrvUavDescriptorSize = 0;
	DXGI_FORMAT                                  mBackBufferFormat        = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT                                  mDepthStencilFormat      = DXGI_FORMAT_D24_UNORM_S8_UINT;

	D3D12_VIEWPORT mScreenViewport;
	D3D12_RECT     mScissorRect;

	std::wstring    mMainWndCaption = L"d3d App";
	D3D_DRIVER_TYPE md3dDriverType  = D3D_DRIVER_TYPE_HARDWARE;
	int             mClientWidth    = 800;
	int             mClientHeight   = 600;
};
