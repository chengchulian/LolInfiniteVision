#include "OverlayEventPump.h"

#include <mutex>

namespace attackrange
{
    namespace
    {
        struct Pump
        {
            HWND window = nullptr;
            HHOOK hook = nullptr;
            UINT message = 0;
            OverlayTick tick = nullptr;
            void* context = nullptr;
            bool active = false;
        };

        Pump g_pump;
        std::recursive_mutex g_lock;
        const UINT_PTR kTimerId = reinterpret_cast<UINT_PTR>(&g_pump);

        LRESULT CALLBACK MessageHook(int code, WPARAM remove, LPARAM parameter)
        {
            if (code == HC_ACTION && remove == PM_REMOVE)
            {
                const auto* message = reinterpret_cast<const MSG*>(parameter);
                std::lock_guard<std::recursive_mutex> guard(g_lock);
                if (g_pump.active && message->hwnd == g_pump.window)
                {
                    if (message->message == g_pump.message)
                    {
                        // SetTimer is installed by the window's owner thread.
                        if (!SetTimer(g_pump.window, kTimerId, 30, nullptr))
                        {
                            g_pump.active = false;
                        }
                        else
                        {
                            g_pump.tick(g_pump.context);
                        }
                    }
                    else if (message->message == WM_TIMER && message->wParam == kTimerId)
                    {
                        g_pump.tick(g_pump.context);
                    }
                }
            }
            return CallNextHookEx(nullptr, code, remove, parameter);
        }
    }

    bool StartEventPump(HWND window, OverlayTick tick, void* context)
    {
        DWORD process = 0;
        const DWORD thread = GetWindowThreadProcessId(window, &process);
        if (!thread || process != GetCurrentProcessId() || !tick)
        {
            return false;
        }
        std::lock_guard<std::recursive_mutex> guard(g_lock);
        if (g_pump.hook)
        {
            return false;
        }
        const UINT message = RegisterWindowMessageW(L"LolInfiniteVision.NativeAttackRange.Start");
        if (!message)
        {
            return false;
        }
        g_pump.window = window;
        g_pump.message = message;
        g_pump.tick = tick;
        g_pump.context = context;
        g_pump.hook = SetWindowsHookExW(WH_GETMESSAGE, MessageHook, nullptr, thread);
        if (!g_pump.hook)
        {
            g_pump = {};
            return false;
        }
        g_pump.active = true;
        if (!PostMessageW(window, message, 0, 0))
        {
            UnhookWindowsHookEx(g_pump.hook);
            g_pump = {};
            return false;
        }
        return true;
    }

    void StopEventPump()
    {
        HHOOK hook = nullptr;
        {
            // Also waits for a callback already executing on the window thread.
            std::lock_guard<std::recursive_mutex> guard(g_lock);
            g_pump.active = false;
            KillTimer(g_pump.window, kTimerId);
            hook = g_pump.hook;
            g_pump = {};
        }
        if (hook)
        {
            UnhookWindowsHookEx(hook);
        }
    }
}
