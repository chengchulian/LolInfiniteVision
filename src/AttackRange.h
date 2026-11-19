#pragma once

#include <Windows.h>

namespace attackrange
{
    bool Initialize(DWORD64 moduleBase, DWORD64 moduleEnd, DWORD64 gameTimeAddress);
    // Called only on the game window thread. Disabling dispatches a native hide
    // event once; the original event-handler bytes are restored after each call.
    void Update(bool enabled);
}
