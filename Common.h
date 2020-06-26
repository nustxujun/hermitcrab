#pragma once




//stl 
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <algorithm>
#include <functional>
#include <memory>
#include <fstream>
#include <regex>
#include <unordered_map>
#include <map>
#include <set>
#include <sstream>
#include <iostream>
#include <bitset>
#include <chrono>

//#if defined(NO_UE4) || defined(_CONSOLE)
#include <Windows.h>
//#else
//#include "Windows/MinWindows.h"
//#endif
#include <wrl/client.h>

// d3d
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>

// break asio error
#include <WinSock2.h>

// macros
#define ALIGN(x,y) (((x + y - 1) & ~(y - 1)) )


template<class T>
using ComPtr = Microsoft::WRL::ComPtr<T>;


class Common
{
public :

	template<class ... Args>
	static void log(const Args& ... args)
	{
		auto context = format(args ...) + "\n";
		OutputDebugStringA(context.c_str());
		std::cout << context;
		//_CrtDbgBreak();
		//::MessageBoxA(NULL, context.c_str(), NULL, NULL);
	}

	static void checkResult(HRESULT hr, std::string_view info = {});

	static void Assert(bool v, const std::string& what);

	template<class T, class ... Args>
	static std::string format(const T& v, Args&& ... args)
	{
		std::stringstream ss;
		ss << v << format(args...);
		return ss.str();
	}

	static std::string convert(const std::wstring& str);

	static std::wstring convert(const std::string& str);

private:
	static std::string format()
	{
		return {};
	}
};


using Vector2 = std::array<float, 2>;
using Vector3 = std::array<float, 3>;
using Vector4 = std::array<float, 4>;
using Color = std::array<float, 4>;
using Matrix = std::array<Vector4, 4>;
using float2 = Vector2;
using float3 = Vector3;
using float4 = Vector4;
using matrix = Matrix;

struct AABB
{
	Vector3 center;
	Vector3 extent;
};

#define U2M Common::convert
#define M2U Common::convert

#undef min
#undef max


#undef CHECK
#undef ASSERT
#undef LOG

#if _DEBUG
#define CHECK(x) Common::checkResult(x, Common::format(" file: ",__FILE__, " line: ", __LINE__ ))
#define ASSERT(x,y) Common::Assert(x, Common::format(y, " file: ", __FILE__, " line: ", __LINE__ ))
#define CHECK_RENDER_THREAD {Common::Assert(Thread::getId() == 0, "need running on main thread.");}
#else
#define CHECK(x) x;
#define ASSERT(x,y) 
#define CHECK_RENDER_THREAD 
#endif

#define LOG Common::log