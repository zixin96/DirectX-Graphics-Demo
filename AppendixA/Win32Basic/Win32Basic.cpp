// Include the windows header file; this has all the Win32 API
// structures, types, and function declarations we need to program
// Windows.
#include <Windows.h>

// The main window handle; this is used to identify a
// created window. 
HWND ghMainWnd = nullptr; // "global handle to main window"

// Wraps the code necessary to initialize a Windows
// application. Function returns true if initialization
// was successful, otherwise it returns false.
bool InitWindowsApp(HINSTANCE instanceHandle, int show);

// Wraps the message loop code.
int Run();

// The window procedure handles events our window receives.
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Windows equivalent to main()
// WINAPI: having this calling convention after the return type matters when you are calling a function outside of your code (e.g. an OS API) or the OS is calling you (as is the case here with WinMain/WndProc,etc) https://stackoverflow.com/questions/297654/what-is-stdcall
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd) // SW_SHOWDEFAULT (https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow) is passed when the application starts 
{
	// First call our wrapper function (InitWindowsApp) to create
	// and initialize the main application window, passing in the
	// hInstance and nShowCmd values as arguments.
	if (!InitWindowsApp(hInstance, nShowCmd))
	{
		// if WinMain is not entering the message loop, WinMain should return 0 [p. 762]
		return 0;
	}

	// Once our application has been created and initialized we
	// enter the message loop. We stay in the message loop until
	// a WM_QUIT message is received, indicating the application
	// should be terminated.
	return Run();
}

bool InitWindowsApp(HINSTANCE instanceHandle, int show)
{
	// The first task to creating a window is to describe some of its
	// characteristics by filling out a WNDCLASS structure.
	WNDCLASS wc;
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = instanceHandle;
	wc.hIcon         = LoadIcon(nullptr, IDI_EXCLAMATION);  // change icon here
	wc.hCursor       = LoadCursor(nullptr, IDC_HAND);       // change cursor here
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH); // change background color here
	wc.lpszMenuName  = nullptr;
	wc.lpszClassName = L"BasicWndClass";
	// Next, we register this WNDCLASS instance with Windows so
	// that we can create a window based on it.
	if (!RegisterClass(&wc))
	{
		MessageBox(nullptr,                 // the message box has no owner window
		           L"RegisterClass FAILED", // The message to be displayed
		           nullptr,                 // the title is Error
		           MB_OK);                  // same as specifying 0
		return false;
	}
	// With our WNDCLASS instance registered, we can create a
	// window with the CreateWindow function. This function
	// returns a handle to the window it creates (an HWND).
	// If the creation failed, the handle will have the value
	// of zero. A window handle is a way to refer to the window,
	// which is internally managed by Windows. Many of the Win32 API
	// functions that operate on windows require an HWND so that
	// they know what window to act on.
	ghMainWnd = CreateWindow(L"BasicWndClass",            // Registered WNDCLASS instance to use.
	                         L"DirectX 12 Graphics Demo", // window title
	                         WS_OVERLAPPEDWINDOW,         // style flags
	                         CW_USEDEFAULT,               // x-coordinate
	                         CW_USEDEFAULT,               // y-coordinate
	                         CW_USEDEFAULT,               // width
	                         CW_USEDEFAULT,               // height
	                         0,                           // parent window
	                         0,                           // menu handle
	                         instanceHandle,              // app instance
	                         0);                          // extra creation parameters
	if (ghMainWnd == nullptr)
	{
		MessageBox(nullptr, L"CreateWindow FAILED", nullptr, 0);
		return false;
	}
	// Even though we just created a window, it is not initially
	// shown. Therefore, the final step is to show and update the
	// window we just created, which can be done with the following
	// two function calls. Observe that we pass the handle to the
	// window we want to show and update so that these functions know
	// which window to show and update.
	ShowWindow(ghMainWnd, show);
	UpdateWindow(ghMainWnd); // after showing the window, we should refresh it [p. 767]
	return true;
}

// #define OFFICE_APP

int Run()
{
	MSG msg = {nullptr};

	#ifdef OFFICE_APP
	// Loop until we get a WM_QUIT message.
	// The function GetMessage will only return 0 (false) when a WM_QUIT message is received, which effectively exits the loop.
	// The function returns -1 if there is an error. In this case, we should display a message box and exit the message loop
	// Also, note that GetMessage puts the application thread to sleep until there is a message.
	BOOL bRet = 1;
	while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0)
	{
		if (bRet == -1)
		{
			MessageBox(nullptr, L"GetMessage FAILED", L"Error", MB_OK);
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg); // OS will call our custom window procedure, we DO NOT call WndProc ourselves
		}
		OutputDebugString(L"Games!!!\n");
	}
	#else
	// Games require a special message loop 
	while (msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		// PeekMessage returns false if there are no messages
		if (PeekMessage(&msg, // message to fill
		                0,
		                0,
		                0,
		                PM_REMOVE)) // message will be removed from the queue after processing by PeekMesssage
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			// this line will execute if there are no Windows messages to be processed.
			// This implies that we will handle all events before executing the game logic: 
			// OutputDebugString(L"Games!!!\n");
		}
	}
	#endif

	// return the wParam member of the WM_QUIT message [p. 762]
	return (int)msg.wParam;
}

// CALLBACK: this function is a callback function, which means that Windows will be calling this function outside of the code space of the program
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Handle some specific messages. Note that if we handle a message, we should return 0.
	switch (msg)
	{
		// In the case the left mouse button was pressed,
		// then display a message box.
		case WM_LBUTTONDOWN:
			MessageBox(nullptr, L"Hello, World", L"Hello", MB_OK);
			return 0;

		// In the case the escape key was pressed, then
		// destroy the main application window.
		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE)
				DestroyWindow(ghMainWnd); // this will generate a WM_DESTROY message
			return 0;
		// In the case of a destroy message, then send a
		// quit message, which will terminate the message loop.
		case WM_DESTROY:
			PostQuitMessage(0); // this will generate a WM_QUIT message
			return 0;
		// Exercise 2: 
		case WM_CLOSE:
			{
				int result = MessageBox(nullptr, L"Do you really want to exit?", L"Exit?", MB_YESNO);
				if (result == IDYES)
				{
					DestroyWindow(ghMainWnd);
				}
				return 0;
			}
		// Exercise 3: 
		case WM_CREATE:
			MessageBox(nullptr, L"The window has been created.", nullptr, 0);
			return 0;
	}
	// Forward any other messages we did not handle above to the
	// default window procedure. Note that our window procedure
	// must return the return value of DefWindowProc.
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
