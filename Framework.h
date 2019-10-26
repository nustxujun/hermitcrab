#pragma once

#include "Common.h"
#include "Renderer.h"

class Framework
{
public:
	Framework();
	~Framework();

	void resize(int width, int height);
	void update();
private:
	HWND createWindow();
	void registerWindow();
	static LRESULT CALLBACK process(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	HWND mWindow;
	std::string mWindowClass = "window";
	Renderer::Ptr mRenderer;
};