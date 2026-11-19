# Lol Infinite Vision

Windows x64/C++ 项目，提供单文件“无限视距助手”。采用与其他插件相同的拷贝文件部署方式：助手内嵌 DLL、默认配置及 version / dinput8 加载器。插件与加载器分别写入游戏目录，下次进入对局时加载。DLL 加载后扫描时间特征码，定位游戏时间基址，等待游戏时间可用，然后按配置对若干特征码位置进行补丁写入。

## 当前功能

- 启动后延迟初始化，避免过早扫描
- 扫描 `TIME_SIGNATURE_CODE`，定位时间基址
- 持续读取游戏时间，直到大于 `0`
- 通过助手设置初始开关，并在游戏内通过可配置快捷键切换：
  - `visionSwitch`
  - `attackRange`
  - `antiRecoil`（视距防回弹）
  - `towerRange`
- 开发者调试选项仅通过配置文件设置

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

[Hotkeys]
visionSwitch=629
attackRange=630
antiRecoil=631
towerRange=632
```

- `console=1`：DLL 加载后打开控制台
- `console=0`：不打开控制台
- `visionSwitch=1`：启用视距相关补丁
- `attackRange=1`：进入对局后持续派发原生攻击距离事件，使普通攻击距离圈常显
- `antiRecoil=1`：启用视距防回弹相关补丁
- `towerRange=1`：启用防御塔范围相关补丁

`[Config]` 中四个功能开关决定每次进入游戏的初始状态。`[Hotkeys]` 默认依次为 Ctrl+F6（无限视距）、Ctrl+F7（攻击范围）、Ctrl+F8（视距防回弹）、Ctrl+F9（防御塔范围）。在 EXE 中点击对应快捷键输入框，直接按下想绑定的按键即可捕获，支持 Ctrl、Alt、Shift 组合；按退格键清空，清空后不响应快捷键。不同功能不能绑定相同组合。

快捷键只在游戏窗口位于前台时响应，长按只切换一次，松开后可再次切换。游戏内切换不写回 INI，下次进入游戏仍使用配置的初始状态。视距、防回弹、塔范围在关闭时恢复原始补丁字节；攻击范围关闭时派发原生隐藏事件。

手动编辑时，快捷键为 Windows 虚拟键码加修饰键编码：`键码 + 256 × 修饰键`，Shift=1、Ctrl=2、Alt=4，可相加。例如 Ctrl+F6 为 `629`，`0` 表示禁用。无效绑定会禁用，直接修改 INI 造成重复时，重复的绑定均禁用。缺少 `[Hotkeys]` 的旧配置自动使用默认快捷键。

开发者选项 `console` 不在 EXE 中显示。需要调试时在插件目录的 INI 中设置 `console=1`；再次写入插件会保留此值和其他额外配置。

## 使用无限视距助手

运行 `dist/无限视距助手.exe`，按系统提示授予管理员权限，无需安装 Python 或另外携带 DLL。

1. 退出游戏进程，可以保留英雄联盟客户端。
2. 点击“选择目录”，选择包含 `League of Legends.exe` 的 `Game` 目录或上一级安装目录；也可使用“自动查找”。
3. 在“加载器”栏查看检测结果。无加载器时，选择 `version.dll` 或 `dinput8.dll` 并点击“写入加载器”。
4. 设置四项功能的初始开关，并点击各快捷键输入框按键绑定。
5. 点击“写入视距插件”，仅写入插件 DLL 和配置。下次进入对局时加载，在游戏内按快捷键随时切换。修改初始状态或快捷键绑定后需重新写入并启动游戏进程。

加载器栏位于游戏目录与功能开关之间。选择目录、手动修改目录、写入或清理后会刷新检测结果，界面打开期间每两秒自动刷新：

- 无本项目加载器：提示“无加载器，视距插件不生效”。
- 只有一个本项目加载器：显示当前使用的 `version.dll` 或 `dinput8.dll`，并在选择目录时选中它。
- 两个本项目加载器同时存在：提示“存在多个加载器”。写入时阻止新增第二个本项目加载器；切换类型需先清理原加载器。
- 同名文件与内置资源不同：不计入本项目加载器数量，分别提示“version.dll 加载器已被其他 DLL 占用”或“dinput8.dll 加载器已被其他 DLL 占用”，跳过写入和清理。另一未被占用的名称仍可用于部署本项目加载器。

两个清理按钮均需先退出游戏：

- **清理加载器**：位于加载器栏，只删除当前选中且内容匹配的加载器，保留视距插件与配置。插件目录仍有其他 DLL 时停止清理，避免影响共用加载器的其他插件。
- **清理插件**：位于“打开插件目录”右侧，删除 `LolInfiniteVision.dll` 和同目录配置，保留加载器与其他插件。

清理时先将目标文件移入暂存目录；文件被占用等导致移动失败时恢复已移动的文件。重复清理已经移除的文件会直接成功。

默认部署布局：

```text
Game/
  version.dll  # 或 dinput8.dll，单独写入其中一种
  load/
    LolInfiniteVision.dll
    LolInfiniteVision.ini
```

若 `load.ini` 已设置 `[Config] LoadPath`，沿用游戏目录内的相对子目录。内容相同的加载器直接复用；被其他 DLL 占用的同名文件不覆盖、不清理。与其他插件使用同一个加载器时，可将多个插件放在同一个插件目录。

只更新无限视距的 DLL 和配置；保留其他插件及已有配置中的额外设置。文件先写入暂存目录，再逐一替换；写入失败时尝试恢复原文件。游戏运行时阻止写入。“写入完成”仅表示文件部署成功，游戏中的功能是否生效需进入对局验证。

## 构建方式

安装 Visual Studio 2022 / 2026 或 Build Tools，包含 MSVC x64/x86、Windows SDK 和 C++ CMake 工具。在任意目录运行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\dev\ClionProjects\LolInfiniteVision\tools\Build.ps1"
```

脚本自动查找工具链，编译 DLL 和原生助手，执行快捷键、原生攻击范围开关、部署与清理回滚、界面控件和启动、最终 EXE 部署测试。测试成功后输出 `dist/Release/`，并将单文件助手复制到 `dist/无限视距助手.exe`。非管理员终端会跳过需要管理员权限的 EXE 测试。

也可通过 CMake 预设构建，例如 Visual Studio 2022：

```powershell
cmake --preset vs2022-x64-release
cmake --build --preset build-vs2022-release
ctest --preset test-vs2022-release
```

预设构建输出位于对应的 `build/` 目录；使用 `tools/Build.ps1` 生成发布目录。修改 DLL、INI 或加载器后重新构建，内嵌资源会自动更新。

## 项目结构

- `src/`：C++ 源码与头文件
- `installer/`：原生助手界面、文件部署与资源清单
- `installer/assets/version.dll` / `dinput8.dll`：可供其他插件共用的已有加载器
- `tools/`：构建、资源生成和 EXE 验证脚本
- `tests/`：部署、配置保留、冲突检查和失败回滚测试
- `dist/`：单文件助手及发布目录
- `build/`：CMake 构建输出目录
- 根目录：`CMakeLists.txt`、`CMakePresets.json`、`LolInfiniteVision.ini`

## 代码模块

- `dllmain.cpp`：DLL 入口、配置读取、定时初始化
- `FeatureControls.cpp` / `Hotkeys.h`：初始状态、窗口前台检查、快捷键切换、可恢复补丁
- `AttackRange.cpp` / `OverlayEventPump.cpp`：原生攻击范围事件与窗口线程调度
- `GameTime.cpp`：时间特征码扫描与时间基址解析
- `Method.cpp`：控制台输出、模块地址范围、特征码扫描、配置读取
- `Memory.cpp`：进程内存读写封装
- `Constant.cpp`：特征码、补丁字节和配置文件名常量

## 运行说明

当前逻辑依赖目标进程的模块布局与特征码匹配结果。若特征码未命中，程序会输出告警并跳过对应补丁。

攻击范围通过特征码定位 `FN_DispatchCommand` 和 `evtToggleShowOverlayAttackRange` 的原生处理器，从匹配指令的 RIP 相对地址解析全局入口；不校验游戏版本号、时间戳或固定 RVA。特征码缺失、重复命中或解析地址越界时会输出具体告警并跳过。

游戏时间和本地玩家就绪后，开启状态在游戏窗口所属线程每 750 毫秒派发一次开启事件；关闭时派发一次隐藏事件并停止刷新。每次派发临时选择处理器的开启或关闭分支，结束后恢复原始指令。圆圈仍由游戏绘制，不需要按空格。此项对应游戏设置中的普通攻击距离圈。

当前实现与离线复核方式见 [原生攻击距离事件](docs/native-attack-range.md)。离线测试使用自建镜像与测试窗口，不接触游戏进程；游戏内显示仍需实际进入对局验证。
