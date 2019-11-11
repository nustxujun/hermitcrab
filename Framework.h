#pragma once

#include "Common.h"
#include "Renderer.h"

class Framework
{
public:
	Framework();
	virtual ~Framework();

	void resize(int width, int height);
	void update();
private:
	HWND createWindow();
	void registerWindow();
	static LRESULT CALLBACK process(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	virtual void updateImpl() = 0;
private:
	HWND mWindow;
	std::string mWindowClass = "window";
	Renderer::Ptr mRenderer;
};