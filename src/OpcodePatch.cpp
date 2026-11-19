#include "OpcodePatch.h"

#include "Memory.h"
#include "Method.h"

namespace features {
void OpcodePatch::Locate(const gametime::ReadyContext& context, const char* signature, BYTE original, BYTE target) {
    original_ = original;
    target_ = target;
    enabled_ = false;
    address_ = method::LocateSignature(context.process, signature,
        context.moduleStartAddress, context.moduleEndAddress, 0);
    BYTE current = 0;
    if (!address_ || !memory::Read(context.process, address_, current) || current != original_) {
        method::PrintToConsole(L"[warn] feature patch unavailable: %hs", signature);
        address_ = 0;
    }
}

bool OpcodePatch::Set(bool value) {
    if (!address_) return false;
    if (enabled_ == value) return true;
    BYTE current = 0;
    const HANDLE process = GetCurrentProcess();
    if (!memory::Read(process, address_, current) || current != (enabled_ ? target_ : original_)) return false;
    DWORD previous = 0;
    auto* location = reinterpret_cast<void*>(address_);
    if (!VirtualProtect(location, 1, PAGE_EXECUTE_READWRITE, &previous)) return false;
    const bool written = memory::Write(process, address_, value ? target_ : original_);
    const BOOL flushed = FlushInstructionCache(process, location, 1);
    DWORD unused = 0;
    const BOOL restored = VirtualProtect(location, 1, previous, &unused);
    if (written) enabled_ = value;
    return written && flushed && restored;
}
}
