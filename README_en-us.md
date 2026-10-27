**[简体中文](./README.md) | English**

# TrafficMonitor taskbar edition

This branch displays monitoring information in the Windows taskbar. The taskbar window opens automatically and shows upload and download speeds by default. The legacy `show_task_bar_wnd` option is ignored.

## Features

- Network speed, CPU, memory, CPU frequency, GPU usage and disk usage in the taskbar.
- Automatic or manual network adapter selection; configurable items, order, fonts, colors, units and graphs.
- Windows 10 / Windows 11 layouts and secondary taskbars.
- Tray menu, startup settings, languages and monitoring interval.
- Plugins that provide additional taskbar items.
- Optional temperature monitoring in standard builds; Lite builds omit built-in temperature monitoring.

The desktop floating window, main window settings, skins, historical traffic files and statistics, network details dialog, and application/plugin update checks have been removed.

## Usage

1. Run `TrafficMonitor.exe` to show monitoring information in the taskbar.
2. Right-click the display and choose **Display Settings** to select and order items.
3. Choose **Options** to configure taskbar appearance and general settings.
4. Use **Plugin Manager** to manage plugins. Place plugin DLLs in the `plugins` directory beside the executable and restart to load them.
5. Choose **Exit** to quit. Closing the taskbar display window also exits the application.

The tray icon provides the same menu and remains an access point when the taskbar is unavailable. See the [FAQ](./Help_en-us.md) for troubleshooting.

Today's traffic in the tooltip counts only traffic observed during this run. It resets at midnight or on restart and is never saved to a historical traffic file.

![Taskbar display example](./Screenshots/en_us/taskbar.png)

## Build

Install Visual Studio 2022 with Desktop development with C++, MFC and the Windows SDK. Open `TrafficMonitor_Lite.sln`, or build the recommended Lite edition from a developer shell:

```powershell
msbuild TrafficMonitor_Lite.sln /m /p:Configuration="Release (lite)" /p:Platform=x64
```

Supported solution platforms are `x64`, `x86` and `ARM64EC`; the debug configuration is `Debug (lite)`. With Visual Studio 2026, append `/p:PlatformToolset=v145`. The default toolset is `v143`.

The x64 release executable is written to `Bin/x64/Release (lite)/TrafficMonitor.exe` and requires the matching Microsoft Visual C++ runtime. `TrafficMonitor.sln` retains standard builds and their hardware monitoring dependency.

## Attribution and license

Based on [zhongyang219/TrafficMonitor](https://github.com/zhongyang219/TrafficMonitor). Upstream packages and documentation may include features removed from this branch. This branch's code and documentation define its supported behavior.

See [LICENSE](./LICENSE) and [LICENSE_CN](./LICENSE_CN). Historical release notes remain in [UpdateLog](./UpdateLog/update_log_en-us.md).
