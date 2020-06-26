#include "Common.h"
#include "Renderer.h"

void Common::checkResult(HRESULT hr, std::string_view info)
{
	if (hr == S_OK) return;

	if (hr == 0x887a0005)
	{
		CHECK(Renderer::getSingleton()->getDevice()->GetDeviceRemovedReason());
		return ;
	}

	char msg[1024] = { 0 };
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, hr, 0, msg, sizeof(msg), 0);
	std::cout << Common::format(msg, info);
	OutputDebugStringA(msg);
	MessageBoxA(NULL, Common::format(msg, info).c_str(), NULL, MB_ICONERROR);
	_CrtDbgBreak();
	//abort();
	throw std::exception("terminate client");
}

void Common::Assert(bool v, const std::string& what)
{
	if (v)
		return;
	std::cout << what;
	OutputDebugStringA(what.c_str());
	MessageBoxA(0, what.c_str(), 0, MB_ICONERROR);
	_CrtDbgBreak();
	//abort();
	throw std::exception("terminate client");
}

std::string Common::convert(const std::wstring& str)
{
	std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>>
		converter(new std::codecvt<wchar_t, char, std::mbstate_t>("CHS"));
	return converter.to_bytes(str);
}

std::wstring Common::convert(const std::string& str)
{
	std::wstring_convert<std::codecvt<wchar_t, char, std::mbstate_t>>
		converter(new std::codecvt<wchar_t, char, std::mbstate_t>("CHS"));
	return converter.from_bytes(str);
}
