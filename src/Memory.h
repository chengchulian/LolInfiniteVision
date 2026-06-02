#pragma once

#include <Windows.h>

namespace memory
{
    bool ReadRaw(HANDLE hProcess, DWORD64 address, void* buffer, SIZE_T size, SIZE_T* bytesRead = nullptr);
    bool ReadInt32(HANDLE hProcess, DWORD64 address, INT32& value);
    bool WriteRaw(HANDLE hProcess, DWORD64 address, const void* buffer, SIZE_T size, SIZE_T* bytesWritten = nullptr);
    bool WriteInt32(HANDLE hProcess, DWORD64 address, INT32 value);

    template <typename T>
    bool Read(HANDLE hProcess, DWORD64 address, T& value)
    {
        return ReadRaw(hProcess, address, &value, sizeof(T), nullptr);
    }

    template <typename T>
    bool Write(HANDLE hProcess, DWORD64 address, const T& value)
    {
        return WriteRaw(hProcess, address, &value, sizeof(T), nullptr);
    }
}
