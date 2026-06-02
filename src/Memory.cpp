#include "Memory.h"

namespace memory
{
    bool ReadRaw(HANDLE hProcess, DWORD64 address, void* buffer, SIZE_T size, SIZE_T* bytesRead)
    {
        return ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address), buffer, size, bytesRead) != FALSE;
    }

    bool ReadInt32(HANDLE hProcess, DWORD64 address, INT32& value)
    {
        return ReadRaw(hProcess, address, &value, sizeof(value), nullptr);
    }

    bool WriteRaw(HANDLE hProcess, DWORD64 address, const void* buffer, SIZE_T size, SIZE_T* bytesWritten)
    {
        return WriteProcessMemory(hProcess, reinterpret_cast<LPVOID>(address), buffer, size, bytesWritten) != FALSE;
    }

    bool WriteInt32(HANDLE hProcess, DWORD64 address, INT32 value)
    {
        return WriteRaw(hProcess, address, &value, sizeof(value), nullptr);
    }
}
