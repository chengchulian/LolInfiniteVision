#include <Windows.h>

#include "Constant.h"
#include "GameTime.h"
#include "Memory.h"
#include "Method.h"

namespace
{
    PTP_TIMER g_timer = nullptr;
    bool g_enableVisionSwitch = true;
    bool g_enableAttackRange = true;
    bool g_enableAntiRecoil = true;
    bool g_enableTowerRange = true;

    void PatchSignatureOpcode(
        const gametime::ReadyContext& context,
        const char* signatureCode,
        BYTE expectedOpcode,
        BYTE targetOpcode,
        const wchar_t* signatureName)
    {
        const DWORD64 signatureAddress = method::LocateSignature(
            context.process,
            signatureCode,
            context.moduleStartAddress,
            context.moduleEndAddress,
            0);

        if (!signatureAddress)
        {
            method::PrintToConsole(L"[warn] %ls not found", signatureName);
            return;
        }

        method::PrintToConsole(L"[info] %ls address = 0x%llX", signatureName, signatureAddress);

        BYTE opcode = 0;
        if (!memory::Read(context.process, signatureAddress, opcode))
        {
            method::PrintToConsole(L"[warn] failed to read %ls opcode", signatureName);
            return;
        }

        if (opcode != expectedOpcode)
        {
            method::PrintToConsole(L"[info] %ls opcode = 0x%02X, skip patch", signatureName, opcode);
            return;
        }

        if (!memory::Write(context.process, signatureAddress, targetOpcode))
        {
            method::PrintToConsole(L"[warn] failed to patch %ls opcode", signatureName);
            return;
        }

        method::PrintToConsole(L"[info] patched %ls opcode: 0x%02X -> 0x%02X",
            signatureName,
            opcode,
            targetOpcode);
    }

    void HandleGameTimeReady(const gametime::ReadyContext& context)
    {
        method::PrintToConsole(L"[info] current game time = %f", context.gameTime);

        if (g_enableVisionSwitch)
        {
            PatchSignatureOpcode(
                context,
                VISION_SWITCH_SIGNATURE_CODE,
                VISION_SWITCH_EXPECTED_OPCODE,
                VISION_SWITCH_TARGET_OPCODE,
                L"vision switch signature");
        }
        else
        {
            method::PrintToConsole(L"[info] vision switch disabled by config");
        }

        if (g_enableAttackRange)
        {
            PatchSignatureOpcode(
                context,
                ATTACK_RANGE_SIGNATURE_CODE,
                ATTACK_RANGE_EXPECTED_OPCODE,
                ATTACK_RANGE_TARGET_OPCODE,
                L"attack range signature");
        }
        else
        {
            method::PrintToConsole(L"[info] attack range disabled by config");
        }

        if (g_enableAntiRecoil)
        {
            PatchSignatureOpcode(
                context,
                ANTI_RECOIL_RELEASE_START_SIGNATURE_CODE,
                ANTI_RECOIL_RELEASE_START_EXPECTED_OPCODE,
                ANTI_RECOIL_RELEASE_START_TARGET_OPCODE,
                L"anti recoil release start signature");

            PatchSignatureOpcode(
                context,
                ANTI_RECOIL_RELEASE_END_SIGNATURE_CODE,
                ANTI_RECOIL_RELEASE_END_EXPECTED_OPCODE,
                ANTI_RECOIL_RELEASE_END_TARGET_OPCODE,
                L"anti recoil release end signature");
        }
        else
        {
            method::PrintToConsole(L"[info] anti recoil disabled by config");
        }

        if (g_enableTowerRange)
        {
            PatchSignatureOpcode(
                context,
                TOWER_RANGE_SIGNATURE_CODE,
                TOWER_RANGE_EXPECTED_OPCODE,
                TOWER_RANGE_TARGET_OPCODE,
                L"tower range signature");
        }
        else
        {
            method::PrintToConsole(L"[info] tower range disabled by config");
        }
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

        g_enableVisionSwitch =
            method::GetIntPrivateProfile(L"Config", L"visionSwitch", 1) != 0;
        g_enableAttackRange = 
            method::GetIntPrivateProfile(L"Config", L"attackRange", 1) != 0;
        g_enableAntiRecoil =
            method::GetIntPrivateProfile(L"Config", L"antiRecoil", 1) != 0;
        g_enableTowerRange =
            method::GetIntPrivateProfile(L"Config", L"towerRange", 1) != 0;

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
