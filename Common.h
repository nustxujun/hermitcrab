#pragma once

// std::wstring_convert is deprecated in c++17, but there is no easy way to replace it. 
// display warnings and remain using it
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING


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
#include <map>
#include <set>
#include <sstream>
#include <iostream>
#include <bitset>
#include <chrono>

#if defined(NO_UE4) || defined(_CONSOLE)
#include <Windows.h>
#else
#include "Windows/MinWindows.h"
#endif
#include <wrl/client.h>

// d3d
#include <d3d12.h>
#include <dxgi1_4.h>
#include <D3Dcompiler.h>


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
	}

	static void checkResult(HRESULT hr, const std::string& info = {})
	{
		if (hr == S_OK) return;
	
	
		char msg[1024] = { 0 };
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, hr, 0, msg, sizeof(msg), 0);
		std::cout << Common::format(msg, info) ;
		MessageBoxA(NULL, Common::format(msg, info).c_str(), NULL, MB_ICONERROR);
		_CrtDbgBreak();
		//abort();
		throw std::exception("terminate client");
	}

	static void Assert(bool v, const std::string& what)
	{
		if (v)
			return;
		std::cout << what;
		MessageBoxA(0, what.c_str(), 0, MB_ICONERROR);
		_CrtDbgBreak();
		//abort();
		throw std::exception("terminate client");
	}

	template<class T, class ... Args>
	static std::string format(const T& v, Args&& ... args)
	{
		std::stringstream ss;
		ss << v << format(args...);
		return ss.str();
	}

	static std::string convert(const std::wstring& str)
	{
		std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>>
			converter(new std::codecvt<wchar_t, char, std::mbstate_t>("CHS"));
		return converter.to_bytes(str);
	}

	static std::wstring convert(const std::string& str)
	{
		std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>>
			converter(new std::codecvt<wchar_t, char, std::mbstate_t>("CHS"));
		return converter.from_bytes(str);
	}

private:
	static std::string format()
	{
		return {};
	}
};

template<class T>
class UpValue
{
public:
	UpValue()
	{
		mValue = std::shared_ptr<T>(new T);
	};

	//template<class ... Args>
	//UpValue(Args&& ... args)
	//{
	//	mValue = std::shared_ptr<T>(new T(std::forward<Args>(args)...));
	//}

	UpValue(UpValue&& uv) :
		mValue(uv.mValue)
	{
	}
	UpValue(const UpValue& uv) :
		mValue(uv.mValue)
	{
	}

	void operator=(const T& v)
	{
		*mValue	= v;
	}

	T* operator->()
	{
		return mValue.get();
	}

	operator T&()
	{
		return *mValue.get();
	}

	T& get()
	{
		return *mValue.get();
	}
private:
	std::shared_ptr<T> mValue;
};

using Vector2 = std::array<float, 2>;
using Vector3 = std::array<float, 3>;
using Vector4 = std::array<float, 4>;
using Color = std::array<float, 4>;
using Matrix = std::array<Vector4, 4>;
struct AABB
{
	Vector3 center;
	Vector3 extent;
};

#define U2M Common::convert
#define M2U Common::convert

#undef min
#undef max