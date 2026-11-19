#include "AttackRange.h"

#include <cmath>
#include <cstring>

#include "Method.h"
#include "NativeOverlayBindings.h"

namespace attackrange
{
    namespace
    {
        constexpr char kEventName[] = "evtToggleShowOverlayAttackRange";

        // Borrowed, long MSVC x64 string representation. The synchronous dispatcher
        // reads size/capacity at +0x10/+0x18 and does not take ownership of the input.
        struct CommandString
        {
            const char* data;
            UINT64 reserved;
            UINT64 size;
            UINT64 capacity;
        };
        static_assert(sizeof(CommandString) == 32);
        static_assert(sizeof(kEventName) - 1 > 15);

        struct Runtime
        {
            NativeOverlayBindings native;
            const float* gameTime = nullptr;
            bool failed = false;
            bool reported = false;
            bool applied = false;
            ULONGLONG nextRefresh = 0;
        };
        Runtime g_runtime;

        bool SetAndDispatch(Runtime* runtime, bool enabled, bool& dispatched)
        {
            __try
            {
                const auto& native = runtime->native;
                const float time = *runtime->gameTime;
                if (!std::isfinite(time) || time <= 0)
                    return true;

                // Match the event handler's local-player selection; it assumes a
                // real player exists before it dereferences the player's interface.
                const auto selector = *native.selectorSlot;
                if (!selector || !*native.hudSlot)
                    return true;
                const BYTE index = selector[native.playerIndexOffset];
                if (index == *native.invalidPlayer ||
                    !*reinterpret_cast<void**>(native.playerTable + static_cast<size_t>(index) * 0x120))
                    return true;
                const auto dispatcher = *native.dispatcherSlot;
                if (!dispatcher || !*reinterpret_cast<void**>(dispatcher + 0x88) ||
                    !*reinterpret_cast<void**>(dispatcher + 0x90))
                    return true;

                // Force the native true/false path only for this synchronous
                // event. EB 05 selects mov r9b,1; EB 00 selects xor r9b,r9b.
                // Restore JE 05 even when dispatch raises an exception.
                BYTE* branch = native.enableBranch;
                if (branch[0] != 0x74 || branch[1] != 0x05) return false;
                DWORD previous = 0;
                if (!VirtualProtect(branch, 2, PAGE_EXECUTE_READWRITE, &previous)) return false;
                bool restored = false;
                __try {
                    branch[0] = 0xEB;
                    branch[1] = enabled ? 0x05 : 0x00;
                    if (FlushInstructionCache(GetCurrentProcess(), branch, 2)) {
                        const CommandString command{kEventName, 0, sizeof(kEventName) - 1, sizeof(kEventName) - 1};
                        using DispatchCommand = void (*)(const CommandString*);
                        reinterpret_cast<DispatchCommand>(native.dispatch)(&command);
                        dispatched = true;
                    }
                } __finally {
                    branch[0] = 0x74;
                    branch[1] = 0x05;
                    const BOOL flushed = FlushInstructionCache(GetCurrentProcess(), branch, 2);
                    DWORD unused = 0;
                    restored = VirtualProtect(branch, 2, previous, &unused) && flushed;
                }
                return dispatched && restored;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

    }

    void Update(bool enabled) {
            auto* runtime = &g_runtime;
            if (runtime->failed)
                return;
            const auto now = GetTickCount64();
            if (enabled == runtime->applied && (!enabled || now < runtime->nextRefresh)) return;
            bool dispatched = false;
            if (!SetAndDispatch(runtime, enabled, dispatched))
            {
                runtime->failed = true;
                method::PrintToConsole(L"[warn] native attack-range event stopped after a validation/call failure");
            }
            else if (dispatched) {
                runtime->applied = enabled;
                runtime->nextRefresh = now + 750;
                if (!runtime->reported) {
                    runtime->reported = true;
                    method::PrintToConsole(L"[info] native attack-range event dispatched on window thread");
                }
            }
    }

    bool Initialize(DWORD64 moduleBase, DWORD64 moduleEnd, DWORD64 gameTimeAddress)
    {
        auto* base = reinterpret_cast<BYTE*>(moduleBase);
        if (moduleEnd <= moduleBase || moduleEnd - moduleBase < sizeof(float) ||
            gameTimeAddress < moduleBase || gameTimeAddress > moduleEnd - sizeof(float))
        {
            method::PrintToConsole(L"[warn] native attack range: invalid module/time address range");
            return false;
        }
        NativeOverlayBindings bindings;
        if (!ResolveNativeOverlay(base, reinterpret_cast<BYTE*>(moduleEnd), bindings))
            return false;
        g_runtime = {};
        g_runtime.native = bindings;
        g_runtime.gameTime = reinterpret_cast<const float*>(gameTimeAddress);
        method::PrintToConsole(L"[info] native attack-range event resolved by signature match");
        return true;
    }
}
