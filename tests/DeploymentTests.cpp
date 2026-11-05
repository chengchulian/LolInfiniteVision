#include "Deployment.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>

namespace {
namespace fs = std::filesystem;
using deployment::Bytes;

void Check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(EXIT_FAILURE); }
}

void Write(const fs::path& file, const Bytes& bytes) {
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    Check(static_cast<bool>(stream), "Cannot write fixture file");
}

Bytes Read(const fs::path& file) {
    std::ifstream stream(file, std::ios::binary);
    Check(static_cast<bool>(stream), "Cannot read deployed file");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

Bytes Text(const std::string& value) { return {value.begin(), value.end()}; }
Bytes Pe(unsigned char marker) {
    Bytes bytes(128, 0);
    bytes[0] = 'M'; bytes[1] = 'Z'; bytes[0x3c] = 64;
    bytes[64] = 'P'; bytes[65] = 'E'; bytes[68] = 0x64; bytes[69] = 0x86;
    bytes[32] = marker;
    return bytes;
}

void Reject(const std::function<void()>& operation, const char* message) {
    try { operation(); }
    catch (const deployment::Error&) { return; }
    Check(false, message);
}
}

int main() {
    const auto parent = fs::canonical(fs::current_path());
    const auto root = parent / (L"deployment-fixture-" + std::to_wstring(GetCurrentProcessId()) +
        L"-" + std::to_wstring(GetTickCount64()));
    Check(fs::create_directory(root), "Fixture must be a newly created directory");
    const auto game = root / L"Game";
    fs::create_directory(game);
    Write(game / L"League of Legends.exe", Pe(1));
    const deployment::Assets assets{Pe(2), Pe(3),
        Text("[Config]\nvisionSwitch=1\nattackRange=1\nantiRecoil=1\ntowerRange=1\nconsole=0\ncustomSetting=250\n")};

    const auto installed = deployment::Install(root, assets, {true, false, true, false, true});
    Check(installed.game == game && installed.plugins == game / L"load", "Game directory normalization failed");
    Check(Read(game / L"version.dll") == assets.loader, "Loader differs from payload");
    Check(Read(installed.plugins / L"LolInfiniteVision.dll") == assets.plugin, "Plugin differs from payload");
    const auto options = deployment::ReadOptions(installed);
    Check(options.visionSwitch && !options.attackRange && options.antiRecoil && !options.towerRange && options.console,
          "GUI options were not deployed");

    const auto config = installed.plugins / L"LolInfiniteVision.ini";
    const auto sentinel = Text("another mod must stay unchanged");
    Write(installed.plugins / L"Other.dll", sentinel);
    Write(game / L"LolSurrender.ini", sentinel);
    Write(installed.plugins / L"LolSurrender.dll", sentinel);
    Check(WritePrivateProfileStringW(L"Config", L"customSetting", L"777", config.c_str()) != FALSE,
          "Cannot set existing custom config");
    deployment::Install(game, assets, {false, true, false, true, false});
    const auto updated = deployment::ReadOptions(installed);
    Check(!updated.visionSwitch && updated.attackRange && !updated.antiRecoil && updated.towerRange && !updated.console,
          "Existing config options did not update");
    Check(GetPrivateProfileIntW(L"Config", L"customSetting", 0, config.c_str()) == 777,
          "Updating options discarded custom settings");
    Check(Read(installed.plugins / L"Other.dll") == sentinel && Read(game / L"LolSurrender.ini") == sentinel &&
          Read(installed.plugins / L"LolSurrender.dll") == sentinel,
          "Deployment modified an unrelated mod");

    const auto originalConfig = Read(config);
    Write(game / L"version.dll", sentinel);
    Reject([&] { deployment::Install(game, assets, {}); }, "Different existing loader was overwritten");
    Check(Read(game / L"version.dll") == sentinel && Read(config) == originalConfig,
          "Conflicting loader changed files before rejection");
    Write(game / L"version.dll", assets.loader);

    const HANDLE locked = CreateFileW(config.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(locked != INVALID_HANDLE_VALUE, "Cannot lock fixture for rollback test");
    auto changed = assets;
    changed.plugin = Pe(8);
    Reject([&] { deployment::Install(game, changed, {true, false, true, false, true}); }, "Locked target unexpectedly replaced");
    CloseHandle(locked);
    Check(Read(installed.plugins / L"LolInfiniteVision.dll") == assets.plugin && Read(config) == originalConfig,
          "Partial deployment was not rolled back");
    for (const auto& item : fs::directory_iterator(installed.plugins))
        Check(item.path().filename().wstring().find(L".LolInfiniteVision-stage-") != 0, "Successful rollback left temporary files");

    const auto loadIni = game / L"load.ini";
    Write(loadIni, Text("[Config]\nLoadPath=plugins\\custom\n"));
    const auto custom = deployment::Install(game, assets, {});
    Check(custom.plugins == game / L"plugins" / L"custom", "Existing relative loader configuration was ignored");
    Check(Read(custom.plugins / L"LolInfiniteVision.dll") == assets.plugin, "Custom plugin directory is empty");

    Write(loadIni, Text("[Config]\nLoadPath=..\\outside\n"));
    Reject([&] { deployment::Install(game, assets, {}); }, "Directory traversal was accepted");
    Write(loadIni, Text("[Config]\nLoadPath=C:\\outside\n"));
    Reject([&] { deployment::Install(game, assets, {}); }, "Absolute plugin directory was accepted");
    Write(loadIni, Text("[Config]\nLoadPath=.\n"));
    Reject([&] { deployment::Install(game, assets, {}); }, "Game root accepted as plugin subdirectory");
    fs::remove(loadIni);
    Reject([&] { deployment::Install(root / L"missing", assets, {}); }, "Missing game directory accepted");
    auto invalid = assets;
    invalid.plugin = Text("invalid");
    Reject([&] { deployment::Install(game, invalid, {}); }, "Invalid embedded PE accepted");

    // Cleanup protects shared/foreign loaders and restores files when a target is locked.
    const auto beforeCleanup = Read(config);
    Reject([&] { deployment::Uninstall(game, assets.loader, true); }, "Shared loader was removed");
    Check(Read(game / L"version.dll") == assets.loader && Read(config) == beforeCleanup,
          "Rejected shared-loader cleanup changed files");
    deployment::Uninstall(game, assets.loader, false);
    Check(!fs::exists(config) && !fs::exists(installed.plugins / L"LolInfiniteVision.dll"),
          "Plugin cleanup left plugin files");
    Check(Read(game / L"version.dll") == assets.loader && Read(installed.plugins / L"LolSurrender.dll") == sentinel,
          "Plugin cleanup modified the loader or another plugin");
    deployment::Uninstall(game, assets.loader, false); // Already clean is successful.
    deployment::Install(game, assets, {});
    fs::remove(installed.plugins / L"Other.dll");
    fs::remove(installed.plugins / L"LolSurrender.dll");
    Write(game / L"version.dll", sentinel);
    Reject([&] { deployment::Uninstall(game, assets.loader, true); }, "Foreign loader was removed");
    Check(fs::exists(config) && Read(game / L"version.dll") == sentinel,
          "Rejected foreign-loader cleanup changed files");
    Write(game / L"version.dll", assets.loader);
    const HANDLE cleanupLock = CreateFileW(config.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(cleanupLock != INVALID_HANDLE_VALUE, "Cannot lock cleanup fixture");
    Reject([&] { deployment::Uninstall(game, assets.loader, true); }, "Locked cleanup unexpectedly succeeded");
    CloseHandle(cleanupLock);
    Check(Read(installed.plugins / L"LolInfiniteVision.dll") == assets.plugin && fs::exists(config) &&
          Read(game / L"version.dll") == assets.loader, "Failed cleanup did not restore files");
    deployment::Uninstall(game, assets.loader, true);
    Check(!fs::exists(config) && !fs::exists(installed.plugins / L"LolInfiniteVision.dll") &&
          !fs::exists(game / L"version.dll"), "Full cleanup left plugin or loader files");
    deployment::Uninstall(game, assets.loader, true);
    for (const auto& item : fs::directory_iterator(game))
        Check(item.path().filename().wstring().find(L".LolInfiniteVision-cleanup-") != 0,
              "Cleanup left temporary files");

    // The fixture was created here; resolve and verify its boundary before recursive cleanup.
    Check(fs::canonical(root).parent_path() == parent && root.filename().wstring().find(L"deployment-fixture-") == 0,
          "Fixture cleanup escaped the test workspace");
    fs::remove_all(root);
    std::puts("All deployment, configuration, conflict, rollback and cleanup tests passed");
}
