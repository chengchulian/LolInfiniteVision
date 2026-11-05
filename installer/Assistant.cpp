#include "Deployment.h"
#include "resource.h"
#include <CommCtrl.h>
#include <ShlObj.h>
#include <ShObjIdl.h>
#include <Shellapi.h>
#include <TlHelp32.h>
#include <wrl/client.h>
#include <iostream>
#include <string>

namespace {
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
HINSTANCE g_instance = nullptr;
HFONT g_headingFont = nullptr;
bool g_smoke = false;

deployment::Bytes Resource(int identifier) {
    const auto resource = FindResourceW(g_instance, MAKEINTRESOURCEW(identifier), RT_RCDATA);
    const auto loaded = resource ? LoadResource(g_instance, resource) : nullptr;
    const auto* data = loaded ? static_cast<const unsigned char*>(LockResource(loaded)) : nullptr;
    const DWORD size = resource ? SizeofResource(g_instance, resource) : 0;
    if (!data || !size)
        throw deployment::Error(L"内嵌资源缺失，请重新构建无限视距助手。");
    return {data, data + size};
}

deployment::Assets Assets() {
    return {Resource(IDR_PLUGIN), Resource(IDR_LOADER), Resource(IDR_CONFIG)};
}

std::wstring PathText(HWND dialog) {
    const auto control = GetDlgItem(dialog, IDC_GAME_PATH);
    const int size = GetWindowTextLengthW(control);
    std::wstring text(static_cast<std::size_t>(size) + 1, L'\0');
    GetWindowTextW(control, text.data(), static_cast<int>(text.size()));
    text.resize(static_cast<std::size_t>(size));
    return text;
}

void Status(HWND dialog, const std::wstring& text) {
    SetDlgItemTextW(dialog, IDC_STATUS, text.c_str());
}

void ChooseLocation(HWND dialog, const fs::path& selected) {
    const auto location = deployment::Inspect(selected);
    SetDlgItemTextW(dialog, IDC_GAME_PATH, location.game.c_str());
    const auto options = deployment::ReadOptions(location);
    CheckDlgButton(dialog, IDC_VISION, options.visionSwitch ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_ATTACK_RANGE, options.attackRange ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_ANTI_RECOIL, options.antiRecoil ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_TOWER_RANGE, options.towerRange ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dialog, IDC_CONSOLE, options.console ? BST_CHECKED : BST_UNCHECKED);
    Status(dialog, L"已选择游戏目录。游戏大厅点击“写入视距插件”。");
}

fs::path Detect() {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        bool found = Process32FirstW(snapshot, &entry) != FALSE;
        while (found) {
            if (_wcsicmp(entry.szExeFile, L"League of Legends.exe") == 0 ||
                _wcsicmp(entry.szExeFile, L"LeagueClient.exe") == 0 ||
                _wcsicmp(entry.szExeFile, L"LeagueClientUx.exe") == 0) {
                const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
                if (process) {
                    wchar_t executable[32768]{};
                    DWORD length = static_cast<DWORD>(std::size(executable));
                    const bool queried = QueryFullProcessImageNameW(process, 0, executable, &length) != FALSE;
                    CloseHandle(process);
                    if (queried) {
                        const auto directory = fs::path(executable).parent_path();
                        for (const auto& candidate : {directory, directory.parent_path(), directory.parent_path() / L"Game"}) {
                            try {
                                const auto location = deployment::Inspect(candidate);
                                CloseHandle(snapshot);
                                return location.game;
                            } catch (const deployment::Error&) {}
                        }
                    }
                }
            }
            found = Process32NextW(snapshot, &entry) != FALSE;
        }
        CloseHandle(snapshot);
    }
    for (const auto* candidate : {L"D:\\WeGameApps\\英雄联盟（含经典模式）", L"D:\\WeGameApps\\英雄联盟",
                                  L"C:\\Riot Games\\League of Legends", L"D:\\Riot Games\\League of Legends"}) {
        try { return deployment::Inspect(candidate).game; }
        catch (const deployment::Error&) {}
    }
    return {};
}

void Browse(HWND dialog) {
    ComPtr<IFileOpenDialog> picker;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&picker))))
        throw deployment::Error(L"无法打开文件夹选择窗口。");
    DWORD flags = 0;
    picker->GetOptions(&flags);
    picker->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    picker->SetTitle(L"选择含 League of Legends.exe 的 Game 目录");
    const auto current = PathText(dialog);
    ComPtr<IShellItem> folder;
    if (!current.empty() && SUCCEEDED(SHCreateItemFromParsingName(current.c_str(), nullptr, IID_PPV_ARGS(&folder))))
        picker->SetFolder(folder.Get());
    const auto result = picker->Show(dialog);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        return;
    if (FAILED(result) || FAILED(picker->GetResult(&folder)))
        throw deployment::Error(L"无法读取所选目录。");
    PWSTR selected = nullptr;
    if (FAILED(folder->GetDisplayName(SIGDN_FILESYSPATH, &selected)))
        throw deployment::Error(L"请选择本机文件夹。");
    const fs::path path(selected);
    CoTaskMemFree(selected);
    ChooseLocation(dialog, path);
}

void Install(HWND dialog) {
    if (deployment::IsGameRunning())
        throw deployment::Error(L"游戏正在运行。请先退出当前对局并关闭游戏进程，再写入插件。");
    EnableWindow(GetDlgItem(dialog, IDC_INSTALL), FALSE);
    Status(dialog, L"正在写入视距插件…");
    const deployment::Options options{
        IsDlgButtonChecked(dialog, IDC_VISION) == BST_CHECKED,
        IsDlgButtonChecked(dialog, IDC_ATTACK_RANGE) == BST_CHECKED,
        IsDlgButtonChecked(dialog, IDC_ANTI_RECOIL) == BST_CHECKED,
        IsDlgButtonChecked(dialog, IDC_TOWER_RANGE) == BST_CHECKED,
        IsDlgButtonChecked(dialog, IDC_CONSOLE) == BST_CHECKED
    };
    try {
        const auto location = deployment::Install(PathText(dialog), Assets(), options);
        SetDlgItemTextW(dialog, IDC_GAME_PATH, location.game.c_str());
        Status(dialog, L"写入完成。下次进入对局时加载；修改配置后请重新进入游戏。");
    } catch (...) {
        EnableWindow(GetDlgItem(dialog, IDC_INSTALL), TRUE);
        throw;
    }
    EnableWindow(GetDlgItem(dialog, IDC_INSTALL), TRUE);
}

void Cleanup(HWND dialog, bool removeLoader) {
    const auto location = deployment::Uninstall(PathText(dialog), Resource(IDR_LOADER), removeLoader);
    SetDlgItemTextW(dialog, IDC_GAME_PATH, location.game.c_str());
    Status(dialog, removeLoader ? L"清理完成。已移除无限视距插件、配置和注入器。"
                                : L"清理完成。已移除无限视距插件和配置，保留注入器。");
}

INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wparam, LPARAM) {
    try {
        switch (message) {
        case WM_INITDIALOG: {
            SendDlgItemMessageW(dialog, IDC_GAME_PATH, EM_SETCUEBANNER, FALSE,
                reinterpret_cast<LPARAM>(L"选择 Game 目录或其上一级安装目录"));
            for (const auto id : {IDC_VISION, IDC_ATTACK_RANGE, IDC_ANTI_RECOIL, IDC_TOWER_RANGE})
                CheckDlgButton(dialog, id, BST_CHECKED);
            LOGFONTW font{};
            const auto original = reinterpret_cast<HFONT>(SendMessageW(dialog, WM_GETFONT, 0, 0));
            if (GetObjectW(original, sizeof(font), &font)) {
                font.lfWeight = FW_BOLD;
                font.lfHeight = font.lfHeight * 3 / 2;
                g_headingFont = CreateFontIndirectW(&font);
                SendDlgItemMessageW(dialog, IDC_HEADING, WM_SETFONT, reinterpret_cast<WPARAM>(g_headingFont), TRUE);
            }
            if (g_smoke) {
                Assets();
                for (const auto id : {IDC_VISION, IDC_ATTACK_RANGE, IDC_ANTI_RECOIL, IDC_TOWER_RANGE,
                                      IDC_CONSOLE, IDC_INSTALL, IDC_CLEAN_PLUGIN, IDC_CLEAN_ALL}) {
                    if (!GetDlgItem(dialog, id))
                        throw deployment::Error(L"界面控件缺失，请重新构建无限视距助手。");
                }
                SetTimer(dialog, 1, 200, nullptr);
            } else {
                const auto detected = Detect();
                if (!detected.empty())
                    ChooseLocation(dialog, detected);
            }
            return TRUE;
        }
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
            case IDC_BROWSE: Browse(dialog); return TRUE;
            case IDC_DETECT: {
                const auto detected = Detect();
                if (detected.empty())
                    Status(dialog, L"未找到游戏目录。请点击“选择目录”，或打开英雄联盟客户端后重试。");
                else
                    ChooseLocation(dialog, detected);
                return TRUE;
            }
            case IDC_INSTALL: Install(dialog); return TRUE;
            case IDC_CLEAN_PLUGIN: Cleanup(dialog, false); return TRUE;
            case IDC_CLEAN_ALL: Cleanup(dialog, true); return TRUE;
            case IDC_OPEN_FOLDER: {
                const auto location = deployment::Inspect(PathText(dialog));
                const auto directory = fs::exists(location.plugins) ? location.plugins : location.game;
                const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(dialog, L"open", directory.c_str(),
                                                                            nullptr, nullptr, SW_SHOWNORMAL));
                if (result <= 32)
                    throw deployment::Error(L"无法打开插件目录。");
                return TRUE;
            }
            case IDCANCEL: DestroyWindow(dialog); return TRUE;
            }
            break;
        case WM_TIMER:
            if (g_smoke) { DestroyWindow(dialog); return TRUE; }
            break;
        case WM_CLOSE: DestroyWindow(dialog); return TRUE;
        case WM_DESTROY:
            if (g_headingFont) { DeleteObject(g_headingFont); g_headingFont = nullptr; }
            PostQuitMessage(0);
            return TRUE;
        }
    } catch (const deployment::Error& error) {
        if (g_smoke) { DestroyWindow(dialog); PostQuitMessage(1); }
        else { Status(dialog, error.message); MessageBoxW(dialog, error.message.c_str(), L"无限视距助手", MB_OK | MB_ICONERROR); }
        return TRUE;
    } catch (const std::exception&) {
        if (g_smoke) { DestroyWindow(dialog); PostQuitMessage(1); }
        else { Status(dialog, L"操作失败，请检查所选目录和文件权限。"); }
        return TRUE;
    }
    return FALSE;
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    g_instance = instance;
    int count = 0;
    auto** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments)
        return 1;
    const bool install = count >= 2 && std::wstring(arguments[1]) == L"--install";
    g_smoke = count >= 2 && std::wstring(arguments[1]) == L"--smoke-test";
    const fs::path target = install && count >= 3 ? arguments[2] : L"";
    LocalFree(arguments);
    if (install) {
        try {
            if (target.empty() || deployment::IsGameRunning())
                throw deployment::Error(L"目录为空或游戏正在运行。");
            const auto location = deployment::Install(target, Assets(),
                deployment::ReadOptions(deployment::Inspect(target)));
            std::wcout << location.plugins.wstring() << L'\n';
            return 0;
        } catch (const deployment::Error& error) { std::wcerr << error.message << L'\n'; return 1; }
        catch (const std::exception&) { return 1; }
    }
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);
    const auto dialog = CreateDialogParamW(instance, MAKEINTRESOURCEW(IDD_ASSISTANT), nullptr, DialogProc, 0);
    if (!dialog) { if (SUCCEEDED(com)) CoUninitialize(); return 1; }
    ShowWindow(dialog, g_smoke ? SW_HIDE : show);
    MSG message{};
    BOOL result = 0;
    while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
        if (!IsDialogMessageW(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (SUCCEEDED(com))
        CoUninitialize();
    return result < 0 ? 1 : static_cast<int>(message.wParam);
}
