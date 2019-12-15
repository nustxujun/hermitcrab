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
#include <map>
#include <set>

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
#include <D3Dcompiler.h>


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
		_CrtDbgBreak();
		abort();
	}

	static void Assert(bool v, const std::wstring& what)
	{
		if (v)
			return;
		MessageBoxW(0, what.c_str(), 0, MB_ICONERROR);
		_CrtDbgBreak();
		abort();

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


using Vector3 = std::array<float, 3>;
using Vector4 = std::array<float, 4>;
using Color = std::array<float, 4>;
using Matrix = std::array<Vector4, 4>;

#define U2M Common::convert
#define M2U Common::convert

#undef min
#undef max