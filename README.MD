# Lol Infinite Vision

Windows x64 的“无限视距助手”，提供无限视距、普通攻击范围、视距防回弹和防御塔范围功能。单文件 EXE 内嵌插件、默认配置及 version / dinput8 两种加载器，支持分别部署和清理。

## 快速开始

1. 运行 `dist/无限视距助手.exe`，按提示授予管理员权限。退出游戏进程，可以保留游戏客户端。
2. 选择包含 `League of Legends.exe` 的 `Game` 目录或其上一级安装目录。
3. 查看加载器状态；无加载器时，在加载器栏选择一种并点击“写入加载器”。
4. 设置功能初始开关，点击快捷键输入框直接按键绑定，然后点击“写入视距插件”。
5. 下次进入对局后，按快捷键切换功能。

| 功能 | 默认快捷键 |
| --- | --- |
| 无限视距 | Ctrl+F6 |
| 普通攻击范围 | Ctrl+F7 |
| 视距防回弹 | Ctrl+F8 |
| 防御塔范围 | Ctrl+F9 |

“写入视距插件”只更新 DLL 和配置，不写入加载器。没有本项目加载器时，界面会提示插件不生效；同名 DLL 被其他文件占用时会显示对应提示并保留该文件。

快捷键只在游戏窗口位于前台时响应。初始开关及绑定保存在插件同目录的 INI 中，游戏内切换不写回配置。开发者选项只通过配置文件修改。

## 文档导航

| 文档 | 内容 |
| --- | --- |
| [使用指南](docs/usage.md) | 目录选择、加载器状态、更新、清理和常见问题。 |
| [配置参考](docs/configuration.md) | 默认 INI、功能开关、按键捕获、键码及开发者选项。 |
| [构建与验证](docs/development.md) | 工具链、构建命令、发布目录、测试和助手命令行。 |
| [架构说明](docs/architecture.md) | 部署助手与游戏内插件的模块分工、执行流程和验证边界。 |
| [原生攻击距离事件](docs/native-attack-range.md) | 攻击范围定位、事件派发及离线复核。 |

## 从源码构建

安装 Visual Studio 2022 / 2026 或对应 Build Tools，包含 MSVC x64/x86、Windows SDK 和 C++ CMake 工具。在仓库根目录执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\Build.ps1
```

构建并测试成功后，单文件助手输出到 `dist/无限视距助手.exe`，完整发布目录为 `dist/Release/`。更多构建方式见[构建与验证](docs/development.md)。

离线测试和文件部署成功不能代替实际对局验证；游戏内效果仍需实测。
