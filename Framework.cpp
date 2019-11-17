#include "Framework.h"

Framework::Framework()
{
	CreateDirectoryA("cache/", NULL);

	registerWindow();
	createWindow();
	mRenderer = Renderer::create();
	mRenderer->initialize(mWindow);

	resize(1600, 900);
}

Framework::~Framework()
{
	Renderer::destory();
}

void Framework::resize(int width, int height)
{
	resize(mWindow, width, height);
}

void Framework::update()
{
	MSG msg = {};
	while (WM_QUIT != msg.message && WM_CLOSE != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			updateImpl();
			mRenderer->onRender();
		}
	}
}

HWND Framework::createWindow()
{
	HINSTANCE instance = ::GetModuleHandle(NULL);

	mWindow = CreateWindowW(mWindowClass.c_str(), L"", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, instance, nullptr);

	ShowWindow(mWindow, true);
	UpdateWindow(mWindow);

	return mWindow;
}

void Framework::registerWindow()
{
	HINSTANCE instance = ::GetModuleHandle(NULL);

	WNDCLASSEXW wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = process;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = instance;
	wcex.hIcon = NULL;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = mWindowClass.c_str();
	wcex.hIconSm = NULL;

	RegisterClassExW(&wcex);
}

LRESULT Framework::process(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CLOSE:
	case WM_DESTROY:
		{
			PostMessage(hWnd, WM_CLOSE, 0, 0);
		}
		return 0;
	case WM_SIZE:
		if (wParam == SIZE_RESTORED)
		{
			auto w = LOWORD(lParam); 
			auto h = HIWORD(lParam); 

			resize(hWnd, w,h);
		}
		return 0 ;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}

void Framework::resize(HWND hwnd, int width, int height)
{
	RECT win, client;
	::GetClientRect(hwnd, &client);
	::GetWindowRect(hwnd, &win);

	auto w = win.right - win.left - client.right;
	auto h = win.bottom - win.top - client.bottom;

	::MoveWindow(hwnd, win.left, win.top, w + width, h + height, FALSE);
	auto r = Renderer::getSingleton();
	if (r)
		Renderer::getSingleton()->resize(width, height);
}
