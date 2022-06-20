#pragma once

#if defined(DEBUG) || defined(_DEBUG)
// Enable memory leak detection: https://docs.microsoft.com/en-us/visualstudio/debugger/finding-memory-leaks-using-the-crt-library?view=vs-2022
#define _CRTDBG_MAP_ALLOC // CRT: C Run Time Library
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

	HINSTANCE AppInst() const;
	HWND      MainWnd() const;
	float     AspectRatio() const;

	// bool Get4xMsaaState() const;
	// void Set4xMsaaState(bool value);

	int Run();

	virtual bool    Initialize();
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	virtual void CreateRtvAndDsvDescriptorHeaps();
	virtual void OnResize(); //! client may need to override this (to recompute projection matrix, for example)
	virtual void Update(const GameTimer& gt) =0;
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
	bool InitMainWindow();
	bool InitDirect3D();
	void CreateCommandObjects();
	void CreateSwapChain();

	void FlushCommandQueue();

	ID3D12Resource*             CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	void CalculateFrameStats();

	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);
// private:
// 	void Query4XMSAAQualityLevel();
protected:
	static D3DApp* mApp;
	HINSTANCE      mhAppInst        = nullptr; // application instance handle
	HWND           mhMainWnd        = nullptr; // main window handle
	bool           mAppPaused       = false;   // is the application paused?
	bool           mMinimized       = false;   // is the application minimized?
	bool           mMaximized       = false;   // is the application maximized?
	bool           mResizing        = false;   // are the resize bars being dragged?
	bool           mFullscreenState = false;   // fullscreen enabled

	// TODO: MSAA must be set to false. We must create an MSAA render target that we explicitly resolve in D3D 12. https://stackoverflow.com/q/56286975/13795171
	// quality level of 4X MSAA, range from [0, NumQualityLevels-1]

	//bool                                              m4xMsaaState     = false;   
	//UINT                                              m4xMsaaQuality   = 0;      

	GameTimer                                         mTimer;       // Used to keep track of the “delta-time” and game time
	Microsoft::WRL::ComPtr<IDXGIFactory4>             mdxgiFactory; // IDXGIFactory4 is used to create our swap chain and a WARP adapter if necessary
	Microsoft::WRL::ComPtr<IDXGISwapChain>            mSwapChain;   // swap chain in D3D is represented by IDXGISwapChain. It stores the front and back buffer textures, and provides methods for resizing and presenting. 
	Microsoft::WRL::ComPtr<ID3D12Device>              md3dDevice;
	Microsoft::WRL::ComPtr<ID3D12Fence>               mFence;            // fence is used to sync GPU and CPU
	UINT64                                            mCurrentFence = 0; // a fence object maintains a UINT64 value, which is an integer to identify a fence point in time. Every time we need to mark a new fence point, we increment the integer. 
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>        mCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    mDirectCmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList;
	static const int                                  SwapChainBufferCount = 2;
	int                                               mCurrBackBuffer      = 0; // we need to track which buffer is the current back buffer so we know which one to render to 
	Microsoft::WRL::ComPtr<ID3D12Resource>            mSwapChainBuffer[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource>            mDepthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      mRtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      mDsvHeap;
	D3D12_VIEWPORT                                    mScreenViewport;
	D3D12_RECT                                        mScissorRect;
	UINT                                              mRtvDescriptorSize       = 0; // working with descriptors requires us to now their size, but their sizes vary across GPUs so we need to query this information and cache them so that it is available when we need it for various descriptor types
	UINT                                              mDsvDescriptorSize       = 0;
	UINT                                              mCbvSrvUavDescriptorSize = 0;
	std::wstring                                      mMainWndCaption          = L"d3d App";                    // Derived class should set these in derived constructor to customize starting values.
	D3D_DRIVER_TYPE                                   md3dDriverType           = D3D_DRIVER_TYPE_HARDWARE;      // Derived class should set these in derived constructor to customize starting values.
	DXGI_FORMAT                                       mBackBufferFormat        = DXGI_FORMAT_R8G8B8A8_UNORM;    // Derived class should set these in derived constructor to customize starting values.
	DXGI_FORMAT                                       mDepthStencilFormat      = DXGI_FORMAT_D24_UNORM_S8_UINT; // Derived class should set these in derived constructor to customize starting values.
	int                                               mClientWidth             = 800;                           // Derived class should set these in derived constructor to customize starting values.
	int                                               mClientHeight            = 600;                           // Derived class should set these in derived constructor to customize starting values.
};
