#pragma once

#include <Windows.h>

#include <functional>

namespace gametime
{
    struct ReadyContext
    {
        HANDLE process = nullptr;
        DWORD64 moduleStartAddress = 0;
        DWORD64 moduleEndAddress = 0;
        DWORD64 signatureAddress = 0;
        INT32 relativeOffset = 0;
        DWORD64 timeBaseAddress = 0;
        float gameTime = 0.0f;
    };

    using ReadyCallback = std::function<void(const ReadyContext&)>;

    bool RunWhenReady(const ReadyCallback& callback);
}
