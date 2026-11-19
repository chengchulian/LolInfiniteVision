#pragma once

#include <Windows.h>

namespace attackrange
{
    struct NativeOverlayBindings
    {
        BYTE* dispatch = nullptr;
        BYTE* enableBranch = nullptr;
        BYTE** dispatcherSlot = nullptr;
        BYTE** hudSlot = nullptr;
        BYTE** selectorSlot = nullptr;
        BYTE* invalidPlayer = nullptr;
        BYTE* playerTable = nullptr;
        DWORD playerIndexOffset = 0;
    };

    // Resolves instruction signatures and RIP-relative references; no build IDs,
    // timestamps, image-size fingerprints or fixed module RVAs are used.
    bool ResolveNativeOverlay(BYTE* begin, BYTE* end, NativeOverlayBindings& result);
}
