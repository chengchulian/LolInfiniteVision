#pragma once
#include <Windows.h>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>
#include <utility>

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
    bool console = false;
};
struct Assets { Bytes plugin, loader, config; };
struct Location { fs::path game, plugins; };

std::wstring WindowsError(DWORD code);
Location Inspect(const fs::path& selected);
Options ReadOptions(const Location& location);
bool IsGameRunning();
Location Install(const fs::path& selected, const Assets& assets, const Options& options);
Location Uninstall(const fs::path& selected, const Bytes& loader, bool removeLoader);
} // namespace deployment
