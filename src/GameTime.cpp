#include "GameTime.h"

#include <utility>
#include <thread>

#include "Constant.h"
#include "Memory.h"
#include "Method.h"

namespace gametime
{
    namespace
    {
        bool WaitForNonZeroGameTime(HANDLE process, DWORD64 timeBaseAddress, float& gameTime)
        {
            if (!memory::Read(process, timeBaseAddress, gameTime))
            {
                method::PrintToConsole(L"[warn] failed to read game time");
                return false;
            }

            while (gameTime <= 0.0f)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1314));
                if (!memory::Read(process, timeBaseAddress, gameTime))
                {
                    method::PrintToConsole(L"[warn] failed to read game time in loop");
                    return false;
                }
            }

            return true;
        }
    }

    bool RunWhenReady(const ReadyCallback& callback)
    {
        method::PrintToConsole(L"[info] start time signature scan");

        std::this_thread::sleep_for(std::chrono::milliseconds(1314));

        ReadyContext context;
        context.process = GetCurrentProcess();

        const std::pair<DWORD64, DWORD64> addressRange = method::GetModuleAddressRange(GetModuleHandleW(nullptr));
        context.moduleStartAddress = addressRange.first;
        context.moduleEndAddress = addressRange.second;

        method::PrintToConsole(L"[info] module start = 0x%llX", context.moduleStartAddress);
        method::PrintToConsole(L"[info] module end = 0x%llX", context.moduleEndAddress);

        context.signatureAddress = method::LocateSignature(
            context.process,
            TIME_SIGNATURE_CODE,
            context.moduleStartAddress,
            context.moduleEndAddress,
            4);

        if (!context.signatureAddress)
        {
            method::PrintToConsole(L"[warn] time signature not found");
            return false;
        }

        method::PrintToConsole(L"[info] time signature address = 0x%llX", context.signatureAddress);

        if (!memory::ReadInt32(context.process, context.signatureAddress, context.relativeOffset))
        {
            method::PrintToConsole(L"[warn] failed to read time base offset");
            return false;
        }

        method::PrintToConsole(L"[info] time base offset = 0x%X", context.relativeOffset);

        context.timeBaseAddress = context.signatureAddress + context.relativeOffset + 4;
        method::PrintToConsole(L"[info] resolved time base = 0x%llX", context.timeBaseAddress);

        if (!WaitForNonZeroGameTime(context.process, context.timeBaseAddress, context.gameTime))
        {
            return false;
        }

        if (callback)
        {
            callback(context);
        }

        return true;
    }
}
