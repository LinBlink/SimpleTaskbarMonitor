# TrafficMonitor Lite 运行目录

源码入口：`{{SOURCE_DIR}}`。所有代码修改、测试和提交均先在源码仓库完成，再部署到这里。

主程序为 Lite 构建；当前正在运行。配置固定在本目录的 `config.ini` 和 `global_cfg.ini`，颜色按项目 ID 保存。CPU/GPU 温度由 `sensors/` 中从源码构建的采集进程提供。

AI 按服务合并显示，例如 `CX:17%,3.1h,54%,100.1h`，依次表示 5h 剩余%、5h 重置小时、周剩余%、周重置小时。MiniMax 和 Claude 使用相同顺序，Cursor 为总体剩余%和月重置天数。

右键任务栏 → “显示设置”选择项目；“内置功能”配置 AI 用量和双倒数日。

开发与部署命令应在源码仓库执行：

```powershell
.\test.cmd
.\build.ps1
.\deploy.ps1 -Destination C:\_FLEX\TrafficMonitor
```

部署来源和提交记录在 `.deployment.json`，配置与程序备份在 `artifacts/source-deploy-时间戳/`。账号配置、密钥和缓存保留原样。
