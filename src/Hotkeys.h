#pragma once

#include <Windows.h>
#include <array>

namespace hotkeys {
// Same encoding as the Windows hotkey control: virtual key in the low byte,
// Shift=1, Ctrl=2, Alt=4 in the high byte. Zero disables a binding.
inline constexpr std::array<UINT, 4> Defaults{
    VK_F6 | (2U << 8), VK_F7 | (2U << 8), VK_F8 | (2U << 8), VK_F9 | (2U << 8)};
inline constexpr std::array<const wchar_t*, 4> Names{
    L"visionSwitch", L"attackRange", L"antiRecoil", L"towerRange"};

inline UINT Normalize(UINT value) {
    const UINT key = value & 0xFF;
    if (value > 0xFFFF || (value & 0xF000) || !key || key == 0xFF || key <= VK_XBUTTON2 ||
        key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU ||
        (key >= VK_LSHIFT && key <= VK_RMENU) || key == VK_LWIN || key == VK_RWIN)
        return 0;
    return value & 0x07FF; // Ignore the control's extended-key display flag.
}

inline bool Unique(const std::array<UINT, 4>& bindings) {
    for (size_t i = 0; i < bindings.size(); ++i)
        for (size_t j = 0; j < i; ++j)
            if (Normalize(bindings[i]) && Normalize(bindings[i]) == Normalize(bindings[j]))
                return false;
    return true;
}

struct Edge {
    bool held = false;
    bool Poll(UINT binding, bool keyDown, UINT modifiers, bool foreground) {
        // Track the physical key even outside the game. Returning to the game
        // or adding modifiers while holding it must not synthesize a press.
        const bool pressed = keyDown && !held;
        held = keyDown;
        return Normalize(binding) && pressed && foreground && modifiers == (Normalize(binding) >> 8);
    }
};
}
