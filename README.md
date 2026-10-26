# agino-gpu

agino 的双显卡切换：在“集显 + 独显”的机器上，按使用场景选择和切换显卡。

- **混合**（默认）：集显负责桌面，独显空闲时休眠；指定的应用用独显，不用注销。
- **集显**：完全不用独显，省电（重启生效）。
- **独显优先**：所有程序由独显渲染（注销后生效）。
- **独显直连**：有 MUX 的机型上屏幕直接接独显（重启生效）。

组成：系统服务 `agino-gpud`、命令行 `agino-gpu-run`、dde-tray-loader 托盘插件和快捷面板磁贴、deepin 控制中心“显示 → 显卡”页面；启动器和任务栏的“使用独立显卡运行”右键菜单由 agino 的 dde-launchpad、dde-shell、dde-application-manager 补丁提供。

目标环境：agino 26（Debian 13 + DDE，X11）。当前状态：设计阶段，见 [docs/design.md](docs/design.md)。

许可证：GPL-3.0-or-later（见 [COPYING](COPYING)）。
