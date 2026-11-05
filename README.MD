# Lol Infinite Vision

Windows x64/C++ 项目，提供单文件“无限视距助手”。采用与投降助手相同的拷贝文件部署方式：助手内嵌 DLL、默认配置和已有加载器，写入游戏目录后，下次进入对局时加载。DLL 加载后扫描时间特征码，定位游戏时间基址，等待游戏时间可用，然后按配置对若干特征码位置进行补丁写入。

## 当前功能

- 启动后延迟初始化，避免过早扫描
- 扫描 `TIME_SIGNATURE_CODE`，定位时间基址
- 持续读取游戏时间，直到大于 `0`
- 按配置启用/关闭以下补丁：
  - `visionSwitch`
  - `attackRange`
  - `antiRecoil`（视距防回弹）
  - `towerRange`
- 可选打开控制台输出，查看扫描和补丁结果

## 配置文件

根目录下提供默认配置文件 `LolInfiniteVision.ini`：

构建后会自动将该配置复制到 DLL 输出目录。运行时从 DLL 所在目录读取 `LolInfiniteVision.ini`，部署时请将配置文件与 DLL 放在同一目录。配置缺失时使用代码中的默认值；重新构建链接后，输出目录的配置会更新为项目根目录中的版本。

```ini
[Config]
console=0
visionSwitch=1
attackRange=1
antiRecoil=1
towerRange=1
```

- `console=1`：DLL 加载后打开控制台
- `console=0`：不打开控制台
- `visionSwitch=1`：启用视距相关补丁
- `attackRange=1`：启用攻击距离相关补丁
- `antiRecoil=1`：启用视距防回弹相关补丁
- `towerRange=1`：启用防御塔范围相关补丁

## 使用无限视距助手

运行 `dist/无限视距助手.exe`，按系统提示授予管理员权限，无需安装 Python 或另外携带 DLL。

1. 退出游戏进程，可以保留英雄联盟客户端。
2. 点击“选择目录”，选择包含 `League of Legends.exe` 的 `Game` 目录或上一级安装目录；也可使用“自动查找”。
3. 设置“无限视距”“攻击范围”“视距防回弹”“防御塔范围”“显示调试窗口”。
4. 点击“写入视距插件”，下次进入对局时加载。修改配置后需要重新启动游戏进程。

界面“清理”区域提供两个按钮，均需先退出游戏：

- **清理插件**：删除 `LolInfiniteVision.dll` 和同目录配置，保留加载器与其他插件。
- **清理插件和注入器**：同时删除游戏目录的 `version.dll`。若加载器内容不同，或插件目录还存在其他 DLL，会停止清理并提示，避免影响共用加载器的插件。

清理时先将目标文件移入暂存目录；文件被占用等导致移动失败时恢复已移动的文件。重复清理已经移除的文件会直接成功。

默认部署布局：

```text
Game/
  version.dll
  load/
    LolInfiniteVision.dll
    LolInfiniteVision.ini
```

若 `load.ini` 已设置 `[Config] LoadPath`，沿用游戏目录内的相对子目录。相同的 `version.dll` 直接复用；内容不同则停止写入，不覆盖现有加载器。与投降助手使用同一个加载器时，可将两个插件放在同一个插件目录。

只更新无限视距的 DLL 和配置；保留其他插件及已有配置中的额外设置。文件先写入暂存目录，再逐一替换；写入失败时尝试恢复原文件。游戏运行时阻止写入。“写入完成”仅表示文件部署成功，游戏中的功能是否生效需进入对局验证。

## 构建方式

安装 Visual Studio 2022 / 2026 或 Build Tools，包含 MSVC x64/x86、Windows SDK 和 C++ CMake 工具。在任意目录运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\dev\ClionProjects\LolInfiniteVision\tools\Build.ps1"
```

脚本自动查找工具链，编译 DLL 和原生助手，执行部署与清理回滚、界面控件和启动、最终 EXE 部署测试。测试成功后输出 `dist/Release/`，并将单文件助手复制到 `dist/无限视距助手.exe`。非管理员终端会跳过需要管理员权限的 EXE 测试。

也可通过 CMake 预设构建，例如 Visual Studio 2022：

```powershell
cmake --preset vs2022-x64-release
cmake --build --preset build-vs2022-release
ctest --preset test-vs2022-release
```

预设构建输出位于对应的 `build/` 目录；使用 `tools/Build.ps1` 生成发布目录。修改 DLL、INI 或加载器后重新构建，内嵌资源会自动更新。原生助手不依赖旧 Python/PyInstaller 注入器及其更新脚本，旧版文件不参与构建。

## 项目结构

- `src/`：C++ 源码与头文件
- `installer/`：原生助手界面、文件部署与资源清单
- `installer/assets/version.dll`：与投降助手相同的现有加载器
- `tools/`：构建、资源生成和 EXE 验证脚本
- `tests/`：部署、配置保留、冲突检查和失败回滚测试
- `dist/`：单文件助手及发布目录
- `build/`：CMake 构建输出目录
- 根目录：`CMakeLists.txt`、`CMakePresets.json`、`LolInfiniteVision.ini`

## 代码模块

- `dllmain.cpp`：DLL 入口、配置读取、定时初始化、补丁调度
- `GameTime.cpp`：时间特征码扫描与时间基址解析
- `Method.cpp`：控制台输出、模块地址范围、特征码扫描、配置读取
- `Memory.cpp`：进程内存读写封装
- `Constant.cpp`：特征码、补丁字节和配置文件名常量

## 运行说明

当前逻辑依赖目标进程的模块布局与特征码匹配结果。若特征码未命中，程序会输出告警并跳过对应补丁。
