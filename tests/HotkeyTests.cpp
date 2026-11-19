#include "Hotkeys.h"
#include <cstdio>
#include <cstdlib>

void Check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(EXIT_FAILURE); }
}

int main() {
    hotkeys::Edge key;
    Check(key.Poll(VK_F6, true, 0, true), "First press must toggle");
    Check(!key.Poll(VK_F6, true, 0, true), "Held key must not repeat");
    Check(!key.Poll(VK_F6, false, 0, true), "Release must not toggle");
    Check(!key.Poll(VK_F6, true, 0, false), "Background press must not toggle");
    Check(!key.Poll(VK_F6, true, 0, true), "Returning with held key must not toggle");
    key.Poll(VK_F6, false, 0, true);
    Check(!key.Poll(VK_F6 | (2U << 8), true, 0, true), "Missing Ctrl must not toggle");
    Check(!key.Poll(VK_F6 | (2U << 8), true, 2, true), "Adding Ctrl to held key must not toggle");
    key.Poll(VK_F6, false, 2, true);
    Check(key.Poll(VK_F6 | (2U << 8), true, 2, true), "Ctrl chord must toggle");
    key.Poll(VK_F6, false, 2, true);
    Check(!key.Poll(VK_F6, true, 2, true), "Extra Ctrl must not trigger a plain binding");
    key.Poll(0, false, 0, true);
    Check(!key.Poll(0, true, 0, true), "Unbound shortcut must not toggle");
    Check(!hotkeys::Normalize(0xFFFFFFFF) && !hotkeys::Normalize(VK_CONTROL), "Invalid binding accepted");
    Check(!hotkeys::Normalize(VK_F6 | 0x1000), "Unsupported modifier was accepted");
    Check(hotkeys::Normalize(VK_F6 | (8U << 8)) == VK_F6, "Extended flag must not change a binding");
    Check(hotkeys::Unique({0, 0, VK_F6, VK_F6 | (2U << 8)}), "Distinct chords were rejected");
    Check(!hotkeys::Unique({VK_F6, VK_F6 | (8U << 8), 0, 0}), "Duplicate normalized bindings accepted");
    std::puts("Shortcut chords, focus, repeat suppression and validation passed");
}
