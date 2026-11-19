#include "FeatureControls.h"

#include "AttackRange.h"
#include "Constant.h"
#include "Method.h"
#include "OpcodePatch.h"
#include "OverlayEventPump.h"
#include <cwchar>

namespace features {
namespace {
struct Runtime {
    std::array<bool, 4> enabled{};
    std::array<UINT, 4> bindings{};
    std::array<hotkeys::Edge, 4> keys{};
    OpcodePatch vision, recoilStart, recoilEnd, tower;
    bool attackReady = false;
    bool initialized = false;
} g_runtime;

bool Apply(size_t index, bool enabled) {
    switch (index) {
    case 0: return g_runtime.vision.Set(enabled);
    case 1: return g_runtime.attackReady; // Applied by Update on the window thread.
    case 2: {
        const bool previous = g_runtime.recoilStart.Enabled();
        if (!g_runtime.recoilStart.Set(enabled)) return false;
        if (g_runtime.recoilEnd.Set(enabled)) return true;
        g_runtime.recoilStart.Set(previous);
        return false;
    }
    case 3: return g_runtime.tower.Set(enabled);
    default: return false;
    }
}

bool Down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }

void Tick(void*) {
    if (!g_runtime.initialized) {
        for (size_t i = 0; i < g_runtime.enabled.size(); ++i)
            if (!Apply(i, g_runtime.enabled[i])) g_runtime.enabled[i] = false;
        g_runtime.initialized = true;
    }
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    const UINT modifiers = (Down(VK_SHIFT) ? 1U : 0U) |
        (Down(VK_CONTROL) ? 2U : 0U) | (Down(VK_MENU) ? 4U : 0U);
    const bool foreground = process == GetCurrentProcessId() && !Down(VK_LWIN) && !Down(VK_RWIN);
    for (size_t i = 0; i < g_runtime.keys.size(); ++i) {
        const auto binding = g_runtime.bindings[i];
        if (g_runtime.keys[i].Poll(binding, binding && Down(binding & 0xFF), modifiers, foreground)) {
            const bool value = !g_runtime.enabled[i];
            if (Apply(i, value)) {
                g_runtime.enabled[i] = value;
                method::PrintToConsole(L"[info] %ls = %d", hotkeys::Names[i], value);
            } else {
                method::PrintToConsole(L"[warn] could not toggle %ls", hotkeys::Names[i]);
            }
        }
    }
    if (g_runtime.attackReady) attackrange::Update(g_runtime.enabled[1]);
}

BOOL CALLBACK FindWindow(HWND window, LPARAM parameter) {
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    wchar_t name[64]{};
    GetClassNameW(window, name, ARRAYSIZE(name));
    if (process == GetCurrentProcessId() && IsWindowVisible(window) &&
        std::wcscmp(name, L"RiotWindowClass") == 0) {
        *reinterpret_cast<HWND*>(parameter) = window;
        return FALSE;
    }
    return TRUE;
}
}

void Start(const gametime::ReadyContext& context, const std::array<bool, 4>& enabled,
    const std::array<UINT, 4>& bindings) {
    HWND window = nullptr;
    EnumWindows(FindWindow, reinterpret_cast<LPARAM>(&window));
    HMODULE pinned = nullptr;
    if (!window || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&Start), &pinned)) {
        method::PrintToConsole(L"[warn] game window unavailable; feature controls not started");
        return;
    }
    g_runtime.bindings = bindings;
    for (auto& binding : g_runtime.bindings) binding = hotkeys::Normalize(binding);
    if (!hotkeys::Unique(g_runtime.bindings)) {
        method::PrintToConsole(L"[warn] duplicate hotkeys; duplicate bindings disabled");
        const auto all = g_runtime.bindings;
        for (size_t i = 0; i < all.size(); ++i)
            for (size_t j = 0; j < all.size(); ++j)
                if (i != j && all[i] == all[j]) g_runtime.bindings[i] = 0;
    }
    g_runtime.vision.Locate(context, VISION_SWITCH_SIGNATURE_CODE, VISION_SWITCH_EXPECTED_OPCODE, VISION_SWITCH_TARGET_OPCODE);
    g_runtime.recoilStart.Locate(context, ANTI_RECOIL_RELEASE_START_SIGNATURE_CODE,
        ANTI_RECOIL_RELEASE_START_EXPECTED_OPCODE, ANTI_RECOIL_RELEASE_START_TARGET_OPCODE);
    g_runtime.recoilEnd.Locate(context, ANTI_RECOIL_RELEASE_END_SIGNATURE_CODE,
        ANTI_RECOIL_RELEASE_END_EXPECTED_OPCODE, ANTI_RECOIL_RELEASE_END_TARGET_OPCODE);
    g_runtime.tower.Locate(context, TOWER_RANGE_SIGNATURE_CODE, TOWER_RANGE_EXPECTED_OPCODE, TOWER_RANGE_TARGET_OPCODE);
    g_runtime.attackReady = attackrange::Initialize(context.moduleStartAddress, context.moduleEndAddress, context.timeBaseAddress);
    // Apply initial states on the game window thread as well as later changes.
    g_runtime.enabled = enabled;
    for (size_t i = 0; i < bindings.size(); ++i)
        g_runtime.keys[i].held = bindings[i] && Down(bindings[i] & 0xFF);
    if (!attackrange::StartEventPump(window, Tick, nullptr))
        method::PrintToConsole(L"[warn] could not install in-game feature controls");
}
}
