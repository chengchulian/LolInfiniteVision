# 加载器资源

`version.dll` 和 `dinput8.dll` 是可供其他插件共用的已有加载器资源，构建时分别嵌入原生助手，不在此项目中修改加载逻辑。

在加载器栏选择一种并单独写入 `Game/version.dll` 或 `Game/dinput8.dll`。写入视距插件只更新插件与配置，默认放入 `Game/load/`；已有 `load.ini` 时沿用 `[Config] LoadPath` 指定的游戏目录内相对子目录。内容相同的加载器可以复用；同名文件内容不同时提示对应加载器已被其他 DLL 占用，不覆盖、不清理。
