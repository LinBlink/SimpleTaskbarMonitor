**简体中文 | [English](./README_en-us.md)**

# TrafficMonitor 任务栏版

本分支仅在 Windows 任务栏中显示监控信息。启动后自动创建任务栏窗口，默认显示上传和下载速度；旧配置中的 `show_task_bar_wnd` 不再影响启动。

## 保留功能

- 任务栏显示网速、CPU、内存、CPU 频率、显卡和硬盘利用率。
- 自动或手动选择网卡；配置显示项目、顺序、字体、颜色、单位和占用图。
- Windows 10 / Windows 11 任务栏布局，以及副显示器任务栏支持。
- 托盘菜单、开机启动、语言设置与监控间隔设置。
- 为任务栏提供额外显示项目的插件系统。
- 标准构建中的可选温度采集；Lite 构建不含内置温度监控。

已移除桌面悬浮窗、主窗口设置、皮肤系统、历史流量文件与统计界面、网络详情界面、程序和插件的自动更新检查。

## 使用

1. 启动 `TrafficMonitor.exe`，监控信息自动出现在任务栏中。
2. 右键任务栏信息，选择“显示设置”配置显示项目和顺序。
3. 选择“选项”设置任务栏字体、颜色、布局，以及常规参数。
4. 选择“插件管理”管理插件。插件 DLL 放在程序目录的 `plugins` 文件夹中，重启后加载。
5. 选择“退出”关闭程序。关闭任务栏显示窗口也会退出程序。

托盘图标提供同一套菜单。任务栏不可用时保留托盘入口；遇到显示问题请参考[常见问题](./Help.md)。

任务栏提示中的今日流量仅累计本次运行期间的数据，跨日清零，重启程序后重新累计，不写入历史流量文件。

![任务栏显示示例](./Screenshots/taskbar.PNG)

## 构建

使用 Visual Studio 2022 的“使用 C++ 的桌面开发”、MFC 和 Windows SDK 组件，打开 `TrafficMonitor_Lite.sln`。推荐构建不含内置温度监控的 Lite 版本：

```powershell
msbuild TrafficMonitor_Lite.sln /m /p:Configuration="Release (lite)" /p:Platform=x64
```

可选平台为 `x64`、`x86` 和 `ARM64EC`，调试配置为 `Debug (lite)`。如使用 Visual Studio 2026，可追加 `/p:PlatformToolset=v145`；默认工具集为 `v143`。

x64 Release 产物位于 `Bin/x64/Release (lite)/TrafficMonitor.exe`。运行需要对应架构的 Microsoft Visual C++ 运行库。`TrafficMonitor.sln` 保留标准构建及其硬件监控依赖。

## 来源与许可

基于 [zhongyang219/TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor) 精简。上游发布包与文档可能包含本分支已移除的功能，请以本分支代码和说明为准。

许可见 [LICENSE](./LICENSE) 和 [LICENSE_CN](./LICENSE_CN)。历史版本记录保留在 [UpdateLog](./UpdateLog/update_log.md)。
