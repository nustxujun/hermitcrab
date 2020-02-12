#pragma once

#include "Common.h"
#include "Renderer.h"

class Framework
{
public:
	Framework();
	virtual ~Framework();
	void initialize();

	void resize(int width, int height);
	void update();

	static void setProcessor(const std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)>& f);
private:
	HWND createWindow();
	void registerWindow();
	static LRESULT CALLBACK process(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static void resize(HWND hwnd, int width, int height);
	virtual void updateImpl() = 0;
private:
	HWND mWindow;
	std::wstring mWindowClass = L"_frame_window";
	Renderer::Ptr mRenderer;
	static bool needPaint;

	static std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> processor;
};