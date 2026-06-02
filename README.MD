# Lol Infinite Vision

这个项目现在只保留“定位时间特征码”的逻辑。

DLL 注入后会执行一次最小化流程：

1. 获取当前模块的地址范围
2. 扫描 `TIME_SIGNATURE_CODE`
3. 读取相对偏移并计算真正的时间基址
4. 读取当前游戏时间，直到读到大于 `0` 的值

## 构建方式

项目使用 CMake 构建。

```powershell
cmake --preset vs2026-x64-debug
cmake --build --preset build-x64-debug
```

也支持单配置的 Ninja 构建：

```powershell
cmake --preset ninja-release
cmake --build --preset build-ninja-release
```

## 当前保留内容

- 时间特征码扫描
- 模块地址范围获取
- 进程内存读取
- 可选控制台日志输出

## 配置文件

根目录提供默认配置文件 `LolInfiniteVision.ini`：

```ini
[Config]
console=0
visionSwitch=1
attackRange=1
```

- `console=1`：DLL 加载后打开控制台
- `console=0`：DLL 加载后不打开控制台
- `visionSwitch=1`：启用视距功能补丁
- `visionSwitch=0`：禁用视距功能补丁
- `attackRange=1`：启用攻击距离功能补丁
- `attackRange=0`：禁用攻击距离功能补丁

## 目录结构

- `src/`: 所有 C++ 源码和头文件
- `build/`: CMake 构建输出目录
- 根目录: 仓库级配置文件，例如 `CMakeLists.txt`、`CMakePresets.json`、`.gitignore`

## 已移除内容

- 视距修改
- RCX 定位
- 窗口消息接管
- 热键处理
- 配置保存
- 鼠标滚轮同步
