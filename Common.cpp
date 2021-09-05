#include "Common.h"
#include "Renderer.h"

void Common::checkD3DResult(HRESULT hr, const std::string& info)
{
	checkResult(Renderer::getSingleton()->getDevice()->GetDeviceRemovedReason(), info);
}

void Common::checkWindowsResult(HRESULT hr, const std::string& info)
{
	char msg[1024] = { 0 };
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, hr, 0, msg, sizeof(msg), 0);
	std::cout << msg << " " << info;
	OutputDebugStringA(msg);
	MessageBoxA(NULL, msg, NULL, MB_ICONERROR);
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
