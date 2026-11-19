// Uses a synthetic image and our own window; never opens a game process.
#include "AttackRange.h"
#include "AttackRangeSignatures.h"
#include "NativeOverlayBindings.h"
#include "OverlayEventPump.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <thread>

namespace method
{
    void PrintToConsole(const wchar_t*, ...) {}
}

namespace
{
    constexpr size_t kImageSize = 0x40000;
    constexpr size_t kDispatch = 0x4FEC; // Straddles a page-protection boundary.
    constexpr size_t kHandler = 0x8000;
    constexpr size_t kSelection = kHandler + 0x4E;
    constexpr size_t kTable = kHandler + 0x8D;
    constexpr size_t kBranch = kHandler + 0x118;
    constexpr size_t kTime = 0x3E000;
    constexpr char kEvent[] = "evtToggleShowOverlayAttackRange";
    BYTE* g_image = nullptr;
    std::atomic<int> g_calls{0};
    std::atomic<DWORD> g_callbackThread{0};
    std::atomic<bool> g_badCommand{false};
    std::atomic<bool> g_visible{false};
    std::atomic<bool> g_enabled{false};
    std::atomic<bool> g_failNextDispatch{false};
    std::atomic<bool> g_faultObserved{false};

    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "%s\n", message);
            std::exit(EXIT_FAILURE);
        }
    }

    void CaptureCommand(const BYTE* command)
    {
        if (g_failNextDispatch.exchange(false)) {
            g_faultObserved = true;
            RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
        }
        const auto* name = *reinterpret_cast<const char* const*>(command);
        const auto size = *reinterpret_cast<const UINT64*>(command + 0x10);
        const auto capacity = *reinterpret_cast<const UINT64*>(command + 0x18);
        if (size != sizeof(kEvent) - 1 || capacity < size || capacity <= 15 ||
            std::memcmp(name, kEvent, sizeof(kEvent)) != 0)
            g_badCommand = true;
        // Simulate the two outcomes of the native handler. Repeated dispatches
        // must select true after the conditional branch has been patched.
        g_visible = g_image[kBranch] == 0xEB ? g_image[kBranch + 1] == 0x05 : !g_visible.load();
        g_callbackThread = GetCurrentThreadId();
        ++g_calls;
    }

    size_t WritePattern(size_t offset, const char* pattern)
    {
        size_t size = 0;
        while (*pattern)
        {
            if (*pattern == ' ')
            {
                ++pattern;
                continue;
            }
            char token[3]{pattern[0], pattern[1], 0};
            g_image[offset + size++] = *pattern == '?' ? 0 : static_cast<BYTE>(std::strtoul(token, nullptr, 16));
            pattern += 2;
        }
        return size;
    }

    void WriteRip(size_t instruction, size_t target)
    {
        const INT32 displacement = static_cast<INT32>(target) - static_cast<INT32>(instruction + 7);
        std::memcpy(g_image + instruction + 3, &displacement, sizeof(displacement));
    }

    void CreateImage()
    {
        g_image = static_cast<BYTE*>(VirtualAlloc(nullptr, kImageSize,
            MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
        Check(g_image != nullptr, "Cannot allocate synthetic image");
        // Deliberately no PE header, timestamp or known image size/RVA layout.
        std::memset(g_image, 0xAB, 0x100);
        const size_t dispatchSize = WritePattern(kDispatch, attackrange::signatures::Dispatch);
        WritePattern(kHandler, attackrange::signatures::Handler);
        WritePattern(kBranch, attackrange::signatures::EnableBranch);
        WritePattern(kSelection, attackrange::signatures::PlayerSelection);
        WritePattern(kTable, attackrange::signatures::PlayerTable);
        WriteRip(kDispatch + 6, 0x20000);
        WriteRip(kHandler + 20, 0x20008);
        WriteRip(kSelection, 0x20010);
        WriteRip(kSelection + 7, 0x3F000);
        WriteRip(kTable + 11, 0x24000);
        const DWORD indexOffset = 0x423;
        std::memcpy(g_image + kSelection + 34, &indexOffset, sizeof(indexOffset));

        // Synthetic dispatcher: execute its matched prologue, restore the stack
        // and tail-call our capture function. No game function is called.
        BYTE tail[] = {0x48, 0x83, 0xC4, 0x20, 0x5F, 0x48, 0xB8,
            0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0};
        const auto capture = reinterpret_cast<UINT64>(&CaptureCommand);
        std::memcpy(tail + 7, &capture, sizeof(capture));
        std::memcpy(g_image + kDispatch + dispatchSize, tail, sizeof(tail));
        *reinterpret_cast<BYTE**>(g_image + 0x20000) = g_image + 0x21000;
        *reinterpret_cast<BYTE**>(g_image + 0x20008) = g_image + 0x23000;
        *reinterpret_cast<BYTE**>(g_image + 0x20010) = g_image + 0x22000;
        *reinterpret_cast<BYTE**>(g_image + 0x21088) = g_image;
        *reinterpret_cast<BYTE**>(g_image + 0x21090) = g_image;
        g_image[0x22423] = 1;
        *reinterpret_cast<BYTE**>(g_image + 0x24120) = g_image + 0x23000;
        FlushInstructionCache(GetCurrentProcess(), g_image, kImageSize);
        DWORD previous = 0;
        Check(VirtualProtect(g_image + 0x5000, 0x1000, PAGE_EXECUTE_READ, &previous) != FALSE,
            "Cannot create readable-region boundary");
    }

    void CheckResolution()
    {
        attackrange::NativeOverlayBindings bindings;
        auto resolve = [&] { return attackrange::ResolveNativeOverlay(g_image, g_image + kImageSize, bindings); };
        Check(resolve(), "Relocated signatures with no version metadata must resolve");
        Check(bindings.dispatch == g_image + kDispatch && bindings.enableBranch == g_image + kBranch &&
            bindings.dispatcherSlot == reinterpret_cast<BYTE**>(g_image + 0x20000) &&
            bindings.hudSlot == reinterpret_cast<BYTE**>(g_image + 0x20008) &&
            bindings.selectorSlot == reinterpret_cast<BYTE**>(g_image + 0x20010) &&
            bindings.invalidPlayer == g_image + 0x3F000 && bindings.playerTable == g_image + 0x24000 &&
            bindings.playerIndexOffset == 0x423, "Incorrect instruction reference decoding");
        for (size_t offset : {kDispatch, kHandler, kBranch, kSelection, kTable})
        {
            const BYTE saved = g_image[offset];
            g_image[offset] = 0x90;
            Check(!resolve(), "Missing signature must be rejected");
            g_image[offset] = saved;
        }
        for (auto signature : {attackrange::signatures::Dispatch, attackrange::signatures::Handler})
        {
            const size_t size = WritePattern(0x11000, signature);
            Check(!resolve(), "Ambiguous global signature must be rejected");
            std::memset(g_image + 0x11000, 0, size);
        }
        const size_t duplicateSize = WritePattern(kHandler + 0x170, attackrange::signatures::EnableBranch);
        Check(!resolve(), "Ambiguous handler branch must be rejected");
        std::memset(g_image + kHandler + 0x170, 0, duplicateSize);
        WriteRip(kDispatch + 6, kImageSize + 0x1000);
        Check(!resolve(), "Out-of-image RIP reference must be rejected");
        WriteRip(kDispatch + 6, 0x20000);
        Check(resolve(), "Restored fixture must resolve");
        Check(g_image[kBranch] == 0x74, "Resolver must not patch code");
        FlushInstructionCache(GetCurrentProcess(), g_image, kImageSize);
    }

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (message == WM_APP + 1)
        {
            *reinterpret_cast<float*>(g_image + kTime) = wparam ? 1.0f : 0.0f;
            return 0;
        }
        if (message == WM_CLOSE)
        {
            DestroyWindow(window);
            return 0;
        }
        if (message == WM_DESTROY)
        {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }

    bool WaitForCalls(int count)
    {
        const ULONGLONG deadline = GetTickCount64() + 5000;
        while (g_calls < count && GetTickCount64() < deadline)
            Sleep(10);
        return g_calls >= count;
    }
}

int main()
{
    CreateImage();
    CheckResolution();
    std::promise<HWND> created;
    auto ready = created.get_future();
    DWORD windowThread = 0;
    std::thread owner([&] {
        windowThread = GetCurrentThreadId();
        WNDCLASSW type{};
        type.lpfnWndProc = WindowProc;
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"RiotWindowClass";
        if (!RegisterClassW(&type))
        {
            created.set_value(nullptr);
            return;
        }
        HWND window = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, type.lpszClassName,
            L"Offline overlay test", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, type.hInstance, nullptr);
        ShowWindow(window, SW_SHOWNOACTIVATE);
        created.set_value(window);
        MSG message;
        while (GetMessageW(&message, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        UnregisterClassW(type.lpszClassName, type.hInstance);
    });
    HWND window = ready.get();
    Check(window != nullptr, "Cannot create own test window");
    const auto base = reinterpret_cast<DWORD64>(g_image);
    Check(!attackrange::Initialize(base, base + kImageSize, base + kImageSize), "Out-of-image time must be rejected");
    Check(attackrange::Initialize(base, base + kImageSize, base + kTime), "Cannot resolve synthetic image");
    Check(attackrange::StartEventPump(window, [](void*) { attackrange::Update(g_enabled); }, nullptr),
        "Cannot start own-window event pump");
    Sleep(900);
    Check(g_calls == 0, "Dispatched before positive game time");
    Check(g_image[kBranch] == 0x74, "Patched handler before prerequisites were ready");
    SendMessageW(window, WM_APP + 1, 1, 0);
    Sleep(100);
    Check(g_calls == 0, "Initially disabled feature dispatched an enable event");
    g_enabled = true;
    Check(WaitForCalls(3), "Periodic native event did not run");
    Check(g_callbackThread == windowThread, "Native event ran on the worker thread");
    Check(!g_badCommand, "MSVC string ABI or event name was wrong");
    // Synchronize with the window thread so branch restoration has completed.
    SendMessageW(window, WM_NULL, 0, 0);
    Check(g_image[kBranch] == 0x74 && g_image[kBranch + 1] == 0x05 && g_visible,
        "Native handler was not restored after enabling");
    g_visible = false;
    const int beforeRecovery = g_calls.load();
    Check(WaitForCalls(beforeRecovery + 1) && g_visible, "Event did not re-enable the overlay");
    g_enabled = false;
    const int beforeDisable = g_calls.load();
    Check(WaitForCalls(beforeDisable + 1) && !g_visible, "Disabling did not hide the overlay");
    SendMessageW(window, WM_NULL, 0, 0);
    Check(g_image[kBranch] == 0x74 && g_image[kBranch + 1] == 0x05, "Disabling left the handler patched");
    const int disabled = g_calls.load();
    Sleep(900);
    Check(g_calls == disabled, "Disabled feature kept dispatching events");
    g_enabled = true;
    Check(WaitForCalls(disabled + 1) && g_visible, "Re-enabling did not restore the overlay");
    SendMessageW(window, WM_APP + 1, 0, 0);
    const int paused = g_calls.load();
    Sleep(900);
    Check(g_calls == paused, "Dispatched while game time was inactive");
    g_failNextDispatch = true;
    SendMessageW(window, WM_APP + 1, 1, 0);
    const ULONGLONG faultDeadline = GetTickCount64() + 3000;
    while (!g_faultObserved && GetTickCount64() < faultDeadline) Sleep(10);
    Check(g_faultObserved, "Synthetic dispatch exception was not reached");
    SendMessageW(window, WM_NULL, 0, 0);
    Check(g_image[kBranch] == 0x74 && g_image[kBranch + 1] == 0x05,
        "Native handler was not restored after a dispatch exception");
    Sleep(100);
    Check(g_calls == paused, "Failed dispatcher continued running");
    attackrange::StopEventPump();
    SendMessageW(window, WM_APP + 1, 1, 0);
    Sleep(900);
    Check(g_calls == paused, "Callback continued after StopEventPump");
    PostMessageW(window, WM_CLOSE, 0, 0);
    owner.join();
    VirtualFree(g_image, 0, MEM_RELEASE);
    std::puts("Offline signature resolution, ABI, readiness, window-thread dispatch and lifecycle passed");
}
