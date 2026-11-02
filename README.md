# SimpleTaskbarMonitor 任务栏版（原 TrafficMonitor Lite）

源码只在本仓库维护，运行目录为 `C:\Users\wangb\Developer\GIT_PROJECTS\SimpleTaskbarMonitor\run`。先修改、测试并提交源码，再部署；不使用旧版主程序，不切换到标准版构建。

保留任务栏网速、CPU/内存等监控，以及内置 AI 用量、双倒数日、电池和功耗。四项功能直接链接进主程序，不扫描外部插件 DLL。

## AI 按服务合并显示

同一 AI 只占一个显示项目：

- Codex：`CX:17%,3.1h,54%,100.1h`
- MiniMax：`MM:17%,3.1h,54%,100.1h`
- Claude：`CL:17%,3.1h,54%,100.1h`
- Cursor：`CU:54%,15.2d`

前三项的四个数值依次是 **5 小时剩余%、5 小时重置剩余小时、周剩余%、周重置剩余小时**。Cursor 使用总体剩余比例和月账期重置天数。未知值显示 `--`，查询失败或过期的旧值带 `*`，到期但尚未查询确认的倒计时为 `0.0h*`。

旧的分散 AI 项目自动归并到对应服务；颜色继承该服务原短周期项目的颜色（Cursor 继承总体额度项目），已设置的合并项目颜色优先。原有显示勾选与顺序会一并迁移。

## 配置与颜色

所有构建（包括 Debug）固定读写 `SimpleTaskbarMonitor.exe` 同目录的 `config.ini` 和 `global_cfg.ini`。工作目录、AppData、旧 `portable_mode=false` 设置都不能改变这个位置。配置目录不可写时不会回退到其他目录。

`install/config.ini` 和 `install/global_cfg.ini` 是默认配置，与 `C:\Users\wangb\Developer\GIT_PROJECTS\SimpleTaskbarMonitor\run` 当前生效配置保持一致。部署时若目标目录缺少对应配置文件，会从 `install/` 复制一份作为起点；已有配置绝不被覆盖。

文本颜色仍使用原有 `task_bar_text_color`、预设 `defaultN_text_color` 数值，新增对应的 `_item_ids` 元数据。增删、重排温度或 AI 项目后按 ID 对齐颜色，避免按序号读错。部署前自动备份程序目录配置；对旧配置做一次颜色及顺序迁移，并保留当前勾选的 AI/倒数日项目。

## Lite 中的温度采集

继续使用 `Release (lite)` / `Debug (lite)` 和 `WITHOUT_TEMPERATURE` 编译主程序。CPU、GPU 温度作为新增 Lite 能力，由本仓库 `helpers/TemperatureWorker.cs` 构建的隔离进程采集；复用已随源码提供的 LibreHardwareMonitor 传感器库，不加载旧 `OpenHardwareMonitorApi.dll`，不启动旧 TrafficMonitor。

设置中的 CPU/GPU 监控开关控制采集，任务栏“显示设置”控制显示。没有可用读数或数据过期时显示 `--`。采集进程有父进程与 Windows Job 约束，并支持退出信号；结果保存在程序目录 `temperature.cache.ini`。传感器库保留其原有许可及硬件访问要求。

## 开发与部署

需要 Visual Studio C++、MFC、Windows SDK、.NET Framework 4.x C# 编译器和 Python 3。

```powershell
.\build.ps1 -Configuration "Debug (lite)"
.\test.cmd
.\build.ps1
.\deploy.ps1
```

产物位于 `artifacts/build/x64/配置名/`。部署脚本会关闭安装目录中的当前实例，保存并备份其配置，然后复制从本仓库构建的 Lite 主程序、AI 查询助手和温度采集进程，再启动新版。部署标记记录本仓库路径、Git 提交和构建类型。

AI 用量认证仍由现有查询助手处理，配置、加密密钥和缓存不随构建复制。双倒数日配置保留在 `plugins/Countdown.ini`。功耗是电池端充放电功率：正值为充电、负值为放电，不代表整机插座功耗。

## 验证

`test.cmd` 验证网络快照边界、AI 额度解析、倒数日、电池未知值、颜色 ID 对齐、旧配置迁移，以及实际任务栏窗口、功能设置、程序目录保存和退出。测试使用临时程序目录，并断言工作目录和 AppData 配置没有被改写。

可额外运行实际传感器测试：

```powershell
python tests/temperature_smoke.py "artifacts/build/x64/Debug (lite)/SimpleTaskbarMonitor.exe"
```

预先存在的临时诊断改动保存在 `artifacts/before-native-refactor/`。

上游为 [zhongyang219/TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor)，许可见 [LICENSE](./LICENSE)。
