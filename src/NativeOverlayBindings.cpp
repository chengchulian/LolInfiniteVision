#include "NativeOverlayBindings.h"

#include <cstring>

#include "AttackRangeSignatures.h"
#include "Method.h"

namespace attackrange
{
    namespace
    {
        struct Pattern
        {
            BYTE bytes[64]{};
            bool wildcard[64]{};
            size_t size = 0;
        };

        unsigned Hex(char value)
        {
            return value <= '9' ? value - '0' : value - 'A' + 10;
        }

        Pattern Parse(const char* text)
        {
            Pattern pattern;
            while (*text && pattern.size < ARRAYSIZE(pattern.bytes))
            {
                if (*text == ' ')
                {
                    ++text;
                    continue;
                }
                pattern.wildcard[pattern.size] = text[0] == '?';
                if (!pattern.wildcard[pattern.size])
                    pattern.bytes[pattern.size] = static_cast<BYTE>(Hex(text[0]) * 16 + Hex(text[1]));
                ++pattern.size;
                text += 2;
            }
            return pattern;
        }

        bool ScanRegion(BYTE* begin, BYTE* end, const Pattern& pattern, BYTE*& found)
        {
            __try
            {
                for (BYTE* cursor = begin; static_cast<size_t>(end - cursor) >= pattern.size; ++cursor)
                {
                    size_t i = 0;
                    for (; i < pattern.size; ++i)
                        if (!pattern.wildcard[i] && cursor[i] != pattern.bytes[i])
                            break;
                    if (i == pattern.size)
                    {
                        if (found)
                            return false; // Ambiguous signatures never select a target.
                        found = cursor;
                    }
                }
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        BYTE* FindUnique(BYTE* begin, BYTE* end, const char* text, const wchar_t* label)
        {
            const Pattern pattern = Parse(text);
            BYTE* found = nullptr;
            BYTE* readableBegin = nullptr;
            for (BYTE* cursor = begin; cursor < end;)
            {
                MEMORY_BASIC_INFORMATION region{};
                if (!VirtualQuery(cursor, &region, sizeof(region)))
                {
                    method::PrintToConsole(L"[warn] native attack range: %ls signature scan incomplete", label);
                    return nullptr;
                }
                auto* next = static_cast<BYTE*>(region.BaseAddress) + region.RegionSize;
                if (next <= cursor)
                    return nullptr;
                if (next > end)
                    next = end;
                const DWORD access = region.Protect & 0xFF;
                if (region.State == MEM_COMMIT && !(region.Protect & PAGE_GUARD) &&
                    access != PAGE_NOACCESS && access != PAGE_EXECUTE && access != 0)
                {
                    // Preserve candidate starts across adjacent readable regions
                    // even when their page protections differ.
                    if (!readableBegin)
                        readableBegin = cursor;
                    const size_t available = static_cast<size_t>(cursor - readableBegin);
                    const size_t overlap = available < pattern.size - 1 ? available : pattern.size - 1;
                    if (!ScanRegion(cursor - overlap, next, pattern, found))
                    {
                        method::PrintToConsole(L"[warn] native attack range: %ls signature ambiguous/unreadable", label);
                        return nullptr;
                    }
                }
                else
                {
                    readableBegin = nullptr;
                }
                cursor = next;
            }
            if (!found)
                method::PrintToConsole(L"[warn] native attack range: %ls signature not found", label);
            return found;
        }

        BYTE* Rip(BYTE* instruction)
        {
            INT32 displacement = 0;
            std::memcpy(&displacement, instruction + 3, sizeof(displacement));
            return instruction + 7 + displacement;
        }

        bool InRange(BYTE* begin, BYTE* end, const void* address, size_t size)
        {
            const auto value = reinterpret_cast<UINT_PTR>(address);
            return value >= reinterpret_cast<UINT_PTR>(begin) && value < reinterpret_cast<UINT_PTR>(end) &&
                size <= reinterpret_cast<UINT_PTR>(end) - value;
        }
    }

    bool ResolveNativeOverlay(BYTE* begin, BYTE* end, NativeOverlayBindings& result)
    {
        if (begin >= end)
            return false;
        NativeOverlayBindings resolved;
        resolved.dispatch = FindUnique(begin, end, signatures::Dispatch, L"dispatch command");
        BYTE* handler = FindUnique(begin, end, signatures::Handler, L"attack-range event handler");
        if (!resolved.dispatch || !handler)
            return false;
        BYTE* handlerEnd = handler + 0x200 < end ? handler + 0x200 : end;
        resolved.enableBranch = FindUnique(handler, handlerEnd, signatures::EnableBranch, L"enable branch");
        BYTE* selection = FindUnique(handler, handlerEnd, signatures::PlayerSelection, L"local-player selection");
        BYTE* table = FindUnique(handler, handlerEnd, signatures::PlayerTable, L"local-player table");
        if (!resolved.enableBranch || !selection || !table)
            return false;

        resolved.dispatcherSlot = reinterpret_cast<BYTE**>(Rip(resolved.dispatch + 6));
        resolved.hudSlot = reinterpret_cast<BYTE**>(Rip(handler + 20));
        resolved.selectorSlot = reinterpret_cast<BYTE**>(Rip(selection));
        resolved.invalidPlayer = Rip(selection + 7);
        resolved.playerTable = Rip(table + 11);
        std::memcpy(&resolved.playerIndexOffset, selection + 34, sizeof(DWORD));
        // The table's x9 followed by <<5 is part of its matched instruction pattern.
        if (!InRange(begin, end, resolved.dispatcherSlot, sizeof(void*)) ||
            !InRange(begin, end, resolved.hudSlot, sizeof(void*)) ||
            !InRange(begin, end, resolved.selectorSlot, sizeof(void*)) ||
            !InRange(begin, end, resolved.invalidPlayer, 1) ||
            !InRange(begin, end, resolved.playerTable, 256 * 0x120) ||
            resolved.playerIndexOffset > 0x10000)
        {
            method::PrintToConsole(L"[warn] native attack range: signature references outside module");
            return false;
        }
        result = resolved;
        method::PrintToConsole(L"[info] native attack range dispatch = 0x%llX, enable branch = 0x%llX",
            reinterpret_cast<UINT64>(resolved.dispatch), reinterpret_cast<UINT64>(resolved.enableBranch));
        return true;
    }
}
