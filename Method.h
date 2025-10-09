#pragma once
#include <utility>
#include <Windows.h>

namespace method
{
	struct AddressRange
	{
		DWORD64 start;
		DWORD64 end;
	};

	extern HANDLE hConsole;

	void RedireceConsole();
	void PrintToConsole(const wchar_t* format, ...);
	void exit();


	/*
	* @brief 获取模块地址范围
	* @param hModule 模块句柄
	* @return 返回一个包含模块起始地址和结束地址的pair
	*/
	std::pair<DWORD64, DWORD64> GetModuleAddressRange(HMODULE hModule);
	BOOL CompareArrays(const BYTE* source, const BYTE* target, size_t size);
	size_t ConvertStringToByteArray(const char* pattern, BYTE* outBuffer, size_t maxSize);
	DWORD64 LocateSignature(HANDLE hProcess, const char* maskedPattern, DWORD64 start, DWORD64 end, int offset);

	UINT GetIntPrivateProfile(LPCWSTR lpAppName, LPCWSTR lpKeyName, INT nDefault);
	DWORD GetStringPrivateProfile(LPCWSTR lpAppName, LPCWSTR lpKeyName, LPCWSTR lpDefault, LPWSTR lpReturnedString, DWORD nSize);
	BOOL WriteStringPrivateProfile(LPCWSTR lpAppName, LPCWSTR lpKeyName, LPCWSTR lpString);
}
