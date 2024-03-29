#pragma once

#include "Common.h"
#include "Renderer.h"
#include "Thread.h"
class Framework
{
public:
	Framework();
	virtual ~Framework();
	void initialize();

	void resize(int width, int height);
	static void resize(HWND hwnd, int width, int height);

	void update();
	std::pair<int, int> getSize();
	static void setProcessor(const std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)>& f);
	HWND getWindow()const{return mWindow;}
private:
	HWND createWindow();
	void registerWindow();
	static LRESULT CALLBACK process(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	virtual void updateImpl() = 0;
private:
	HWND mWindow;
	std::wstring mWindowClass = L"_frame_window";
	Renderer::Ptr mRenderer;

	std::vector<Thread> mThread;

	static bool needPaint;
	static std::function<LRESULT(HWND, UINT, WPARAM, LPARAM)> processor;
};