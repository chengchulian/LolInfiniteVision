#pragma once

#include <utility>
#include <Windows.h>

namespace method
{
    extern HANDLE hConsole;

    void RedireceConsole();
    void PrintToConsole(const wchar_t* format, ...);
    void exit();

    std::pair<DWORD64, DWORD64> GetModuleAddressRange(HMODULE hModule);
    BOOL CompareArrays(const BYTE* source, const BYTE* target, size_t size);
    size_t ConvertStringToByteArray(const char* pattern, BYTE* outBuffer, size_t maxSize);
    DWORD64 LocateSignature(HANDLE hProcess, const char* maskedPattern, DWORD64 start, DWORD64 end, int offset);

    UINT GetIntPrivateProfile(LPCWSTR lpAppName, LPCWSTR lpKeyName, INT nDefault);
}
