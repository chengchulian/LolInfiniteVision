#include "pch.h"

#include <cwchar>
#include <future>
#include <thread>

#include "Method.h"
#include <Windows.h>
#include "Constant.h"
#define LOG(...) method::PrintToConsole(__VA_ARGS__)
using namespace method;


struct clientInfo
{
    DWORD pid; // 进程pid
    HWND hWnd; // 窗口句柄
    WNDPROC lpPrevWndFunc; // 旧的窗口过程
    HANDLE hProcess; // 进程句柄
    DWORD64 startAddress; // 起始地址
    DWORD64 endAddress; // 结束地址
};

PTP_TIMER g_timer = nullptr;
clientInfo client;
DWORD64 RCX;
float distanceValue;
std::atomic<ULONGLONG> lastWheelTime{0};
std::atomic<bool> keepRunning{false};

static bool GetRcxAddress()
{
    // RCX特征码定位到地址
    DWORD64 rcxByteCode;
    rcxByteCode = method::LocateSignature(client.hProcess, RCX_SIGNATURE_CODE, client.startAddress, client.endAddress,
                                          0);

    if (rcxByteCode)
    {
        method::PrintToConsole(L"[信息] RCX-特征码搜索到的地址：0x%llX", rcxByteCode);
    }
    else
    {
        method::PrintToConsole(L"[信息] RCX-特征码没读取到地址，可能特征码已过期");
        return false;
    }

    // 定位到mov指令地址
    DWORD64 movInstructionAddr = rcxByteCode + 0x3;
    method::PrintToConsole(L"[信息] 定位-> mov rcx,[0x%llX] ", movInstructionAddr);

    // 读取相对偏移量 RIP
    INT32 ripRelativeOffset = 0;
    if (!ReadProcessMemory(client.hProcess, (LPCVOID)(movInstructionAddr), &ripRelativeOffset,
                           sizeof(ripRelativeOffset), 0))
    {
        method::PrintToConsole(L"[信息] RCX-读取相对偏移量失败");
        return false;
    }
    method::PrintToConsole(L"[信息] RCX-读取到的相对偏移量 0x%X", ripRelativeOffset);

    // 计算下一条指令地址（当前指令地址+7字节）
    DWORD64 nextInstructionAddr = rcxByteCode + 0x7;
    method::PrintToConsole(L"[信息] RCX-下一条指令地址 0x%llX", nextInstructionAddr);

    // 计算目标地址
    DWORD64 targetAddr = nextInstructionAddr + ripRelativeOffset;
    method::PrintToConsole(L"[信息] RCX-计算得到的目标地址 0x%llX", targetAddr);

    // 读取RCX指针的值
    DWORD64 rcxValue = 0;
    if (!ReadProcessMemory(client.hProcess, (LPCVOID)targetAddr, &rcxValue, sizeof(rcxValue), 0))
    {
        method::PrintToConsole(L"[信息] RCX-读取目标地址失败");
        return false;
    }
    method::PrintToConsole(L"[信息] RCX-指针值 0x%llX", rcxValue);


    // 获取最终RCX值（指针+0x18）
    if (!ReadProcessMemory(client.hProcess, (LPCVOID)(rcxValue + RCX_OFFSET_DEFAULT), &RCX, sizeof(RCX), 0))
    {
        method::PrintToConsole(L"[信息] RCX-读取RCX+0x18失败");
        return false;
    }
    method::PrintToConsole(L"[信息] RCX-最终值 0x%llX", RCX);

    return true;
}

/**
* @brief 批量写入视距数据
* @note 会同时设置最小视距
*/
static void WriteInsightData()
{
    // 写入目标地址
    if (!WriteProcessMemory(client.hProcess, LPVOID(RCX + RCX_OFFSET_CURRENT), &distanceValue, sizeof(distanceValue),
                            0))
    {
        method::PrintToConsole(L"[错误] 当前视距限制修改失败 (错误代码: %d)", GetLastError());
    }
    else
    {
        method::PrintToConsole(L"[成功] 当前视距限制修改成功：", distanceValue);
    }
    float current = 0.0;
    ReadProcessMemory(client.hProcess, LPCVOID(RCX + RCX_OFFSET_CURRENT), &current, sizeof(current), 0);
    if (distanceValue == current)
    {
        method::PrintToConsole(L"[成功] 视距修改成功 当前：%F 内存：%F", distanceValue, current);
    }
    else
    {
        method::PrintToConsole(L"[错误] 视距修改失败 (错误代码: %d) 预期数据：%F 内存数据：%F", GetLastError(), distanceValue, current);
    }
}

static LRESULT APIENTRY NewProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    //https://learn.microsoft.com/zh-cn/windows/win32/inputdev/virtual-key-codes 虚拟键码
    if (uMsg == WM_SYSKEYUP && wParam == VK_F4)
    {
        // ALT + F4 -> 立即终止程序
        method::PrintToConsole(L"[信息] Alt + F4 -> ExitProcess(1)");
        method::PrintToConsole(L"[信息] 如果是云顶之弈或斗魂竞技场，需要点击退出游戏才行，不然就要等所有人的游戏都结束才能重新开始");
        // exit(0);		// 正常终止程序，返回退出码
        // abort();		// 触发异常终止，通常生成 SIGABRT 信号，在 Windows 上表现为崩溃
        ExitProcess(1); // 强制终止当前进程（包括所有线程），并返回退出码 
        return TRUE;
    }
    // CTRL 按下
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
    {
        switch (wParam)
        {
        case 'S':
            {
                // 使用 C++20的新特性 格式化float // 生成的文件体积太大，已废弃
                //std::wstring wideStr = std::format(L"{:.2f}", distanceValue);
                WCHAR wideStr[32];
                swprintf_s(wideStr, L"%.2f", distanceValue);
                if (method::WriteStringPrivateProfile(L"Config", L"DistanceValue", wideStr))
                {
                    method::PrintToConsole(L"[成功] CTRL + S 保存视距 保存的值：%F  %s", distanceValue, wideStr);
                }
                else
                {
                    method::PrintToConsole(L"[失败] CTRL + S 保存视距 错误代码: %d", GetLastError());
                }
                return TRUE;
            }
        default:
            break;
        }
    }
    //鼠标滚动时读取当前视距
    if (uMsg == WM_MOUSEWHEEL) //鼠标滚轮被滚动
    {
        lastWheelTime = GetTickCount64(); // 更新滚轮触发时间
        // 保留逻辑顺序，耗时操作异步
        std::thread([]() {
            Sleep(100);
            ReadProcessMemory(client.hProcess, LPCVOID(RCX + RCX_OFFSET_CURRENT), &distanceValue,
                              sizeof(distanceValue), 0);
            method::PrintToConsole(L"[信息] 滚轮读取当前视距：%f", distanceValue);
        }).detach();
    }

    return CallWindowProc(client.lpPrevWndFunc, hWnd, uMsg, wParam, lParam);
}

static VOID CALLBACK TimerCallback(PTP_CALLBACK_INSTANCE, PVOID, PTP_TIMER)
{
    if (g_timer)
    {
        CloseThreadpoolTimer(g_timer);
        g_timer = nullptr;
    }
    method::PrintToConsole(L"[提示] 开始初始化");
    // 获取设定的视距大小并赋值给distanceValue TCHAR buffer[8] 视距在7位数字以内(九百万) 加上末尾\0空字符所以填8 不需要太多空间
    TCHAR buffer[8];
    method::GetStringPrivateProfile(L"Config", L"DistanceValue", L"2250", buffer, 8);
    WCHAR* endPtr = nullptr;
    float result = wcstof(buffer, &endPtr);

    if (endPtr == buffer || *endPtr != L'\0')
    {
        distanceValue = 2250.0f;
        method::PrintToConsole(L"[警告] Config->DistanceValue 配置无效，使用默认值 2250.0");
    }
    else
    {
        distanceValue = result;
        // 提示视距值
        method::PrintToConsole(L"[信息] 配置中的视距: %f", distanceValue);
    }

    // 等待一下就开始循环寻找客户端窗口句柄
    std::this_thread::sleep_for(std::chrono::milliseconds(1314));
    while (client.hWnd == NULL) //如果窗口句柄是NULL就继续循环
    {
        //寻找窗口句柄
        client.hWnd = FindWindow(CLIENT_NAME, NULL);
        //"RiotWindowClass","League of Legends (TM) Client"
        // method::PrintToConsole(L"FindWindowW = %d", client.hWnd);
        std::this_thread::sleep_for(std::chrono::milliseconds(1314));
    }
    // 获取程序ID和句柄
    client.pid = GetCurrentProcessId();
    client.hProcess = GetCurrentProcess();
    method::PrintToConsole(L"[信息] 窗口句柄: %d", client.hWnd);
    method::PrintToConsole(L"[信息] 进程ID: %d", client.pid);
    method::PrintToConsole(L"[信息] 进程句柄: %d", client.hProcess);
    //子类化窗口 自定义处理消息
    client.lpPrevWndFunc = (WNDPROC)SetWindowLongPtrW(client.hWnd, GWLP_WNDPROC, (LRESULT)NewProc);
    if (client.lpPrevWndFunc == NULL)
    {
        //无法接管消息 输出错误结果
        method::PrintToConsole(L"[错误] SetWindowLong - (错误代码：%d)", GetLastError());
        method::PrintToConsole(L"[错误] 无法子类化游戏客户端，快捷键已失效");
        //return 0;
    }
    else
    {
        method::PrintToConsole(L"[成功] SetWindowLong - %d", client.lpPrevWndFunc);
    }
    // 获得模块起始地址和结束地址
    std::pair<DWORD64, DWORD64> addressRange = GetModuleAddressRange(GetModuleHandleW(nullptr));
    client.startAddress = addressRange.first;
    client.endAddress = addressRange.second;
    method::PrintToConsole(L"[信息] 客户端模块起始地址 = 0x%llX", client.startAddress);
    method::PrintToConsole(L"[信息] 客户端模块结束地址 = 0x%llX", client.endAddress);
    int Manual = method::GetIntPrivateProfile(L"Config", L"Manual", 0);
    if (Manual == 1)
    {
        method::PrintToConsole(L"[提示] 当前为手动视距修改，泉水中按下CTRL + HOME修改视距");
        return;
    }
    else
    {
        method::PrintToConsole(L"[提示] 当前为自动视距修改，识别到进入游戏时自动修改");
    }

    // 定位时间地址
    DWORD64 timeAddress = method::LocateSignature(client.hProcess, TIME_SIGNATURE_CODE, client.startAddress,
                                                  client.endAddress, 4);
    if (timeAddress)
    {
        method::PrintToConsole(L"[信息] 特征码搜索到的时间地址 = 0x%llX", timeAddress);
        DWORD64 基址, 基址值 = 0;
        ReadProcessMemory(client.hProcess, LPCVOID(timeAddress), &基址值, 4, 0);
        //输出时间基址值
        method::PrintToConsole(L"[信息] 特征定位时间基址 = 0x%llX", 基址值);
        基址 = timeAddress + 基址值 + 4; //基址的计算结果
        method::PrintToConsole(L"[信息] 计算真正时间基址 = 0x%llX", 基址);
        float times = 0.0;
        ReadProcessMemory(client.hProcess, LPCVOID(基址), &times, 4, 0);
        //循环读取时间基址，直到时间不为0
        while (times <= 0)
        {
            ReadProcessMemory(client.hProcess, LPCVOID(基址), &times, 4, 0);
            // method::PrintToConsole(L"[信息] 循环读到的时间 = %f", times);
            std::this_thread::sleep_for(std::chrono::milliseconds(1314));
        }
        method::PrintToConsole(L"[信息] 当前游戏时间 = %f", times);
    }
    else
    {
        method::PrintToConsole(L"[警告] 特征码没读取到时间地址，可能特征码已过期，可使用CTRL + HOME尝试手动修改视距(视距特征码有效的情况下)");
        return;
    }
    // 获取RCX地址
    if (!GetRcxAddress())
    {
        // 失败则无法继续
        return;
    }

    DWORD64 addr = LocateSignature(client.hProcess, VISION_SIGNATURE_CODE, client.startAddress, client.endAddress, 0);
    if (addr)
    {
        BYTE oldByte = 0, newByte = 0x4E;
        SIZE_T bytesRead = 0, bytesWritten = 0;

        if (ReadProcessMemory(client.hProcess, reinterpret_cast<LPCVOID>(addr), &oldByte, 1, &bytesRead) && oldByte ==
            0x77)
        {
            WriteProcessMemory(client.hProcess, reinterpret_cast<LPVOID>(addr), &newByte, 1, &bytesWritten);
            LOG(L"[信息] 无限视距修改成功 (0x77 -> 0x4E)");
        }
    }
    WriteInsightData();
    keepRunning = true;
}

DWORD WINAPI LockThread(LPVOID lp_thread_parameter)
{
    const ULONGLONG idleThreshold = 500; // 0.5秒没有滚轮事件
    while (true)
    {
        if (keepRunning)
        {
            ULONGLONG now = GetTickCount64();
            if (now - lastWheelTime > idleThreshold)
            {
                WriteInsightData();
                Sleep(100);
            }
            
        }
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // 读取配置文件里的 ShowConsole 值，默认不显示
        if (GetIntPrivateProfile(L"Config", L"OpenConsole", 0))
        {
            RedireceConsole();
        }
        g_timer = CreateThreadpoolTimer(TimerCallback, nullptr, nullptr);
        CreateThread(nullptr, 0, LockThread, nullptr, 0, nullptr);
        if (g_timer)
        {
            FILETIME ft;
            ULONGLONG delay = static_cast<ULONGLONG>(-5210) * 10000;
            ft.dwHighDateTime = static_cast<DWORD>(delay >> 32);
            ft.dwLowDateTime = static_cast<DWORD>(delay & 0xFFFFFFFF);
            SetThreadpoolTimer(g_timer, &ft, 0, 0);
        }
        break;
    case DLL_PROCESS_DETACH:
        exit();
        break;
    default:
        break;
    }
    return TRUE;
}
