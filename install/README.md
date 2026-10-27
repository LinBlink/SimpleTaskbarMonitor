# TrafficMonitor Lite 运行目录

源码入口：`{{SOURCE_DIR}}`。所有代码修改、测试和提交均先在源码仓库完成，再部署到这里。

主程序为 Lite 构建；当前正在运行。配置固定在本目录的 `config.ini` 和 `global_cfg.ini`，颜色按项目 ID 保存。CPU/GPU 温度由 `sensors/` 中从源码构建的采集进程提供。

AI 按服务合并显示，例如 Codex 上行 `CX: 17% 3.1h`、下行 `周 54% 100.1h`，分别表示 5h 和周剩余额度、距重置时间。MiniMax 和 Claude 使用相同顺序，Cursor 为总体剩余%和月重置天数。

AI 使用紧凑双行：上行显示 5h 剩余和重置时间，下行“周”显示周剩余和重置时间。Cursor 上行为剩余%，下行“月”为重置天数。百分比、时间按固定列对齐，无逗号和逐值星号；标签旁橙点表示该行有过期数据，详情见悬浮提示。

运行期间观察到 MiniMax、Codex 或 Claude 的 5h 重置时间到达时，对应项目背景以 500 毫秒间隔闪烁，30 秒后自动停止。启动时已过期的缓存不触发提醒；周/月重置不触发。

右键任务栏 → “显示设置”选择项目；“内置功能”配置 AI 用量和双倒数日。

开发与部署命令应在源码仓库执行：

```powershell
.\test.cmd
.\build.ps1
.\deploy.ps1 -Destination C:\_FLEX\TrafficMonitor
```

部署来源和提交记录在 `.deployment.json`，配置与程序备份在 `artifacts/source-deploy-时间戳/`。账号配置、密钥和缓存保留原样。
