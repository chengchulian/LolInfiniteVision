#include "Method.h"

#include <Psapi.h>
#include <cwchar>

#include "Constant.h"
#include "Memory.h"

namespace method
{
    static bool console = false;

    HANDLE hConsole = nullptr;

    void RedireceConsole()
    {
        if (console) return;

        AllocConsole();
        SetConsoleOutputCP(CP_UTF8);

        hConsole = CreateFileW(
            L"CONOUT$",
            GENERIC_WRITE,
            FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (hConsole != INVALID_HANDLE_VALUE)
        {
            SetStdHandle(STD_OUTPUT_HANDLE, hConsole);
            console = true;
        }
        else
        {
            hConsole = nullptr;
        }
    }

    void PrintToConsole(const wchar_t* format, ...)
    {
        if (!console || hConsole == nullptr) return;

        va_list args;
        va_start(args, format);

        WCHAR buffer[512];
        wmemcpy(buffer, APP_CODE, 512);
        _vsnwprintf_s(buffer + 5, 507, _TRUNCATE, format, args);

        va_end(args);

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);

        DWORD written = 0;
        WriteConsoleW(hConsole, buffer, static_cast<DWORD>(wcslen(buffer)), &written, nullptr);
        WriteConsoleW(hConsole, L"\n", 1, &written, nullptr);

        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    }

    std::pair<DWORD64, DWORD64> GetModuleAddressRange(HMODULE hModule)
    {
        MODULEINFO moduleInfo;
        if (GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo)))
        {
            const DWORD_PTR baseAddress = reinterpret_cast<DWORD_PTR>(moduleInfo.lpBaseOfDll);
            const DWORD moduleSize = moduleInfo.SizeOfImage;

            const DWORD64 startAddress = static_cast<DWORD64>(baseAddress);
            const DWORD64 endAddress = startAddress + static_cast<DWORD64>(moduleSize);

            return std::make_pair(startAddress, endAddress);
        }

        return std::make_pair(0, 0);
    }

    BOOL CompareArrays(const BYTE* source, const BYTE* target, size_t size)
    {
        for (size_t i = 0; i < size; ++i)
        {
            if (source[i] != target[i] && source[i] != 0xFF) return FALSE;
        }

        return TRUE;
    }

    static BYTE HexCharToByte(char c)
    {
        if (c >= '0' && c <= '9') return static_cast<BYTE>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<BYTE>(10 + (c - 'a'));
        if (c >= 'A' && c <= 'F') return static_cast<BYTE>(10 + (c - 'A'));
        return 0;
    }

    size_t ConvertStringToByteArray(const char* pattern, BYTE* outBuffer, size_t maxSize)
    {
        size_t count = 0;
        for (size_t i = 0; pattern[i] && count < maxSize; ++i)
        {
            if (pattern[i] == ' ') continue;

            if (pattern[i] == '?' && pattern[i + 1] == '?')
            {
                outBuffer[count++] = 0xFF;
                ++i;
            }
            else
            {
                outBuffer[count++] = static_cast<BYTE>(
                    (HexCharToByte(pattern[i]) << 4) | HexCharToByte(pattern[i + 1]));
                ++i;
            }
        }

        return count;
    }

    DWORD64 LocateSignature(HANDLE hProcess, const char* maskedPattern, DWORD64 start, DWORD64 end, int offset)
    {
        constexpr size_t kMaxPatternSize = 256;
        constexpr size_t kPageSize = 4096;

        BYTE page[kPageSize] = {0};
        BYTE pattern[kMaxPatternSize] = {0};

        const size_t patternSize = ConvertStringToByteArray(maskedPattern, pattern, kMaxPatternSize);
        if (patternSize == 0) return 0;

        while (start < end)
        {
            if (!memory::ReadRaw(hProcess, start, page, kPageSize, nullptr))
            {
                start += kPageSize;
                continue;
            }

            for (size_t i = 0; i <= kPageSize - patternSize; ++i)
            {
                if (CompareArrays(pattern, page + i, patternSize))
                {
                    return start + i + offset;
                }
            }

            start += kPageSize - patternSize;
        }

        return 0;
    }

    UINT GetIntPrivateProfile(LPCWSTR lpAppName, LPCWSTR lpKeyName, INT nDefault)
    {
        return GetPrivateProfileIntW(lpAppName, lpKeyName, nDefault, CONFIG_PATH);
    }

    void exit()
    {
        if (console)
        {
            FreeConsole();
            console = false;
            hConsole = nullptr;
        }
    }
}
