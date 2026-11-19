#pragma once

#include <Windows.h>

namespace attackrange
{
    // The callback runs on the thread which retrieves messages for the window.
    // Stop must run before the callback or its context is destroyed.
    using OverlayTick = void (*)(void*);
    bool StartEventPump(HWND window, OverlayTick tick, void* context);
    void StopEventPump();
}
