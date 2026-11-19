#include <Windows.h>

#include "FeatureControls.h"
#include "Method.h"

namespace {
VOID CALLBACK Initialize(PTP_CALLBACK_INSTANCE, PVOID, PTP_TIMER timer) {
    CloseThreadpoolTimer(timer);
    std::array<bool, 4> enabled{};
    std::array<UINT, 4> bindings{};
    if (method::GetIntPrivateProfile(L"Config", L"console", 0)) method::RedireceConsole();
    for (size_t i = 0; i < enabled.size(); ++i) {
        enabled[i] = method::GetIntPrivateProfile(L"Config", hotkeys::Names[i], 1) != 0;
        bindings[i] = method::GetIntPrivateProfile(L"Hotkeys", hotkeys::Names[i], hotkeys::Defaults[i]);
    }
    gametime::RunWhenReady([&](const gametime::ReadyContext& context) {
        features::Start(context, enabled, bindings);
    });
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        method::InitializeConfigPath(module);
        // Pin before scheduling code that can outlive an explicit FreeLibrary.
        HMODULE pinned = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&Initialize), &pinned)) return FALSE;
        const auto timer = CreateThreadpoolTimer(Initialize, nullptr, nullptr);
        if (timer) {
            ULARGE_INTEGER delay{};
            delay.QuadPart = static_cast<ULONGLONG>(-5210LL * 10000);
            FILETIME due{delay.LowPart, delay.HighPart};
            SetThreadpoolTimer(timer, &due, 0, 0);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        method::exit();
    }
    return TRUE;
}
