#pragma once
#include <Windows.h>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>
#include <utility>
#include "../src/Hotkeys.h"

namespace deployment {
namespace fs = std::filesystem;
using Bytes = std::vector<unsigned char>;

struct Error : std::exception {
    std::wstring message;
    explicit Error(std::wstring text) : message(std::move(text)) {}
    const char* what() const noexcept override { return "Plugin deployment failed"; }
};

struct Options {
    bool visionSwitch = true;
    bool attackRange = true;
    bool antiRecoil = true;
    bool towerRange = true;
    std::array<UINT, 4> hotkeys = hotkeys::Defaults;
};
struct Assets { Bytes plugin, loader, config, dinput8Loader; };
struct Location { fs::path game, plugins; };
enum class LoaderKind { Version, Dinput8 };
enum class LoaderState { Missing, Matching, Foreign };
struct Loaders { LoaderState version, dinput8; };

std::wstring WindowsError(DWORD code);
Location Inspect(const fs::path& selected);
Options ReadOptions(const Location& location);
bool IsGameRunning();
const wchar_t* LoaderName(LoaderKind kind);
Loaders InspectLoaders(const Location& location, const Assets& assets);
std::wstring DescribeLoaders(const Loaders& state);
Location InstallLoader(const fs::path& selected, const Assets& assets, LoaderKind kind);
Location RemoveLoader(const fs::path& selected, const Assets& assets, LoaderKind kind);
Location Install(const fs::path& selected, const Assets& assets, const Options& options);
Location Uninstall(const fs::path& selected);
} // namespace deployment
