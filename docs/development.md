# 构建与验证

[返回首页](../README.MD) · [架构说明](architecture.md)

## 环境

目标平台为 Windows x64，代码使用 C++17。安装 Visual Studio 2022 / 2026 或对应 Build Tools，包含 MSVC x64/x86、Windows SDK 和 C++ CMake 工具。建议使用安装包附带的 CMake；使用预设时需支持 `CMakePresets.json` 的 version 6 格式。

下面的命令均从仓库根目录执行。构建不依赖 Python；仅离线 dump 核对脚本需要 Python，且无需第三方 Python 包。

## 构建发布包

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Build.ps1
```

脚本自动查找工具链、配置 CMake、编译 DLL 和助手，再运行 CTest。测试失败时停止打包。需要 Debug 构建时添加 `-Configuration Debug`。

默认 Release 输出：

```text
dist/
  无限视距助手.exe
  Release/
    无限视距助手.exe
    LolInfiniteVision.dll
    LolInfiniteVision.ini
    README.md
    docs/
```

EXE 已内嵌插件、默认配置和两种加载器，单独使用 EXE 即可部署。发布目录中的 DLL、INI 和文档供单独部署、检查及查阅。

助手资源由 `tools/GenerateAssistantResource.cmake` 生成。修改插件、INI、界面资源或任一种加载器后重新构建，资源依赖会触发更新。

## 使用 CMake 预设

Visual Studio 2022 的 Release 构建与测试：

```powershell
cmake --preset vs2022-x64-release
cmake --build --preset build-vs2022-release
ctest --preset test-vs2022-release
```

Visual Studio 2026 的 Release 构建与测试：

```powershell
cmake --preset vs2026-x64-release
cmake --build --preset build-x64-release
ctest --test-dir build/vs2026-x64-release -C Release --output-on-failure
```

预设构建使用 `build/<配置预设名>/`；`Build.ps1` 使用 `build/vs2022-x64/` 或 `build/vs2026-x64/`。CMake 预设只生成构建输出，发布包由 `Build.ps1` 生成。

另有 Ninja 的 Debug / Release 预设，需要先配置 MSVC 或 clang-cl 的 x64 工具链和 Windows SDK 环境。

## 测试范围

| CTest 名称 | 验证内容 |
| --- | --- |
| `reversible-feature-patches` | 补丁开启、关闭、重复切换、页面保护恢复和字节归属检查。 |
| `in-game-hotkeys` | 组合键匹配、前台条件、长按去重、无效及重复绑定。 |
| `native-attack-range-offline` | 自建镜像中的特征码解析、窗口线程派发、开关、异常恢复与回调停止。 |
| `plugin-deployment` | 插件与加载器独立操作、加载器状态、其他 DLL 保护、配置保留和失败回滚。 |
| `assistant-gui-startup` | 助手启动、内嵌资源、界面控件及快捷键捕获。 |
| `assistant-packaged-deployment` | 最终 EXE 在测试目录部署插件和两种加载器，验证共用与占用保护。 |

后两项需要管理员权限，非管理员终端会跳过。部署测试会检查游戏是否运行；执行整套测试前退出游戏进程，避免部署操作被拒绝。

测试使用自建镜像或临时目录，不能证明实际对局中的功能效果。攻击范围的静态复核见[原生攻击距离事件](native-attack-range.md)。

## 助手命令行

以下命令与界面共用部署逻辑，需要管理员权限，并要求游戏进程已退出。将示例中的目录替换为实际游戏目录。

```powershell
# 仅写入插件和配置，沿用已有配置值。
& .\dist\无限视距助手.exe --install "D:\Games\League of Legends\Game"

# 单独写入一种加载器，类型可为 version 或 dinput8。
& .\dist\无限视距助手.exe --install-loader "D:\Games\League of Legends\Game" version

# 仅清理指定加载器，保留插件和配置。
& .\dist\无限视距助手.exe --remove-loader "D:\Games\League of Legends\Game" version
```

返回码 `0` 表示操作完成，`1` 表示失败。插件部署成功不表示目录中已有加载器，也不表示游戏内效果已验证。`--smoke-test` 供界面启动测试使用。
