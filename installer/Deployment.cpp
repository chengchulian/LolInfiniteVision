#include "Deployment.h"
#include <TlHelp32.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>

namespace deployment {
namespace {
[[noreturn]] void Fail(const std::wstring& message, DWORD code = 0) {
    throw Error(code ? message + L"\n" + WindowsError(code) : message);
}

bool Exists(const fs::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return true;
    const auto error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
        return false;
    Fail(L"无法检查文件：" + path.wstring(), error);
}

void OrdinaryPath(const fs::path& path, bool directory) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const auto error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
            return;
        Fail(L"无法检查目标：" + path.wstring(), error);
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
        ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) != directory)
        Fail(L"目标不是普通" + std::wstring(directory ? L"目录：" : L"文件：") + path.wstring());
}

Bytes ReadFileBytes(const fs::path& path) {
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error || size > 16 * 1024 * 1024)
        Fail(L"无法读取文件或文件过大：" + path.wstring());
    Bytes bytes(static_cast<std::size_t>(size));
    std::ifstream stream(path, std::ios::binary);
    if (!stream || (size && !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))))
        Fail(L"读取文件失败：" + path.wstring());
    return bytes;
}

void WriteFileBytes(const fs::path& path, const Bytes& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream)
        Fail(L"无法写入文件：" + path.wstring());
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream)
        Fail(L"写入文件失败：" + path.wstring());
}

bool SameComponent(const fs::path& left, const fs::path& right) {
    return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
}

bool IsPe64(const Bytes& bytes) {
    if (bytes.size() < 64 || bytes[0] != 'M' || bytes[1] != 'Z')
        return false;
    DWORD offset = 0;
    std::memcpy(&offset, bytes.data() + 0x3c, sizeof(offset));
    return offset <= bytes.size() - 6 && bytes[offset] == 'P' && bytes[offset + 1] == 'E' &&
           bytes[offset + 2] == 0 && bytes[offset + 3] == 0 &&
           bytes[offset + 4] == 0x64 && bytes[offset + 5] == 0x86;
}

struct Change {
    fs::path target, staged;
    Bytes payload, previous;
    bool existed = false;
    bool committed = false;
};
}

std::wstring WindowsError(DWORD code) {
    wchar_t* text = nullptr;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, reinterpret_cast<LPWSTR>(&text), 0, nullptr);
    std::wstring message = length ? std::wstring(text, length) : L"Windows error " + std::to_wstring(code);
    if (text)
        LocalFree(text);
    return message;
}

Location Inspect(const fs::path& selected) {
    std::error_code error;
    auto game = fs::canonical(selected, error);
    if (error)
        Fail(L"请选择有效的游戏目录。");
    if (!Exists(game / L"League of Legends.exe") && Exists(game / L"Game" / L"League of Legends.exe"))
        game = fs::canonical(game / L"Game", error);
    OrdinaryPath(game, true);
    OrdinaryPath(game / L"League of Legends.exe", false);
    if (!Exists(game / L"League of Legends.exe"))
        Fail(L"所选目录中没有 League of Legends.exe。请选择 Game 目录或其上一级安装目录。");

    OrdinaryPath(game / L"version.dll", false);
    OrdinaryPath(game / L"dinput8.dll", false);
    OrdinaryPath(game / L"load.ini", false);
    wchar_t loadPath[32768]{};
    const auto count = GetPrivateProfileStringW(L"Config", L"LoadPath", L"load", loadPath,
        static_cast<DWORD>(std::size(loadPath)), (game / L"load.ini").c_str());
    if (!count || count >= std::size(loadPath) - 1)
        Fail(L"load.ini 中的插件目录无效。");
    const fs::path relative(loadPath);
    if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory())
        Fail(L"load.ini 指向绝对路径。为保证插件写入游戏目录，请使用游戏目录内的相对子目录。");
    auto plugins = (game / relative).lexically_normal();
    if (plugins.filename().empty())
        plugins = plugins.parent_path();
    auto child = plugins.begin();
    for (auto parent = game.begin(); parent != game.end(); ++parent, ++child) {
        if (child == plugins.end() || !SameComponent(*parent, *child))
            Fail(L"load.ini 中的插件目录超出了所选游戏目录。");
    }
    if (child == plugins.end())
        Fail(L"插件必须放在游戏目录的子目录中。");
    auto checked = game;
    for (; child != plugins.end(); ++child) {
        const auto part = child->wstring();
        if (part == L".." || part.find(L':') != std::wstring::npos)
            Fail(L"插件目录包含无效路径。");
        checked /= *child;
        OrdinaryPath(checked, true);
    }
    for (const auto* name : {L"LolInfiniteVision.dll", L"LolInfiniteVision.ini"})
        OrdinaryPath(plugins / name, false);
    return {game, plugins};
}

Options ReadOptions(const Location& location) {
    const auto config = location.plugins / L"LolInfiniteVision.ini";
    return {
        GetPrivateProfileIntW(L"Config", L"visionSwitch", 1, config.c_str()) != 0,
        GetPrivateProfileIntW(L"Config", L"attackRange", 1, config.c_str()) != 0,
        GetPrivateProfileIntW(L"Config", L"antiRecoil", 1, config.c_str()) != 0,
        GetPrivateProfileIntW(L"Config", L"towerRange", 1, config.c_str()) != 0,
        {hotkeys::Normalize(GetPrivateProfileIntW(L"Hotkeys", L"visionSwitch", hotkeys::Defaults[0], config.c_str())),
         hotkeys::Normalize(GetPrivateProfileIntW(L"Hotkeys", L"attackRange", hotkeys::Defaults[1], config.c_str())),
         hotkeys::Normalize(GetPrivateProfileIntW(L"Hotkeys", L"antiRecoil", hotkeys::Defaults[2], config.c_str())),
         hotkeys::Normalize(GetPrivateProfileIntW(L"Hotkeys", L"towerRange", hotkeys::Defaults[3], config.c_str()))}
    };
}

bool IsGameRunning() {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        Fail(L"无法检查游戏是否正在运行。", GetLastError());
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool running = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"League of Legends.exe") == 0) {
                running = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return running;
}

Location Install(const fs::path& selected, const Assets& assets, const Options& options) {
    if (!hotkeys::Unique(options.hotkeys))
        Fail(L"快捷键重复，请为各功能设置不同的快捷键，或清空以禁用快捷键。");
    if (IsGameRunning())
        Fail(L"游戏正在运行，请退出游戏进程后再写入插件。");
    const auto location = Inspect(selected);
    if (!IsPe64(assets.plugin) || assets.config.empty())
        Fail(L"内嵌插件资源无效，请重新构建程序。");

    std::vector<Change> changes;
    for (const auto& item : std::vector<std::pair<fs::path, Bytes>>{
            {location.plugins / L"LolInfiniteVision.dll", assets.plugin},
            {location.plugins / L"LolInfiniteVision.ini", assets.config}}) {
        Change change;
        change.target = item.first;
        change.payload = item.second;
        change.existed = Exists(change.target);
        if (change.existed)
            change.previous = ReadFileBytes(change.target);
        if (change.target.filename() == L"LolInfiniteVision.ini" && change.existed)
            change.payload = change.previous;
        changes.push_back(std::move(change));
    }

    std::error_code error;
    fs::create_directories(location.plugins, error);
    if (error)
        Fail(L"无法创建插件目录：" + location.plugins.wstring(), static_cast<DWORD>(error.value()));
    const auto stage = location.plugins / (L".LolInfiniteVision-stage-" + std::to_wstring(GetCurrentProcessId()) +
        L"-" + std::to_wstring(GetTickCount64()));
    if (!fs::create_directory(stage, error) || error)
        Fail(L"无法创建写入暂存目录。", static_cast<DWORD>(error.value()));
    std::vector<fs::path> temporaryFiles;
    const auto cleanup = [&] {
        std::error_code ignored;
        for (const auto& temporary : temporaryFiles)
            fs::remove(temporary, ignored);
        fs::remove(stage, ignored);
    };
    try {
        for (auto& change : changes) {
            change.staged = stage / change.target.filename();
            temporaryFiles.push_back(change.staged);
            WriteFileBytes(change.staged, change.payload);
            if (change.target.filename() == L"LolInfiniteVision.ini") {
                for (const auto& field : {std::pair{L"visionSwitch", options.visionSwitch},
                                         std::pair{L"attackRange", options.attackRange},
                                         std::pair{L"antiRecoil", options.antiRecoil},
                                         std::pair{L"towerRange", options.towerRange}}) {
                    if (!WritePrivateProfileStringW(L"Config", field.first, field.second ? L"1" : L"0",
                                                   change.staged.c_str()))
                        Fail(L"无法写入无限视距助手配置。", GetLastError());
                }
                for (size_t i = 0; i < options.hotkeys.size(); ++i) {
                    const auto value = std::to_wstring(hotkeys::Normalize(options.hotkeys[i]));
                    if (!WritePrivateProfileStringW(L"Hotkeys", hotkeys::Names[i], value.c_str(), change.staged.c_str()))
                        Fail(L"无法写入快捷键配置。", GetLastError());
                }
                WritePrivateProfileStringW(nullptr, nullptr, nullptr, change.staged.c_str());
            }
        }
        for (auto& change : changes) {
            OrdinaryPath(change.target, false);
            if (!MoveFileExW(change.staged.c_str(), change.target.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                Fail(L"无法替换文件：" + change.target.wstring() + L"。请退出游戏后重试。", GetLastError());
            change.committed = true;
        }
    } catch (...) {
        const auto original = std::current_exception();
        bool restored = true;
        for (auto item = changes.rbegin(); item != changes.rend(); ++item) {
            if (!item->committed)
                continue;
            if (item->existed) {
                try {
                    WriteFileBytes(item->staged, item->previous);
                    restored = MoveFileExW(item->staged.c_str(), item->target.c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE && restored;
                } catch (...) { restored = false; }
            } else {
                restored = DeleteFileW(item->target.c_str()) != FALSE && restored;
            }
        }
        if (!restored)
            Fail(L"写入失败，且部分原文件无法恢复。请检查目录：" + location.plugins.wstring());
        cleanup();
        std::rethrow_exception(original);
    }
    cleanup();
    return location;
}

namespace {
void RemoveFiles(const Location& location, const std::vector<fs::path>& paths) {
    std::vector<std::pair<fs::path, fs::path>> changes;
    for (const auto& path : paths) {
        if (Exists(path))
            changes.emplace_back(path, fs::path{});
    }
    if (changes.empty())
        return;

    const auto stage = location.game / (L".LolInfiniteVision-cleanup-" + std::to_wstring(GetCurrentProcessId()) +
        L"-" + std::to_wstring(GetTickCount64()));
    std::error_code error;
    if (!fs::create_directory(stage, error) || error)
        Fail(L"无法创建清理暂存目录。", static_cast<DWORD>(error.value()));
    try {
        for (auto& change : changes) {
            OrdinaryPath(change.first, false);
            const auto staged = stage / change.first.filename();
            if (!MoveFileExW(change.first.c_str(), staged.c_str(), MOVEFILE_WRITE_THROUGH))
                Fail(L"无法清理文件：" + change.first.wstring() + L"。请退出游戏后重试。", GetLastError());
            change.second = staged;
        }
    } catch (...) {
        const auto original = std::current_exception();
        bool restored = true;
        for (auto change = changes.rbegin(); change != changes.rend(); ++change) {
            if (!change->second.empty())
                restored = MoveFileExW(change->second.c_str(), change->first.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE && restored;
        }
        if (!restored)
            Fail(L"清理失败，且部分文件无法恢复。原文件保留在：" + stage.wstring());
        fs::remove(stage, error);
        std::rethrow_exception(original);
    }
    // Only files moved by this operation are removed; never recursively delete a plugin directory.
    for (const auto& change : changes) {
        if (!DeleteFileW(change.second.c_str()))
            Fail(L"文件已移出插件目录，但无法删除暂存文件：" + change.second.wstring(), GetLastError());
    }
    fs::remove(stage, error);
}

const Bytes& LoaderBytes(const Assets& assets, LoaderKind kind) {
    return kind == LoaderKind::Dinput8 ? assets.dinput8Loader : assets.loader;
}

LoaderState InspectLoader(const fs::path& path, const Bytes& expected) {
    OrdinaryPath(path, false);
    if (!Exists(path)) return LoaderState::Missing;
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error) Fail(L"无法读取文件：" + path.wstring());
    if (!IsPe64(expected) || size != expected.size()) return LoaderState::Foreign;
    return ReadFileBytes(path) == expected ? LoaderState::Matching : LoaderState::Foreign;
}
}

const wchar_t* LoaderName(LoaderKind kind) {
    return kind == LoaderKind::Dinput8 ? L"dinput8.dll" : L"version.dll";
}

Loaders InspectLoaders(const Location& location, const Assets& assets) {
    return {InspectLoader(location.game / L"version.dll", assets.loader),
        InspectLoader(location.game / L"dinput8.dll", assets.dinput8Loader)};
}

std::wstring DescribeLoaders(const Loaders& state) {
    std::wstring text;
    if (state.version == LoaderState::Matching && state.dinput8 == LoaderState::Matching)
        text = L"存在多个加载器：version.dll、dinput8.dll。请仅保留一个加载器。";
    else if (state.version == LoaderState::Matching || state.dinput8 == LoaderState::Matching)
        text = std::wstring(L"当前加载器：") + (state.version == LoaderState::Matching ? L"version.dll" : L"dinput8.dll");
    else
        text = L"当前加载器：无加载器，视距插件不生效。";
    if (state.version == LoaderState::Foreign)
        text += L"\r\nversion.dll 加载器已被其他 DLL 占用（已跳过）";
    if (state.dinput8 == LoaderState::Foreign)
        text += L"\r\ndinput8.dll 加载器已被其他 DLL 占用（已跳过）";
    return text;
}

Location InstallLoader(const fs::path& selected, const Assets& assets, LoaderKind kind) {
    if (IsGameRunning()) Fail(L"游戏正在运行，请退出游戏进程后再写入加载器。");
    const auto location = Inspect(selected);
    const auto& bytes = LoaderBytes(assets, kind);
    if (!IsPe64(bytes)) Fail(L"内嵌加载器资源无效，请重新构建程序。");
    const auto target = location.game / LoaderName(kind);
    const auto current = InspectLoader(target, bytes);
    if (current == LoaderState::Foreign)
        Fail(std::wstring(LoaderName(kind)) + L" 加载器已被其他 DLL 占用，已跳过写入。");
    if (current == LoaderState::Matching) return location;
    const auto other = location.game / LoaderName(kind == LoaderKind::Version ? LoaderKind::Dinput8 : LoaderKind::Version);
    const auto otherKind = kind == LoaderKind::Version ? LoaderKind::Dinput8 : LoaderKind::Version;
    if (InspectLoader(other, LoaderBytes(assets, otherKind)) == LoaderState::Matching)
        Fail(L"目录中已存在另一种本项目加载器。请先清理另一种加载器，再写入所选加载器，避免重复加载。");
    const auto stage = location.game / (L".LolInfiniteVision-loader-" + std::to_wstring(GetCurrentProcessId()) +
        L"-" + std::to_wstring(GetTickCount64()));
    std::error_code error;
    if (!fs::create_directory(stage, error) || error) Fail(L"无法创建加载器暂存目录。");
    const auto staged = stage / LoaderName(kind);
    try {
        WriteFileBytes(staged, bytes);
        OrdinaryPath(target, false);
        // Never replace a file created by another application after inspection.
        if (!MoveFileExW(staged.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH))
            Fail(L"无法写入加载器。", GetLastError());
    } catch (...) {
        fs::remove(staged, error);
        fs::remove(stage, error);
        throw;
    }
    fs::remove(stage, error);
    return location;
}

Location RemoveLoader(const fs::path& selected, const Assets& assets, LoaderKind kind) {
    if (IsGameRunning()) Fail(L"游戏正在运行，请退出游戏进程后再清理加载器。");
    const auto location = Inspect(selected);
    const auto target = location.game / LoaderName(kind);
    const auto state = InspectLoader(target, LoaderBytes(assets, kind));
    if (state == LoaderState::Missing) return location;
    if (state != LoaderState::Matching) Fail(std::wstring(LoaderName(kind)) + L" 加载器已被其他 DLL 占用，已跳过清理。");
    if (Exists(location.plugins)) {
        for (const auto& item : fs::directory_iterator(location.plugins)) {
            if (_wcsicmp(item.path().extension().c_str(), L".dll") == 0 &&
                _wcsicmp(item.path().filename().c_str(), L"LolInfiniteVision.dll") != 0)
                Fail(L"插件目录中还有其他 DLL，可能共用加载器。请先处理其他插件，再清理加载器。");
        }
    }
    RemoveFiles(location, {target});
    return location;
}

Location Uninstall(const fs::path& selected) {
    if (IsGameRunning()) Fail(L"游戏正在运行，请退出游戏进程后再清理插件。");
    const auto location = Inspect(selected);
    RemoveFiles(location, {location.plugins / L"LolInfiniteVision.dll", location.plugins / L"LolInfiniteVision.ini"});
    return location;
}
} // namespace deployment
