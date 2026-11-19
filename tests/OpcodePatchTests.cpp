#include "OpcodePatch.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

void Check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(EXIT_FAILURE); }
}

int main() {
    auto* page = static_cast<BYTE*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(page != nullptr, "Cannot allocate own patch fixture");
    const BYTE original[]{0x75, 0x0E, 0x48, 0x8B};
    std::memcpy(page + 100, original, sizeof(original));
    DWORD previous = 0;
    Check(VirtualProtect(page, 4096, PAGE_EXECUTE_READ, &previous) != FALSE, "Cannot protect own fixture");
    gametime::ReadyContext context{};
    context.process = GetCurrentProcess();
    context.moduleStartAddress = reinterpret_cast<DWORD64>(page);
    context.moduleEndAddress = context.moduleStartAddress + 4096;
    features::OpcodePatch patch;
    patch.Locate(context, "75 0E 48 8B", 0x75, 0x74);
    Check(patch.Set(false) && page[100] == 0x75, "Initial disabled state changed code");
    for (int i = 0; i < 3; ++i) {
        Check(patch.Set(true) && patch.Enabled() && page[100] == 0x74, "Enable did not patch");
        Check(patch.Set(true) && page[100] == 0x74, "Repeated enable changed code");
        Check(patch.Set(false) && !patch.Enabled(), "Disable failed");
        Check(std::memcmp(page + 100, original, sizeof(original)) == 0, "Disable did not restore original bytes");
        MEMORY_BASIC_INFORMATION region{};
        Check(VirtualQuery(page, &region, sizeof(region)) && region.Protect == PAGE_EXECUTE_READ,
            "Toggle did not restore page protection");
    }
    features::OpcodePatch missing;
    missing.Locate(context, "90 90 90", 0x90, 0xEB);
    Check(!missing.Set(true), "Missing signature was patched");
    features::OpcodePatch mismatch;
    mismatch.Locate(context, "75 0E 48 8B", 0x74, 0xEB);
    Check(!mismatch.Set(true) && page[100] == 0x75, "Unexpected opcode was overwritten");
    Check(patch.Set(true), "Cannot enable before ownership check");
    Check(VirtualProtect(page, 4096, PAGE_READWRITE, &previous) != FALSE, "Cannot change fixture");
    page[100] = 0x90; // Simulate another component taking ownership of the byte.
    Check(!patch.Set(false) && page[100] == 0x90, "Disable overwrote another component's change");
    VirtualFree(page, 0, MEM_RELEASE);
    std::puts("Reversible opcodes, repeat toggles, page protection and ownership checks passed");
}
