# 加载器资源

`version.dll` 复用相邻 `LolSurrender/installer/assets/version.dll` 中的已有文件，构建时嵌入原生助手，不在此项目中修改加载逻辑。

默认部署到 `Game/version.dll`，插件与配置放入 `Game/load/`；已有 `load.ini` 时沿用 `[Config] LoadPath` 指定的游戏目录内相对子目录。相同加载器可供投降助手与无限视距助手复用，内容不同时拒绝覆盖。
