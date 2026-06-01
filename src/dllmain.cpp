#include <Windows.h>

#include "Constant.h"
#include "GameTime.h"
#include "Method.h"

namespace
{
    PTP_TIMER g_timer = nullptr;

    void HandleGameTimeReady(const gametime::ReadyContext& context)
    {
        method::PrintToConsole(L"[info] current game time = %f", context.gameTime);
    }

    VOID CALLBACK TimerCallback(PTP_CALLBACK_INSTANCE, PVOID, PTP_TIMER)
    {
        if (g_timer)
        {
            CloseThreadpoolTimer(g_timer);
            g_timer = nullptr;
        }

        gametime::RunWhenReady(HandleGameTimeReady);
    }
}

BOOL APIENTRY DllMain(HMODULE, DWORD ul_reason_for_call, LPVOID)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        if (method::GetIntPrivateProfile(L"Config", L"console", 0))
        {
            method::RedireceConsole();
        }

        g_timer = CreateThreadpoolTimer(TimerCallback, nullptr, nullptr);
        if (g_timer)
        {
            FILETIME ft;
            const ULONGLONG delay = static_cast<ULONGLONG>(-5210) * 10000;
            ft.dwHighDateTime = static_cast<DWORD>(delay >> 32);
            ft.dwLowDateTime = static_cast<DWORD>(delay & 0xFFFFFFFF);
            SetThreadpoolTimer(g_timer, &ft, 0, 0);
        }
        break;

    case DLL_PROCESS_DETACH:
        method::exit();
        break;

    default:
        break;
    }

    return TRUE;
}
