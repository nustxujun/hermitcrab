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
#include <format>

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
	static void log(std::string_view cont, Args && ... args)
	{
		auto str = std::format(cont,std::forward<Args>(args) ...);
		OutputDebugStringA(str.c_str());
		std::cout << str << std::endl;
	}

	template<class ... Args>
	static void error(std::string_view cont, Args && ... args)
	{
		auto str = std::format(cont,std::forward<Args>(args) ...);
		::MessageBoxA(NULL, str.c_str(), NULL, NULL);
		std::cout << str;
		_CrtDbgBreak();
	}

	template<class ... Args>
	static void checkResult(HRESULT hr, Args && ... args)
	{
		if (hr == S_OK) return;

		auto str = std::format(std::forward<Args>(args) ...);
		if (hr == 0x887a0005)
		{
			checkD3DResult(hr, str);
		}
		else
		{
			checkWindowsResult(hr, str);
		}

	}

	template<class ... Args>
	static void Assert(bool v, Args && ... args)
	{
		if (!v) 
		{
			error(std::forward<Args>(args)...);
		}
	}


	static std::string convert(const std::wstring& str);

	static std::wstring convert(const std::string& str);

	static void hash_combine(std::size_t& seed) { }

	template <typename T, typename... Rest>
	static void hash_combine(std::size_t& seed, const T& v, Rest... rest) {
		std::hash<T> hasher;
		seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		hash_combine(seed, rest...);
	}
private:
	static void checkD3DResult(HRESULT hr, const std::string& info);
	static void checkWindowsResult(HRESULT hr, const std::string& info);
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
#define LOG Common::log
#if _DEBUG
#define CHECK(x) Common::checkResult(x, " file: {}, line: {}",__FILE__, __LINE__ );
#define ASSERT(x,y, ...) Common::Assert(x,y, __VA_ARGS__);
#define WARN(x, ...) ASSERT(false,x, __VA_ARGS__)
#define CHECK_RENDER_THREAD {Common::Assert(Thread::getId() == 0, "need running on main thread.");}

#else
#define CHECK(x) x;
#define ASSERT(...) 
#define WARN(x) LOG(x)
#define CHECK_RENDER_THREAD 
#endif

