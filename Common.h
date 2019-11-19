#pragma once

//stl 
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <functional>
#include <memory>
#include <fstream>
#include <regex>
#include <unordered_map>

// windows
#if defined(NO_UE4) || defined(_CONSOLE)
#include <Windows.h>
#else
#include "Windows/MinWindows.h"
#endif
#include <wrl/client.h>

// d3d
#include <d3d12.h>
#include <dxgi1_4.h>


// macros
#define ALIGN(x,y) (((x + y - 1) & ~(y - 1)) )

template<class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;


class Common
{
public :
	static void checkResult(HRESULT hr)
	{
		if (hr == S_OK) return;
	
	
		TCHAR msg[1024] = { 0 };
		FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, hr, 0, msg, sizeof(msg), 0);
		MessageBox(NULL, msg, NULL, MB_ICONERROR);
		abort();
	}

	static void Assert(bool v, const std::string& what)
	{
		if (v)
			return;
		MessageBoxA(0, what.c_str(), 0, MB_ICONERROR);
		abort();

	}
};

using Vector3 = std::array<float, 3>;
using Vector4 = std::array<float, 4>;
using Color = std::array<float, 4>;
using Matrix = std::array<Vector4, 4>;

