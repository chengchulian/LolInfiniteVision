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
}
