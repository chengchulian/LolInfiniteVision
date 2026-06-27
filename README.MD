# Lol Infinite Vision

一个基于 DLL 注入的 Windows/C++ 项目。当前版本的核心流程是：注入后扫描时间特征码，定位游戏时间基址，等待游戏时间可用，然后按配置对若干特征码位置进行补丁写入。

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

## 构建方式

项目使用 CMake 构建。

Visual Studio 预设：

```powershell
cmake --preset vs2026-x64-debug
cmake --build --preset build-x64-debug
```

Ninja 预设：

```powershell
cmake --preset ninja-release
cmake --build --preset build-ninja-release
```

## 项目结构

- `src/`：C++ 源码与头文件
- `build/`：CMake 构建输出目录
- 根目录：`CMakeLists.txt`、`CMakePresets.json`、`LolInfiniteVision.ini`

## 代码模块

- `dllmain.cpp`：DLL 入口、配置读取、定时初始化、补丁调度
- `GameTime.cpp`：时间特征码扫描与时间基址解析
- `Method.cpp`：控制台输出、模块地址范围、特征码扫描、配置读取
- `Memory.cpp`：进程内存读写封装
- `Constant.cpp`：特征码、补丁字节和配置路径常量

## 运行说明

当前逻辑依赖目标进程的模块布局与特征码匹配结果。若特征码未命中，程序会输出告警并跳过对应补丁。
