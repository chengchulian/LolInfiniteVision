#pragma once

namespace attackrange::signatures
{
    inline constexpr char Dispatch[] =
        "40 57 48 83 EC 20 48 8B 05 ?? ?? ?? ?? 4C 8B C9 48 8B B8 90 00 00 00 "
        "48 85 FF 0F 84 ?? ?? ?? ?? 4C 8B 90 88 00 00 00";
    inline constexpr char Handler[] =
        "48 83 EC 28 8B 51 50 48 89 74 24 48 48 8B F1 48 89 7C 24 20 "
        "48 8B 3D ?? ?? ?? ?? 44 0F B7 C2 44 3B 87 ?? ?? ?? ??";
    inline constexpr char EnableBranch[] =
        "74 05 45 32 C9 EB 03 41 B1 01 BA 0C 00 00 00";
    inline constexpr char PlayerSelection[] =
        "48 8B 05 ?? ?? ?? ?? 0F B6 0D ?? ?? ?? ?? 48 85 C0 48 89 5C 24 38 "
        "41 0F 95 C0 48 85 C0 74 09 0F B6 90 ?? ?? ?? ?? EB 03";
    inline constexpr char PlayerTable[] =
        "0F B6 C1 48 8D 0C C0 48 C1 E1 05 48 8D 05 ?? ?? ?? ?? 48 8B 0C 01";
}
