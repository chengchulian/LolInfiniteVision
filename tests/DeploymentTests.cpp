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
        Text("[Config]\nvisionSwitch=1\nattackRange=1\nantiRecoil=1\ntowerRange=1\nconsole=0\ncustomSetting=250\n"), Pe(4)};
    using Kind = deployment::LoaderKind;
    using State = deployment::LoaderState;

    const std::array<UINT, 4> customKeys{VK_F10 | (2U << 8), VK_F11, 0, VK_F12 | (5U << 8)};
    const auto installed = deployment::Install(root, assets, {true, false, true, false, customKeys});
    Check(installed.game == game && installed.plugins == game / L"load", "Game directory normalization failed");
    Check(!fs::exists(game / L"version.dll") && !fs::exists(game / L"dinput8.dll"),
          "Plugin deployment unexpectedly installed a loader");
    Check(deployment::InspectLoaders(installed, assets).version == State::Missing &&
          deployment::DescribeLoaders(deployment::InspectLoaders(installed, assets)).find(L"无加载器") != std::wstring::npos,
          "Missing loader status is wrong");
    deployment::InstallLoader(game, assets, Kind::Version);
    deployment::InstallLoader(game, assets, Kind::Version); // Reuse identical loader.
    Check(Read(game / L"version.dll") == assets.loader, "Version loader differs from payload");
    Reject([&] { deployment::InstallLoader(game, assets, Kind::Dinput8); }, "Installed two project loaders");
    Check(!fs::exists(game / L"dinput8.dll"), "Rejected dual-loader install changed files");
    Write(game / L"dinput8.dll", assets.dinput8Loader);
    Check(deployment::DescribeLoaders(deployment::InspectLoaders(installed, assets)).find(L"存在多个加载器") != std::wstring::npos,
          "Multiple loader status is wrong");
    deployment::RemoveLoader(game, assets, Kind::Dinput8);
    Check(Read(game / L"version.dll") == assets.loader && !fs::exists(game / L"dinput8.dll"),
          "Removing one loader changed the other");
    Check(Read(installed.plugins / L"LolInfiniteVision.dll") == assets.plugin, "Plugin differs from payload");
    const auto options = deployment::ReadOptions(installed);
    Check(options.visionSwitch && !options.attackRange && options.antiRecoil && !options.towerRange && options.hotkeys == customKeys,
          "GUI options were not deployed");

    const auto config = installed.plugins / L"LolInfiniteVision.ini";
    const auto sentinel = Text("another mod must stay unchanged");
    Write(installed.plugins / L"Other.dll", sentinel);
    Write(game / L"LolSurrender.ini", sentinel);
    Write(installed.plugins / L"LolSurrender.dll", sentinel);
    Check(WritePrivateProfileStringW(L"Config", L"customSetting", L"777", config.c_str()) != FALSE,
          "Cannot set existing custom config");
    Check(WritePrivateProfileStringW(L"Config", L"console", L"1", config.c_str()) != FALSE,
          "Cannot set hidden developer config");
    deployment::Install(game, assets, {false, true, false, true});
    const auto updated = deployment::ReadOptions(installed);
    Check(!updated.visionSwitch && updated.attackRange && !updated.antiRecoil && updated.towerRange && updated.hotkeys == hotkeys::Defaults,
          "Existing config options did not update");
    Check(GetPrivateProfileIntW(L"Config", L"customSetting", 0, config.c_str()) == 777,
          "Updating options discarded custom settings");
    Check(GetPrivateProfileIntW(L"Config", L"console", 0, config.c_str()) == 1,
          "Updating GUI options overwrote hidden developer config");
    const auto beforeDuplicate = Read(config);
    Reject([&] { deployment::Install(game, assets, {true, true, true, true, {VK_F6, VK_F6, 0, 0}}); },
           "Duplicate shortcuts were accepted");
    Check(Read(config) == beforeDuplicate, "Rejected shortcuts changed the config");
    Check(Read(installed.plugins / L"Other.dll") == sentinel && Read(game / L"LolSurrender.ini") == sentinel &&
          Read(installed.plugins / L"LolSurrender.dll") == sentinel,
          "Deployment modified an unrelated mod");

    const auto originalConfig = Read(config);
    Write(game / L"version.dll", sentinel);
    Reject([&] { deployment::InstallLoader(game, assets, Kind::Version); }, "Different existing loader was overwritten");
    Check(Read(game / L"version.dll") == sentinel && Read(config) == originalConfig,
          "Conflicting loader changed files before rejection");
    deployment::Install(game, assets, deployment::ReadOptions(installed));
    Check(Read(game / L"version.dll") == sentinel, "Plugin deployment modified a foreign loader");
    auto loaderState = deployment::InspectLoaders(installed, assets);
    Check(loaderState.version == State::Foreign && loaderState.dinput8 == State::Missing &&
          deployment::DescribeLoaders(loaderState).find(L"version.dll 加载器已被其他 DLL 占用") != std::wstring::npos &&
          deployment::DescribeLoaders(loaderState).find(L"无加载器") != std::wstring::npos,
          "Foreign DLL was recognized as a project loader");
    deployment::InstallLoader(game, assets, Kind::Dinput8);
    loaderState = deployment::InspectLoaders(installed, assets);
    Check(loaderState.dinput8 == State::Matching && Read(game / L"version.dll") == sentinel &&
          deployment::DescribeLoaders(loaderState).find(L"存在多个加载器") == std::wstring::npos,
          "Foreign DLL was counted as a second loader or overwritten");
    Write(game / L"dinput8.dll", sentinel);
    loaderState = deployment::InspectLoaders(installed, assets);
    Check(loaderState.version == State::Foreign && loaderState.dinput8 == State::Foreign &&
          deployment::DescribeLoaders(loaderState).find(L"存在多个加载器") == std::wstring::npos,
          "Two foreign DLLs were counted as project loaders");
    Reject([&] { deployment::RemoveLoader(game, assets, Kind::Dinput8); }, "Foreign dinput8 DLL was deleted");
    fs::remove(game / L"dinput8.dll");
    Write(game / L"version.dll", assets.loader);

    const HANDLE locked = CreateFileW(config.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(locked != INVALID_HANDLE_VALUE, "Cannot lock fixture for rollback test");
    auto changed = assets;
    changed.plugin = Pe(8);
    Reject([&] { deployment::Install(game, changed, {true, false, true, false}); }, "Locked target unexpectedly replaced");
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
    Reject([&] { deployment::RemoveLoader(game, assets, Kind::Version); }, "Shared loader was removed");
    Check(Read(game / L"version.dll") == assets.loader && Read(config) == beforeCleanup,
          "Rejected shared-loader cleanup changed files");
    deployment::Uninstall(game);
    Check(!fs::exists(config) && !fs::exists(installed.plugins / L"LolInfiniteVision.dll"),
          "Plugin cleanup left plugin files");
    Check(Read(game / L"version.dll") == assets.loader && Read(installed.plugins / L"LolSurrender.dll") == sentinel,
          "Plugin cleanup modified the loader or another plugin");
    deployment::Uninstall(game); // Already clean is successful.
    deployment::Install(game, assets, {});
    fs::remove(installed.plugins / L"Other.dll");
    fs::remove(installed.plugins / L"LolSurrender.dll");
    Write(game / L"version.dll", sentinel);
    Reject([&] { deployment::RemoveLoader(game, assets, Kind::Version); }, "Foreign loader was removed");
    Check(fs::exists(config) && Read(game / L"version.dll") == sentinel,
          "Rejected foreign-loader cleanup changed files");
    Write(game / L"version.dll", assets.loader);
    const HANDLE cleanupLock = CreateFileW(config.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(cleanupLock != INVALID_HANDLE_VALUE, "Cannot lock cleanup fixture");
    Reject([&] { deployment::Uninstall(game); }, "Locked plugin cleanup unexpectedly succeeded");
    CloseHandle(cleanupLock);
    Check(Read(installed.plugins / L"LolInfiniteVision.dll") == assets.plugin && fs::exists(config) &&
          Read(game / L"version.dll") == assets.loader, "Failed cleanup did not restore files");
    deployment::RemoveLoader(game, assets, Kind::Version);
    Check(fs::exists(config) && fs::exists(installed.plugins / L"LolInfiniteVision.dll"),
          "Loader cleanup deleted plugin files");
    deployment::RemoveLoader(game, assets, Kind::Version); // Already clean is successful.
    deployment::InstallLoader(game, assets, Kind::Dinput8);
    const HANDLE loaderLock = CreateFileW((game / L"dinput8.dll").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(loaderLock != INVALID_HANDLE_VALUE, "Cannot lock loader fixture");
    Reject([&] { deployment::RemoveLoader(game, assets, Kind::Dinput8); }, "Locked loader cleanup succeeded");
    CloseHandle(loaderLock);
    Check(Read(game / L"dinput8.dll") == assets.dinput8Loader && fs::exists(config), "Failed loader cleanup changed files");
    deployment::RemoveLoader(game, assets, Kind::Dinput8);
    deployment::Uninstall(game);
    Check(!fs::exists(config) && !fs::exists(installed.plugins / L"LolInfiniteVision.dll") &&
          !fs::exists(game / L"version.dll") && !fs::exists(game / L"dinput8.dll"), "Separate cleanups left plugin or loader files");
    deployment::Uninstall(game);
    for (const auto& item : fs::directory_iterator(game))
        Check(item.path().filename().wstring().find(L".LolInfiniteVision-cleanup-") != 0,
              "Cleanup left temporary files");

    // The fixture was created here; resolve and verify its boundary before recursive cleanup.
    Check(fs::canonical(root).parent_path() == parent && root.filename().wstring().find(L"deployment-fixture-") == 0,
          "Fixture cleanup escaped the test workspace");
    fs::remove_all(root);
    std::puts("All deployment, configuration, conflict, rollback and cleanup tests passed");
}
